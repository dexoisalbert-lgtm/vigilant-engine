#include "World/PaleotrenchAirPocket.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Created by Tools/Editor/prepare.py; without it the surface stays hidden.
	const TCHAR* const WaterSurfaceMaterialPath = TEXT("/Game/Materials/M_WaterSurface.M_WaterSurface");
}

APaleotrenchAirPocket::APaleotrenchAirPocket()
{
	PrimaryActorTick.bCanEverTick = false;

	AirVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("AirVolume"));
	RootComponent = AirVolume;
	// Query-only so the post-process volume can measure distance to it;
	// nothing collides with air.
	AirVolume->SetCollisionProfileName(TEXT("Trigger"));
	AirVolume->SetGenerateOverlapEvents(false);
	AirVolume->SetCanEverAffectNavigation(false);
	AirVolume->SetMobility(EComponentMobility::Static);
	AirVolume->ShapeColor = FColor(120, 220, 255);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	WaterSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterSurface"));
	WaterSurface->SetupAttachment(AirVolume);
	WaterSurface->SetStaticMesh(Plane.Object);
	WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterSurface->SetCastShadow(false);
	WaterSurface->SetMobility(EComponentMobility::Static);

	AirLook = CreateDefaultSubobject<UPostProcessComponent>(TEXT("AirLook"));
	AirLook->SetupAttachment(AirVolume);
	AirLook->bUnbound = false;  // only inside the AirVolume box
	AirLook->Priority = 1.0f;
	AirLook->BlendRadius = 30.0f;
	// Out of the water: no blue-green grade and a lighter mask vignette.
	AirLook->Settings.bOverride_SceneColorTint = true;
	AirLook->Settings.SceneColorTint = FLinearColor(1.0f, 0.97f, 0.92f);
	AirLook->Settings.bOverride_VignetteIntensity = true;
	AirLook->Settings.VignetteIntensity = 0.2f;
	// Pockets are small: the lamp lands a metre away, so the eye stops down.
	AirLook->Settings.bOverride_AutoExposureBias = true;
	AirLook->Settings.AutoExposureBias = -1.0f;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(AirVolume);
	Glow->SetMobility(EComponentMobility::Movable);
	Glow->IntensityUnits = ELightUnits::Lumens;
	Glow->SetIntensity(150.0f);
	Glow->SetLightColor(FLinearColor(0.75f, 0.9f, 1.0f));
	Glow->SetCastShadows(false);
}

void APaleotrenchAirPocket::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector Extent = AirExtent.ComponentMax(FVector(20.0));
	AirVolume->SetBoxExtent(Extent);
	// The engine plane is 100 x 100 cm and faces +Z.
	WaterSurface->SetRelativeLocation(FVector(0.0, 0.0, -Extent.Z));
	WaterSurface->SetRelativeScale3D(FVector(Extent.X / 50.0, Extent.Y / 50.0, 1.0));
	UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr, WaterSurfaceMaterialPath, nullptr, LOAD_NoWarn);
	WaterSurface->SetMaterial(0, Water);
	WaterSurface->SetVisibility(Water != nullptr);
	Glow->SetRelativeLocation(FVector(0.0, 0.0, Extent.Z * 0.4));
	Glow->SetAttenuationRadius(static_cast<float>(FMath::Max(Extent.X, Extent.Y) * 3.0));
}

bool APaleotrenchAirPocket::ContainsPoint(const FVector& WorldPoint) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	const FVector Extent = AirVolume->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

FName APaleotrenchAirPocket::GetCheckpointId() const
{
	return CheckpointId.IsNone() ? GetFName() : CheckpointId;
}

FTransform APaleotrenchAirPocket::GetRespawnTransform() const
{
	const FVector Extent = AirVolume->GetUnscaledBoxExtent();
	// Capsule centre 30 cm above the water line keeps the head in the air.
	const FVector Local(0.0, 0.0, -Extent.Z + FMath::Min(30.0, Extent.Z));
	return FTransform(FRotator(0.0, GetActorRotation().Yaw, 0.0), GetActorTransform().TransformPosition(Local));
}

APaleotrenchAirPocket* APaleotrenchAirPocket::FindContaining(const UWorld* World, const FVector& WorldPoint)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<APaleotrenchAirPocket> It(World); It; ++It)
	{
		if (It->ContainsPoint(WorldPoint))
		{
			return *It;
		}
	}
	return nullptr;
}

APaleotrenchAirPocket* APaleotrenchAirPocket::FindById(const UWorld* World, FName Id)
{
	if (!World || Id.IsNone())
	{
		return nullptr;
	}
	for (TActorIterator<APaleotrenchAirPocket> It(World); It; ++It)
	{
		if (It->GetCheckpointId() == Id)
		{
			return *It;
		}
	}
	return nullptr;
}
