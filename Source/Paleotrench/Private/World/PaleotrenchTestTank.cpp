#include "World/PaleotrenchTestTank.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APaleotrenchTestTank::APaleotrenchTestTank()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TankRoot"));
	RootComponent->SetMobility(EComponentMobility::Static);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto Box = [this](FName Name, FVector Location, FVector Size, FRotator Rotation = FRotator::ZeroRotator)
	{
		auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetRelativeLocation(Location);
		Mesh->SetRelativeScale3D(Size / 100.0);
		Mesh->SetRelativeRotation(Rotation);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetMobility(EComponentMobility::Static);
	};

	// Interior: 40 x 30 x 16 metres. All six boundaries have swept collision.
	Box(TEXT("Floor"), FVector(0, 0, -50), FVector(4200, 3200, 100));
	// The ceiling has a 4 x 4 m opening (X -1500..-1100, Y -200..200) under an
	// air dome. Tools/Editor/prepare.py places the AirPocket actor in the dome
	// at (-1300, 0, 1825) with half-size (200, 200, 125): keep them in step.
	Box(TEXT("CeilingWest"), FVector(-1800, 0, 1650), FVector(600, 3200, 100));
	Box(TEXT("CeilingEast"), FVector(500, 0, 1650), FVector(3200, 3200, 100));
	Box(TEXT("CeilingSouth"), FVector(-1300, -900, 1650), FVector(400, 1400, 100));
	Box(TEXT("CeilingNorth"), FVector(-1300, 900, 1650), FVector(400, 1400, 100));
	Box(TEXT("DomeWest"), FVector(-1550, 0, 1825), FVector(100, 600, 250));
	Box(TEXT("DomeEast"), FVector(-1050, 0, 1825), FVector(100, 600, 250));
	Box(TEXT("DomeSouth"), FVector(-1300, -250, 1825), FVector(400, 100, 250));
	Box(TEXT("DomeNorth"), FVector(-1300, 250, 1825), FVector(400, 100, 250));
	Box(TEXT("DomeRoof"), FVector(-1300, 0, 2000), FVector(600, 600, 100));
	Box(TEXT("WallWest"), FVector(-2050, 0, 800), FVector(100, 3200, 1600));
	Box(TEXT("WallEast"), FVector(2050, 0, 800), FVector(100, 3200, 1600));
	Box(TEXT("WallSouth"), FVector(0, -1550, 800), FVector(4000, 100, 1600));
	Box(TEXT("WallNorth"), FVector(0, 1550, 800), FVector(4000, 100, 1600));
	Box(TEXT("LowPillar"), FVector(-300, -600, 300), FVector(220, 220, 600));
	Box(TEXT("HighPillar"), FVector(300, 600, 1050), FVector(220, 220, 1100));
	Box(TEXT("SlopedRock"), FVector(-650, 550, 250), FVector(700, 350, 100), FRotator(25, 0, 0));

	// Two gates at different elevations exercise clearance, pitch and roll.
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const double X = Index == 0 ? 600.0 : 1400.0;
		const double Z = Index == 0 ? 500.0 : 1100.0;
		Box(FName(*FString::Printf(TEXT("Gate%dLeft"), Index)), FVector(X, -230, Z), FVector(100, 100, 600));
		Box(FName(*FString::Printf(TEXT("Gate%dRight"), Index)), FVector(X, 230, Z), FVector(100, 100, 600));
		Box(FName(*FString::Printf(TEXT("Gate%dTop"), Index)), FVector(X, 0, Z + 250), FVector(100, 360, 100));
		Box(FName(*FString::Printf(TEXT("Gate%dBottom"), Index)), FVector(X, 0, Z - 250), FVector(100, 360, 100));
	}

	// Depth ladder on the east wall: metres below the dome's water line, the
	// same depth the wrist computer shows.
	for (int32 Depth = 2; Depth <= 16; Depth += 2)
	{
		auto* Mark = CreateDefaultSubobject<UTextRenderComponent>(
			FName(*FString::Printf(TEXT("Depth%d"), Depth)));
		Mark->SetupAttachment(RootComponent);
		Mark->SetRelativeLocation(FVector(1980, -700, SurfaceZ - Depth * 100));
		Mark->SetRelativeRotation(FRotator(0, 180, 0));
		Mark->SetText(FText::AsNumber(Depth));
		Mark->SetWorldSize(60.0f);
		Mark->SetTextRenderColor(FColor(120, 220, 220));
	}

	WaterFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("WaterFog"));
	WaterFog->SetupAttachment(RootComponent);
	WaterFog->SetFogDensity(0.04f);
	WaterFog->SetFogHeightFalloff(0.001f);
	WaterFog->FogInscatteringLuminance = FLinearColor(0.015f, 0.07f, 0.085f);
	WaterFog->SetFogMaxOpacity(0.97f);
	WaterFog->SetStartDistance(25.0f);
	WaterFog->bEnableVolumetricFog = false;

	WaterGrade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("WaterGrade"));
	WaterGrade->SetupAttachment(RootComponent);
	WaterGrade->bUnbound = true;
	auto& PP = WaterGrade->Settings;
	PP.bOverride_SceneColorTint = true;
	PP.SceneColorTint = FLinearColor(0.6f, 0.88f, 1.0f);
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.35f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.0f;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMinBrightness = 1.0f;
	PP.AutoExposureMaxBrightness = 1.0f;

	const FVector LightPositions[] = {FVector(-1300, -800, 1000), FVector(1000, 600, 1100), FVector(1500, -900, 300)};
	const FLinearColor LightColors[] = {FLinearColor(0.1f, 0.7f, 0.8f), FLinearColor(0.2f, 0.4f, 1.0f), FLinearColor(1.0f, 0.5f, 0.15f)};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* Light = CreateDefaultSubobject<UPointLightComponent>(
			FName(*FString::Printf(TEXT("GuideLight%d"), Index)));
		Light->SetupAttachment(RootComponent);
		Light->SetRelativeLocation(LightPositions[Index]);
		Light->SetMobility(EComponentMobility::Movable);
		Light->IntensityUnits = ELightUnits::Lumens;
		Light->SetIntensity(1600.0f);
		Light->SetLightColor(LightColors[Index]);
		Light->SetAttenuationRadius(1800.0f);
		Light->SetCastShadows(false);
	}
}
