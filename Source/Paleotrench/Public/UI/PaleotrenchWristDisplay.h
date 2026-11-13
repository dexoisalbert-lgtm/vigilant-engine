#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PaleotrenchWristDisplay.generated.h"

class UBorder;
class UTextBlock;

// Everything the dive computer shows. The diver fills it while the wrist is up.
struct FPaleotrenchWristReadout
{
	float PressureBar = 0.0f;
	float CapacityBar = 200.0f;
	float ReserveBar = 50.0f;
	float CriticalBar = 20.0f;
	float AirMinutes = 0.0f;      // negative while breathing air
	float DepthMetres = 0.0f;
	float HeadingDegrees = 0.0f;  // 0 = north (+X), 90 = east (+Y)
	float PackFraction = 0.0f;
	float LampFraction = 0.0f;
	int32 LampLevel = 0;          // pt::ELampLevel actually lit
	bool bLampLow = false;
	int32 SpareTanks = 0;
	int32 MaxSpareTanks = 0;
	float DiveSeconds = 0.0f;     // since leaving the last air pocket
	FText Status;
	FLinearColor StatusColor = FLinearColor::White;
	float Time = 0.0f;            // drives blinking
};

// The wrist dive computer's screen, built in C++ (no Blueprint). Shown on the
// diver's forearm through a world-space widget component, so it is diegetic.
UCLASS()
class PALEOTRENCH_API UPaleotrenchWristDisplay : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	void Refresh(const FPaleotrenchWristReadout& Readout);

	static constexpr int32 PixelWidth = 512;
	static constexpr int32 PixelHeight = 352;

private:
	void Build();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PressureText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DepthText;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PressureFill;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PressureEmpty;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AirText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpareText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LampText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PackText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadingText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DiveTimeText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
};
