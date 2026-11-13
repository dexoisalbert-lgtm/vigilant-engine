#include "World/PaleotrenchSpareTank.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PaleotrenchGameMode.h"
#include "Player/PaleotrenchDiveGear.h"
#include "Player/PaleotrenchDiver.h"
#include "UObject/ConstructorHelpers.h"

APaleotrenchSpareTank::APaleotrenchSpareTank()
{
	PrimaryActorTick.bCanEverTick = true;

	Reach = CreateDefaultSubobject<USphereComponent>(TEXT("Reach"));
	RootComponent = Reach;
	Reach->InitSphereRadius(70.0f);
	Reach->SetCollisionProfileName(TEXT("Trigger"));
	Reach->SetGenerateOverlapEvents(true);
	Reach->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	// A 12 L dive cylinder: about 18 cm across and 65 cm long, lying on its side.
	Cylinder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cylinder"));
	Cylinder->SetupAttachment(Reach);
	Cylinder->SetStaticMesh(CylinderMesh.Object);
	Cylinder->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	Cylinder->SetRelativeScale3D(FVector(0.18, 0.18, 0.65));
	Cylinder->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Valve = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Valve"));
	Valve->SetupAttachment(Reach);
	Valve->SetStaticMesh(CylinderMesh.Object);
	Valve->SetRelativeLocation(FVector(38.0, 0.0, 0.0));
	Valve->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	Valve->SetRelativeScale3D(FVector(0.07, 0.07, 0.12));
	Valve->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Strobe = CreateDefaultSubobject<UPointLightComponent>(TEXT("Strobe"));
	Strobe->SetupAttachment(Reach);
	Strobe->SetRelativeLocation(FVector(0.0, 0.0, 16.0));
	Strobe->IntensityUnits = ELightUnits::Lumens;
	Strobe->SetIntensity(0.0f);
	Strobe->SetAttenuationRadius(450.0f);
	Strobe->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
	Strobe->SetCastShadows(false);
}

void APaleotrenchSpareTank::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Dive-yellow body, dark valve. BasicShapeMaterial exposes "Color".
	if (UMaterialInstanceDynamic* Body = Cylinder->CreateAndSetMaterialInstanceDynamic(0))
	{
		Body->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.62f, 0.05f));
	}
	if (UMaterialInstanceDynamic* Metal = Valve->CreateAndSetMaterialInstanceDynamic(0))
	{
		Metal->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.08f, 0.09f));
	}
}

void APaleotrenchSpareTank::BeginPlay()
{
	Super::BeginPlay();
	// A random strobe phase so several tanks never blink in unison.
	Clock = FMath::FRandRange(0.0f, 2.0f);
	if (const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<APaleotrenchGameMode>() : nullptr)
	{
		SetCollected(Mode->IsPickupCollected(GetPickupId()));
	}
}

FName APaleotrenchSpareTank::GetPickupId() const
{
	return PickupId.IsNone() ? GetFName() : PickupId;
}

void APaleotrenchSpareTank::SetCollected(bool bNewCollected)
{
	bCollected = bNewCollected;
	SetActorHiddenInGame(bCollected);
	Reach->SetCollisionEnabled(bCollected ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	Strobe->SetIntensity(0.0f);
}

void APaleotrenchSpareTank::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCollected)
	{
		return;
	}
	Clock += DeltaSeconds;
	// Slow drift and roll, as if hanging in a faint current.
	Cylinder->SetRelativeRotation(FRotator(90.0, 0.0, 0.0) + FRotator(0.0, Clock * 6.0, FMath::Sin(Clock * 0.7) * 8.0));
	// Dive strobe: a short flash every two seconds.
	Strobe->SetIntensity(FMath::Fmod(Clock, 2.0f) < 0.08f ? 350.0f : 0.0f);

	TArray<AActor*> Nearby;
	Reach->GetOverlappingActors(Nearby, APaleotrenchDiver::StaticClass());
	for (AActor* Actor : Nearby)
	{
		auto* Diver = Cast<APaleotrenchDiver>(Actor);
		if (Diver && Diver->CollectSpareTank())
		{
			SetCollected(true);
			if (auto* Mode = GetWorld()->GetAuthGameMode<APaleotrenchGameMode>())
			{
				Mode->PickupCollected(GetPickupId());
			}
			break;
		}
	}
}
