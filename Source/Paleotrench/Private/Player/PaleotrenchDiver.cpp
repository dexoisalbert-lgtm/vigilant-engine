#include "Player/PaleotrenchDiver.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Paleotrench.h"
#include "Player/PaleotrenchSwimMovement.h"

namespace
{
	// Q/E lean: maximum head tilt (degrees) and sideways peek slide (cm).
	constexpr double LeanAngle = 12.0;
	constexpr double LeanPeekDistance = 35.0;
}

APaleotrenchDiver::APaleotrenchDiver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPaleotrenchSwimMovement>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	// A spherical capsule gives the greybox diver the same clearance at every
	// viewing angle, avoiding rotation tunnelling when rolling near a wall.
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 34.0f);
	GetMesh()->SetHiddenInGame(true);
	DiveCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DiveCamera"));
	DiveCamera->SetupAttachment(GetCapsuleComponent());
	DiveCamera->bUsePawnControlRotation = false;
	DiveCamera->FieldOfView = Settings.FieldOfView;
	Headlamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlamp"));
	Headlamp->SetupAttachment(DiveCamera);
	Headlamp->SetRelativeLocation(FVector(15, 6, -8));
	Headlamp->IntensityUnits = ELightUnits::Lumens;
	Headlamp->SetIntensity(1800.0f);
	Headlamp->SetAttenuationRadius(2200.0f);
	Headlamp->SetInnerConeAngle(18.0f);
	Headlamp->SetOuterConeAngle(36.0f);
	Headlamp->SetCastShadows(true);
	Headlamp->SetMobility(EComponentMobility::Movable);
	// The lamp follows the camera's position but lags its rotation (see Tick).
	Headlamp->SetUsingAbsoluteRotation(true);
	SwimSettingsTable = TSoftObjectPtr<UDataTable>(
		FSoftObjectPath(TEXT("/Game/Data/DT_SwimTuning.DT_SwimTuning")));
}

void APaleotrenchDiver::BeginPlay()
{
	Super::BeginPlay();
	bool bLoaded = false;
	if (const UDataTable* Table = SwimSettingsTable.LoadSynchronous())
	{
		if (const auto* Row = Table->FindRow<FPaleotrenchSwimSettings>(TEXT("Default"), TEXT("Diver")))
		{
			if (Row->IsValid())
			{
				Settings = *Row;
				bLoaded = true;
			}
		}
	}
	if (!bLoaded)
	{
		UE_LOG(LogPaleotrench, Warning, TEXT("Swim tuning missing/invalid; using safe defaults. Run Tools/PrepareM1.bat."));
	}
	auto* Movement = CastChecked<UPaleotrenchSwimMovement>(GetCharacterMovement());
	Movement->SetSwimTuning(Settings.ToCore());
	Movement->SetMovementMode(MOVE_Custom, UPaleotrenchSwimMovement::DivingMode);
	Movement->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	ViewYaw = GetActorRotation().Yaw;
	ViewPitch = ViewRoll = 0.0;
	ViewOrientation = FRotator(ViewPitch, ViewYaw, ViewRoll).Quaternion();
	CameraOrientation = LampOrientation = ViewOrientation;
	CameraBaseLocation = DiveCamera->GetRelativeLocation();
	DiveCamera->SetWorldRotation(CameraOrientation);
	Headlamp->SetWorldRotation(LampOrientation);
	DiveCamera->SetFieldOfView(Settings.FieldOfView);
	UE_LOG(LogPaleotrench, Display, TEXT("M1 diver ready: cruise %.0f cm/s, sprint %.0f cm/s, tuning %s"),
		Settings.CruiseSpeed, Settings.SprintSpeed, bLoaded ? TEXT("CSV") : TEXT("fallback"));
}

void APaleotrenchDiver::BuildInputMapping()
{
	if (DiveMapping)
	{
		return;
	}
	DiveMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Diving"));
	auto Action = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		auto* Result = NewObject<UInputAction>(this, FName(Name));
		Result->ValueType = Type;
		Result->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
		return Result;
	};
	MoveAction = Action(TEXT("IA_Swim"), EInputActionValueType::Axis3D);
	MouseLookAction = Action(TEXT("IA_MouseLook"), EInputActionValueType::Axis2D);
	GamepadLookAction = Action(TEXT("IA_GamepadLook"), EInputActionValueType::Axis2D);
	RollAction = Action(TEXT("IA_Roll"), EInputActionValueType::Axis1D);
	SprintAction = Action(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	LevelAction = Action(TEXT("IA_LevelView"), EInputActionValueType::Boolean);
	HeadlampAction = Action(TEXT("IA_Headlamp"), EInputActionValueType::Boolean);

	auto KeyAxis = [this](UInputAction* InputAction, FKey Key, int32 Axis, bool bNegative)
	{
		auto& Mapping = DiveMapping->MapKey(InputAction, Key);
		if (bNegative)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(DiveMapping));
		}
		if (Axis != 0)
		{
			auto* Swizzle = NewObject<UInputModifierSwizzleAxis>(DiveMapping);
			Swizzle->Order = Axis == 1 ? EInputAxisSwizzle::YXZ : EInputAxisSwizzle::ZYX;
			Mapping.Modifiers.Add(Swizzle);
		}
	};
	KeyAxis(MoveAction, EKeys::W, 0, false);
	KeyAxis(MoveAction, EKeys::S, 0, true);
	KeyAxis(MoveAction, EKeys::D, 1, false);
	KeyAxis(MoveAction, EKeys::A, 1, true);
	KeyAxis(MoveAction, EKeys::SpaceBar, 2, false);
	KeyAxis(MoveAction, EKeys::LeftControl, 2, true);
	KeyAxis(MoveAction, EKeys::Gamepad_RightShoulder, 2, false);
	KeyAxis(MoveAction, EKeys::Gamepad_LeftShoulder, 2, true);
	KeyAxis(RollAction, EKeys::Q, 0, true);
	KeyAxis(RollAction, EKeys::E, 0, false);
	KeyAxis(RollAction, EKeys::Gamepad_DPad_Left, 0, true);
	KeyAxis(RollAction, EKeys::Gamepad_DPad_Right, 0, false);

	auto AddStick = [this](UInputAction* InputAction, FKey Key, bool bSwapXY)
	{
		auto& Mapping = DiveMapping->MapKey(InputAction, Key);
		auto* DeadZone = NewObject<UInputModifierDeadZone>(DiveMapping);
		DeadZone->Type = EDeadZoneType::Radial;
		DeadZone->LowerThreshold = 0.18f;
		DeadZone->UpperThreshold = 1.0f;
		Mapping.Modifiers.Add(DeadZone);
		if (bSwapXY)
		{
			auto* Swizzle = NewObject<UInputModifierSwizzleAxis>(DiveMapping);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
	};
	AddStick(MoveAction, EKeys::Gamepad_Left2D, true);
	AddStick(GamepadLookAction, EKeys::Gamepad_Right2D, false);
	DiveMapping->MapKey(MouseLookAction, EKeys::Mouse2D);
	DiveMapping->MapKey(SprintAction, EKeys::LeftShift);
	DiveMapping->MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);
	DiveMapping->MapKey(LevelAction, EKeys::R);
	DiveMapping->MapKey(LevelAction, EKeys::Gamepad_RightThumbstick);
	DiveMapping->MapKey(HeadlampAction, EKeys::F);
	DiveMapping->MapKey(HeadlampAction, EKeys::Gamepad_FaceButton_Top);
}

void APaleotrenchDiver::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	auto* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	auto* PC = Cast<APlayerController>(GetController());
	if (!ensure(Input && PC && PC->GetLocalPlayer()))
	{
		return;
	}
	RemoveInputMapping();
	BuildInputMapping();
	InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!ensure(InputSubsystem.IsValid()))
	{
		return;
	}
	InputSubsystem->AddMappingContext(DiveMapping, 0);
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APaleotrenchDiver::Move);
	Input->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APaleotrenchDiver::MouseLook);
	Input->BindAction(GamepadLookAction, ETriggerEvent::Triggered, this, &APaleotrenchDiver::GamepadLook);
	Input->BindAction(RollAction, ETriggerEvent::Triggered, this, &APaleotrenchDiver::Roll);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &APaleotrenchDiver::StartSprint);
	Input->BindAction(LevelAction, ETriggerEvent::Started, this, &APaleotrenchDiver::LevelView);
	Input->BindAction(HeadlampAction, ETriggerEvent::Started, this, &APaleotrenchDiver::ToggleHeadlamp);
	for (const auto Event : {ETriggerEvent::Completed, ETriggerEvent::Canceled})
	{
		Input->BindAction(MoveAction, Event, this, &APaleotrenchDiver::StopMove);
		Input->BindAction(GamepadLookAction, Event, this, &APaleotrenchDiver::StopGamepadLook);
		Input->BindAction(RollAction, Event, this, &APaleotrenchDiver::StopRoll);
		Input->BindAction(SprintAction, Event, this, &APaleotrenchDiver::StopSprint);
	}
}

void APaleotrenchDiver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsLocallyControlled())
	{
		return;
	}
	const float Dt = FMath::Clamp(DeltaSeconds, 0.0f, 0.25f);
	const bool bSprinting = bWantsSprint && !MoveInput.IsNearlyZero();
	const float TurnScale = bSprinting ? Settings.SprintTurnScale : 1.0f;
	// Mouse input is already a frame delta; only stick/roll rates use dt.
	const FVector2D Look = (MouseDelta * Settings.MouseSensitivity
		+ StickLook * Settings.GamepadTurnRate * Dt) * TurnScale;
	const double PitchSign = Settings.bInvertLookY ? -1.0 : 1.0;
	ViewYaw = FRotator::NormalizeAxis(ViewYaw + Look.X);
	ViewPitch = FMath::Clamp(ViewPitch + Look.Y * PitchSign, -MaxViewPitch, MaxViewPitch);
	// Lean: hold Q/E to tilt, release to drift back level. A critically damped
	// spring gives smooth ease-in/ease-out instead of a constant-rate roll.
	// Q leans left, E leans right (positive roll tips the right side down).
	const double LeanTarget = Settings.RollRate > 0 ? RollInput * LeanAngle : 0.0;
	const double LeanOmega = 7.0;
	const double LeanAccel = LeanOmega * LeanOmega * (LeanTarget - ViewRoll) - 2.0 * LeanOmega * LeanVelocity;
	LeanVelocity += LeanAccel * Dt;
	ViewRoll = FMath::Clamp(ViewRoll + LeanVelocity * Dt, -MaxViewRoll, MaxViewRoll);
	ViewOrientation = FRotator(ViewPitch, ViewYaw, ViewRoll).Quaternion();
	MouseDelta = FVector2D::ZeroVector;
	UpdateCameraFeel(Dt, bSprinting);
	AddMovementInput(ViewOrientation.GetForwardVector(), MoveInput.X);
	AddMovementInput(ViewOrientation.GetRightVector(), MoveInput.Y);
	AddMovementInput(ViewOrientation.GetUpVector(), MoveInput.Z);
	CastChecked<UPaleotrenchSwimMovement>(GetCharacterMovement())->SetSprinting(bSprinting);

	if (!MoveInput.IsNearlyZero() && GetVelocity().SizeSquared() > 100.0)
	{
		KickElapsed += Dt * (bSprinting ? 1.5f : 1.0f);
		if (KickElapsed >= Settings.FinKickInterval)
		{
			KickElapsed = FMath::Fmod(KickElapsed, Settings.FinKickInterval);
			OnFinKick.Broadcast(GetActorLocation(), bSprinting ? 2.0f : 1.0f);
		}
	}
	else
	{
		KickElapsed = 0;
	}
}

void APaleotrenchDiver::UpdateCameraFeel(float Dt, bool bSprinting)
{
	const auto Ease = [Dt](double Rate) { return 1.0 - FMath::Exp(-Rate * Dt); };

	// Head weight: the camera eases toward the intended view (~50 ms).
	CameraOrientation = FQuat::Slerp(CameraOrientation, ViewOrientation, Ease(20.0)).GetNormalized();

	// Swim rhythm: bob strength follows actual speed; one bob per fin kick.
	const FVector Velocity = GetVelocity();
	const double SpeedFraction = FMath::Clamp(Velocity.Size() / FMath::Max(Settings.CruiseSpeed, 1.0f), 0.0, 1.5);
	const double StrokeTarget = MoveInput.IsNearlyZero() ? 0.0 : SpeedFraction;
	StrokeAmount = FMath::Lerp(StrokeAmount, StrokeTarget, Ease(3.0));
	const double KickRate = (bSprinting ? 1.5 : 1.0) / FMath::Max(Settings.FinKickInterval, 0.1f);
	StrokePhase = FMath::Fmod(StrokePhase + UE_TWO_PI * KickRate * Dt, UE_TWO_PI * 2.0);
	BreathPhase = FMath::Fmod(BreathPhase + UE_TWO_PI * 0.22 * Dt, UE_TWO_PI);

	// Slight bank into strafes, like a body tilting as it moves sideways.
	const double Lateral = FVector::DotProduct(Velocity, ViewOrientation.GetRightVector())
		/ FMath::Max(Settings.CruiseSpeed, 1.0f);
	StrafeBank = FMath::Lerp(StrafeBank, FMath::Clamp(Lateral, -1.5, 1.5) * 2.5, Ease(4.0));

	const double Breath = FMath::Sin(BreathPhase);
	const double Stroke = FMath::Sin(StrokePhase);
	const double Sway = FMath::Sin(StrokePhase * 0.5);
	const FQuat SwayRotation = FRotator(
		Breath * 0.35 + Stroke * 0.6 * StrokeAmount,
		Sway * 0.4 * StrokeAmount,
		StrafeBank + Sway * 0.8 * StrokeAmount).Quaternion();
	const FQuat Shown = CameraOrientation * SwayRotation;
	DiveCamera->SetWorldRotation(Shown);

	const FVector ViewOffset(0.0, Sway * 1.2 * StrokeAmount, Breath * 0.9 + Stroke * 1.8 * StrokeAmount);
	// Peek: leaning also slides the head sideways (horizontally, along the
	// heading) so the player can look around corners. The slide follows the
	// sprung lean angle, so it eases in and out with the tilt.
	const FVector PeekOffset = FRotator(0.0, ViewYaw, 0.0).Quaternion().GetRightVector()
		* (FMath::Clamp(ViewRoll / LeanAngle, -1.0, 1.0) * LeanPeekDistance);
	const FVector Pivot = GetActorLocation() + GetActorQuat().RotateVector(CameraBaseLocation);
	FVector Desired = Pivot + PeekOffset + CameraOrientation.RotateVector(ViewOffset);
	// Never push the camera into a wall: sweep a small sphere from the head pivot.
	if (const UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DiverPeek), false, this);
		if (World->SweepSingleByChannel(Hit, Pivot, Desired, FQuat::Identity, ECC_Camera,
			FCollisionShape::MakeSphere(10.0f), Params))
		{
			Desired = Hit.Location;
		}
	}
	DiveCamera->SetWorldLocation(Desired);

	// Flashlight: trails the view when turning (~0.2 s) but never more than 15°.
	LampOrientation = FQuat::Slerp(LampOrientation, Shown, Ease(9.0)).GetNormalized();
	const double MaxLampLag = FMath::DegreesToRadians(15.0);
	const double LampLag = LampOrientation.AngularDistance(Shown);
	if (LampLag > MaxLampLag)
	{
		LampOrientation = FQuat::Slerp(Shown, LampOrientation, MaxLampLag / LampLag).GetNormalized();
	}
	Headlamp->SetWorldRotation(LampOrientation);
}

FVector APaleotrenchDiver::GetPawnViewLocation() const { return DiveCamera->GetComponentLocation(); }
FRotator APaleotrenchDiver::GetViewRotation() const { return ViewOrientation.Rotator(); }
void APaleotrenchDiver::Move(const FInputActionValue& Value) { MoveInput = Value.Get<FVector>().GetClampedToMaxSize(1.0); }
void APaleotrenchDiver::StopMove() { MoveInput = FVector::ZeroVector; }
void APaleotrenchDiver::MouseLook(const FInputActionValue& Value) { MouseDelta += Value.Get<FVector2D>(); }
void APaleotrenchDiver::GamepadLook(const FInputActionValue& Value) { StickLook = Value.Get<FVector2D>(); }
void APaleotrenchDiver::StopGamepadLook() { StickLook = FVector2D::ZeroVector; }
void APaleotrenchDiver::Roll(const FInputActionValue& Value) { RollInput = FMath::Clamp(Value.Get<float>(), -1.0f, 1.0f); }
void APaleotrenchDiver::StopRoll() { RollInput = 0; }
void APaleotrenchDiver::StartSprint() { bWantsSprint = true; }
void APaleotrenchDiver::StopSprint() { bWantsSprint = false; }
void APaleotrenchDiver::ToggleHeadlamp() { Headlamp->ToggleVisibility(); }

void APaleotrenchDiver::LevelView()
{
	// Explicit comfort control: reset pitch/roll while retaining heading.
	ViewPitch = ViewRoll = LeanVelocity = 0.0;
	ViewOrientation = FRotator(ViewPitch, ViewYaw, ViewRoll).Quaternion();
}

void APaleotrenchDiver::ClearInput()
{
	StopMove();
	StopGamepadLook();
	StopRoll();
	StopSprint();
	MouseDelta = FVector2D::ZeroVector;
	KickElapsed = 0;
	ConsumeMovementInputVector();
	CastChecked<UPaleotrenchSwimMovement>(GetCharacterMovement())->SetSprinting(false);
}

void APaleotrenchDiver::RemoveInputMapping()
{
	if (InputSubsystem.IsValid() && DiveMapping)
	{
		InputSubsystem->RemoveMappingContext(DiveMapping);
	}
	InputSubsystem.Reset();
	ClearInput();
}

void APaleotrenchDiver::UnPossessed()
{
	RemoveInputMapping();
	Super::UnPossessed();
}

void APaleotrenchDiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveInputMapping();
	Super::EndPlay(EndPlayReason);
}
