#include "UI/PaleotrenchWristDisplay.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "PaleotrenchWrist"

namespace
{
	const FLinearColor WristScreen(0.004f, 0.012f, 0.016f, 1.0f);
	const FLinearColor WristInk(0.80f, 0.95f, 1.0f, 1.0f);
	const FLinearColor WristDim(0.35f, 0.50f, 0.55f, 1.0f);
	const FLinearColor WristCyan(0.15f, 0.80f, 1.0f, 1.0f);
	const FLinearColor WristAmber(1.0f, 0.62f, 0.12f, 1.0f);
	const FLinearColor WristRed(1.0f, 0.18f, 0.14f, 1.0f);
	const FLinearColor WristBarTrack(0.03f, 0.07f, 0.09f, 1.0f);

	UTextBlock* MakeWristText(UWidgetTree* Tree, const TCHAR* Typeface, float Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	UHorizontalBoxSlot* AddWristChild(UHorizontalBox* Row, UWidget* Child, bool bFill = false,
		EVerticalAlignment VAlign = VAlign_Center)
	{
		UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Child);
		Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		Slot->SetVerticalAlignment(VAlign);
		return Slot;
	}

	UHorizontalBox* MakeWristRow(UWidgetTree* Tree, UWidget* Left, UWidget* Right)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddWristChild(Row, Left);
		AddWristChild(Row, Tree->ConstructWidget<USpacer>(USpacer::StaticClass()), true);
		AddWristChild(Row, Right);
		return Row;
	}

	void SetWristFill(UBorder* Part, float Value)
	{
		if (UHorizontalBoxSlot* Slot = Part ? Cast<UHorizontalBoxSlot>(Part->Slot) : nullptr)
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = Value;
			Slot->SetSize(Size);
		}
	}

	FText WristPercent(float Fraction)
	{
		return FText::AsPercent(FMath::Clamp(Fraction, 0.0f, 1.0f));
	}

	FText WristLampName(int32 Level)
	{
		switch (Level)
		{
		case 1: return LOCTEXT("LampLow", "LOW");
		case 2: return LOCTEXT("LampMedium", "MED");
		case 3: return LOCTEXT("LampHigh", "HIGH");
		case 4: return LOCTEXT("LampBurst", "BURST");
		default: return LOCTEXT("LampOff", "OFF");
		}
	}

	FString WristCompass(float Degrees)
	{
		static const TCHAR* const Points[8] = {
			TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};
		const float Heading = FMath::Fmod(FMath::Fmod(Degrees, 360.0f) + 360.0f, 360.0f);
		const int32 Point = FMath::RoundToInt(Heading / 45.0f) % 8;
		return FString::Printf(TEXT("%s %03d°"), Points[Point], FMath::RoundToInt(Heading) % 360);
	}
}

bool UPaleotrenchWristDisplay::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Build();
	}
	return bResult;
}

void UPaleotrenchWristDisplay::Build()
{
	UWidgetTree* Tree = WidgetTree;
	UBorder* Root = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Root->SetBrushColor(WristScreen);
	Root->SetPadding(FMargin(22.0f, 14.0f));
	Tree->RootWidget = Root;

	UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Root->SetContent(Column);
	const auto AddRow = [Column](UWidget* Row, float Top)
	{
		UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Row);
		Slot->SetPadding(FMargin(0.0f, Top, 0.0f, 0.0f));
		Slot->SetHorizontalAlignment(HAlign_Fill);
	};

	// Tank pressure dominates, as on a real dive computer; depth top right.
	PressureText = MakeWristText(Tree, TEXT("Bold"), 64.0f, WristCyan);
	UTextBlock* BarUnit = MakeWristText(Tree, TEXT("Bold"), 20.0f, WristDim);
	BarUnit->SetText(LOCTEXT("BarUnit", "BAR"));
	DepthText = MakeWristText(Tree, TEXT("Bold"), 32.0f, WristInk);
	UHorizontalBox* Top = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddWristChild(Top, PressureText, false, VAlign_Bottom);
	AddWristChild(Top, BarUnit, false, VAlign_Bottom)->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 14.0f));
	AddWristChild(Top, Tree->ConstructWidget<USpacer>(USpacer::StaticClass()), true);
	AddWristChild(Top, DepthText, false, VAlign_Top);
	AddRow(Top, 0.0f);

	UHorizontalBox* Bar = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PressureFill = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PressureFill->SetBrushColor(WristCyan);
	PressureEmpty = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PressureEmpty->SetBrushColor(WristBarTrack);
	AddWristChild(Bar, PressureFill, true, VAlign_Fill);
	AddWristChild(Bar, PressureEmpty, true, VAlign_Fill);
	USizeBox* BarBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetHeightOverride(10.0f);
	BarBox->SetContent(Bar);
	AddRow(BarBox, 2.0f);

	AirText = MakeWristText(Tree, TEXT("Bold"), 22.0f, WristInk);
	SpareText = MakeWristText(Tree, TEXT("Bold"), 22.0f, WristInk);
	AddRow(MakeWristRow(Tree, AirText, SpareText), 12.0f);

	LampText = MakeWristText(Tree, TEXT("Bold"), 22.0f, WristInk);
	PackText = MakeWristText(Tree, TEXT("Bold"), 22.0f, WristInk);
	AddRow(MakeWristRow(Tree, LampText, PackText), 6.0f);

	HeadingText = MakeWristText(Tree, TEXT("Regular"), 22.0f, WristDim);
	DiveTimeText = MakeWristText(Tree, TEXT("Regular"), 22.0f, WristDim);
	AddRow(MakeWristRow(Tree, HeadingText, DiveTimeText), 6.0f);

	StatusText = MakeWristText(Tree, TEXT("Bold"), 24.0f, WristInk);
	StatusText->SetJustification(ETextJustify::Center);
	AddRow(StatusText, 10.0f);
}

void UPaleotrenchWristDisplay::Refresh(const FPaleotrenchWristReadout& R)
{
	if (!PressureText)
	{
		return;
	}
	const bool bCritical = R.PressureBar <= R.CriticalBar;
	const FLinearColor TankColor = bCritical ? WristRed : (R.PressureBar <= R.ReserveBar ? WristAmber : WristCyan);
	const bool bBlinkOn = !bCritical || FMath::Fmod(R.Time * 2.5f, 1.0f) < 0.6f;
	const FLinearColor Shown = bBlinkOn ? TankColor : TankColor * FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);

	PressureText->SetText(FText::AsNumber(FMath::Max(0, FMath::CeilToInt(R.PressureBar))));
	PressureText->SetColorAndOpacity(FSlateColor(Shown));
	PressureFill->SetBrushColor(Shown);
	const float Fraction = FMath::Clamp(R.PressureBar / FMath::Max(R.CapacityBar, 1.0f), 0.0f, 1.0f);
	SetWristFill(PressureFill, Fraction);
	SetWristFill(PressureEmpty, 1.0f - Fraction);

	FNumberFormattingOptions OneDecimal;
	OneDecimal.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
	DepthText->SetText(FText::Format(LOCTEXT("Depth", "{0} m"),
		FText::AsNumber(FMath::Max(R.DepthMetres, 0.0f), &OneDecimal)));

	if (R.AirMinutes < 0.0f)
	{
		AirText->SetText(LOCTEXT("AirSurface", "AIR --"));
	}
	else if (R.AirMinutes < 1.0f)
	{
		AirText->SetText(LOCTEXT("AirUnderMinute", "AIR <1 MIN"));
	}
	else
	{
		AirText->SetText(FText::Format(LOCTEXT("AirMinutes", "AIR {0} MIN"), FText::AsNumber(FMath::FloorToInt(R.AirMinutes))));
	}
	AirText->SetColorAndOpacity(FSlateColor(TankColor == WristCyan ? WristInk : TankColor));

	SpareText->SetText(FText::Format(LOCTEXT("Spares", "SPARE {0}/{1}"), R.SpareTanks, R.MaxSpareTanks));
	SpareText->SetColorAndOpacity(FSlateColor(R.SpareTanks > 0 ? WristInk : WristDim));

	LampText->SetText(FText::Format(LOCTEXT("Lamp", "LAMP {0} {1}"), WristLampName(R.LampLevel), WristPercent(R.LampFraction)));
	LampText->SetColorAndOpacity(FSlateColor(R.bLampLow ? WristAmber : WristInk));
	PackText->SetText(FText::Format(LOCTEXT("Pack", "PACK {0}"), WristPercent(R.PackFraction)));
	PackText->SetColorAndOpacity(FSlateColor(R.PackFraction <= 0.0f ? WristRed : (R.PackFraction < 0.2f ? WristAmber : WristInk)));

	HeadingText->SetText(FText::FromString(WristCompass(R.HeadingDegrees)));
	const int32 Seconds = FMath::Max(0, FMath::FloorToInt(R.DiveSeconds));
	DiveTimeText->SetText(FText::FromString(FString::Printf(TEXT("DIVE %02d:%02d"), Seconds / 60, Seconds % 60)));

	StatusText->SetText(R.Status);
	StatusText->SetColorAndOpacity(FSlateColor(R.StatusColor));
}

#undef LOCTEXT_NAMESPACE
