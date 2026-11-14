#include "Player/PaleotrenchDiver.h"

#include "Animation/AnimSequence.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Paleotrench.h"
#include "PaleotrenchGameMode.h"
#include "Player/PaleotrenchSwimMovement.h"
#include "UI/PaleotrenchMaskOverlay.h"
#include "UI/PaleotrenchWristDisplay.h"
#include "UObject/ConstructorHelpers.h"
#include "World/PaleotrenchAirPocket.h"
#include "World/PaleotrenchTestTank.h"

#define LOCTEXT_NAMESPACE "PaleotrenchDiver"

namespace
{
	// Q/E lean: maximum head tilt (degrees) and sideways peek slide (cm).
	constexpr double LeanAngle = 12.0;
	constexpr double LeanPeekDistance = 35.0;

	// Wrist computer poses relative to the camera (cm): raised in the lower
	// left of the view, lowered out of frame.
	const FVector WristRaisedLocation(24.0, -6.5, -8.5);
	const FVector WristLoweredLocation(16.0, -13.0, -38.0);
	constexpr float WristRaiseSeconds = 0.3f;
	constexpr float WristHoldSeconds = 0.35f;  // held longer than this: lowers on release
	constexpr float NoticeSeconds = 3.0f;
	constexpr float GlanceSeconds = 1.6f;
	constexpr float BurstStrobeHz = 12.0f;

	// Player model and worn gear (imported from Meshy output; see Tools/Art).
	// Every path is optional: without the content the diver simply has no body.
	const TCHAR* const BodyMeshPath = TEXT("/Game/Art/Diver/Player/SK_Diver.SK_Diver");
	// First-person arm: the left arm cut from the body mesh (same skeleton, finger bones) held in a one-frame pose.
	const TCHAR* const ArmMeshPath = TEXT("/Game/Art/Diver/Player/SK_Diver_FPArm.SK_Diver_FPArm");
	const TCHAR* const ArmPosePath = TEXT("/Game/Art/Diver/Player/A_Diver_FP_LeftArm.A_Diver_FP_LeftArm");
	const TCHAR* const ArmComponentName = TEXT("FirstPersonArm");
	const TCHAR* const ComputerBone = TEXT("LeftForeArm");
	constexpr float ArmFillLumens = 1.6f;
	const TCHAR* const ComputerMeshPath = TEXT("/Game/Art/Diver/Gear/DiveComputer/SM_DiveComputer.SM_DiveComputer");
	// Where the eyes sit in the body mesh (cm; +Y is the face, +Z is up).
	const FVector BodyEyeInMesh(0.0, 14.0, 157.0);
	constexpr double BodyEaseRate = 4.0;
	constexpr double BodyViewDistance = 300.0;

	// First-person wrist: the dive computer model (12 x 6.2 x 8 cm, scaled 1.1)
	// with its screen plane just behind the widget. The model's front (+Y)
	// faces the wrist's +X, so it is yawed -90.
	constexpr double ComputerScale = 1.1;
	const FVector ComputerLocation(-3.12, 0.0, 1.13);
	constexpr float ComputerWidgetScale = 0.01483f;  // 512 px -> 7.6 cm, fitted to the screen glass
	constexpr double ComputerWidgetLift = 0.9;       // cm up, so the widget centres on the glass

	struct FWornGearSpec
	{
		const TCHAR* Name;
		const TCHAR* MeshPath;
		const TCHAR* Bone;
		FVector Location;  // relative to the bone, cm
		FRotator Rotation;
		FVector Scale;
		bool bCastShadow;  // the head lamp and mask sit in the lamp's beam
	};
	// Placed on the body in the editor (reference pose), then baked to bone-relative numbers.
	// A function rather than a static table, so edits to the numbers also apply under Live Coding.
	TArray<FWornGearSpec> MakeWornGearSpecs()
	{
		return TArray<FWornGearSpec>({
			{TEXT("WornMask"), TEXT("/Game/Art/Diver/Gear/DiveMask/SM_DiveMask.SM_DiveMask"), TEXT("Head"),
				FVector(0.146, -4.568, 4.999), FRotator(-3.423, -4.981, -101.967), FVector(1.0, 0.8, 1.0), false},
			{TEXT("WornHeadlamp"), TEXT("/Game/Art/Diver/Gear/Headlamp/SM_Headlamp.SM_Headlamp"), TEXT("Head"),
				FVector(-0.974, -17.245, 4.755), FRotator(77.56, -79.249, -163.909), FVector(0.6), false},
			{TEXT("WornAirTank"), TEXT("/Game/Art/Diver/Gear/AirTank/SM_AirTank.SM_AirTank"), TEXT("Spine01"),
				FVector(-1.36, 14.852, -21.802), FRotator(2.052, -179.209, 95.855), FVector(1.0), true},
			{TEXT("WornBatteryPack"), TEXT("/Game/Art/Diver/Gear/BatteryPack/SM_BatteryPack.SM_BatteryPack"), TEXT("Spine01"),
				FVector(-5.737, 0.659, 18.154), FRotator(2.052, -179.209, 95.855), FVector(1.0), true},
			// Worn so its text reads toward the hand with the top toward the little finger (also the first-person arm's mount).
			{TEXT("WornDiveComputer"), TEXT("/Game/Art/Diver/Gear/DiveComputer/SM_DiveComputer.SM_DiveComputer"), TEXT("LeftForeArm"),
				FVector(-0.899, -17.884, -3.288), FRotator(7.658, -84.819, 103.126), FVector(1.1), true},
			{TEXT("WornKnife"), TEXT("/Game/Art/Diver/Gear/DiveKnife/SM_DiveKnife.SM_DiveKnife"), TEXT("LeftLeg"),
				FVector(8.548, -18.315, 0.459), FRotator(-86.195, 178.164, 98.108), FVector(1.0), true},
		});
	}

	const FLinearColor DiverGreen(0.25f, 1.0f, 0.45f);
	const FLinearColor DiverCyan(0.15f, 0.8f, 1.0f);
	const FLinearColor DiverAmber(1.0f, 0.62f, 0.12f);
	const FLinearColor DiverRed(1.0f, 0.18f, 0.14f);

	// The screen faces the eye; a slight roll reads as a turned wrist.
	FQuat WristFacing(const FVector& Location, double ExtraPitch)
	{
		const FVector ToEye = (-Location).GetSafeNormal();
		const FQuat Facing = FRotationMatrix::MakeFromXZ(ToEye, FVector::UpVector).ToQuat();
		return Facing * FRotator(ExtraPitch, 0.0, -10.0).Quaternion();
	}

	float DiverBlink(float Time, float Hz)
	{
		return FMath::Fmod(Time * Hz, 1.0f) < 0.5f ? 1.0f : 0.0f;
	}

	FLinearColor DiverAlpha(const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Alpha);
	}

	void SetupWristMesh(UStaticMeshComponent* Mesh, UStaticMesh* Shape, USceneComponent* Parent)
	{
		Mesh->SetupAttachment(Parent);
		Mesh->SetStaticMesh(Shape);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetVisibility(false);
		// Lighting channel 1 only: the head lamp (channel 0) would blow the
		// arm out to white at this range; the screen's own glow lights it.
		Mesh->LightingChannels.bChannel0 = false;
		Mesh->LightingChannels.bChannel1 = true;
	}

	// Puts the first-person arm where the dive computer on it coincides with the wrist case. The computer is
	// mounted on the forearm bone exactly as on the body (WornDiveComputer), so the arm follows from the case.
	void AlignFirstPersonArm(USkeletalMeshComponent& Arm, const UStaticMeshComponent& Case)
	{
		const TArray<FWornGearSpec> Specs = MakeWornGearSpecs();
		const FWornGearSpec* Spec = nullptr;
		for (const FWornGearSpec& Candidate : Specs)
		{
			if (FCString::Strcmp(Candidate.Name, TEXT("WornDiveComputer")) == 0)
			{
				Spec = &Candidate;
			}
		}
		if (!Spec)
		{
			return;
		}
		// The pose is a single frame: evaluate it now so the bone transforms are valid.
		Arm.TickAnimation(0.0f, false);
		Arm.RefreshBoneTransforms();
		const FTransform BoneInComponent = Arm.GetSocketTransform(ComputerBone, RTS_Component);
		const FTransform ComputerOnBone(Spec->Rotation, Spec->Location, Spec->Scale);
		const FTransform ComputerInComponent = ComputerOnBone * BoneInComponent;
		Arm.SetRelativeTransform(ComputerInComponent.Inverse() * Case.GetRelativeTransform());
	}

	void TintBasicShape(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			// Matte, so the screen glow inches away does not sheen the surface white.
			Material->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
		}
	}
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
	// The body: the camera sits inside its head, so the player never sees it
	// (owner no-see), but it still casts shadows. The capsule is the collision.
	{
		USkeletalMeshComponent* Body = GetMesh();
		ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> BodyAsset(BodyMeshPath);
		if (BodyAsset.Succeeded())
		{
			Body->SetSkeletalMeshAsset(BodyAsset.Get());
		}
		Body->SetOwnerNoSee(true);
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetGenerateOverlapEvents(false);
		Body->SetCanEverAffectNavigation(false);
	}
	DiveCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DiveCamera"));
	DiveCamera->SetupAttachment(GetCapsuleComponent());
	DiveCamera->bUsePawnControlRotation = false;
	DiveCamera->FieldOfView = Settings.FieldOfView;
	DiveCamera->PostProcessBlendWeight = 1.0f;
	Headlamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlamp"));
	Headlamp->SetupAttachment(DiveCamera);
	Headlamp->SetRelativeLocation(FVector(15, 6, -8));
	Headlamp->IntensityUnits = ELightUnits::Lumens;
	Headlamp->SetIntensity(1400.0f);
	Headlamp->SetAttenuationRadius(2200.0f);
	Headlamp->SetInnerConeAngle(18.0f);
	Headlamp->SetOuterConeAngle(36.0f);
	Headlamp->SetCastShadows(true);
	Headlamp->SetMobility(EComponentMobility::Movable);
	// The lamp follows the camera's position but lags its rotation (see Tick).
	Headlamp->SetUsingAbsoluteRotation(true);
	SwimSettingsTable = TSoftObjectPtr<UDataTable>(
		FSoftObjectPath(TEXT("/Game/Data/DT_SwimTuning.DT_SwimTuning")));

	Gear = CreateDefaultSubobject<UPaleotrenchDiveGear>(TEXT("DiveGear"));

	// Worn gear rides on the body's bones.
	for (const FWornGearSpec& Spec : MakeWornGearSpecs())
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Spec.Name);
		Piece->SetupAttachment(GetMesh(), Spec.Bone);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetRelativeLocationAndRotation(Spec.Location, Spec.Rotation);
		Piece->SetRelativeScale3D(Spec.Scale);
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> PieceMesh(Spec.MeshPath);
		if (PieceMesh.Succeeded())
		{
			Piece->SetStaticMesh(PieceMesh.Get());
		}
		Piece->SetOwnerNoSee(true);
		Piece->SetCastShadow(Spec.bCastShadow);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetGenerateOverlapEvents(false);
		Piece->SetCanEverAffectNavigation(false);
		WornGear.Add(Piece);
	}
	// Debug fill light for pt.Body (the camera is far from the head lamp then).
	BodyViewLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BodyViewLight"));
	BodyViewLight->SetupAttachment(DiveCamera);
	// Held back behind the camera so the nearest part of the body is never much closer than the rest.
	BodyViewLight->SetRelativeLocation(FVector(-250.0, 0.0, 0.0));
	BodyViewLight->SetMobility(EComponentMobility::Movable);
	BodyViewLight->IntensityUnits = ELightUnits::Lumens;
	BodyViewLight->SetIntensity(2400.0f);
	BodyViewLight->SetAttenuationRadius(1200.0f);
	BodyViewLight->SetCastShadows(false);
	BodyViewLight->SetVisibility(false);

	// Greybox forearm with a dive computer strapped to it.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	WristPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WristPivot"));
	WristPivot->SetupAttachment(DiveCamera);
	WristPivot->SetRelativeLocation(WristLoweredLocation);
	WristCase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WristCase"));
	SetupWristMesh(WristCase, CubeMesh.Object, WristPivot);
	WristCase->SetRelativeLocation(FVector(-0.9, 0.0, 0.0));
	WristCase->SetRelativeScale3D(FVector(0.016, 0.094, 0.07));
	Forearm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Forearm"));
	SetupWristMesh(Forearm, CylinderMesh.Object, WristPivot);
	Forearm->SetRelativeLocation(FVector(-5.5, 5.0, -1.0));
	Forearm->SetRelativeRotation(FRotator(0.0, 0.0, 90.0));
	Forearm->SetRelativeScale3D(FVector(0.07, 0.07, 0.28));
	// The real dive computer replaces the greybox case when the model is imported.
	ConstructorHelpers::FObjectFinderOptional<UStaticMesh> ComputerMesh(ComputerMeshPath);
	bRealWristComputer = ComputerMesh.Succeeded();
	if (bRealWristComputer)
	{
		WristCase->SetStaticMesh(ComputerMesh.Get());
		WristCase->SetRelativeLocationAndRotation(ComputerLocation, FRotator(0.0, -90.0, 0.0));
		WristCase->SetRelativeScale3D(FVector(ComputerScale));
	}
	// The diver's own left arm replaces the greybox forearm when it is imported. It is placed in BeginPlay so the
	// dive computer sits on it exactly as on the body (see AlignFirstPersonArm); the pose is a single frame.
	// Not a header member yet: found by name (promote it at the next full build).
	ConstructorHelpers::FObjectFinderOptional<USkeletalMesh> ArmMesh(ArmMeshPath);
	ConstructorHelpers::FObjectFinderOptional<UAnimSequence> ArmPose(ArmPosePath);
	if (bRealWristComputer && ArmMesh.Succeeded() && ArmPose.Succeeded())
	{
		USkeletalMeshComponent* Arm = CreateDefaultSubobject<USkeletalMeshComponent>(ArmComponentName);
		Arm->SetupAttachment(WristPivot);
		Arm->SetSkeletalMeshAsset(ArmMesh.Get());
		Arm->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Arm->AnimationData.AnimToPlay = ArmPose.Get();
		Arm->AnimationData.bSavedLooping = false;
		Arm->AnimationData.bSavedPlaying = false;
		Arm->AnimationData.SavedPosition = 0.0f;
		Arm->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Arm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Arm->SetGenerateOverlapEvents(false);
		Arm->SetCanEverAffectNavigation(false);
		Arm->SetCastShadow(false);
		Arm->SetOnlyOwnerSee(true);
		Arm->SetVisibility(false);
		// Same lighting channel as the rest of the wrist: the head lamp would white the arm out at this range.
		Arm->LightingChannels.bChannel0 = false;
		Arm->LightingChannels.bChannel1 = true;
		// The screen's glow is far too weak to show the arm and hand (and a stronger one would chrome the case), so
		// the arm also has its own dim fill, on lighting channel 2 only.
		Arm->LightingChannels.bChannel2 = true;
		UPointLightComponent* ArmFill = CreateDefaultSubobject<UPointLightComponent>(TEXT("ArmFill"));
		ArmFill->SetupAttachment(WristPivot);
		ArmFill->SetRelativeLocation(FVector(16.0, -6.0, 10.0));  // wrist space: +X toward the eye, +Z up, -Y camera right
		ArmFill->IntensityUnits = ELightUnits::Lumens;
		ArmFill->SetIntensity(ArmFillLumens);
		ArmFill->SetAttenuationRadius(90.0f);
		ArmFill->SetLightColor(FLinearColor(0.7f, 0.85f, 1.0f));
		ArmFill->SetCastShadows(false);
		ArmFill->SetVisibility(false);
		ArmFill->LightingChannels.bChannel0 = false;
		ArmFill->LightingChannels.bChannel1 = false;
		ArmFill->LightingChannels.bChannel2 = true;
		Forearm->SetHiddenInGame(true);
	}
	WristScreen = CreateDefaultSubobject<UWidgetComponent>(TEXT("WristScreen"));
	WristScreen->SetupAttachment(WristPivot);
	WristScreen->SetRelativeLocation(FVector(0.05, 0.0, bRealWristComputer ? ComputerWidgetLift : 0.0));
	WristScreen->SetWidgetSpace(EWidgetSpace::World);
	WristScreen->SetWidgetClass(UPaleotrenchWristDisplay::StaticClass());
	WristScreen->SetDrawSize(FVector2D(UPaleotrenchWristDisplay::PixelWidth, UPaleotrenchWristDisplay::PixelHeight));
	// 512 px wide -> 8 cm on the greybox case, or fitted to the model's glass.
	WristScreen->SetRelativeScale3D(FVector(bRealWristComputer ? ComputerWidgetScale : 0.0156f));
	WristScreen->SetBlendMode(EWidgetBlendMode::Opaque);
	WristScreen->SetTwoSided(true);
	WristScreen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WristScreen->SetCastShadow(false);
	WristScreen->SetVisibility(false);
	WristGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("WristGlow"));
	WristGlow->SetupAttachment(WristPivot);
	WristGlow->SetRelativeLocation(FVector(4.0, 0.0, 0.0));
	WristGlow->IntensityUnits = ELightUnits::Lumens;
	WristGlow->SetIntensity(0.2f);  // a few lux on the arm, like a backlit screen
	WristGlow->SetAttenuationRadius(40.0f);
	WristGlow->SetLightColor(FLinearColor(0.55f, 0.9f, 1.0f));
	WristGlow->SetCastShadows(false);
	WristGlow->SetVisibility(false);
	WristGlow->LightingChannels.bChannel0 = false;
	WristGlow->LightingChannels.bChannel1 = true;
}

void APaleotrenchDiver::BeginPlay()
{
	// Components (including the dive gear and its tuning) begin play here.
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
		UE_LOG(LogPaleotrench, Warning, TEXT("Swim tuning missing/invalid; using safe defaults. Run Tools/Prepare.bat."));
	}
	auto* Movement = CastChecked<UPaleotrenchSwimMovement>(GetCharacterMovement());
	Movement->SetSwimTuning(Settings.ToCore());
	Movement->SetMovementMode(MOVE_Custom, UPaleotrenchSwimMovement::DivingMode);
	// No extra tick prerequisite: the engine already orders the movement
	// component before this actor, and adding the reverse formed a cycle that
	// logged a warning every frame (M1).
	ViewYaw = GetActorRotation().Yaw;
	ViewPitch = ViewRoll = 0.0;
	ViewOrientation = FRotator(ViewPitch, ViewYaw, ViewRoll).Quaternion();
	CameraOrientation = LampOrientation = ViewOrientation;
	CameraBaseLocation = DiveCamera->GetRelativeLocation();
	DiveCamera->SetWorldRotation(CameraOrientation);
	Headlamp->SetWorldRotation(LampOrientation);
	DiveCamera->SetFieldOfView(Settings.FieldOfView);

	// Depth is measured from the test tank's water line until caves (M3)
	// provide their own surface reference.
	for (TActorIterator<APaleotrenchTestTank> It(GetWorld()); It; ++It)
	{
		SurfaceZ = It->GetSurfaceWorldZ();
		break;
	}
	if (!bRealWristComputer)
	{
		TintBasicShape(WristCase, FLinearColor(0.02f, 0.022f, 0.026f));
	}
	BodyOrientation = ViewOrientation;
	ApplyBodyVisibility();
	TintBasicShape(Forearm, FLinearColor(0.010f, 0.011f, 0.013f));
	TInlineComponentArray<USkeletalMeshComponent*> SkeletalComponents(this);
	for (USkeletalMeshComponent* Candidate : SkeletalComponents)
	{
		if (Candidate->GetFName() == FName(ArmComponentName))
		{
			AlignFirstPersonArm(*Candidate, *WristCase);
		}
	}

	UE_LOG(LogPaleotrench, Display, TEXT("M2 diver ready: cruise %.0f cm/s, sprint %.0f cm/s, tuning %s"),
		Settings.CruiseSpeed, Settings.SprintSpeed, bLoaded ? TEXT("CSV") : TEXT("fallback"));

	if (APaleotrenchGameMode* Mode = GetWorld()->GetAuthGameMode<APaleotrenchGameMode>())
	{
		Mode->RegisterDiver(this);
	}
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
	LampWheelAction = Action(TEXT("IA_LampWheel"), EInputActionValueType::Axis1D);
	LampBrighterAction = Action(TEXT("IA_LampBrighter"), EInputActionValueType::Boolean);
	LampDimmerAction = Action(TEXT("IA_LampDimmer"), EInputActionValueType::Boolean);
	BurstAction = Action(TEXT("IA_LampBurst"), EInputActionValueType::Boolean);
	WristAction = Action(TEXT("IA_Wrist"), EInputActionValueType::Boolean);
	ChargeAction = Action(TEXT("IA_ChargeLamp"), EInputActionValueType::Boolean);
	CrankAction = Action(TEXT("IA_CrankPack"), EInputActionValueType::Boolean);
	SwapAction = Action(TEXT("IA_SwapTank"), EInputActionValueType::Boolean);

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
	DiveMapping->MapKey(LampWheelAction, EKeys::MouseWheelAxis);
	DiveMapping->MapKey(LampBrighterAction, EKeys::Gamepad_DPad_Up);
	DiveMapping->MapKey(LampDimmerAction, EKeys::Gamepad_DPad_Down);
	DiveMapping->MapKey(BurstAction, EKeys::X);
	DiveMapping->MapKey(BurstAction, EKeys::Gamepad_FaceButton_Right);
	DiveMapping->MapKey(WristAction, EKeys::Tab);
	DiveMapping->MapKey(WristAction, EKeys::Gamepad_FaceButton_Left);
	DiveMapping->MapKey(ChargeAction, EKeys::C);
	DiveMapping->MapKey(ChargeAction, EKeys::Gamepad_FaceButton_Bottom);
	DiveMapping->MapKey(CrankAction, EKeys::V);
	DiveMapping->MapKey(CrankAction, EKeys::Gamepad_Special_Right);
	DiveMapping->MapKey(SwapAction, EKeys::T);
	DiveMapping->MapKey(SwapAction, EKeys::Gamepad_Special_Left);
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
	Input->BindAction(HeadlampAction, ETriggerEvent::Started, this, &APaleotrenchDiver::ToggleLamp);
	Input->BindAction(LampWheelAction, ETriggerEvent::Triggered, this, &APaleotrenchDiver::LampWheel);
	Input->BindAction(LampBrighterAction, ETriggerEvent::Started, this, &APaleotrenchDiver::LampBrighter);
	Input->BindAction(LampDimmerAction, ETriggerEvent::Started, this, &APaleotrenchDiver::LampDimmer);
	Input->BindAction(BurstAction, ETriggerEvent::Started, this, &APaleotrenchDiver::LampBurst);
	Input->BindAction(WristAction, ETriggerEvent::Started, this, &APaleotrenchDiver::WristPressed);
	Input->BindAction(ChargeAction, ETriggerEvent::Started, this, &APaleotrenchDiver::StartCharge);
	Input->BindAction(CrankAction, ETriggerEvent::Started, this, &APaleotrenchDiver::StartCrank);
	Input->BindAction(SwapAction, ETriggerEvent::Started, this, &APaleotrenchDiver::StartSwap);
	for (const auto Event : {ETriggerEvent::Completed, ETriggerEvent::Canceled})
	{
		Input->BindAction(MoveAction, Event, this, &APaleotrenchDiver::StopMove);
		Input->BindAction(GamepadLookAction, Event, this, &APaleotrenchDiver::StopGamepadLook);
		Input->BindAction(RollAction, Event, this, &APaleotrenchDiver::StopRoll);
		Input->BindAction(SprintAction, Event, this, &APaleotrenchDiver::StopSprint);
		Input->BindAction(WristAction, Event, this, &APaleotrenchDiver::WristReleased);
		Input->BindAction(ChargeAction, Event, this, &APaleotrenchDiver::StopCharge);
		Input->BindAction(CrankAction, Event, this, &APaleotrenchDiver::StopCrank);
		Input->BindAction(SwapAction, Event, this, &APaleotrenchDiver::StopSwap);
	}

	MaskOverlay = CreateWidget<UPaleotrenchMaskOverlay>(PC, UPaleotrenchMaskOverlay::StaticClass());
	if (MaskOverlay)
	{
		MaskOverlay->AddToViewport(1);
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
	Clock += Dt;
	const bool bBlackedOut = Gear->IsBlackedOut();
	const bool bHasAir = Gear->GetOxygen().PressureBar > 0.0;
	const bool bSprinting = bWantsSprint && !MoveInput.IsNearlyZero() && !Gear->IsBusy() && bHasAir && !bBlackedOut;
	const float TurnScale = bSprinting ? Settings.SprintTurnScale : 1.0f;
	if (!bBlackedOut)
	{
		// Mouse input is already a frame delta; only stick/roll rates use dt.
		const FVector2D Look = (MouseDelta * Settings.MouseSensitivity
			+ StickLook * Settings.GamepadTurnRate * Dt) * TurnScale;
		const double PitchSign = Settings.bInvertLookY ? -1.0 : 1.0;
		ViewYaw = FRotator::NormalizeAxis(ViewYaw + Look.X);
		ViewPitch = FMath::Clamp(ViewPitch + Look.Y * PitchSign, -MaxViewPitch, MaxViewPitch);
	}
	// Lean: hold Q/E to tilt, release to drift back level. A critically damped
	// spring gives smooth ease-in/ease-out instead of a constant-rate roll.
	// Q leans left, E leans right (positive roll tips the right side down).
	const double LeanTarget = (Settings.RollRate > 0 && !bBlackedOut) ? RollInput * LeanAngle : 0.0;
	const double LeanOmega = 7.0;
	const double LeanAccel = LeanOmega * LeanOmega * (LeanTarget - ViewRoll) - 2.0 * LeanOmega * LeanVelocity;
	LeanVelocity += LeanAccel * Dt;
	ViewRoll = FMath::Clamp(ViewRoll + LeanVelocity * Dt, -MaxViewRoll, MaxViewRoll);
	ViewOrientation = FRotator(ViewPitch, ViewYaw, ViewRoll).Quaternion();
	MouseDelta = FVector2D::ZeroVector;

	// Air pockets: the head (camera) must be above the water line.
	APaleotrenchAirPocket* Pocket = bBlackedOut
		? nullptr
		: APaleotrenchAirPocket::FindContaining(GetWorld(), DiveCamera->GetComponentLocation());
	const bool bInAir = Pocket != nullptr;
	if (bInAir && !bWasInAir)
	{
		if (APaleotrenchGameMode* Mode = GetWorld()->GetAuthGameMode<APaleotrenchGameMode>())
		{
			Mode->DiverReachedAir(this, Pocket);
		}
	}
	bWasInAir = bInAir;
	DiveSeconds = bInAir ? 0.0f : DiveSeconds + Dt;

	pt::EBreathing Activity = pt::EBreathing::Rest;
	if (Gear->GetTask() == EPaleotrenchHandsTask::Crank)
	{
		Activity = pt::EBreathing::Exertion;
	}
	else if (bSprinting)
	{
		Activity = pt::EBreathing::Sprint;
	}
	else if (!MoveInput.IsNearlyZero())
	{
		Activity = pt::EBreathing::Swim;
	}
	HandleGearEvents(Gear->Step(Dt, Activity, bInAir));
	if (Gear->IsBlackedOut() && !bBlackoutReported)
	{
		bBlackoutReported = true;
		ClearInput();
		if (APaleotrenchGameMode* Mode = GetWorld()->GetAuthGameMode<APaleotrenchGameMode>())
		{
			Mode->DiverBlackedOut(this);
		}
	}

	auto* Movement = CastChecked<UPaleotrenchSwimMovement>(GetCharacterMovement());
	Movement->SetSpeedScale(Gear->GetSwimScale() * (Gear->IsBusy() ? Settings.BusySpeedScale : 1.0f));
	UpdateCameraFeel(Dt, bSprinting);
	UpdateBody(Dt);
	if (!Gear->IsBlackedOut())
	{
		AddMovementInput(ViewOrientation.GetForwardVector(), MoveInput.X);
		AddMovementInput(ViewOrientation.GetRightVector(), MoveInput.Y);
		AddMovementInput(ViewOrientation.GetUpVector(), MoveInput.Z);
	}
	Movement->SetSprinting(bSprinting);

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

	UpdateLamp(Dt);
	UpdateWrist(Dt);
	UpdateDistress(Dt);
	UpdateMaskLeds();
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
	// Breathing quickens as the air runs out.
	const double BreathRate = 0.22 + 0.35 * Gear->GetDistress();
	BreathPhase = FMath::Fmod(BreathPhase + UE_TWO_PI * BreathRate * Dt, UE_TWO_PI);

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
	FVector SweepFrom = Pivot;
	FVector BodyFocus = FVector::ZeroVector;
	if (bBodyView)
	{
		// Debug view (pt.Body): orbit the swimming body, which trails behind the head.
		BodyFocus = Pivot - FRotator(ViewPitch, ViewYaw, 0.0).Vector() * 70.0;
		SweepFrom = BodyFocus;
		Desired = BodyFocus - FRotator(0.0, ViewYaw + BodyOrbitYaw, 0.0).Vector() * BodyViewDistance + FVector(0.0, 0.0, 40.0);
	}
	// Never push the camera into a wall: sweep a small sphere from the head pivot.
	if (const UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DiverPeek), false, this);
		if (World->SweepSingleByChannel(Hit, SweepFrom, Desired, FQuat::Identity, ECC_Camera,
			FCollisionShape::MakeSphere(10.0f), Params))
		{
			Desired = Hit.Location;
		}
	}
	DiveCamera->SetWorldLocation(Desired);
	if (bBodyView)
	{
		DiveCamera->SetWorldRotation(FRotationMatrix::MakeFromX(BodyFocus - Desired).ToQuat());
	}

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

void APaleotrenchDiver::UpdateBody(float Dt)
{
	// The body swims along the view's heading and pitch (not its lean), turning
	// a little slower than the head. The standing mesh is turned to face +X and
	// then pitched face-down, so its head leads and its eyes sit at the camera.
	const FQuat Heading = FRotator(ViewPitch, ViewYaw, 0.0).Quaternion();
	BodyOrientation = FQuat::Slerp(BodyOrientation, Heading, 1.0 - FMath::Exp(-BodyEaseRate * Dt)).GetNormalized();
	const FQuat SwimPose = FRotator(-90.0, 0.0, 0.0).Quaternion() * FRotator(0.0, -90.0, 0.0).Quaternion();
	const FQuat MeshRotation = BodyOrientation * SwimPose;
	const FVector Pivot = GetActorLocation() + GetActorQuat().RotateVector(CameraBaseLocation);
	GetMesh()->SetWorldLocationAndRotation(Pivot - MeshRotation.RotateVector(BodyEyeInMesh), MeshRotation);
}

void APaleotrenchDiver::ApplyBodyVisibility()
{
	// The player's own camera is inside the head, so only the debug view shows the body.
	const bool bHide = !bBodyView;
	GetMesh()->SetOwnerNoSee(bHide);
	for (UStaticMeshComponent* Piece : WornGear)
	{
		Piece->SetOwnerNoSee(bHide);
	}
	BodyViewLight->SetVisibility(bBodyView);
}

void APaleotrenchDiver::SetBodyView(bool bOn, float OrbitYaw)
{
	bBodyView = bOn;
	BodyOrbitYaw = OrbitYaw;
	ApplyBodyVisibility();
}

void APaleotrenchDiver::UpdateLamp(float Dt)
{
	const pt::ELampLevel Lit = Gear->GetLitLevel();
	const FPaleotrenchLampLevelSettings& Level = Gear->GetLampLevel(Lit);
	float Intensity = Level.IntensityLumens;
	if (Lit == pt::ELampLevel::Burst)
	{
		Intensity *= DiverBlink(Clock, BurstStrobeHz);
	}
	else if (Lit != pt::ELampLevel::Off && Gear->IsLampLow())
	{
		// A dying cell browns out at random, more often as it empties.
		const pt::PowerTuning& Tuning = Gear->GetPowerTuning();
		const float Threshold = static_cast<float>(Tuning.LampCellCapacity * Tuning.LowCellFraction);
		const float Empty = 1.0f - FMath::Clamp(static_cast<float>(Gear->GetPower().LampCell) / FMath::Max(Threshold, 0.01f), 0.0f, 1.0f);
		if (FlickerRemaining <= 0.0f && FMath::FRand() < (0.6f + 5.0f * Empty) * Dt)
		{
			FlickerRemaining = FMath::FRandRange(0.04f, 0.2f);
			FlickerDepth = FMath::FRandRange(0.03f, 0.5f);
		}
		if (FlickerRemaining > 0.0f)
		{
			FlickerRemaining -= Dt;
			Intensity *= FlickerDepth;
		}
	}
	else
	{
		FlickerRemaining = 0.0f;
	}
	const bool bOn = Lit != pt::ELampLevel::Off && Intensity > 0.0f;
	if (Headlamp->IsVisible() != bOn)
	{
		Headlamp->SetVisibility(bOn);
	}
	Headlamp->SetIntensity(Intensity);
	Headlamp->SetAttenuationRadius(Level.AttenuationRadius);
	Headlamp->SetInnerConeAngle(Level.InnerConeAngle);
	Headlamp->SetOuterConeAngle(Level.OuterConeAngle);
}

void APaleotrenchDiver::UpdateWrist(float Dt)
{
	// Only the player raises the wrist (Tab / gamepad X); nothing raises it for them.
	const bool bWanted = bWristUp && !Gear->IsBlackedOut();
	WristBlend = FMath::Clamp(WristBlend + (bWanted ? Dt : -Dt) / WristRaiseSeconds, 0.0f, 1.0f);
	const bool bShown = WristBlend > 0.001f;
	if (WristScreen->IsVisible() != bShown)
	{
		WristPivot->SetVisibility(bShown, true);
		WristScreen->SetComponentTickEnabled(bShown);
	}
	if (!bShown)
	{
		return;
	}
	const float Ease = FMath::SmoothStep(0.0f, 1.0f, WristBlend);
	FVector Location = FMath::Lerp(WristLoweredLocation, WristRaisedLocation, static_cast<double>(Ease));
	Location.X += FMath::Sin(Ease * UE_PI) * 3.0;  // a slight arc toward the eye
	const FQuat Rotation = FQuat::Slerp(WristFacing(WristLoweredLocation, -40.0), WristFacing(WristRaisedLocation, 0.0), Ease);
	WristPivot->SetRelativeLocationAndRotation(Location, Rotation);

	auto* Display = Cast<UPaleotrenchWristDisplay>(WristScreen->GetUserWidgetObject());
	if (!Display)
	{
		return;
	}
	const pt::OxygenState& Oxygen = Gear->GetOxygen();
	const pt::OxygenTuning& OxygenTuning = Gear->GetOxygenTuning();
	const pt::PowerState& Power = Gear->GetPower();
	const pt::PowerTuning& PowerTuning = Gear->GetPowerTuning();
	FPaleotrenchWristReadout Readout;
	Readout.PressureBar = static_cast<float>(Oxygen.PressureBar);
	Readout.CapacityBar = static_cast<float>(OxygenTuning.TankCapacityBar);
	Readout.ReserveBar = static_cast<float>(OxygenTuning.ReserveBar);
	Readout.CriticalBar = static_cast<float>(OxygenTuning.CriticalBar);
	Readout.AirMinutes = Gear->GetAirMinutesLeft();
	Readout.DepthMetres = GetDepthMetres();
	Readout.HeadingDegrees = static_cast<float>(ViewYaw);
	Readout.PackFraction = static_cast<float>(Power.Pack / PowerTuning.PackCapacity);
	Readout.LampFraction = static_cast<float>(Power.LampCell / PowerTuning.LampCellCapacity);
	Readout.LampLevel = static_cast<int32>(Gear->GetLitLevel());
	Readout.bLampLow = Gear->IsLampLow();
	Readout.SpareTanks = Oxygen.SpareTanks;
	Readout.MaxSpareTanks = OxygenTuning.MaxSpareTanks;
	Readout.DiveSeconds = DiveSeconds;
	Readout.Time = Clock;

	// One status line, most urgent first.
	switch (Gear->GetTask())
	{
	case EPaleotrenchHandsTask::SwapTank:
		Readout.Status = FText::Format(LOCTEXT("Swapping", "SWAPPING TANK {0}"), FText::AsPercent(Gear->GetSwapFraction()));
		Readout.StatusColor = DiverAmber;
		break;
	case EPaleotrenchHandsTask::ChargeLamp:
		Readout.Status = Power.LampCell >= PowerTuning.LampCellCapacity ? LOCTEXT("LampIsFull", "LAMP FULL")
			: Power.Pack <= 0.0 ? LOCTEXT("ChargePackEmpty", "PACK EMPTY")
			: LOCTEXT("Charging", "CHARGING LAMP");
		Readout.StatusColor = Power.Pack <= 0.0 ? DiverRed : DiverCyan;
		break;
	case EPaleotrenchHandsTask::Crank:
		Readout.Status = Power.Pack >= PowerTuning.PackCapacity ? LOCTEXT("PackIsFull", "PACK FULL") : LOCTEXT("Cranking", "CRANKING PACK");
		Readout.StatusColor = DiverCyan;
		break;
	default:
		if (Clock < NoticeUntil)
		{
			Readout.Status = NoticeText;
			Readout.StatusColor = NoticeColor;
		}
		else if (Gear->IsBreathingAir())
		{
			Readout.Status = LOCTEXT("InAir", "BREATHING AIR");
			Readout.StatusColor = DiverGreen;
		}
		else if (Oxygen.PressureBar <= 0.0)
		{
			Readout.Status = LOCTEXT("NoAirStatus", "NO AIR");
			Readout.StatusColor = DiverRed;
		}
		else if (Oxygen.PressureBar <= OxygenTuning.CriticalBar)
		{
			Readout.Status = LOCTEXT("Critical", "CRITICAL");
			Readout.StatusColor = DiverRed;
		}
		else if (Oxygen.PressureBar <= OxygenTuning.ReserveBar)
		{
			Readout.Status = LOCTEXT("Reserve", "RESERVE");
			Readout.StatusColor = DiverAmber;
		}
		break;
	}
	Display->Refresh(Readout);
}

void APaleotrenchDiver::UpdateDistress(float Dt)
{
	// Low oxygen: tunnel vision, draining colour and a pounding pulse.
	const float Distress = Gear->GetDistress();
	HeartPhase = FMath::Fmod(HeartPhase + Dt * (70.0f + 90.0f * Distress) / 60.0f, 1.0f);
	const float Beat = FMath::Pow(FMath::Max(0.0f, FMath::Sin(HeartPhase * UE_TWO_PI)), 12.0f);
	FPostProcessSettings& Post = DiveCamera->PostProcessSettings;
	const bool bActive = Distress > 0.001f;
	Post.bOverride_VignetteIntensity = bActive;
	Post.VignetteIntensity = 0.35f + 0.9f * Distress + 0.3f * Distress * Beat;
	Post.bOverride_ColorSaturation = bActive;
	const float Saturation = 1.0f - 0.75f * Distress;
	Post.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
	Post.bOverride_SceneFringeIntensity = bActive;
	Post.SceneFringeIntensity = 4.0f * Distress * (0.5f + 0.5f * Beat);
}

void APaleotrenchDiver::UpdateMaskLeds()
{
	if (!MaskOverlay)
	{
		return;
	}
	if (Gear->IsBlackedOut())
	{
		MaskOverlay->SetLeds(FLinearColor::Transparent, FLinearColor::Transparent);
		return;
	}
	const pt::OxygenState& Oxygen = Gear->GetOxygen();
	const pt::OxygenTuning& OxygenTuning = Gear->GetOxygenTuning();
	FLinearColor OxygenLed = DiverAlpha(DiverGreen, 0.35f);
	if (Gear->IsBreathingAir())
	{
		OxygenLed = DiverAlpha(DiverGreen, 0.9f);
	}
	else if (Oxygen.PressureBar <= 0.0)
	{
		OxygenLed = DiverAlpha(DiverRed, DiverBlink(Clock, 6.0f));
	}
	else if (Oxygen.PressureBar <= OxygenTuning.CriticalBar)
	{
		OxygenLed = DiverAlpha(DiverRed, 0.25f + 0.75f * DiverBlink(Clock, 3.0f));
	}
	else if (Oxygen.PressureBar <= OxygenTuning.ReserveBar)
	{
		OxygenLed = DiverAlpha(DiverAmber, 0.2f + 0.8f * DiverBlink(Clock, 1.0f));
	}

	const pt::PowerState& Power = Gear->GetPower();
	const pt::PowerTuning& PowerTuning = Gear->GetPowerTuning();
	FLinearColor PowerLed = FLinearColor::Transparent;
	if (Gear->GetTask() == EPaleotrenchHandsTask::ChargeLamp || Gear->GetTask() == EPaleotrenchHandsTask::Crank)
	{
		PowerLed = DiverAlpha(DiverCyan, 0.8f);
	}
	else if (Power.LampCell <= 0.0 && Power.Pack <= 0.0)
	{
		PowerLed = DiverAlpha(DiverRed, DiverBlink(Clock, 2.0f));
	}
	else if (Gear->IsLampLow() || Power.Pack < PowerTuning.PackCapacity * 0.15)
	{
		PowerLed = DiverAlpha(DiverAmber, 0.2f + 0.8f * DiverBlink(Clock, 1.0f));
	}
	MaskOverlay->SetLeds(OxygenLed, PowerLed);
}

void APaleotrenchDiver::HandleGearEvents(const FPaleotrenchGearEvents& Events)
{
	if (Events.bSwapped)
	{
		PostNotice(LOCTEXT("Swapped", "TANK SWAPPED"), DiverCyan, true);
	}
	if (Events.bCellFull)
	{
		PostNotice(LOCTEXT("LampCharged", "LAMP CHARGED"), DiverCyan, false);
	}
	if (Events.bPackFlat)
	{
		PostNotice(LOCTEXT("PackFlat", "PACK EMPTY"), DiverRed, true);
	}
	if (Events.bLampDied)
	{
		PostNotice(LOCTEXT("LampDied", "LAMP DEAD"), DiverRed, false);
	}
	if (Events.bRanDry)
	{
		PostNotice(LOCTEXT("RanDry", "NO AIR"), DiverRed, true);
	}
}

void APaleotrenchDiver::PostNotice(const FText& Text, const FLinearColor& Color, bool bGlance)
{
	NoticeText = Text;
	NoticeColor = Color;
	NoticeUntil = Clock + NoticeSeconds;
	if (bGlance)
	{
		GlanceUntil = Clock + GlanceSeconds;
	}
}

float APaleotrenchDiver::GetDepthMetres() const
{
	return static_cast<float>((SurfaceZ - DiveCamera->GetComponentLocation().Z) / 100.0);
}

void APaleotrenchDiver::RespawnAt(const FVector& Location, float Yaw)
{
	TeleportTo(Location, FRotator(0.0, Yaw, 0.0), false, true);
	GetCharacterMovement()->StopMovementImmediately();
	ClearInput();
	ViewYaw = Yaw;
	ViewPitch = ViewRoll = LeanVelocity = 0.0;
	ViewOrientation = FRotator(ViewPitch, ViewYaw, ViewRoll).Quaternion();
	CameraOrientation = LampOrientation = BodyOrientation = ViewOrientation;
	DiveCamera->SetWorldRotation(CameraOrientation);
	Headlamp->SetWorldRotation(LampOrientation);
	StrokeAmount = StrafeBank = 0.0;
	DiveSeconds = 0.0f;
	bWasInAir = false;
	bBlackoutReported = false;
	bWristUp = false;
	WristBlend = 0.0f;
	NoticeUntil = GlanceUntil = 0.0f;
	FlickerRemaining = 0.0f;
}

void APaleotrenchDiver::NotifyCheckpointSaved()
{
	PostNotice(LOCTEXT("Checkpoint", "CHECKPOINT SAVED"), DiverGreen, true);
}

bool APaleotrenchDiver::CollectSpareTank()
{
	if (Gear->IsBlackedOut() || !Gear->AddSpareTank())
	{
		return false;
	}
	PostNotice(LOCTEXT("SpareCollected", "SPARE TANK +1"), DiverCyan, true);
	return true;
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
void APaleotrenchDiver::ToggleLamp() { Gear->ToggleLamp(); }
void APaleotrenchDiver::LampBrighter() { Gear->StepLamp(1); }
void APaleotrenchDiver::LampDimmer() { Gear->StepLamp(-1); }
void APaleotrenchDiver::StartCharge() { BeginHandsTask(EPaleotrenchHandsTask::ChargeLamp); }
void APaleotrenchDiver::StopCharge() { Gear->EndTask(EPaleotrenchHandsTask::ChargeLamp); }
void APaleotrenchDiver::StartCrank() { BeginHandsTask(EPaleotrenchHandsTask::Crank); }
void APaleotrenchDiver::StopCrank() { Gear->EndTask(EPaleotrenchHandsTask::Crank); }
void APaleotrenchDiver::StartSwap() { BeginHandsTask(EPaleotrenchHandsTask::SwapTank); }
void APaleotrenchDiver::StopSwap() { Gear->EndTask(EPaleotrenchHandsTask::SwapTank); }

void APaleotrenchDiver::LampWheel(const FInputActionValue& Value)
{
	const float Notches = Value.Get<float>();
	if (Notches != 0.0f)
	{
		Gear->StepLamp(Notches > 0.0f ? 1 : -1);
	}
}

void APaleotrenchDiver::LampBurst()
{
	if (!Gear->IsBlackedOut() && !Gear->Burst() && Gear->GetPower().LampCell <= 0.0)
	{
		PostNotice(LOCTEXT("BurstDead", "LAMP DEAD"), DiverRed, true);
	}
}

void APaleotrenchDiver::BeginHandsTask(EPaleotrenchHandsTask Task)
{
	if (Gear->IsBlackedOut() || Gear->BeginTask(Task) || Gear->IsBusy())
	{
		return;
	}
	if (Task == EPaleotrenchHandsTask::SwapTank)
	{
		PostNotice(LOCTEXT("NoSpare", "NO SPARE TANK"), DiverAmber, true);
	}
	else if (Task == EPaleotrenchHandsTask::ChargeLamp)
	{
		PostNotice(LOCTEXT("NothingToCharge", "PACK EMPTY"), DiverRed, true);
	}
}

void APaleotrenchDiver::WristPressed()
{
	WristPressTime = Clock;
	bWristHeld = true;
	bWristUp = !bWristUp;
	bWristPressRaised = bWristUp;
}

void APaleotrenchDiver::WristReleased()
{
	// A quick tap leaves it raised; a long hold was a glance, so lower it.
	if (bWristHeld && bWristPressRaised && Clock - WristPressTime > WristHoldSeconds)
	{
		bWristUp = false;
	}
	bWristHeld = false;
}

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
	bWristHeld = false;
	if (Gear)
	{
		Gear->CancelTask();
	}
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
	if (MaskOverlay)
	{
		MaskOverlay->RemoveFromParent();
		MaskOverlay = nullptr;
	}
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

#undef LOCTEXT_NAMESPACE
