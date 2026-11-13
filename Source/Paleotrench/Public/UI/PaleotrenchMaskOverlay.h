#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PaleotrenchMaskOverlay.generated.h"

class UBorder;

// Two small LEDs on the inner edge of the dive mask: oxygen (upper) and power
// (lower). Peripheral warnings without raising the wrist (GDD section 6).
UCLASS()
class PALEOTRENCH_API UPaleotrenchMaskOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	// Colours already include any blinking; zero alpha turns an LED off.
	void SetLeds(const FLinearColor& Oxygen, const FLinearColor& Power);

private:
	void Build();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OxygenLed;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> OxygenHalo;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PowerLed;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PowerHalo;
};
