#include "UI/PaleotrenchMaskOverlay.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

namespace
{
	UBorder* AddMaskDot(UWidgetTree* Tree, UCanvasPanel* Canvas, float CenterY, float Diameter)
	{
		UBorder* Dot = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Dot->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, Diameter * 0.5f));
		Dot->SetBrushColor(FLinearColor::Transparent);
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Dot);
		Slot->SetAnchors(FAnchors(0.0f, 0.5f));
		Slot->SetAlignment(FVector2D(0.5f, 0.5f));
		Slot->SetPosition(FVector2D(30.0f, CenterY));
		Slot->SetSize(FVector2D(Diameter, Diameter));
		return Dot;
	}

	FLinearColor MaskHaloOf(const FLinearColor& Color)
	{
		return FLinearColor(Color.R, Color.G, Color.B, Color.A * 0.22f);
	}
}

bool UPaleotrenchMaskOverlay::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	return bResult;
}

void UPaleotrenchMaskOverlay::Build()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;
	// Halos first so the LEDs draw on top of them.
	OxygenHalo = AddMaskDot(WidgetTree, Canvas, -16.0f, 26.0f);
	PowerHalo = AddMaskDot(WidgetTree, Canvas, 16.0f, 26.0f);
	OxygenLed = AddMaskDot(WidgetTree, Canvas, -16.0f, 8.0f);
	PowerLed = AddMaskDot(WidgetTree, Canvas, 16.0f, 8.0f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPaleotrenchMaskOverlay::SetLeds(const FLinearColor& Oxygen, const FLinearColor& Power)
{
	if (!OxygenLed)
	{
		return;
	}
	OxygenLed->SetBrushColor(Oxygen);
	OxygenHalo->SetBrushColor(MaskHaloOf(Oxygen));
	PowerLed->SetBrushColor(Power);
	PowerHalo->SetBrushColor(MaskHaloOf(Power));
}
