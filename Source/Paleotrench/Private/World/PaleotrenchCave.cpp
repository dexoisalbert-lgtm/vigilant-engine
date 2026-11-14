#include "World/PaleotrenchCave.h"

#include "Async/ParallelFor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Queue.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Paleotrench.h"
#include "PaleotrenchCore/CaveField.h"
#include "PaleotrenchCore/CaveGraph.h"
#include "PaleotrenchCore/CaveMesher.h"
#include "PaleotrenchCore/CaveNav.h"
#include "PaleotrenchCore/Random.h"
#include "PaleotrenchCore/Seed.h"
#include "ProceduralMeshComponent.h"
#include "Tasks/Task.h"
#include "World/PaleotrenchAirPocket.h"

#include <atomic>

namespace
{
	// Created by Tools/Editor/prepare.py.
	const TCHAR* const TuningTablePath = TEXT("/Game/Data/DT_CaveTuning.DT_CaveTuning");
	const TCHAR* const RockMaterialPath = TEXT("/Game/Materials/M_CaveRock.M_CaveRock");
	constexpr double StreamInterval = 0.2;   // seconds between streaming updates
	constexpr double StartRadius = 2600.0;   // cm around the entrance meshed before the dive starts
	constexpr double AirHalfHeight = 125.0;  // APaleotrenchAirPocket's default AirExtent.Z: its box sits on the water

	FName CaveRowName(uint8 CaveId)
	{
		switch (CaveId)
		{
		case 1: return TEXT("Livyatan");
		case 2: return TEXT("Tusoteuthis");
		case 3: return TEXT("Stygiomedusa");
		case 4: return TEXT("Arthropleura");
		default: return TEXT("Livyatan");
		}
	}

	FVector ToFVector(const pt::Vec3& V)
	{
		return FVector(V.X, V.Y, V.Z);
	}

	pt::Vec3 ToVec3(const FVector& V)
	{
		return pt::Vec3(V.X, V.Y, V.Z);
	}

	pt::CaveChunkKey ToKey(const FIntVector& V)
	{
		return pt::CaveChunkKey{V.X, V.Y, V.Z};
	}

	FName PocketId(uint8 CaveId, int32 Node)
	{
		return FName(*FString::Printf(TEXT("Cave%d_Air%d"), CaveId, Node));
	}
}

// One finished chunk, made on a worker thread.
struct FPaleotrenchCaveChunkResult
{
	FIntVector Key;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> Colors;
};

// Everything a worker needs to mesh chunks; shared, so a rebuild never pulls it from under a running task.
struct FPaleotrenchCaveBuild
{
	FPaleotrenchCaveBuild(const pt::CaveGraph& Graph, const pt::CaveFieldTuning& FieldTuning, const pt::CaveMeshSettings& InMesh, bool bInFlip)
		: Field(Graph, FieldTuning)
		, Mesh(InMesh)
		, bFlipWinding(bInFlip)
	{
	}

	const pt::CaveField Field;
	const pt::CaveMeshSettings Mesh;
	const bool bFlipWinding;
	TQueue<TSharedPtr<FPaleotrenchCaveChunkResult, ESPMode::ThreadSafe>, EQueueMode::Mpsc> Finished;
	std::atomic<bool> bCancelled{false};
	// Built once on a worker; read only after bNavReady.
	pt::CaveNav Nav;
	std::atomic<bool> bNavReady{false};
};

namespace
{
	TSharedPtr<FPaleotrenchCaveChunkResult, ESPMode::ThreadSafe> MeshChunk(const FPaleotrenchCaveBuild& Build, const FIntVector& Key)
	{
		TSharedPtr<FPaleotrenchCaveChunkResult, ESPMode::ThreadSafe> Result = MakeShared<FPaleotrenchCaveChunkResult, ESPMode::ThreadSafe>();
		Result->Key = Key;
		pt::CaveChunkMesh Mesh;
		pt::MeshCaveChunk(Build.Field, ToKey(Key), Build.Mesh, Mesh);
		if (Mesh.Empty())
		{
			return Result;
		}
		const std::vector<int> Nearby = Build.Field.FeaturesNear(pt::CaveChunkBox(ToKey(Key), Build.Mesh).Expanded(400.0));
		const int32 Count = static_cast<int32>(Mesh.VertexCount());
		Result->Vertices.Reserve(Count);
		Result->Normals.Reserve(Count);
		Result->Colors.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			const FVector P(Mesh.Positions[3 * i], Mesh.Positions[3 * i + 1], Mesh.Positions[3 * i + 2]);
			const FVector N(Mesh.Normals[3 * i], Mesh.Normals[3 * i + 1], Mesh.Normals[3 * i + 2]);
			Result->Vertices.Add(P);
			Result->Normals.Add(N);
			// Red: how open the water is a little way out from the wall (crevices and corners stay dark).
			// Green: slow bands of colour across the cave.
			const double Open = -Build.Field.SampleWith(ToVec3(P + N * 70.0), Nearby);
			const float Occlusion = FMath::Clamp(static_cast<float>(Open / 70.0), 0.0f, 1.0f);
			const float Band = FMath::Clamp(FMath::PerlinNoise3D(P * 0.0006) * 0.5f + 0.5f, 0.0f, 1.0f);
			Result->Colors.Add(FColor(static_cast<uint8>(Occlusion * 255.0f), static_cast<uint8>(Band * 255.0f), 0, 255));
		}
		// The core mesher winds triangles counter-clockwise for a right-handed frame; Unreal is left-handed,
		// so the order is reversed here (bFlipWinding undoes that, for testing).
		const bool bReverse = !Build.bFlipWinding;
		Result->Triangles.Reserve(static_cast<int32>(Mesh.Indices.size()));
		for (size_t t = 0; t + 2 < Mesh.Indices.size(); t += 3)
		{
			Result->Triangles.Add(static_cast<int32>(Mesh.Indices[t]));
			Result->Triangles.Add(static_cast<int32>(Mesh.Indices[bReverse ? t + 2 : t + 1]));
			Result->Triangles.Add(static_cast<int32>(Mesh.Indices[bReverse ? t + 1 : t + 2]));
		}
		return Result;
	}
}

APaleotrenchCave::APaleotrenchCave()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CaveRoot"));
	TuningTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TuningTablePath));
	RockMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(RockMaterialPath));

	// The same underwater look as the test tank.
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
	FPostProcessSettings& PP = WaterGrade->Settings;
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
}

bool APaleotrenchCave::IsBuilt() const
{
	return Current.IsValid();
}

void APaleotrenchCave::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();
	Super::EndPlay(EndPlayReason);
}

void APaleotrenchCave::Clear()
{
	if (Current)
	{
		Current->bCancelled = true;
	}
	Current.Reset();
	for (TPair<FIntVector, TObjectPtr<UProceduralMeshComponent>>& Pair : Chunks)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	Chunks.Reset();
	for (APaleotrenchAirPocket* Pocket : AirPockets)
	{
		if (Pocket)
		{
			Pocket->Destroy();
		}
	}
	AirPockets.Reset();
	for (UPointLightComponent* Glow : Glows)
	{
		if (Glow)
		{
			Glow->DestroyComponent();
		}
	}
	Glows.Reset();
	Candidates.Reset();
	InFlight.Reset();
}

void APaleotrenchCave::LoadSettings()
{
	Settings = FPaleotrenchCaveSettings();
	const FName Row = CaveRowName(CaveId);
	const UDataTable* Table = TuningTable.LoadSynchronous();
	const FPaleotrenchCaveSettings* Found = Table ? Table->FindRow<FPaleotrenchCaveSettings>(Row, TEXT("cave tuning"), false) : nullptr;
	if (Found && Found->IsValid())
	{
		Settings = *Found;
	}
	else
	{
		UE_LOG(LogPaleotrench, Warning, TEXT("Cave tuning row '%s' is missing or invalid (run Tools/Prepare.bat); using built-in defaults."),
			*Row.ToString());
	}
	LoadedRockMaterial = RockMaterial.LoadSynchronous();
}

void APaleotrenchCave::Build(uint64 SaveSeed)
{
	const double StartTime = FPlatformTime::Seconds();
	Clear();
	LoadSettings();
	CaveSeed = pt::DeriveCaveSeed(SaveSeed, static_cast<pt::ECave>(CaveId));
	const pt::CaveLayoutTuning Layout = Settings.ToLayout();
	const pt::CaveGraph Graph = pt::GenerateCaveGraph(CaveSeed, Layout);
	const pt::CaveReport Report = pt::CheckCaveGraph(Graph, Layout);
	if (!Report.Ok())
	{
		UE_LOG(LogPaleotrench, Warning, TEXT("Cave layout 0x%016llx breaks a rule (%hs); it is still playable."), CaveSeed, Report.Problem.c_str());
	}
	++Generation;
	Current = MakeShared<FPaleotrenchCaveBuild, ESPMode::ThreadSafe>(Graph, Settings.ToField(), Settings.ToMesh(), bFlipWinding);
	for (const pt::CaveChunkKey& Key : pt::CaveChunksToMesh(Current->Field, Current->Mesh))
	{
		Candidates.Add(FIntVector(Key.X, Key.Y, Key.Z));
	}
	SpawnAirPockets();
	SpawnGlows();

	// The rock around the entrance exists before the dive begins: meshed on every core right now.
	const pt::Vec3 Start = Graph.Nodes[static_cast<size_t>(Graph.Entrance)].Centre;
	const double Half = Current->Mesh.ChunkSize() * 0.5;
	TArray<FIntVector> Near;
	for (const FIntVector& Key : Candidates)
	{
		const pt::Box3 Box = pt::CaveChunkBox(ToKey(Key), Current->Mesh);
		if (((Box.Min + Box.Max) * 0.5 - Start).Length() <= StartRadius + Half)
		{
			Near.Add(Key);
		}
	}
	TArray<TSharedPtr<FPaleotrenchCaveChunkResult, ESPMode::ThreadSafe>> Results;
	Results.SetNum(Near.Num());
	const FPaleotrenchCaveBuild& Shared = *Current;
	ParallelFor(Near.Num(), [&Results, &Near, &Shared](int32 Index)
	{
		Results[Index] = MeshChunk(Shared, Near[Index]);
	});
	for (int32 i = 0; i < Near.Num(); ++i)
	{
		InFlight.Add(Near[i]);
		Current->Finished.Enqueue(Results[i]);
	}
	FinishChunks(TNumericLimits<int32>::Max());

	// Navigation for later milestones, built in the background.
	TSharedPtr<FPaleotrenchCaveBuild, ESPMode::ThreadSafe> ForNav = Current;
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [ForNav]()
	{
		if (!ForNav->bCancelled)
		{
			ForNav->Nav.Build(ForNav->Field, pt::CaveNavSettings());
			ForNav->bNavReady = true;
		}
	});
	NextStreamUpdate = 0.0;
	UE_LOG(LogPaleotrench, Display, TEXT("Cave ready in %.0f ms: %s"), (FPlatformTime::Seconds() - StartTime) * 1000.0, *Describe());
}

void APaleotrenchCave::SpawnAirPockets()
{
	UWorld* World = GetWorld();
	if (!World || !Current)
	{
		return;
	}
	for (const pt::CaveAirDome& Dome : Current->Field.AirDomes())
	{
		// The pocket's box stands on the water line.
		const FVector Local = ToFVector(Dome.WaterCentre) + FVector(0.0, 0.0, AirHalfHeight);
		const FTransform At(FRotator::ZeroRotator, GetActorTransform().TransformPosition(Local));
		APaleotrenchAirPocket* Pocket = World->SpawnActorDeferred<APaleotrenchAirPocket>(APaleotrenchAirPocket::StaticClass(), At, this);
		if (!Pocket)
		{
			continue;
		}
		Pocket->SetCheckpointId(PocketId(CaveId, Dome.Node));
		Pocket->FinishSpawning(At);
		AirPockets.Add(Pocket);
	}
}

void APaleotrenchCave::SpawnGlows()
{
	if (!Current || Settings.GlowLumens <= 0.0f || Settings.GlowsPerChamber <= 0)
	{
		return;
	}
	// Faint bioluminescence low on the walls: enough to read a chamber's shape with the lamp off.
	const FLinearColor Colours[] = {FLinearColor(0.1f, 0.9f, 0.7f), FLinearColor(0.2f, 0.6f, 1.0f), FLinearColor(0.5f, 1.0f, 0.3f),
		FLinearColor(0.7f, 0.3f, 1.0f)};
	const pt::CaveGraph& Graph = Current->Field.GetGraph();
	for (size_t i = 0; i < Graph.Nodes.size(); ++i)
	{
		const pt::CaveNode& Node = Graph.Nodes[i];
		if (Node.HasAir())
		{
			continue;  // air pockets have their own light
		}
		pt::Rng R(pt::MixSeed(CaveSeed, 0x6C0000ULL + i));
		for (int32 n = 0; n < Settings.GlowsPerChamber; ++n)
		{
			const double Angle = static_cast<double>(R.RangeFloat(0.0f, 6.2831853f));
			const int32 Pick = static_cast<int32>(R.Below(4));
			const pt::Vec3 Offset(std::cos(Angle) * Node.Radii.X * 0.7, std::sin(Angle) * Node.Radii.Y * 0.7, -Node.Radii.Z * 0.35);
			UPointLightComponent* Glow = NewObject<UPointLightComponent>(this);
			Glow->SetupAttachment(RootComponent);
			Glow->SetRelativeLocation(ToFVector(Node.Centre + Offset));
			Glow->SetMobility(EComponentMobility::Movable);
			Glow->IntensityUnits = ELightUnits::Lumens;
			Glow->SetIntensity(Settings.GlowLumens);
			Glow->SetLightColor(Colours[Pick]);
			Glow->SetAttenuationRadius(static_cast<float>(FMath::Max(Node.Radii.X, Node.Radii.Y) * 1.2));
			Glow->SetCastShadows(false);
			Glow->RegisterComponent();
			Glows.Add(Glow);
		}
	}
}

FVector APaleotrenchCave::StreamFocus() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (Pawn)
	{
		return GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
	}
	const pt::CaveGraph& Graph = Current->Field.GetGraph();
	return ToFVector(Graph.Nodes[static_cast<size_t>(Graph.Entrance)].Centre);
}

void APaleotrenchCave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Current)
	{
		return;
	}
	FinishChunks(ChunksPerFrame);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextStreamUpdate)
	{
		NextStreamUpdate = Now + StreamInterval;
		UpdateStreaming();
	}
}

void APaleotrenchCave::UpdateStreaming()
{
	const pt::Vec3 Focus = ToVec3(StreamFocus());
	const double Half = Current->Mesh.ChunkSize() * 0.5;
	const auto Distance = [this, &Focus](const FIntVector& Key)
	{
		const pt::Box3 Box = pt::CaveChunkBox(ToKey(Key), Current->Mesh);
		return ((Box.Min + Box.Max) * 0.5 - Focus).Length();
	};

	// Drop rock left far behind.
	TArray<FIntVector> Drop;
	for (const TPair<FIntVector, TObjectPtr<UProceduralMeshComponent>>& Pair : Chunks)
	{
		if (Distance(Pair.Key) > Settings.KeepRadius + Half)
		{
			Drop.Add(Pair.Key);
		}
	}
	for (const FIntVector& Key : Drop)
	{
		if (UProceduralMeshComponent* Mesh = Chunks.FindRef(Key))
		{
			Mesh->DestroyComponent();
		}
		Chunks.Remove(Key);
	}

	// Mesh the nearest missing rock first.
	TArray<TPair<double, FIntVector>> Wanted;
	for (const FIntVector& Key : Candidates)
	{
		if (Chunks.Contains(Key) || InFlight.Contains(Key))
		{
			continue;
		}
		const double D = Distance(Key);
		if (D <= Settings.StreamRadius + Half)
		{
			Wanted.Add({D, Key});
		}
	}
	Wanted.Sort([](const TPair<double, FIntVector>& A, const TPair<double, FIntVector>& B) { return A.Key < B.Key; });
	for (const TPair<double, FIntVector>& Item : Wanted)
	{
		if (InFlight.Num() >= MaxChunksInFlight)
		{
			break;
		}
		RequestChunk(Item.Value);
	}
}

void APaleotrenchCave::RequestChunk(const FIntVector& Key)
{
	InFlight.Add(Key);
	TSharedPtr<FPaleotrenchCaveBuild, ESPMode::ThreadSafe> Build = Current;
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [Build, Key]()
	{
		if (!Build->bCancelled)
		{
			Build->Finished.Enqueue(MeshChunk(*Build, Key));
		}
	});
}

void APaleotrenchCave::FinishChunks(int32 Budget)
{
	TSharedPtr<FPaleotrenchCaveChunkResult, ESPMode::ThreadSafe> Result;
	int32 Done = 0;
	while (Done < Budget && Current && Current->Finished.Dequeue(Result))
	{
		InFlight.Remove(Result->Key);
		if (Result->Triangles.Num() == 0)
		{
			Candidates.Remove(Result->Key);  // nothing to draw here: never ask again
			continue;
		}
		if (Chunks.Contains(Result->Key))
		{
			continue;
		}
		UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(this);
		Mesh->bUseAsyncCooking = true;
		Mesh->bUseComplexAsSimpleCollision = true;
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->RegisterComponent();
		Mesh->CreateMeshSection(0, Result->Vertices, Result->Triangles, Result->Normals, TArray<FVector2D>(), Result->Colors,
			TArray<FProcMeshTangent>(), true);
		if (LoadedRockMaterial)
		{
			Mesh->SetMaterial(0, LoadedRockMaterial);
		}
		Chunks.Add(Result->Key, Mesh);
		++Done;
	}
}

FTransform APaleotrenchCave::GetStartTransform() const
{
	if (Current)
	{
		const FName Wanted = PocketId(CaveId, Current->Field.GetGraph().Entrance);
		for (const APaleotrenchAirPocket* Pocket : AirPockets)
		{
			if (Pocket && Pocket->GetCheckpointId() == Wanted)
			{
				return Pocket->GetRespawnTransform();
			}
		}
		const pt::CaveGraph& Graph = Current->Field.GetGraph();
		return FTransform(GetActorTransform().TransformPosition(ToFVector(Graph.Nodes[static_cast<size_t>(Graph.Entrance)].Centre)));
	}
	return GetActorTransform();
}

double APaleotrenchCave::GetSurfaceWorldZ() const
{
	if (Current)
	{
		const int Entrance = Current->Field.GetGraph().Entrance;
		for (const pt::CaveAirDome& Dome : Current->Field.AirDomes())
		{
			if (Dome.Node == Entrance)
			{
				return GetActorTransform().TransformPosition(ToFVector(Dome.WaterCentre)).Z;
			}
		}
	}
	return GetActorLocation().Z;
}

FVector APaleotrenchCave::GetArenaLocation() const
{
	if (!Current)
	{
		return GetActorLocation();
	}
	const pt::CaveGraph& Graph = Current->Field.GetGraph();
	return GetActorTransform().TransformPosition(ToFVector(Graph.Nodes[static_cast<size_t>(Graph.Arena)].Centre));
}

bool APaleotrenchCave::FindSwimPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const
{
	OutPath.Reset();
	if (!Current || !Current->bNavReady)
	{
		return false;
	}
	const FTransform& Cave = GetActorTransform();
	std::vector<pt::Vec3> Path;
	if (!Current->Nav.FindPath(ToVec3(Cave.InverseTransformPosition(From)), ToVec3(Cave.InverseTransformPosition(To)), Path))
	{
		return false;
	}
	for (const pt::Vec3& P : Path)
	{
		OutPath.Add(Cave.TransformPosition(ToFVector(P)));
	}
	return true;
}

FString APaleotrenchCave::Describe() const
{
	if (!Current)
	{
		return TEXT("no cave built yet");
	}
	const pt::CaveGraph& Graph = Current->Field.GetGraph();
	return FString::Printf(TEXT("%hs (cave %d), seed 0x%016llx: %d chambers (%d air pockets, entrance included), %d passages, "
		"shortest swim to the arena %.0f m; rock chunks %d loaded, %d meshing, %d possible; navigation %s"),
		pt::CaveName(static_cast<pt::ECave>(CaveId)), CaveId, CaveSeed, static_cast<int32>(Graph.Nodes.size()),
		Graph.CountKind(pt::ECaveNodeKind::AirPocket) + 1, static_cast<int32>(Graph.Passages.size()),
		Graph.Nodes[static_cast<size_t>(Graph.Arena)].RouteDistance / 100.0, Chunks.Num(), InFlight.Num(), Candidates.Num(),
		Current->bNavReady ? TEXT("ready") : TEXT("building"));
}
