#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Player/PaleotrenchDiveGear.h"
#include "Player/PaleotrenchSwimSettings.h"
#include "PaleotrenchDiver.generated.h"

class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UPaleotrenchMaskOverlay;
class UPointLightComponent;
class USkeletalMeshComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPaleotrenchFinKick, FVector, Location, float, Strength);

UCLASS()
class PALEOTRENCH_API APaleotrenchDiver : public ACharacter
{
	GENERATED_BODY()

public:
	APaleotrenchDiver(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual FVector GetPawnViewLocation() const override;
	virtual FRotator GetViewRotation() const override;
	virtual void UnPossessed() override;

	UPaleotrenchDiveGear* GetGear() const { return Gear; }
	float GetViewYaw() const { return static_cast<float>(ViewYaw); }

	// Game mode: checkpoint loads, respawns and new dives.
	void RespawnAt(const FVector& Location, float Yaw);
	void NotifyCheckpointSaved();
	// Spare-tank pickups: false when there is no room on the harness.
	bool CollectSpareTank();
	// Console pt.Wrist: same as tapping the wrist key.
	void ToggleWrist() { bWristUp = !bWristUp; }
	// Console pt.Body: debug view from outside, to see the body and its gear.
	// OrbitYaw 0 is behind the diver, 180 in front, 90 the diver's right side.
	void SetBodyView(bool bOn, float OrbitYaw);
	bool IsBodyView() const { return bBodyView; }

	// M4 connects this to the typed Sound stimulus system. No AI/audio yet.
	UPROPERTY(BlueprintAssignable, Category = "Diving")
	FPaleotrenchFinKick OnFinKick;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Diving")
	TObjectPtr<UCameraComponent> DiveCamera;
	UPROPERTY(VisibleAnywhere, Category = "Diving")
	TObjectPtr<USpotLightComponent> Headlamp;
	UPROPERTY(VisibleAnywhere, Category = "Diving")
	TObjectPtr<UPaleotrenchDiveGear> Gear;
	UPROPERTY(VisibleAnywhere, Category = "Wrist")
	TObjectPtr<USceneComponent> WristPivot;
	UPROPERTY(VisibleAnywhere, Category = "Wrist")
	TObjectPtr<UStaticMeshComponent> Forearm;
	UPROPERTY(VisibleAnywhere, Category = "Wrist")
	TObjectPtr<UStaticMeshComponent> WristCase;
	UPROPERTY(VisibleAnywhere, Category = "Wrist")
	TObjectPtr<UWidgetComponent> WristScreen;
	UPROPERTY(VisibleAnywhere, Category = "Wrist")
	TObjectPtr<UPointLightComponent> WristGlow;
	// Worn gear on the body mesh (mask, head lamp, tank, battery pack, dive
	// computer, knife). Hidden from the player's own camera; they cast shadows.
	UPROPERTY(VisibleAnywhere, Category = "Body")
	TArray<TObjectPtr<UStaticMeshComponent>> WornGear;
	// Fill light for the pt.Body debug view only.
	UPROPERTY(VisibleAnywhere, Category = "Body")
	TObjectPtr<UPointLightComponent> BodyViewLight;
	// The player's own hands in first person (null when the hand meshes are not imported). Placed on
	// the camera exactly as in the Blender scene they were posed in; see HandPlacement in the .cpp.
	UPROPERTY(VisibleAnywhere, Category = "Hands")
	TObjectPtr<USkeletalMeshComponent> LeftHand;
	UPROPERTY(VisibleAnywhere, Category = "Hands")
	TObjectPtr<USkeletalMeshComponent> RightHand;
	UPROPERTY(VisibleAnywhere, Category = "Hands")
	TObjectPtr<UPointLightComponent> HandFill;
	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	TSoftObjectPtr<UDataTable> SwimSettingsTable;

private:
	void BuildInputMapping();
	void ClearInput();
	void RemoveInputMapping();
	void Move(const FInputActionValue& Value);
	void StopMove();
	void MouseLook(const FInputActionValue& Value);
	void GamepadLook(const FInputActionValue& Value);
	void StopGamepadLook();
	void Roll(const FInputActionValue& Value);
	void StopRoll();
	void StartSprint();
	void StopSprint();
	void LevelView();
	void ToggleLamp();
	void LampWheel(const FInputActionValue& Value);
	void LampBrighter();
	void LampDimmer();
	void LampBurst();
	void WristPressed();
	void WristReleased();
	void StartCharge();
	void StopCharge();
	void StartCrank();
	void StopCrank();
	void StartSwap();
	void StopSwap();
	void BeginHandsTask(EPaleotrenchHandsTask Task);

	void UpdateCameraFeel(float Dt, bool bSprinting);
	void UpdateLamp(float Dt);
	void UpdateWrist(float Dt);
	void UpdateBody(float Dt);
	void ApplyBodyVisibility();
	void UpdateDistress(float Dt);
	void UpdateMaskLeds();
	void HandleGearEvents(const FPaleotrenchGearEvents& Events);
	// Shows a short message on the wrist's status line (seen when it is raised).
	// bGlance is kept for a future accessibility option; it no longer raises it.
	void PostNotice(const FText& Text, const FLinearColor& Color, bool bGlance);
	float GetDepthMetres() const;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DiveMapping;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MouseLookAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GamepadLookAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RollAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LevelAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HeadlampAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LampWheelAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LampBrighterAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LampDimmerAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BurstAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> WristAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ChargeAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CrankAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SwapAction;
	UPROPERTY(Transient)
	TObjectPtr<UPaleotrenchMaskOverlay> MaskOverlay;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
	FPaleotrenchSwimSettings Settings;

	// Head-like view limits: look up/down stops short of vertical and roll is a
	// lean, so the camera can never flip over. Yaw is unlimited.
	static constexpr double MaxViewPitch = 80.0;
	static constexpr double MaxViewRoll = 40.0;
	double ViewYaw = 0.0;
	double ViewPitch = 0.0;
	double ViewRoll = 0.0;
	double LeanVelocity = 0.0;
	// Intended view (drives movement) vs what the camera shows: the camera eases
	// toward the intent and adds breathing, fin-kick bob and strafe bank.
	FQuat ViewOrientation = FQuat::Identity;
	FQuat CameraOrientation = FQuat::Identity;
	FQuat LampOrientation = FQuat::Identity;
	FVector CameraBaseLocation = FVector::ZeroVector;
	// The body swims along the heading and pitch of the view, easing slowly.
	FQuat BodyOrientation = FQuat::Identity;
	bool bBodyView = false;
	float BodyOrbitYaw = 0.0f;
	bool bRealWristComputer = false;
	double BreathPhase = 0.0;
	double StrokePhase = 0.0;
	double StrokeAmount = 0.0;
	double StrafeBank = 0.0;

	FVector MoveInput = FVector::ZeroVector;
	FVector2D MouseDelta = FVector2D::ZeroVector;
	FVector2D StickLook = FVector2D::ZeroVector;
	float RollInput = 0.0f;
	float KickElapsed = 0.0f;
	bool bWantsSprint = false;

	// Dive session
	double SurfaceZ = 0.0;
	float DiveSeconds = 0.0f;
	float Clock = 0.0f;
	bool bWasInAir = false;
	bool bBlackoutReported = false;

	// Wrist computer: tap toggles, holding shows it only while held.
	bool bWristUp = false;
	bool bWristHeld = false;
	bool bWristPressRaised = false;
	float WristPressTime = 0.0f;
	float WristBlend = 0.0f;

	// Lamp flicker when the cell runs low
	float FlickerRemaining = 0.0f;
	float FlickerDepth = 1.0f;
	float HeartPhase = 0.0f;

	FText NoticeText;
	FLinearColor NoticeColor = FLinearColor::White;
	float NoticeUntil = 0.0f;
	float GlanceUntil = 0.0f;
};
