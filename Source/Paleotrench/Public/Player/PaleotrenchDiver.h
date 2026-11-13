#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Player/PaleotrenchSwimSettings.h"
#include "PaleotrenchDiver.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

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
	void ToggleHeadlamp();

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

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
	FPaleotrenchSwimSettings Settings;
	FQuat ViewOrientation = FQuat::Identity;
	FVector MoveInput = FVector::ZeroVector;
	FVector2D MouseDelta = FVector2D::ZeroVector;
	FVector2D StickLook = FVector2D::ZeroVector;
	float RollInput = 0.0f;
	float KickElapsed = 0.0f;
	bool bWantsSprint = false;
};
