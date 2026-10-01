#include "TrackDirector.h"
#include "ShadowCat.h"
#include "RunnerCat.h"
#include "CatRunGameMode.h"
#include "HazardBase.h"
#include "EnemyCube.h"
#include "ItemBox.h"
#include "ShardBurst.h"
#include "InkCanvas.h"
#include "LevelScenery.h"
#include "Buffs.h"
#include "BuffComponent.h"
#include "ItemSlotComponent.h"
#include "LaneMovementComponent.h"
#include "BossGiant.h"
#include "CoinField.h"
#include "InkDrops.h"
#include "SlidingProp.h"
#include "TrackPlatform.h"
#include "InkMarks.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

ATrackDirector::ATrackDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Rundkurs-Level: etwas ruhigeres Tempo, Hindernisse seltener als im Endlos-Modus
	Difficulty.StartSpeed = 900.f;
	Difficulty.MaxSpeed = 1150.f;
	Difficulty.SpeedGainPerSecond = 2.f;
	// mehr Hindernisse: dichtere Reihen, oefter mehrere Fahrbahnen gesperrt
	Difficulty.MaxGapTime = 1.7f;
	Difficulty.MinGapTime = 0.95f;
	Difficulty.GapRampMeters = 2500.f;
	Difficulty.FirstRowMeters = 40.f;
	Difficulty.DoubleBlockChanceStart = 0.2f;
	Difficulty.DoubleBlockChanceMax = 0.5f;
	Difficulty.EnemyStartMeters = 250.f;
	Difficulty.EnemyChanceStart = 0.05f;
	Difficulty.EnemyChanceMax = 0.15f;
	Difficulty.ItemChance = 0.f;

	EndlessLayout.bStraight = true;
	EndlessLayout.NumCircuits = 1;
	EndlessLayout.LanesPerCircuit = 5;

	FItemSpawnEntry Cloud;
	Cloud.Buff = UBuff_InkCloud::StaticClass();
	Cloud.Weight = 1.f;
	Items.Add(Cloud);
	FItemSpawnEntry Bomb;
	Bomb.Buff = UBuff_InkBomb::StaticClass();
	Bomb.Weight = 1.f;
	Items.Add(Bomb);
}

void ATrackDirector::Initialize(ARunnerCat* InCat, ACatRunGameMode* InGame)
{
	Cat = InCat;
	Game = InGame;
	Rng.GenerateNewSeed();

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Canvas = GetWorld()->SpawnActor<AInkCanvas>(AInkCanvas::StaticClass(), FTransform::Identity, P);
	Scenery = GetWorld()->SpawnActor<ALevelScenery>(ALevelScenery::StaticClass(), FTransform::Identity, P);
	UClass* BossCls = LoadClass<ABossGiant>(nullptr, TEXT("/Game/Blueprints/BP_BossGiant.BP_BossGiant_C"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	Boss = GetWorld()->SpawnActor<ABossGiant>(BossCls ? BossCls : ABossGiant::StaticClass(), FTransform::Identity, P);
	Coins = GetWorld()->SpawnActor<ACoinField>(ACoinField::StaticClass(), FTransform::Identity, P);
	Marks = GetWorld()->SpawnActor<AInkMarks>(AInkMarks::StaticClass(), FTransform::Identity, P);
	// bewegte Deko-Hindernisse: Autos fahren einmal quer, Kisten und Stoppschilder pendeln (Haeuser/Baeume bleiben Kulisse)
	for (const TCHAR* Name : { TEXT("Car"), TEXT("Crate"), TEXT("Stopsign") })
	{
		if (UTexture2D* T = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Nature/T_Sprite_%s.T_Sprite_%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			SlideTex.Add(T);
		}
	}
	for (int32 I = 0; I < 6; ++I)
	{
		Boxes.Add(GetWorld()->SpawnActor<AItemBox>(AItemBox::StaticClass(), FTransform::Identity, P));
	}
	SetMode(ERunMode::Tutorial);
}

void ATrackDirector::SetMode(ERunMode InMode)
{
	Mode = InMode;
	Layout = Mode == ERunMode::Endless ? EndlessLayout : TutorialLayout;
	Cat->SetLayout(Layout);
	if (UItemSlotComponent* Slot = Cat->GetSlot())
	{
		Slot->SetPool(Items);
	}
	Canvas->Build(Layout);
	Scenery->Build(Layout, Density, 1234);
	Boss->SetActive(Mode == ERunMode::Endless);
	Coins->ClearAll();
	// Rundkurs: viele Abdruecke, damit man sieht, welche Fahrbahnen schon gefaerbt sind
	Marks->SetPrintCapacity(Layout.bStraight ? 400 : 3200);
	Marks->ClearAll();
	PlaceBoxes();
	UE_LOG(LogShadowCat, Log, TEXT("Modus %s: %d Kurs(e) x %d Fahrbahnen, %d Kacheln"),
		Mode == ERunMode::Endless ? TEXT("Endlos") : TEXT("Tutorial"), Layout.NumCircuits, Layout.LanesPerCircuit, Canvas->GetTotalSections());
}

void ATrackDirector::PlaceBoxes()
{
	for (AItemBox* B : Boxes)
	{
		B->Collect(1.0e9f);
	}
	if (Layout.bStraight)
	{
		NextBoxA = 4500.f;
		return;
	}
	// Rundkurs: feste Stellen je Kurs, versetzt
	const float L = Layout.LoopLength();
	int32 Index = 0;
	for (int32 C = 0; C < Layout.NumCircuits; ++C)
	{
		for (int32 K = 0; K < BoxesPerCircuit && Boxes.IsValidIndex(Index); ++K, ++Index)
		{
			const float A = Layout.WrapA(L * (K + 0.3f + 0.21f * C) / FMath::Max(1, BoxesPerCircuit));
			const int32 G = C * Layout.LanesPerCircuit + Rng.RandHelper(Layout.LanesPerCircuit);
			FVector Pos, Fwd;
			Layout.Sample(A, Layout.LaneLat(G), Pos, Fwd);
			Boxes[Index]->Place(Pos, Fwd, G, A);
		}
	}
}

void ATrackDirector::RebuildScenery(float InDensity)
{
	Density = InDensity;
	if (Scenery)
	{
		Scenery->Build(Layout, Density, 1234);
	}
}

float ATrackDirector::GetMeters() const
{
	return Cat ? Cat->GetTravel() / 100.f : 0.f;
}

int32 ATrackDirector::GetCatCircuit() const
{
	return Cat ? Layout.CircuitOf(Cat->GetLanes()->GetCurrentLane()) : 0;
}

bool ATrackDirector::CanChangeCircuit() const
{
	return Cat && Layout.IsConnection(Cat->GetA());
}

float ATrackDirector::WorldAhead(float HazardA) const
{
	if (Layout.bStraight)
	{
		return HazardA - Cat->GetA();
	}
	// Weltstrecke entlang der aktuellen Fahrbahn der Katze
	const int32 G = Cat->GetLanes()->GetCurrentLane();
	const float LL = Layout.LaneLength(G);
	float D = Layout.LaneArc(G, HazardA) - Layout.LaneArc(G, Cat->GetA());
	D = FMath::Fmod(D + LL * 1.5f, LL) - LL * 0.5f;
	return D;
}

float ATrackDirector::TravelToA(float Travel) const
{
	const int32 Mid = PlanCircuit * Layout.LanesPerCircuit + Layout.LanesPerCircuit / 2;
	return Layout.AFromLaneArc(Mid, Layout.LaneArc(Mid, Cat->GetA()) + (Travel - Cat->GetTravel()));
}

// ------------------------------------------------------------------------------------------------
// Pools
// ------------------------------------------------------------------------------------------------
AObstacle* ATrackDirector::AcquireObstacle(EObstacleType Type)
{
	for (AObstacle* O : Obstacles)
	{
		if (O && !O->IsLive() && O->GetType() == Type)
		{
			return O;
		}
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AObstacle* O = GetWorld()->SpawnActor<AObstacle>(AObstacle::StaticClass(), FTransform(FVector(0.f, 0.f, -5000.f)), P);
	O->Build(Type);
	Obstacles.Add(O);
	return O;
}

AEnemyCube* ATrackDirector::AcquireEnemy()
{
	for (AEnemyCube* E : Enemies)
	{
		if (E && !E->IsLive())
		{
			return E;
		}
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEnemyCube* E = GetWorld()->SpawnActor<AEnemyCube>(AEnemyCube::StaticClass(), FTransform(FVector(0.f, 0.f, -5000.f)), P);
	E->Build();
	Enemies.Add(E);
	return E;
}

void ATrackDirector::Burst(const FVector& Loc, float Gray, bool bGlow, float Size)
{
	AShardBurst* Use = nullptr;
	for (AShardBurst* B : Bursts)
	{
		if (B && !B->IsBusy())
		{
			Use = B;
			break;
		}
	}
	if (!Use)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Use = GetWorld()->SpawnActor<AShardBurst>(AShardBurst::StaticClass(), FTransform(Loc), P);
		Bursts.Add(Use);
	}
	Use->Fire(Loc, Gray, bGlow, Size);
}

void ATrackDirector::SpawnDrops(const FVector& Loc, float Size)
{
	AInkDrops* Use = nullptr;
	for (AInkDrops* D : DropPool)
	{
		if (D && !D->IsBusy())
		{
			Use = D;
			break;
		}
	}
	if (!Use)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Use = GetWorld()->SpawnActor<AInkDrops>(AInkDrops::StaticClass(), FTransform(Loc), P);
		DropPool.Add(Use);
	}
	Use->Fire(Loc, Cat->GetForward(), Size);
}

void ATrackDirector::RetireAllHazards()
{
	for (AObstacle* O : Obstacles) { if (O) O->Retire(); }
	for (AEnemyCube* E : Enemies) { if (E) E->Retire(); }
	for (ASlidingProp* S : Sliders) { if (S) S->Retire(); }
	for (ATrackPlatform* P : Platforms) { if (P) P->Retire(); }
}

float ATrackDirector::GroundAt(int32 G, float InA) const
{
	float H = 0.f;
	for (const ATrackPlatform* P : Platforms)
	{
		if (P && P->IsLive() && P->Lane == G)
		{
			H = FMath::Max(H, P->HeightAt(InA));
		}
	}
	return H;
}

bool ATrackDirector::TerrainBusy(int32 G, float InA0, float InA1) const
{
	for (const ATrackPlatform* P : Platforms)
	{
		if (P && P->IsLive() && P->Lane == G && P->A1 > InA0 && P->A0 < InA1)
		{
			return true;
		}
	}
	return false;
}

bool ATrackDirector::ChasmBusy(float InA0, float InA1) const
{
	for (const FVector2D& Z : ChasmZones)
	{
		if (Z.Y > InA0 && Z.X < InA1)
		{
			return true;
		}
	}
	return false;
}

void ATrackDirector::SpawnPit(int32 G, int32 FirstSec, int32 NumSec)
{
	const float SL = Layout.SectionLength;
	AObstacle* O = AcquireObstacle(EObstacleType::Pit);
	O->SetPitLength(NumSec * SL);
	const float A = (FirstSec + NumSec * 0.5f) * SL;
	for (int32 K = 0; K < NumSec; ++K)
	{
		Canvas->SetHole(G, FirstSec + K);
	}
	FVector Pos, Fwd;
	Layout.Sample(A, Layout.LaneLat(G), Pos, Fwd);
	O->Place(Pos, Fwd.Rotation(), G, A);
}

void ATrackDirector::UpdatePitWalls()
{
	TArray<AObstacle*> Pits;
	for (AObstacle* O : Obstacles)
	{
		if (O && O->IsLive() && O->GetType() == EObstacleType::Pit)
		{
			Pits.Add(O);
		}
	}
	for (AObstacle* P : Pits)
	{
		bool bLeftOpen = false;
		bool bRightOpen = false;
		bool bNearOpen = false;
		bool bFarOpen = false;
		for (const AObstacle* Q : Pits)
		{
			// gleiche Fahrbahn, direkt anschliessend: keine Wand/Kante am Uebergang
			if (Q != P && Q->Lane == P->Lane)
			{
				if (FMath::Abs((Q->A - Q->HalfLength) - (P->A + P->HalfLength)) < 2.f)
				{
					bFarOpen = true;
				}
				if (FMath::Abs((Q->A + Q->HalfLength) - (P->A - P->HalfLength)) < 2.f)
				{
					bNearOpen = true;
				}
			}
			// Nachbar-Fahrbahn mit genau gleichem Abgrund-Abschnitt
			if (Q == P || FMath::Abs(Q->A - P->A) > 1.f || FMath::Abs(Q->HalfLength - P->HalfLength) > 1.f)
			{
				continue;
			}
			const float DLat = Layout.LaneLat(Q->Lane) - Layout.LaneLat(P->Lane);
			if (FMath::Abs(FMath::Abs(DLat) - Layout.LaneWidth) < 1.f)
			{
				(DLat < 0.f ? bLeftOpen : bRightOpen) = true;
			}
		}
		P->SetPitSideWalls(!bLeftOpen, !bRightOpen);
		P->SetPitEnds(!bNearOpen, !bFarOpen);
	}
}

void ATrackDirector::StepChasms()
{
	const float CatA = Cat->GetA();
	ChasmZones.RemoveAll([CatA](const FVector2D& Z) { return Z.Y < CatA - 1500.f; });
	if (!Layout.bStraight || !bHazards || GetMeters() < ChasmStartMeters)
	{
		return;
	}
	// wie das Gelaende direkt hinter der Planungsgrenze: spaetere Reihen lassen die Schlucht frei
	const float Known = CatA + (Planner.GetFrontierX() - Cat->GetTravel());
	NextChasmA = FMath::Max(NextChasmA, Known + 100.f);
	if (NextChasmA > Known + 2500.f)
	{
		return;
	}
	const int32 N = Layout.LanesPerCircuit;
	const float SL = Layout.SectionLength;
	// Abgrund - Insel (nur Mitte) - Abgrund; bei hohem Tempo laengere Insel (Sprungweite waechst)
	const int32 PitSec = 2;
	const int32 IslandSec = Speed > 1100.f ? 3 : 2;
	const int32 S0 = FMath::CeilToInt(NextChasmA / SL);
	const float Z0 = S0 * SL;
	const float Z1 = (S0 + 2 * PitSec + IslandSec) * SL;
	for (int32 L = 0; L < N; ++L)
	{
		if (TerrainBusy(L, Z0 - 600.f, Z1 + 600.f))
		{
			NextChasmA = Z1 + 1000.f;
			return;
		}
	}
	const int32 Mid = N / 2;
	// alle Fahrbahnen in gleichen Stuecken (vorn, Mitte, hinten): zwischen nebeneinanderliegenden Stuecken keine
	// Zwischenwand; nur die Insel der mittleren Fahrbahn hat Waende
	for (int32 L = 0; L < N; ++L)
	{
		SpawnPit(L, S0, PitSec);
		if (L != Mid)
		{
			SpawnPit(L, S0 + PitSec, IslandSec);
		}
		SpawnPit(L, S0 + PitSec + IslandSec, PitSec);
	}
	UpdatePitWalls();
	ChasmZones.Add(FVector2D(Z0 - 400.f, Z1 + 400.f));
	Coins->RemoveRange(Z0 - 300.f, Z1 + 300.f);
	NextTerrainA = FMath::Max(NextTerrainA, Z1 + 1500.f);
	NextSlideA = FMath::Max(NextSlideA, Z1 + 1500.f);
	UE_LOG(LogShadowCat, Log, TEXT("Schlucht bei %.0f m bis %.0f m, Insel %d m auf Fahrbahn %d"), Z0 / 100.f, Z1 / 100.f, IslandSec * 3, Mid + 1);
	NextChasmA = Z1 + Rng.FRandRange(ChasmSpacing.X, ChasmSpacing.Y);
}

void ATrackDirector::StepTerrain()
{
	for (ATrackPlatform* P : Platforms)
	{
		if (P && P->IsLive() && P->A1 < Cat->GetA() - 900.f)
		{
			P->Retire();
		}
	}
	if (!Layout.bStraight || !bHazards || GetMeters() < TerrainStartMeters)
	{
		return;
	}
	// direkt hinter der Planungsgrenze anlegen: alle spaeter geplanten Reihen meiden die Gelaende-Fahrbahnen
	const float Known = Cat->GetA() + (Planner.GetFrontierX() - Cat->GetTravel());
	if (NextTerrainA < Known + 100.f)
	{
		NextTerrainA = Known + 100.f;
	}
	if (NextTerrainA > Known + 2500.f)
	{
		return;
	}
	if (ChasmBusy(NextTerrainA - 600.f, NextTerrainA + 2600.f))
	{
		NextTerrainA += 1000.f;
		return;
	}
	const int32 N = Layout.LanesPerCircuit;
	TArray<int32> Lanes;
	for (int32 L = 0; L < N; ++L)
	{
		Lanes.Add(L);
	}
	for (int32 I = Lanes.Num() - 1; I > 0; --I)
	{
		Lanes.Swap(I, Rng.RandHelper(I + 1));
	}
	// 1-4 Fahrbahnen, mindestens eine bleibt flach
	const int32 Count = Rng.RandRange(1, FMath::Max(1, N - 1));
	float ZoneEnd = NextTerrainA;
	int32 Mask = 0;
	FString Desc;
	for (int32 K = 0; K < Count; ++K)
	{
		const int32 L = Lanes[K];
		ATrackPlatform* Pl = nullptr;
		for (ATrackPlatform* P : Platforms)
		{
			if (P && !P->IsLive())
			{
				Pl = P;
				break;
			}
		}
		if (!Pl)
		{
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Pl = GetWorld()->SpawnActor<ATrackPlatform>(ATrackPlatform::StaticClass(), FTransform(FVector(0.f, 0.f, -5000.f)), SP);
			Platforms.Add(Pl);
		}
		// Rampe mit Plateau (hochlaufen) oder Erhoehung ohne Rampe (draufspringen oder ausweichen)
		const bool bRamp = Rng.FRand() < 0.62f;
		const float H = bRamp ? Rng.FRandRange(110.f, 260.f) : Rng.FRandRange(85.f, 160.f);
		const float Ramp = bRamp ? H * Rng.FRandRange(2.8f, 3.6f) : 0.f;
		const float Plateau = Rng.FRandRange(700.f, 1600.f);
		const float A0 = NextTerrainA + Rng.FRandRange(0.f, 600.f);
		Pl->Setup(L, A0, Ramp, Plateau, H, Layout.LaneLat(L), Layout.LaneWidth);
		ZoneEnd = FMath::Max(ZoneEnd, A0 + Ramp + Plateau);
		Mask |= 1 << L;
		Desc += FString::Printf(TEXT(" F%d:%s%.0f"), L + 1, bRamp ? TEXT("Rampe") : TEXT("Block"), H);
		// manchmal eine hoehere Stufe direkt dahinter: vom Plateau aus hochspringen
		if (H < 200.f && Rng.FRand() < 0.4f)
		{
			ATrackPlatform* Step = nullptr;
			for (ATrackPlatform* P : Platforms)
			{
				if (P && !P->IsLive())
				{
					Step = P;
					break;
				}
			}
			if (!Step)
			{
				FActorSpawnParameters SP;
				SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Step = GetWorld()->SpawnActor<ATrackPlatform>(ATrackPlatform::StaticClass(), FTransform(FVector(0.f, 0.f, -5000.f)), SP);
				Platforms.Add(Step);
			}
			const float H2 = H + Rng.FRandRange(80.f, 110.f);
			const float Len2 = Rng.FRandRange(500.f, 900.f);
			Step->Setup(L, A0 + Ramp + Plateau, 0.f, Len2, H2, Layout.LaneLat(L), Layout.LaneWidth);
			ZoneEnd = FMath::Max(ZoneEnd, A0 + Ramp + Plateau + Len2);
			Desc += FString::Printf(TEXT("+Stufe%.0f"), H2);
		}
	}
	Coins->RemoveRange(NextTerrainA - 200.f, ZoneEnd + 200.f, Mask);
	UE_LOG(LogShadowCat, Log, TEXT("Gelaende bei %.0f m:%s"), NextTerrainA / 100.f, *Desc);
	NextTerrainA = ZoneEnd + Rng.FRandRange(TerrainSpacing.X, TerrainSpacing.Y);
}

void ATrackDirector::StepSliders(float DeltaTime)
{
	const float S = FMath::Max(Speed, 300.f);
	for (ASlidingProp* P : Sliders)
	{
		if (!P || !P->IsLive())
		{
			continue;
		}
		const float Ahead = P->A - Cat->GetA();
		if (Ahead < -700.f)
		{
			P->Retire();
			continue;
		}
		P->StepSlide(DeltaTime, FMath::Max(0.f, Ahead) / S);
	}
	if (SlideTex.Num() == 0 || !bHazards || GetMeters() < SlideStartMeters)
	{
		return;
	}
	NextSlideA = FMath::Max(NextSlideA, Cat->GetA() + 3000.f);
	// nur dort, wo die Hindernisse schon geplant sind, und quer ueber alle Fahrbahnen frei
	const float Known = Cat->GetA() + (Planner.GetFrontierX() - Cat->GetTravel());
	if (NextSlideA > Known - 200.f)
	{
		return;
	}
	// Abgruende, Gelaende und andere bewegte Deko haben Vorrang (weiterschieben); gewoehnliche Hindernisse und
	// Wuerfel in diesem Abschnitt werden stattdessen entfernt, sonst kaemen bei dichter Strecke kaum Autos/Kisten
	const float W0 = NextSlideA - 650.f, W1 = NextSlideA + 650.f;
	bool bBlocked = false;
	for (const AObstacle* O : Obstacles)
	{
		bBlocked |= O && O->IsLive() && O->GetType() == EObstacleType::Pit && O->A + O->HalfLength > W0 - 400.f && O->A - O->HalfLength < W1 + 400.f;
	}
	bBlocked |= ChasmBusy(W0, W1);
	for (int32 L = 0; L < Layout.LanesPerCircuit; ++L)
	{
		bBlocked |= TerrainBusy(L, W0, W1);
	}
	for (const ASlidingProp* Other : Sliders)
	{
		bBlocked |= Other && Other->IsLive() && FMath::Abs(Other->A - NextSlideA) < 1500.f;
	}
	if (bBlocked)
	{
		NextSlideA += 400.f;
		return;
	}
	for (AObstacle* O : Obstacles)
	{
		if (O && O->IsLive() && O->A + O->HalfLength > W0 && O->A - O->HalfLength < W1)
		{
			O->Retire();
		}
	}
	for (AEnemyCube* E : Enemies)
	{
		if (E && E->IsLive() && E->A + E->HalfLength > W0 && E->A - E->HalfLength < W1)
		{
			E->Retire();
		}
	}
	ASlidingProp* Prop = nullptr;
	for (ASlidingProp* P : Sliders)
	{
		if (P && !P->IsLive())
		{
			Prop = P;
			break;
		}
	}
	if (!Prop)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Prop = GetWorld()->SpawnActor<ASlidingProp>(ASlidingProp::StaticClass(), FTransform(FVector(0.f, 0.f, -5000.f)), P);
		Sliders.Add(Prop);
	}
	// Auto 45 %, sonst Kiste oder Stoppschild
	UTexture2D* Tex = SlideTex[Rng.RandHelper(SlideTex.Num())];
	for (UTexture2D* T : SlideTex)
	{
		if (T->GetName().Contains(TEXT("Car")) && Rng.FRand() < 0.45f)
		{
			Tex = T;
		}
	}
	const FString Name = Tex->GetName();
	const bool bCar = Name.Contains(TEXT("Car"));
	const bool bCrate = Name.Contains(TEXT("Crate"));
	const int32 N = Layout.LanesPerCircuit;
	const float Half = N * Layout.LaneWidth * 0.5f;
	FVector Pos, Fwd;
	Layout.Sample(NextSlideA, 0.f, Pos, Fwd);
	float PropH, SlideSpeed;
	if (bCar)
	{
		// faehrt einmal quer: ist beim Eintreffen der Katze irgendwo auf der Strasse (Zeitpunkt leicht gestreut)
		const float Dir = Rng.FRand() < 0.5f ? 1.f : -1.f;
		PropH = Rng.FRandRange(220.f, 250.f);
		// Bild zeigt nach rechts (+Y); faehrt es nach links, wird es gespiegelt
		Prop->Setup(Tex, PropH, 0.85f, Dir);
		Prop->Height = 999.f;
		SlideSpeed = Rng.FRandRange(380.f, 520.f);
		const float Arrive = (NextSlideA - Cat->GetA()) / FMath::Max(Speed, 300.f) + Rng.FRandRange(-0.35f, 0.35f);
		const float Edge = Half + 450.f;
		Prop->LaunchCross(Pos, NextSlideA, -Dir * Edge, Dir * Edge, SlideSpeed, FMath::Max(0.f, Arrive), Rng.FRandRange(-Half + 60.f, Half - 60.f));
	}
	else
	{
		// Kiste (drueberspringbar) oder Stoppschild (nur der Pfahl trifft) pendeln von Rand zu Rand
		PropH = bCrate ? 115.f : 340.f;
		Prop->Setup(Tex, PropH, bCrate ? 0.8f : 0.3f);
		Prop->Height = bCrate ? 108.f : 999.f;
		SlideSpeed = Rng.FRandRange(220.f, 320.f);
		Prop->Launch(Pos, NextSlideA, -Half + 40.f, Half - 40.f, SlideSpeed, Rng.FRand());
	}
	Coins->RemoveRange(NextSlideA - 300.f, NextSlideA + 300.f);
	UE_LOG(LogShadowCat, Log, TEXT("Deko wird Hindernis: %s (%.0f cm) bei %.0f m, %s mit %.0f cm/s"), *Name, PropH, NextSlideA / 100.f, bCar ? TEXT("faehrt quer") : TEXT("pendelt"), SlideSpeed);
	NextSlideA += Rng.FRandRange(SlideSpacing.X, SlideSpacing.Y);
}

// ------------------------------------------------------------------------------------------------
// Ablauf
// ------------------------------------------------------------------------------------------------
void ATrackDirector::ResetTrack()
{
	RetireAllHazards();
	Canvas->ResetInk(GetStartA());
	PlaceBoxes();
	for (FPuddle& Pd : Puddles)
	{
		if (Pd.Visual) Pd.Visual->DestroyComponent();
		for (UStaticMeshComponent* B : Pd.Blobs) { if (B) B->DestroyComponent(); }
	}
	Puddles.Reset();
	Incoming.Reset();
	NextBossAttack = BossFirstAttack;
	NextSalvo = SalvoFirst;
	SalvoCount = 0;
	Coins->ClearAll();
	NextCoinA = GetStartA() + 2500.f;
	NextSlideA = GetStartA() + SlideStartMeters * 100.f;
	NextTerrainA = GetStartA() + TerrainStartMeters * 100.f;
	NextChasmA = GetStartA() + ChasmStartMeters * 100.f;
	ChasmZones.Reset();
	Marks->ClearAll();
	FootHist.Reset();
	NextPrintTravel = PawStep;
	LastSmearA = -1.0e9f;
	Boss->SetActive(Mode == ERunMode::Endless);
	if (UItemSlotComponent* Slot = Cat->GetSlot())
	{
		Slot->Clear();
		Slot->SetPool(Items);
	}
	Speed = 0.f;
	RunTime = 0.f;
	bRunning = false;
	LastPaintLane = INDEX_NONE;
	PlanCircuit = Layout.CircuitOf(GetStartLane());
	Planner.Reset(Layout.LanesPerCircuit, Cat->GetLanes()->SwitchDuration, Difficulty.FirstRowMeters * 100.f, Layout.LaneOf(GetStartLane()), Rng.RandHelper(1 << 30));
	Planner.bAllowPits = Layout.bStraight;
}

void ATrackDirector::BeginRun()
{
	bRunning = true;
	RunTime = 0.f;
	Speed = 0.f;
	LastPaintLane = INDEX_NONE;
	PlanAhead();
}

void ATrackDirector::StopRun()
{
	bRunning = false;
	Speed = 0.f;
}

void ATrackDirector::StepDirector(float DeltaTime)
{
	for (AShardBurst* B : Bursts)
	{
		if (B) B->StepBurst(DeltaTime);
	}
	for (AInkDrops* D : DropPool)
	{
		if (D) D->StepDrops(DeltaTime);
	}
	Marks->StepMarks(DeltaTime);
	Canvas->StepCanvas(DeltaTime);
	Canvas->UpdateWindow(Cat->GetA());
	Scenery->StepScenery(DeltaTime, Cat->GetActorLocation());
	if (Boss)
	{
		Boss->StepGiant(DeltaTime, Cat->GetActorLocation());
	}
	CheckBoxes(DeltaTime);
	if (UItemSlotComponent* Slot = Cat->GetSlot())
	{
		Slot->StepSlot(DeltaTime);
	}
	if (!bRunning || Mode != ERunMode::Endless)
	{
		Coins->StepCoins(DeltaTime, Cat->GetA(), 0.f, 9999.f, false);
	}
	if (!bRunning)
	{
		return;
	}

	RunTime += DeltaTime;
	const float Target = FMath::Min(Difficulty.MaxSpeed, Difficulty.StartSpeed + Difficulty.SpeedGainPerSecond * RunTime);
	Speed = FMath::FInterpConstantTo(Speed, Target, DeltaTime, 1500.f);

	if (bAutopilot)
	{
		RunAutopilot();
	}
	// im Loch kaum Vorwaertsbewegung (faellt nicht durch die Schachtwand); danach hinter dem Loch weiter
	Cat->StepRun(DeltaTime, Cat->IsFalling() ? Speed * 0.15f : Speed);
	if (Cat->ConsumeFallEnd())
	{
		Cat->WarpToA(FMath::Max(Cat->GetA(), FallRespawnA));
	}
	StepPawPrints();
	// Tintentropfen beim Absprung (und kleiner bei der Landung), Sprung-Sound
	if (Cat->ConsumeJumpEvent())
	{
		SpawnDrops(Cat->GetActorLocation(), 1.f);
		if (Game)
		{
			Game->PlayJump();
		}
	}
	if (Cat->ConsumeLandEvent())
	{
		SpawnDrops(Cat->GetActorLocation(), 0.6f);
	}
	PaintTrail();
	if (!bRunning)
	{
		return; // Level fertig
	}
	CheckCircuitChange();
	PlanAhead();
	for (AEnemyCube* E : Enemies)
	{
		if (E && E->IsLive())
		{
			E->StepEnemy(DeltaTime, WorldAhead(E->A), FMath::Max(Speed, 1.f), Difficulty.EnemyFlyTime, Difficulty.EnemySettleLead);
		}
	}
	CheckCollisions();
	if (bRunning && Mode == ERunMode::Endless)
	{
		StepBoss(DeltaTime);
		CheckPuddles();
		StepEndlessBoxes();
		StepChasms();
		StepTerrain();
		StepEndlessCoins(DeltaTime);
		StepSliders(DeltaTime);
	}
	RecycleBehind();
}

void ATrackDirector::StepPawPrints()
{
	// Fussposition merken; Abdruecke entstehen PawLag hinter der Katze, nur wo sie wirklich aufgetreten ist
	FFootSample S;
	S.Travel = Cat->GetTravel();
	S.Pos = Cat->GetActorLocation();
	const bool bGround = Cat->IsGrounded() && !Cat->IsFalling();
	S.Pos.Z = (bGround ? Cat->GetFeetZ() : 0.f) + 1.5f;
	S.Yaw = Cat->GetForward().Rotation().Yaw;
	S.bGround = bGround;
	FootHist.Add(S);
	while (FootHist.Num() > 2 && FootHist[1].Travel < S.Travel - PawLag - 400.f)
	{
		FootHist.RemoveAt(0);
	}
	const float Want = S.Travel - PawLag;
	for (int32 Guard = 0; Guard < 12 && NextPrintTravel <= Want; ++Guard)
	{
		const float T = NextPrintTravel;
		NextPrintTravel += PawStep;
		// Stuetzpunkte links und rechts von T suchen
		int32 I = 0;
		while (I + 1 < FootHist.Num() && FootHist[I + 1].Travel < T)
		{
			++I;
		}
		if (I + 1 >= FootHist.Num() || FootHist[I].Travel > T)
		{
			continue;
		}
		const FFootSample& P0 = FootHist[I];
		const FFootSample& P1 = FootHist[I + 1];
		if (!P0.bGround || !P1.bGround || P1.Travel - P0.Travel > 400.f)
		{
			continue; // Sprung, Sturz oder Versatz hinter ein Loch: keine Abdruecke
		}
		const float Alpha = (T - P0.Travel) / FMath::Max(1.f, P1.Travel - P0.Travel);
		const FVector Pos = FMath::Lerp(P0.Pos, P1.Pos, Alpha);
		const float Yaw = P0.Yaw;
		const FVector Right = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
		bPrintLeft = !bPrintLeft;
		Marks->AddPrint(Pos + Right * (bPrintLeft ? -11.f : 11.f), Yaw + FMath::FRandRange(-6.f, 6.f), PawSize * FMath::FRandRange(0.92f, 1.05f), bPrintLeft);
	}
}

int32 ATrackDirector::BombBlast(float Range)
{
	const float CatA = Cat->GetA();
	const int32 Circuit = GetCatCircuit();
	int32 N = 0;
	auto Ahead = [&](const AHazardBase* H)
	{
		const float D = WorldAhead(H->A);
		return D > -200.f && D < Range;
	};
	// alles im Weg verschwindet: Hindernisse, Deko-Hindernisse, Gegner, weisse Tinte - und Loecher werden zugeschuettet
	bool bPitClosed = false;
	for (AObstacle* O : Obstacles)
	{
		if (!O || !O->IsLive() || Layout.CircuitOf(O->Lane) != Circuit || !Ahead(O))
		{
			continue;
		}
		if (O->GetType() == EObstacleType::Pit)
		{
			// nur Loecher, ueber denen die Katze nicht gerade faellt
			if (WorldAhead(O->A) - O->HalfLength < 0.f)
			{
				continue;
			}
			const float SL = Layout.SectionLength;
			for (int32 K = FMath::RoundToInt((O->A - O->HalfLength) / SL); K < FMath::RoundToInt((O->A + O->HalfLength) / SL); ++K)
			{
				Canvas->ClearHole(O->Lane, K);
			}
			bPitClosed = true;
		}
		else
		{
			Burst(O->GetActorLocation() + FVector(0.f, 0.f, 70.f), 0.05f, false, 1.8f);
		}
		O->Retire();
		++N;
	}
	if (bPitClosed)
	{
		ChasmZones.RemoveAll([&](const FVector2D& Z) { return Z.Y > CatA && Z.X < CatA + Range; });
		UpdatePitWalls();
	}
	for (AEnemyCube* E : Enemies)
	{
		if (E && E->IsLive() && Layout.CircuitOf(E->Lane) == Circuit && Ahead(E))
		{
			DefeatEnemy(E);
			++N;
		}
	}
	for (ASlidingProp* S : Sliders)
	{
		if (S && S->IsLive() && Ahead(S))
		{
			Burst(S->GetActorLocation() + FVector(0.f, 0.f, 150.f), 0.05f, false, 3.f);
			S->Retire();
			++N;
		}
	}
	// weisse Tinte des Riesen vorn wegputzen
	for (int32 I = Puddles.Num() - 1; I >= 0; --I)
	{
		FPuddle& Pd = Puddles[I];
		if (Pd.A1 > CatA - 200.f && Pd.A0 < CatA + Range)
		{
			if (Pd.Visual) Pd.Visual->DestroyComponent();
			for (UStaticMeshComponent* B : Pd.Blobs) { if (B) B->DestroyComponent(); }
			Puddles.RemoveAt(I);
			++N;
		}
	}
	// bereits geworfene weisse Tinte im Bereich landet nicht mehr als Pfuetze
	for (FIncoming& In : Incoming)
	{
		if (In.Mask && In.A > CatA - 200.f && In.A < CatA + Range)
		{
			In.Mask = 0;
			++N;
		}
	}
	if (N > 0 && Game)
	{
		Game->OnInkPainted(10 * N);
	}
	UE_LOG(LogShadowCat, Log, TEXT("Tintenbombe: %d Dinge weggesprengt"), N);
	return N;
}

void ATrackDirector::InkSmearArea(int32 Circuit, float CenterA, float HalfLen, bool bFromBack)
{
	// Tintenbombe: eine zusammenhaengende, glaenzend schwarze Tintenflaeche ueber alle Fahrbahnen. Sie besteht aus kurzen,
	// ueberlappenden Stuecken, die dem Gelaende folgen (Rampen geneigt, Plateaus oben); zur gleich hohen Nachbar-
	// Fahrbahn laeuft sie ueber den Rand hinaus und verschmilzt, an Kanten zu hoeheren/tieferen Fahrbahnen endet sie.
	const int32 N = Layout.LanesPerCircuit;
	const float Step = 60.f;
	const float Ext = 45.f;
	for (int32 L = 0; L < N; ++L)
	{
		const int32 G = Circuit * N + L;
		const float Arc0 = Layout.LaneArc(G, CenterA);
		for (float D = -HalfLen; D < HalfLen; D += Step)
		{
			const float A0 = Layout.AFromLaneArc(G, Arc0 + D);
			const float A1 = Layout.AFromLaneArc(G, Arc0 + FMath::Min(D + Step, HalfLen));
			const float Z0 = Layout.bStraight ? GroundAt(G, A0) : 0.f;
			const float Z1 = Layout.bStraight ? GroundAt(G, A1) : 0.f;
			if (FMath::Abs(Z1 - Z0) > 35.f)
			{
				continue; // senkrechte Kante (Block/Stufe): keine Tinte in der Luft
			}
			const float AM = (A0 + A1) * 0.5f;
			const float ZM = (Z0 + Z1) * 0.5f;
			auto SameHeight = [&](int32 NL)
			{
				return NL >= 0 && NL < N && FMath::Abs((Layout.bStraight ? GroundAt(Circuit * N + NL, AM) : 0.f) - ZM) < 6.f;
			};
			// Seitenlage: +Lat-Seite ist die naechste Fahrbahn (L + 1)
			const float ExtLo = SameHeight(L - 1) ? Ext : 0.f;
			const float ExtHi = SameHeight(L + 1) ? Ext : 0.f;
			const float Lat = Layout.LaneLat(G) + (ExtHi - ExtLo) * 0.5f;
			const float W = Layout.LaneWidth + ExtLo + ExtHi;
			const FVector P0 = Layout.Position(A0, Lat) + FVector(0.f, 0.f, Z0 + 1.5f);
			const FVector P1 = Layout.Position(A1, Lat) + FVector(0.f, 0.f, Z1 + 1.5f);
			// Welle von der Katze nach vorn (bzw. von der Mitte nach aussen)
			const float Delay = bFromBack ? (D + HalfLen) / FMath::Max(1.f, 2.f * HalfLen) * 0.6f : FMath::Abs(D) / FMath::Max(1.f, HalfLen) * 0.3f;
			Marks->AddSheet(P0, P1, W, 3.6f, Delay, ExtLo > 0.f, ExtHi > 0.f);
		}
	}
}

void ATrackDirector::InkSmearTrail(int32 Circuit, float CatA)
{
	// Tintenwolke: alle ~1 m hinter der Katze auf jeder Fahrbahn des Kurses ein Klecks
	if (FMath::Abs(CatA - LastSmearA) < 100.f)
	{
		return;
	}
	LastSmearA = CatA;
	for (int32 L = 0; L < Layout.LanesPerCircuit; ++L)
	{
		const int32 G = Circuit * Layout.LanesPerCircuit + L;
		const float A = Layout.AFromLaneArc(G, Layout.LaneArc(G, CatA) - 120.f + FMath::FRandRange(-30.f, 30.f));
		const FVector P = Layout.Position(A, Layout.LaneLat(G) + FMath::FRandRange(-35.f, 35.f));
		const float Z = Layout.bStraight ? GroundAt(G, A) : 0.f;
		Marks->AddBlob(P + FVector(0.f, 0.f, Z + 2.f), FMath::FRandRange(45.f, 85.f), FMath::FRandRange(1.8f, 2.6f));
	}
}

void ATrackDirector::PaintTrail()
{
	// Die Katze faerbt immer die Fahrbahn, auf der sie gerade ist (auch im Sprung: Tinte tropft weiter)
	const int32 G = Cat->GetLanes()->GetCurrentLane();
	// Spur beginnt etwas hinter der Katze, damit die Fuesse auf weissem Grund gut zu sehen sind
	float A = Layout.AFromLaneArc(G, Layout.LaneArc(G, Cat->GetA()) - TrailLag);
	if (Layout.bStraight)
	{
		A = FMath::Max(A, 0.f);
	}
	int32 N = 0;
	if (G == LastPaintLane)
	{
		N = Canvas->PaintSpan(G, LastPaintA, A);
	}
	else
	{
		N = Canvas->PaintSection(G, Layout.SectionAt(G, A)) ? 1 : 0;
	}
	// Optik: aktuelle Kachel bis zur Katze bedecken, verlassene Kachel fertig einfaerben
	const int32 Sec = Layout.SectionAt(G, A);
	if (LastPaintLane != INDEX_NONE && (G != LastPaintLane || Sec != LastSection))
	{
		Canvas->FinishCoverage(LastPaintLane, LastSection);
		if (G == LastPaintLane)
		{
			// uebersprungene Kacheln (sehr schnelle Frames) ebenfalls fertig
			const int32 NS = Layout.NumSections(G);
			for (int32 K = (LastSection + 1) % NS; K != Sec; K = (K + 1) % NS)
			{
				Canvas->FinishCoverage(G, K);
			}
		}
	}
	const float L = Layout.LaneLength(G);
	const float SecLen = L / Layout.NumSections(G);
	const float Frac = (Layout.LaneArc(G, A) - Sec * SecLen) / SecLen;
	Canvas->SetTrailCoverage(G, Sec, Frac + 0.08f);
	LastSection = Sec;
	LastPaintLane = G;
	LastPaintA = A;
	ReportPainted(N);
}

void ATrackDirector::ReportPainted(int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	if (Game)
	{
		Game->OnInkPainted(Count * PointsPerSection);
	}
	if (Canvas->IsComplete() && bRunning)
	{
		bRunning = false;
		if (Game)
		{
			Game->OnLevelComplete();
		}
	}
}

void ATrackDirector::CheckCircuitChange()
{
	const int32 C = Cat->GetLanes()->GetTargetCircuit();
	if (C == PlanCircuit)
	{
		return;
	}
	// Neuer Kurs: bisherige Gefahren weg, Planung beginnt mit fairem Vorlauf neu
	PlanCircuit = C;
	RetireAllHazards();
	const int32 Local = Layout.LaneOf(Cat->GetLanes()->GetTargetLane());
	Planner.Reset(Layout.LanesPerCircuit, Cat->GetLanes()->SwitchDuration, Cat->GetTravel() + FMath::Max(Speed, 600.f) * 2.2f, Local, Rng.RandHelper(1 << 30));
	UE_LOG(LogShadowCat, Log, TEXT("Kurswechsel auf Kurs %d"), C + 1);
	if (Game)
	{
		Game->OnCircuitChanged(C);
	}
}

void ATrackDirector::PlanAhead()
{
	if (!bHazards)
	{
		return;
	}
	const float Travel = Cat->GetTravel();
	int32 Guard = 0;
	while (Planner.GetFrontierX() < Travel + ViewAhead && Guard++ < 32)
	{
		const float Frontier = Planner.GetFrontierX();
		const float S0 = FMath::Max(Speed, Difficulty.StartSpeed);
		const float TimeTo = FMath::Max(0.f, Frontier - Travel) / S0;
		// Innenbahnen in Kurven sind kuerzer -> mit Zuschlag rechnen (laengere Abstaende = sicherer)
		const float SpeedAt = FMath::Min(Difficulty.MaxSpeed, S0 + Difficulty.SpeedGainPerSecond * TimeTo) * 1.12f;
		SpawnRow(Planner.Next(Difficulty, SpeedAt, Frontier / 100.f, false));
	}
}

void ATrackDirector::SpawnRow(const FPlannedRow& Row)
{
	const int32 Base = PlanCircuit * Layout.LanesPerCircuit;
	if (Row.Hazards.Num() == Layout.LanesPerCircuit && Row.Hazards[0].Kind == EHazardKind::Obstacle)
	{
		static const TCHAR* Names[] = { TEXT("Zaun"), TEXT("Laterne"), TEXT("Abgrund"), TEXT("Balken"), TEXT("Kiste") };
		UE_LOG(LogShadowCat, Log, TEXT("Parkour-Wand: %s bei %.0f m"), Names[int32(Row.Hazards[0].Type)], Row.StartX / 100.f);
	}
	for (const FPlannedHazard& H : Row.Hazards)
	{
		const int32 G = Base + H.Lane;
		// auf Gelaende-Fahrbahnen (Rampe/Plateau/Erhoehung) keine Hindernisse
		const float RowA = TravelToA(Row.StartX);
		if (TerrainBusy(G, RowA - 300.f, RowA + H.Length + 300.f) || ChasmBusy(RowA - 300.f, RowA + H.Length + 300.f))
		{
			continue;
		}
		if (H.Kind == EHazardKind::Enemy)
		{
			AEnemyCube* E = AcquireEnemy();
			const float A = TravelToA(Row.StartX + E->HalfLength);
			FVector Pos, Fwd;
			Layout.Sample(A, Layout.LaneLat(G), Pos, Fwd);
			const bool bFromLeft = H.Lane < 1 ? true : (H.Lane > 1 ? false : Rng.FRand() < 0.5f);
			E->Place(Pos, Fwd, G, A, bFromLeft);
		}
		else
		{
			AObstacle* O = AcquireObstacle(H.Type);
			float A = 0.f;
			if (H.Type == EObstacleType::Pit && Layout.bStraight)
			{
				// echtes Loch: genau auf ganze Abschnitte ausgerichtet, deren Kacheln verschwinden
				const float SL = Layout.SectionLength;
				const int32 NumSec = FMath::Max(1, FMath::RoundToInt(H.Length / SL));
				const int32 First = FMath::CeilToInt(TravelToA(Row.StartX) / SL);
				O->SetPitLength(NumSec * SL);
				A = (First + NumSec * 0.5f) * SL;
				for (int32 K = 0; K < NumSec; ++K)
				{
					Canvas->SetHole(G, First + K);
				}
			}
			else
			{
				A = TravelToA(Row.StartX + O->HalfLength);
			}
			FVector Pos, Fwd;
			Layout.Sample(A, Layout.LaneLat(G), Pos, Fwd);
			O->Place(Pos, Fwd.Rotation(), G, A);
		}
	}
	UpdatePitWalls();
	// Box steht in einer jetzt gesperrten Fahrbahn? -> in eine sichere Fahrbahn dieser Reihe verschieben
	const float RowA = TravelToA(Row.StartX);
	for (AItemBox* B : Boxes)
	{
		if (Layout.CircuitOf(B->Lane) != PlanCircuit || FMath::Abs(Layout.DeltaA(RowA, B->A)) > 400.f)
		{
			continue;
		}
		if (Row.BlockedMask & (1 << Layout.LaneOf(B->Lane)))
		{
			for (int32 L = 0; L < Layout.LanesPerCircuit; ++L)
			{
				if (Row.SafeMask & (1 << L))
				{
					const int32 G = Base + L;
					FVector Pos, Fwd;
					Layout.Sample(B->A, Layout.LaneLat(G), Pos, Fwd);
					const bool bWasAvailable = B->IsAvailable();
					B->Place(Pos, Fwd, G, B->A);
					if (!bWasAvailable)
					{
						B->Collect(BoxRespawnTime * 0.5f);
					}
					break;
				}
			}
		}
	}
}

void ATrackDirector::CheckCollisions()
{
	const float CatLat = Cat->GetLanes()->GetLateralOffset();
	const int32 Circuit = GetCatCircuit();
	const float HL = Cat->CollisionHalfLength;
	const float HW = Cat->CollisionHalfWidth;
	const float FeetZ = Cat->GetFeetZ();
	auto Overlaps = [&](const AHazardBase* H)
	{
		return Layout.CircuitOf(H->Lane) == Circuit
			&& FMath::Abs(WorldAhead(H->A)) < H->HalfLength + HL
			&& FMath::Abs(Layout.LaneLat(H->Lane) - CatLat) < H->HalfWidth + HW;
	};
	// Gelaende: die Katze laeuft auf der Oberflaeche ihrer Fahrbahn (Rampe, Plateau, Erhoehung)
	const int32 CurLane = Cat->GetLanes()->GetCurrentLane();
	const float Ground = Layout.bStraight ? GroundAt(CurLane, Cat->GetA()) : 0.f;
	Cat->SetSupportZ(Ground);
	if (Cat->IsInvulnerable())
	{
		return;
	}
	if (Ground > FeetZ + 35.f && !Cat->IsFalling())
	{
		// gegen die Wand einer Erhoehung gelaufen (vorn ohne Rampe oder seitlich hineingewechselt)
		LastHitReason = FString::Printf(TEXT("Erhoehung auf Fahrbahn %d bei %.0f m (Tempo %.0f)"), CurLane, GetMeters(), Speed);
		const bool bDead = Game ? Game->OnCatHit(LastHitReason) : true;
		if (bDead)
		{
			bRunning = false;
		}
		else
		{
			Speed *= 0.55f;
		}
		return;
	}
	// Abgruende bleiben liegen (werden nicht "zerbrochen")
	auto IsPit = [](const AHazardBase* H)
	{
		const AObstacle* O = Cast<AObstacle>(H);
		return O && O->GetType() == EObstacleType::Pit;
	};
	auto Hit = [&](AHazardBase* H) -> bool
	{
		if (Cat->GetBuffs()->TryAbsorb(H->GetKind()))
		{
			if (AEnemyCube* E = Cast<AEnemyCube>(H))
			{
				DefeatEnemy(E);
			}
			else if (!IsPit(H))
			{
				Burst(H->GetActorLocation() + FVector(0.f, 0.f, 60.f), 0.7f, false, 1.4f);
				H->Retire();
			}
			Cat->OnHitAbsorbed();
			return false;
		}
		LastHitReason = FString::Printf(TEXT("%s auf Fahrbahn %d bei %.0f m (Tempo %.0f)"),
			H->GetKind() == EHazardKind::Enemy ? TEXT("Wuerfel") : *H->GetClass()->GetName(), H->Lane, GetMeters(), Speed);
		const bool bDead = Game ? Game->OnCatHit(LastHitReason) : true;
		if (bDead)
		{
			bRunning = false;
			return true;
		}
		// Leben verloren, weiterlaufen: Hindernis zerbricht, Katze stolpert (kurz langsamer)
		if (IsPit(H))
		{
			Burst(Cat->GetActorLocation() + FVector(0.f, 0.f, 20.f), 0.05f, false, 1.2f);
		}
		else
		{
			Burst(H->GetActorLocation() + FVector(0.f, 0.f, 60.f), 0.7f, false, 1.3f);
			H->Retire();
		}
		Speed *= 0.55f;
		return false;
	};

	for (AObstacle* O : Obstacles)
	{
		if (!O || !O->IsDangerous() || !Overlaps(O))
		{
			continue;
		}
		bool bHit = false;
		switch (O->GetType())
		{
		case EObstacleType::Pit:
			// nur wer am Boden in den Abgrund laeuft/landet (Kanten mit etwas Spielraum)
			bHit = !Cat->IsJumping() && FeetZ < 5.f && FMath::Abs(WorldAhead(O->A)) < O->HalfLength - 20.f;
			break;
		case EObstacleType::Beam:
			// unten durch (kriechen) oder hoch drueber (Tintenwolke, Oberkante 181 cm)
			bHit = !Cat->IsCrawling() && FeetZ < 181.f;
			break;
		case EObstacleType::Crate:
		case EObstacleType::Fence:
			// Zaunspitzen verzeihen etwas; wer beim Kontakt schon abgesprungen ist, kommt drueber
			bHit = FeetZ < O->Height * 0.6f && !(Cat->IsRising() && FeetZ > 10.f);
			break;
		default:
			bHit = FeetZ < O->Height;
			break;
		}
		if (bHit && O->GetType() == EObstacleType::Pit)
		{
			if (Cat->GetBuffs()->TryAbsorb(EHazardKind::Obstacle))
			{
				Cat->OnHitAbsorbed(); // Schutz-Item: schwebt drueber
				continue;
			}
			// ins Loch gefallen: Katze stuerzt hinein, kommt (mit Leben) hinter dem Loch wieder hoch
			Cat->StartFall();
			FallRespawnA = O->A + O->HalfLength + 80.f;
			LastHitReason = FString::Printf(TEXT("Abgrund auf Fahrbahn %d bei %.0f m (Tempo %.0f)"), O->Lane, GetMeters(), Speed);
			const bool bDead = Game ? Game->OnCatHit(LastHitReason) : true;
			if (bDead)
			{
				bRunning = false;
			}
			else
			{
				Speed *= 0.55f;
			}
			return;
		}
		if (bHit && Hit(O))
		{
			return;
		}
	}
	// gleitende Deko: trifft ueber ihre aktuelle seitliche Lage (zu hoch zum Springen)
	for (ASlidingProp* P : Sliders)
	{
		// niedrige (Kiste): drueberspringen moeglich
		if (P && P->IsLive() && FMath::Abs(WorldAhead(P->A)) < P->HalfLength + HL && FMath::Abs(P->GetLat() - CatLat) < P->HalfWidth + HW
			&& (P->Height > 500.f || Cat->GetFeetZ() < P->Height * 0.6f) && Hit(P))
		{
			return;
		}
	}
	for (AEnemyCube* E : Enemies)
	{
		const float EZ = E ? E->GetActorLocation().Z : 0.f;
		if (E && E->IsDangerous() && Overlaps(E) && FeetZ < EZ + 35.f && FeetZ + Cat->CollisionHeight > EZ - 35.f && Hit(E))
		{
			return;
		}
	}
}

void ATrackDirector::CheckBoxes(float DeltaTime)
{
	UItemSlotComponent* Slot = Cat->GetSlot();
	// Items werden gestapelt: Boxen sind immer einsammelbar
	const bool bFull = false;
	SlotFullMsgCooldown -= DeltaTime;
	const float CatLat = Cat->GetLanes()->GetLateralOffset();
	for (AItemBox* B : Boxes)
	{
		B->StepBox(DeltaTime, bFull);
		if (!bRunning || !B->IsAvailable() || !Slot)
		{
			continue;
		}
		if (FMath::Abs(WorldAhead(B->A)) < 80.f && FMath::Abs(Layout.LaneLat(B->Lane) - CatLat) < 80.f && Cat->GetFeetZ() < 170.f)
		{
			if (Slot->TryCollect())
			{
				Burst(B->GetActorLocation() + FVector(0.f, 0.f, 95.f), 1.f, true, 0.9f);
				B->Collect(BoxRespawnTime);
				if (Game)
				{
					Game->OnBoxCollected();
				}
			}
			else if (SlotFullMsgCooldown <= 0.f)
			{
				SlotFullMsgCooldown = 1.5f;
				if (Game)
				{
					Game->OnSlotFull();
				}
			}
		}
	}
}

void ATrackDirector::RecycleBehind()
{
	for (AObstacle* O : Obstacles)
	{
		// erst wenn das hintere Ende (lange Abgruende!) hinter der Katze liegt
		if (O && O->IsLive() && WorldAhead(O->A) + O->HalfLength < -700.f)
		{
			O->Retire();
		}
	}
	for (AEnemyCube* E : Enemies)
	{
		if (E && E->IsLive() && WorldAhead(E->A) < -700.f)
		{
			E->Retire();
		}
	}
}

TArray<AEnemyCube*> ATrackDirector::GetLiveEnemies() const
{
	TArray<AEnemyCube*> Out;
	for (AEnemyCube* E : Enemies)
	{
		if (E && E->IsLive())
		{
			Out.Add(E);
		}
	}
	return Out;
}

void ATrackDirector::DefeatEnemy(AEnemyCube* Enemy)
{
	if (!Enemy || !Enemy->IsLive())
	{
		return;
	}
	Burst(Enemy->GetActorLocation(), 1.f, true, 1.2f);
	Enemy->Retire();
	if (Game)
	{
		Game->OnEnemyDefeated(EnemyBonus);
	}
}

int32 ATrackDirector::DefeatEnemiesNear(const FVector& Center, float Radius)
{
	int32 N = 0;
	for (AEnemyCube* E : Enemies)
	{
		if (E && E->IsLive() && FVector::DistSquared(E->GetActorLocation(), Center) < Radius * Radius)
		{
			DefeatEnemy(E);
			++N;
		}
	}
	return N;
}

void ATrackDirector::DebugPlaceBox(float AheadCm, int32 Lane)
{
	if (Boxes.Num() == 0)
	{
		return;
	}
	AItemBox* B = Boxes[0];
	const float A = Layout.AFromLaneArc(Lane, Layout.LaneArc(Lane, Cat->GetA()) + AheadCm);
	FVector Pos, Fwd;
	Layout.Sample(A, Layout.LaneLat(Lane), Pos, Fwd);
	B->Place(Pos, Fwd, Lane, A);
}

void ATrackDirector::RunAutopilot()
{
	// Nur fuer Tests: weicht ~1 s voraus aus und faerbt systematisch ein, wechselt selbststaendig den Kurs
	ULaneMovementComponent* Lanes = Cat->GetLanes();
	const int32 N = Layout.LanesPerCircuit;
	const int32 Circuit = Layout.CircuitOf(Lanes->GetTargetLane());
	const int32 Base = Circuit * N;
	const int32 Cur = Lanes->GetTargetLane() - Base;
	const float Look = FMath::Max(Speed, 600.f) * 1.0f + 250.f;

	TArray<float> Clear;
	Clear.Init(1e9f, N);
	auto Consider = [&](const AHazardBase* H)
	{
		if (Layout.CircuitOf(H->Lane) != Circuit)
		{
			return;
		}
		const float Ahead = WorldAhead(H->A);
		if (Ahead + H->HalfLength > -Cat->CollisionHalfLength && Ahead - H->HalfLength < Look)
		{
			const int32 L = H->Lane - Base;
			Clear[L] = FMath::Min(Clear[L], FMath::Max(Ahead - H->HalfLength, 0.f));
		}
	};
	// Parkour-Hindernisse sperren nicht: sie werden uebersprungen bzw. unterkrochen (siehe unten)
	// Schlucht: nur die mittlere Fahrbahn hat eine Insel -> alle anderen sperren
	for (const FVector2D& Z : ChasmZones)
	{
		const float Front = Z.X + 400.f - Cat->GetA();
		if (Z.Y > Cat->GetA() && Front < Look * 1.8f)
		{
			for (int32 L = 0; L < N; ++L)
			{
				if (L != N / 2)
				{
					Clear[L] = FMath::Min(Clear[L], FMath::Max(Front, 0.f));
				}
			}
		}
	}
	// lange Abgruende (Schlucht) sind nicht zu ueberspringen -> wie eine Sperre behandeln
	for (const AObstacle* O : Obstacles)
	{
		if (O && O->IsLive() && (!IsActionObstacle(O->GetType()) || (O->GetType() == EObstacleType::Pit && O->HalfLength > 400.f)))
		{
			Consider(O);
		}
	}
	for (const AObstacle* O : Obstacles)
	{
		if (!O || !O->IsLive() || (O->Lane != Lanes->GetCurrentLane() && O->Lane != Lanes->GetTargetLane()))
		{
			continue;
		}
		const float Front = WorldAhead(O->A) - O->HalfLength;
		if (O->GetType() == EObstacleType::Beam)
		{
			if (Front < FMath::Max(Speed, 600.f) * 0.22f && Front > -O->HalfLength * 2.f && !Cat->IsCrawling())
			{
				UE_LOG(LogShadowCat, Log, TEXT("AUTOPILOT kriecht unter Balken"));
				Cat->RequestCrawl();
			}
		}
		else if (IsActionObstacle(O->GetType()) && !Cat->IsJumping() && Front < 120.f + Speed * 0.03f && Front > 20.f)
		{
			UE_LOG(LogShadowCat, Log, TEXT("AUTOPILOT springt ueber %s"), O->GetType() == EObstacleType::Pit ? TEXT("Abgrund") : TEXT("Zaun"));
			Cat->RequestJump();
		}
	}
	for (const AEnemyCube* E : Enemies) { if (E && E->IsLive() && !E->IsDoomed()) Consider(E); }
	auto ConsiderZone = [&](float A0, float A1, int32 Mask)
	{
		const float Front = A0 - Cat->GetA();
		const float Back = A1 - Cat->GetA();
		if (Back > -Cat->CollisionHalfLength && Front < Look)
		{
			for (int32 L = 0; L < N; ++L)
			{
				if (Mask & (1 << (Base + L)))
				{
					Clear[L] = FMath::Min(Clear[L], FMath::Max(Front, 0.f));
				}
			}
		}
	};
	for (const FPuddle& Pd : Puddles) { ConsiderZone(Pd.A0, Pd.A1, Pd.Mask); }
	// Gelaende: nicht seitlich gegen ein Plateau wechseln; Erhoehung ohne Rampe voraus: eigene Fahrbahn -> Sprung
	for (const ATrackPlatform* P : Platforms)
	{
		if (!P || !P->IsLive())
		{
			continue;
		}
		const int32 L = P->Lane - Base;
		const float CatA = Cat->GetA();
		if (L < 0 || L >= N || P->A1 < CatA - Cat->CollisionHalfLength)
		{
			continue;
		}
		const float Front = P->A0 - CatA;
		const bool bMine = P->Lane == Lanes->GetTargetLane();
		if (!bMine)
		{
			if (Front <= FMath::Max(Speed, 600.f) * 0.4f)
			{
				// Gelaende direkt voraus oder neben der Katze: nur wechseln, wenn sie schon hoch genug ist
				if (Cat->GetFeetZ() < P->Height - 30.f)
				{
					Clear[L] = 0.f;
				}
			}
			else if (!P->HasRamp())
			{
				// nicht kurz vor einer Erhoehung in deren Fahrbahn wechseln (Sprung waere zu knapp)
				Clear[L] = FMath::Min(Clear[L], FMath::Max(0.f, Front - 300.f));
			}
		}
		else if (!P->HasRamp() && Front > Cat->CollisionHalfLength)
		{
			if (!Cat->IsJumping() && Front < 130.f + Speed * 0.03f && Front > 30.f)
			{
				UE_LOG(LogShadowCat, Log, TEXT("AUTOPILOT springt auf Erhoehung"));
				Cat->RequestJump();
			}
		}
	}
	// gleitende Deko: Fahrbahnen sperren, auf denen sie beim Eintreffen der Katze voraussichtlich ist
	for (const ASlidingProp* P : Sliders)
	{
		if (!P || !P->IsLive())
		{
			continue;
		}
		const float Ahead = WorldAhead(P->A);
		if (Ahead + P->HalfLength < -Cat->CollisionHalfLength || Ahead > Look * 1.6f)
		{
			continue;
		}
		const float Sec = FMath::Max(0.f, Ahead) / FMath::Max(Speed, 600.f);
		for (float Dt : { -0.3f, -0.15f, 0.f, 0.12f, 0.25f })
		{
			const float PL = P->PredictLat(Sec, FMath::Max(0.f, Sec + Dt));
			for (int32 L = 0; L < N; ++L)
			{
				if (FMath::Abs(Layout.LaneLat(Base + L) - PL) < P->HalfWidth + Cat->CollisionHalfWidth + 30.f)
				{
					Clear[L] = FMath::Min(Clear[L], FMath::Max(Ahead - P->HalfLength, 0.f));
				}
			}
		}
	}
	for (const FIncoming& In : Incoming) { if (In.Mask) ConsiderZone(In.A - PuddleLength * 0.5f, In.A + PuddleLength * 0.5f, In.Mask); }
	// Salven-Reihe ueber alle Fahrbahnen: drueberspringen
	auto JumpZone = [&](float ZoneA0, int32 Mask)
	{
		const int32 All = (1 << N) - 1;
		const float Front = ZoneA0 - Cat->GetA();
		if ((Mask & All) == All && Front > 30.f && Front < 120.f + Speed * 0.03f && !Cat->IsJumping())
		{
			UE_LOG(LogShadowCat, Log, TEXT("AUTOPILOT springt ueber weisse Tinte"));
			Cat->RequestJump();
		}
	};
	for (const FPuddle& Pd : Puddles) { JumpZone(Pd.A0, Pd.Mask); }
	for (const FIncoming& In : Incoming) { if (In.Mask) JumpZone(In.A - PuddleLength * 0.5f, In.Mask); }
	const float SideNeed = Cat->CollisionHalfLength + FMath::Max(Speed, 600.f) * Lanes->SwitchDuration;

	// 1) Ausweichen
	if (Clear[Cur] < Look)
	{
		int32 Best = Cur;
		for (int32 L = 0; L < N; ++L)
		{
			if (Clear[L] > Clear[Best] + 1.f || (FMath::IsNearlyEqual(Clear[L], Clear[Best], 1.f) && FMath::Abs(L - Cur) < FMath::Abs(Best - Cur)))
			{
				Best = L;
			}
		}
		const int32 Step = Best > Cur ? 1 : -1;
		if (Best != Cur && Clear[Cur + Step] > SideNeed)
		{
			Lanes->RequestShift(Step, false);
		}
		return;
	}

	// 2) Items sofort einsetzen
	if (UItemSlotComponent* Slot = Cat->GetSlot())
	{
		if (Slot->HasItem())
		{
			Slot->UseAny();
		}
	}

	// 3) Einfaerben: Fahrbahn mit den meisten weissen Abschnitten voraus; fertiger Kurs -> zum naechsten unfertigen
	// Abstand (in Abschnitten) zur naechsten weissen Stelle einer Fahrbahn, 999 = keine in Sicht
	auto NextMissing = [&](int32 G)
	{
		const int32 NS = Layout.NumSections(G);
		const int32 S = Layout.SectionAt(G, Cat->GetA());
		for (int32 K = 1; K <= 40; ++K)
		{
			if (!Canvas->IsPainted(G, (S + K) % NS))
			{
				return K;
			}
		}
		return 999;
	};
	auto CircuitMissing = [&](int32 C)
	{
		int32 M = 0;
		for (int32 L = 0; L < N; ++L)
		{
			M += Canvas->CountSections(C * N + L) - Canvas->CountPainted(C * N + L);
		}
		return M;
	};
	int32 Want = Cur;
	if (CircuitMissing(Circuit) == 0)
	{
		int32 TargetC = INDEX_NONE;
		for (int32 D = 1; D < Layout.NumCircuits && TargetC == INDEX_NONE; ++D)
		{
			const int32 Cands[2] = { Circuit + D, Circuit - D };
			for (int32 C : Cands)
			{
				if (C >= 0 && C < Layout.NumCircuits && CircuitMissing(C) > 0)
				{
					TargetC = C;
					break;
				}
			}
		}
		if (TargetC != INDEX_NONE)
		{
			const int32 Dir = TargetC > Circuit ? 1 : -1;
			Want = Dir > 0 ? N - 1 : 0;
			if (Cur == Want && CanChangeCircuit() && !Lanes->IsSwitching())
			{
				Lanes->RequestShift(Dir, true);
				return;
			}
		}
	}
	else
	{
		// weisse Stelle voraus: dorthin (einzelne Luecken werden gezielt geschlossen)
		int32 BestK = NextMissing(Base + Cur);
		for (int32 L = 0; L < N; ++L)
		{
			const int32 K = NextMissing(Base + L);
			if (K + 2 < BestK)
			{
				BestK = K;
				Want = L;
			}
		}
	}
	// Endlos: Muenzen voraus einsammeln
	if (Layout.bStraight)
	{
		const int32 CoinLane = Coins->CoinLaneAhead(Cat->GetA(), 1800.f);
		if (CoinLane != INDEX_NONE)
		{
			Want = CoinLane - Base;
		}
	}
	if (Want != Cur)
	{
		const int32 Step = Want > Cur ? 1 : -1;
		if (Clear[Cur + Step] > Look)
		{
			Lanes->RequestShift(Step, false);
		}
	}
}

void ATrackDirector::StepEndlessBoxes()
{
	const float CatA = Cat->GetA();
	for (AItemBox* B : Boxes)
	{
		if (B->IsAvailable() && B->A < CatA - 600.f)
		{
			B->Collect(1.0e9f);
		}
	}
	if (CatA + 13000.f < NextBoxA)
	{
		return;
	}
	AItemBox* Free = nullptr;
	for (AItemBox* B : Boxes)
	{
		if (!B->IsAvailable())
		{
			Free = B;
			break;
		}
	}
	if (!Free)
	{
		return;
	}
	// Fahrbahn ohne Hindernis in der Naehe waehlen
	const int32 N = Layout.LanesPerCircuit;
	int32 Lane = Rng.RandHelper(N);
	for (int32 Try = 0; Try < N; ++Try)
	{
		const int32 G = (Lane + Try) % N;
		bool bBusy = false;
		for (const AObstacle* O : Obstacles) { bBusy |= O && O->IsLive() && O->Lane == G && FMath::Abs(O->A - NextBoxA) < 500.f; }
		for (const AEnemyCube* E : Enemies) { bBusy |= E && E->IsLive() && E->Lane == G && FMath::Abs(E->A - NextBoxA) < 500.f; }
		if (!bBusy)
		{
			Lane = G;
			break;
		}
	}
	FVector Pos, Fwd;
	Layout.Sample(NextBoxA, Layout.LaneLat(Lane), Pos, Fwd);
	Free->Place(Pos, Fwd, Lane, NextBoxA);
	NextBoxA += Rng.FRandRange(EndlessBoxSpacing.X, EndlessBoxSpacing.Y);
}

bool ATrackDirector::LaneBusy(int32 Lane, float A0, float A1) const
{
	for (const AObstacle* O : Obstacles)
	{
		if (O && O->IsLive() && O->Lane == Lane && O->A + O->HalfLength > A0 && O->A - O->HalfLength < A1)
		{
			return true;
		}
	}
	for (const AEnemyCube* E : Enemies)
	{
		if (E && E->IsLive() && E->Lane == Lane && E->A + E->HalfLength > A0 && E->A - E->HalfLength < A1)
		{
			return true;
		}
	}
	if (TerrainBusy(Lane, A0, A1))
	{
		return true;
	}
	// gleitende Deko quert alle Fahrbahnen
	for (const ASlidingProp* S : Sliders)
	{
		if (S && S->IsLive() && S->A + 300.f > A0 && S->A - 300.f < A1)
		{
			return true;
		}
	}
	return false;
}

void ATrackDirector::SpawnPuddle(const FIncoming& In)
{
	// Aufschlag: Pfuetze bleibt als Gefahr liegen
	FPuddle Pd;
	Pd.A0 = In.A - PuddleLength * 0.5f;
	Pd.A1 = In.A + PuddleLength * 0.5f;
	Pd.Mask = In.Mask;
	Pd.TimeLeft = In.Duration;
	int32 First = 99, Last = -1;
	for (int32 L = 0; L < Layout.LanesPerCircuit; ++L)
	{
		if (In.Mask & (1 << L)) { First = FMath::Min(First, L); Last = FMath::Max(Last, L); }
	}
	const float Lat = (Layout.LaneLat(First) + Layout.LaneLat(Last)) * 0.5f;
	const float Width = (Last - First + 1) * Layout.LaneWidth;
	const FVector Center = Layout.Position(In.A, Lat);
	Pd.Mat = RunAssets::NewMID(TEXT("M_Disc"), this);
	if (Pd.Mat)
	{
		Pd.Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 1.f, 1.f));
		Pd.Mat->SetScalarParameterValue(TEXT("Ring"), 0.f);
		Pd.Mat->SetScalarParameterValue(TEXT("Intensity"), 1.6f);
		Pd.Mat->SetScalarParameterValue(TEXT("Opacity"), 1.f);
	}
	Pd.Visual = RunAssets::AddShape(this, RootComponent, TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, Pd.Mat);
	Pd.Visual->SetUsingAbsoluteLocation(true);
	Pd.Visual->SetUsingAbsoluteScale(true);
	Pd.Visual->SetWorldLocation(Center + FVector(0.f, 0.f, 4.f));
	Pd.Visual->SetWorldScale3D(FVector(PuddleLength / 70.f, Width / 70.f, 1.f));
	const int32 NumBlobs = 3 + 2 * (Last - First + 1);
	for (int32 K = 0; K < NumBlobs; ++K)
	{
		const FVector Off(Rng.FRandRange(-0.45f, 0.45f) * PuddleLength, Rng.FRandRange(-0.4f, 0.4f) * Width, 0.f);
		const float Sc = Rng.FRandRange(0.9f, 1.8f);
		UStaticMeshComponent* B = RunAssets::AddShape(this, RootComponent, TEXT("Sphere"), FVector::ZeroVector, FVector(Sc * 1.3f, Sc, 0.28f), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), RunAssets::Mono(0.95f, 0.5f));
		B->SetUsingAbsoluteLocation(true);
		B->SetUsingAbsoluteScale(true);
		B->SetWorldLocation(Center + Off + FVector(0.f, 0.f, 4.f));
		Pd.Blobs.Add(B);
	}
	Puddles.Add(Pd);
	// Tinte spuelt Muenzen weg
	Coins->RemoveRange(Pd.A0 - 60.f, Pd.A1 + 60.f, In.Mask);
	Burst(Center + FVector(0.f, 0.f, 40.f), 1.f, true, 2.f);
	PlaySplash(Incoming.Num() > 1 ? 0.55f : 1.f);
}

void ATrackDirector::StepBoss(float DeltaTime)
{
	if (!Boss)
	{
		return;
	}
	for (const int32 Idx : Boss->ConsumeImpacts())
	{
		if (Incoming.IsValidIndex(Idx) && Incoming[Idx].Mask)
		{
			SpawnPuddle(Incoming[Idx]);
			Incoming[Idx].Mask = 0;
		}
	}
	if (!Boss->IsAttacking())
	{
		Incoming.Reset();
	}
	for (int32 I = Puddles.Num() - 1; I >= 0; --I)
	{
		FPuddle& Pd = Puddles[I];
		Pd.TimeLeft -= DeltaTime;
		if (Pd.Mat)
		{
			Pd.Mat->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Pd.TimeLeft / 0.5f, 0.f, 1.f));
		}
		const float Shrink = FMath::Clamp(Pd.TimeLeft / 0.5f, 0.f, 1.f);
		for (UStaticMeshComponent* B : Pd.Blobs)
		{
			if (B)
			{
				FVector Sc = B->GetComponentScale();
				Sc.Z = 0.28f * Shrink;
				B->SetWorldScale3D(Sc);
			}
		}
		if (Pd.TimeLeft <= 0.f || Pd.A1 < Cat->GetA() - 1500.f)
		{
			if (Pd.Visual) Pd.Visual->DestroyComponent();
			for (UStaticMeshComponent* B : Pd.Blobs) { if (B) B->DestroyComponent(); }
			Puddles.RemoveAt(I);
		}
	}

	if (Boss->IsAttacking())
	{
		return;
	}
	// etwa jede Minute eine Salve
	if (RunTime >= NextSalvo)
	{
		if (StartSalvo())
		{
			NextSalvo = RunTime + Rng.FRandRange(SalvoInterval.X, SalvoInterval.Y);
			NextBossAttack = FMath::Max(NextBossAttack, RunTime + 12.f);
		}
		else
		{
			NextSalvo = RunTime + 2.f;
		}
		return;
	}
	if (RunTime < NextBossAttack)
	{
		return;
	}
	if (NextSalvo - RunTime < 7.f)
	{
		// kurz vor einer Salve kein Einzelwurf
		NextBossAttack = NextSalvo + 8.f;
		return;
	}
	// Einzelwurf: landet so, dass die Pfuetze ~0.7 s vor der Katze liegt; Zielfahrbahnen ohne Hindernisse
	const float Lead = Boss->WindUp + Boss->FlightTime + 0.7f;
	const float AttackA = Cat->GetA() + FMath::Max(Speed, 600.f) * Lead;
	const int32 N = Layout.LanesPerCircuit;
	TArray<int32> Free;
	for (int32 L = 0; L < N; ++L)
	{
		if (!LaneBusy(L, AttackA - 800.f, AttackA + 800.f))
		{
			Free.Add(L);
		}
	}
	if (Free.Num() < 2)
	{
		NextBossAttack = RunTime + 2.f; // kein fairer Platz: etwas spaeter erneut
		return;
	}
	// halb so oft direkt auf die Fahrbahn der Katze
	const int32 CatLane = Cat->GetLanes()->GetTargetLane();
	const int32 L0 = (Free.Contains(CatLane) && Rng.FRand() < 0.5f) ? CatLane : Free[Rng.RandHelper(Free.Num())];
	int32 Mask = 1 << L0;
	const int32 L1 = L0 + (Rng.FRand() < 0.5f ? 1 : -1);
	if (Rng.FRand() < 0.4f && Free.Contains(L1) && Free.Num() >= 3)
	{
		Mask |= 1 << L1;
	}
	int32 First = 99, Last = -1;
	for (int32 L = 0; L < N; ++L)
	{
		if (Mask & (1 << L)) { First = FMath::Min(First, L); Last = FMath::Max(Last, L); }
	}
	const float Lat = (Layout.LaneLat(First) + Layout.LaneLat(Last)) * 0.5f;
	FIncoming In;
	In.A = AttackA;
	In.Mask = Mask;
	In.Duration = PuddleDuration;
	Incoming.Reset();
	Incoming.Add(In);
	Boss->StartAttack(Layout.Position(AttackA, Lat), FVector2D(PuddleLength * 0.5f, (Last - First + 1) * Layout.LaneWidth * 0.5f));
	const float Ramp = FMath::Clamp(RunTime / 180.f, 0.f, 1.f);
	NextBossAttack = RunTime + FMath::Lerp(BossInterval.Y, BossInterval.X, Ramp) * Rng.FRandRange(0.85f, 1.15f);
	UE_LOG(LogShadowCat, Log, TEXT("Riese wirft auf Fahrbahnen %d-%d bei %.0f m"), First + 1, Last + 1, AttackA / 100.f);
}

bool ATrackDirector::StartSalvo()
{
	// Salve: SalvoRows Reihen hintereinander, jede sperrt alle Fahrbahnen bis auf zwei benachbarte. Der freie Weg
	// wandert pro Reihe hoechstens eine Fahrbahn weiter -> Zickzack-Ausweichen, aber immer machbar.
	const int32 N = Layout.LanesPerCircuit;
	if (N < 3 || SalvoRows < 1)
	{
		return false;
	}
	// jede weitere Salve ist schwerer: mehr Reihen, dichter, schneller geworfen, oefter nur eine freie Fahrbahn
	const int32 Level = SalvoCount++;
	const int32 Rows = FMath::Min(SalvoRows + 2 * Level, 18);
	const float RowGap = FMath::Max(950.f, SalvoRowGap - 70.f * Level);
	const float ThrowGap = FMath::Max(0.14f, SalvoThrowGap - 0.04f * Level);
	const float SingleFree = FMath::Min(0.4f + 0.15f * Level, 0.85f);
	// manche Reihen sperren ALLE Fahrbahnen -> drueberspringen (nie zwei hintereinander, nie die erste);
	// danach mehr Abstand, damit man nach der Landung wieder reagieren kann
	const float PAll = FMath::Min(0.25f + 0.1f * Level, 0.55f);
	const float S = FMath::Max(Speed, 600.f);
	const float Half = PuddleLength * 0.5f;
	const float A0 = Cat->GetA() + S * (Boss->WindUp + SalvoPrepareTime + Boss->FlightTime + 1.2f);
	TArray<float> RowA;
	TArray<bool> RowAll;
	for (int32 R = 0; R < Rows; ++R)
	{
		const bool bAll = R > 0 && !RowAll.Last() && Rng.FRand() < PAll;
		RowA.Add(R == 0 ? A0 : RowA.Last() + RowGap + (RowAll.Last() ? S * 0.6f : 0.f));
		RowAll.Add(bAll);
	}
	const float AEnd = RowA.Last() + Half;
	// Bereich freiraeumen (Hindernisse, Gegner, Muenzen); die Hindernis-Planung geht hinter der Salve weiter
	const float Clear0 = A0 - Half - 1200.f;
	for (AObstacle* O : Obstacles)
	{
		if (O && O->IsLive() && O->A > Clear0 && O->A < AEnd + 400.f)
		{
			if (O->GetType() == EObstacleType::Pit)
			{
				// Loch wieder schliessen
				const float SL = Layout.SectionLength;
				for (int32 K = FMath::RoundToInt((O->A - O->HalfLength) / SL); K < FMath::RoundToInt((O->A + O->HalfLength) / SL); ++K)
				{
					Canvas->ClearHole(O->Lane, K);
				}
			}
			O->Retire();
		}
	}
	for (AEnemyCube* E : Enemies)
	{
		if (E && E->IsLive() && E->A > Clear0 && E->A < AEnd + 400.f) E->Retire();
	}
	for (ASlidingProp* Sl : Sliders)
	{
		if (Sl && Sl->IsLive() && Sl->A > Clear0 && Sl->A < AEnd + 400.f) Sl->Retire();
	}
	NextSlideA = FMath::Max(NextSlideA, AEnd + 1500.f);
	for (ATrackPlatform* Pl : Platforms)
	{
		if (Pl && Pl->IsLive() && Pl->A1 > Clear0 && Pl->A0 < AEnd + 400.f) Pl->Retire();
	}
	NextTerrainA = FMath::Max(NextTerrainA, AEnd + 1500.f);
	Coins->RemoveRange(Clear0, AEnd + 400.f);
	NextCoinA = FMath::Max(NextCoinA, AEnd + 600.f);
	if (NextBoxA > Clear0 && NextBoxA < AEnd + 600.f)
	{
		NextBoxA = AEnd + 600.f;
	}

	TArray<FGiantShot> Shots;
	Incoming.Reset();
	int32 Path = Cat->GetLanes()->GetTargetLane();
	FString Pattern;
	for (int32 R = 0; R < Rows; ++R)
	{
		if (R > 0 && Rng.FRand() < 0.8f)
		{
			// am Rand immer nach innen, sonst zufaellig: kein Haengenbleiben am Rand
			const int32 Dir = Path == 0 ? 1 : (Path == N - 1 ? -1 : (Rng.FRand() < 0.5f ? -1 : 1));
			Path += Dir;
			// fordernder: manchmal springt der freie Weg zwei Fahrbahnen weit (zwei schnelle Wechsel), nie nach einer Sprung-Reihe
			if (Rng.FRand() < 0.3f && !RowAll[R - 1] && Path + Dir >= 0 && Path + Dir < N)
			{
				Path += Dir;
			}
		}
		int32 Buddy = Path + (Rng.FRand() < 0.5f ? -1 : 1);
		if (Buddy < 0 || Buddy >= N)
		{
			Buddy = Path == 0 ? 1 : Path - 1;
		}
		// schwerer mit jeder Salve: oefter bleibt nur genau eine Fahrbahn frei (nie in der ersten Reihe)
		if (R > 0 && Rng.FRand() < SingleFree)
		{
			Buddy = Path;
		}
		const int32 Mask = RowAll[R] ? (1 << N) - 1 : ((1 << N) - 1) & ~(1 << Path) & ~(1 << Buddy);
		const float A = RowA[R];
		int32 Seg = 0;
		for (int32 L = 0; L < N;)
		{
			if (!(Mask & (1 << L)))
			{
				++L;
				continue;
			}
			int32 E = L;
			while (E + 1 < N && (Mask & (1 << (E + 1))))
			{
				++E;
			}
			FIncoming In;
			In.A = A;
			for (int32 K = L; K <= E; ++K)
			{
				In.Mask |= 1 << K;
			}
			FGiantShot Shot;
			Shot.Target = Layout.Position(A, (Layout.LaneLat(L) + Layout.LaneLat(E)) * 0.5f);
			Shot.Extent = FVector2D(Half, (E - L + 1) * Layout.LaneWidth * 0.5f);
			Shot.Delay = SalvoPrepareTime + R * ThrowGap + Seg * 0.08f;
			// Pfuetze liegt, bis die Katze vorbei ist
			const float Land = Boss->WindUp + Shot.Delay + Boss->FlightTime;
			const float Pass = (A + Half + 250.f - Cat->GetA()) / S;
			In.Duration = FMath::Max(PuddleDuration, Pass - Land + 0.8f);
			Shots.Add(Shot);
			Incoming.Add(In);
			++Seg;
			L = E + 1;
		}
		for (int32 L = 0; L < N; ++L)
		{
			Pattern += (Mask & (1 << L)) ? TEXT("X") : TEXT("_");
		}
		Pattern += RowAll[R] ? TEXT("(Sprung) ") : TEXT(" ");
	}
	Planner.Reset(N, Cat->GetLanes()->SwitchDuration, Cat->GetTravel() + (AEnd + 900.f - Cat->GetA()), Path, Rng.RandHelper(1 << 30));
	Boss->StartAttack(Shots);
	UE_LOG(LogShadowCat, Log, TEXT("Riese: SALVE %d Wuerfe ab %.0f m bis %.0f m: %s"), Shots.Num(), A0 / 100.f, AEnd / 100.f, *Pattern);
	if (Game)
	{
		Game->OnSalvo();
	}
	return true;
}

void ATrackDirector::StepEndlessCoins(float DeltaTime)
{
	const int32 Got = Coins->StepCoins(DeltaTime, Cat->GetA(), Cat->GetLanes()->GetLateralOffset(), Cat->GetFeetZ(), true);
	if (Got > 0 && Game)
	{
		Game->OnCoinsCollected(Got);
	}
	// nur dort legen, wo die Hindernisse schon geplant sind (sonst koennte spaeter eines auf die Reihe fallen)
	const float Known = bHazards ? Cat->GetA() + (Planner.GetFrontierX() - Cat->GetTravel()) : Cat->GetA() + ViewAhead;
	const int32 N = Layout.LanesPerCircuit;
	for (int32 Guard = 0; Guard < 4; ++Guard)
	{
		const bool bRow = Rng.FRand() < CoinRowChance;
		const int32 Count = bRow ? 10 : 1;
		const float Len = (Count - 1) * CoinSpacing;
		if (NextCoinA + Len + 350.f > Known)
		{
			break;
		}
		const int32 Start = Rng.RandHelper(N);
		int32 Lane = INDEX_NONE;
		for (int32 T = 0; T < N && Lane == INDEX_NONE; ++T)
		{
			const int32 L = (Start + T) % N;
			bool bWet = false;
			for (const FPuddle& Pd : Puddles)
			{
				bWet |= (Pd.Mask & (1 << L)) && Pd.A1 > NextCoinA - 100.f && Pd.A0 < NextCoinA + Len + 100.f;
			}
			if (!bWet && !LaneBusy(L, NextCoinA - 350.f, NextCoinA + Len + 350.f))
			{
				Lane = L;
			}
		}
		if (Lane == INDEX_NONE)
		{
			NextCoinA += 700.f;
			continue;
		}
		const float Lat = Layout.LaneLat(Lane);
		for (int32 K = 0; K < Count; ++K)
		{
			const float A = NextCoinA + K * CoinSpacing;
			FVector Pos, Fwd;
			Layout.Sample(A, Lat, Pos, Fwd);
			Coins->AddCoin(Pos, Lane, Lat, A);
		}
		NextCoinA += Len + Rng.FRandRange(CoinGap.X, CoinGap.Y);
	}
}

float ATrackDirector::DebugPlaceObstacle(EObstacleType Type, float AheadCm)
{
	const int32 G = Cat->GetLanes()->GetTargetLane();
	AObstacle* O = AcquireObstacle(Type);
	float A = Layout.AFromLaneArc(G, Layout.LaneArc(G, Cat->GetA()) + AheadCm);
	if (Type == EObstacleType::Pit && Layout.bStraight)
	{
		const float SL = Layout.SectionLength;
		const int32 First = FMath::CeilToInt(A / SL);
		O->SetPitLength(SL);
		Canvas->SetHole(G, First);
		A = (First + 0.5f) * SL;
	}
	FVector Pos, Fwd;
	Layout.Sample(A, Layout.LaneLat(G), Pos, Fwd);
	O->Place(Pos, Fwd.Rotation(), G, A);
	return A;
}

void ATrackDirector::CheckPuddles()
{
	if (Cat->IsInvulnerable() || Cat->GetFeetZ() > 50.f)
	{
		return;
	}
	const int32 G = Cat->GetLanes()->GetCurrentLane();
	const float A = Cat->GetA();
	for (const FPuddle& Pd : Puddles)
	{
		if ((Pd.Mask & (1 << G)) && A > Pd.A0 - Cat->CollisionHalfLength && A < Pd.A1 + Cat->CollisionHalfLength)
		{
			LastHitReason = FString::Printf(TEXT("weisse Tinte auf Fahrbahn %d bei %.0f m"), G, GetMeters());
			const bool bDead = Game ? Game->OnCatHit(LastHitReason) : true;
			if (bDead)
			{
				bRunning = false;
			}
			return;
		}
	}
}

void ATrackDirector::OnCatLostLife()
{
	if (Boss && Mode == ERunMode::Endless)
	{
		Boss->PlayClip(TEXT("cathit"), false);
	}
}

void ATrackDirector::OnPlayerDead()
{
	if (Boss && Mode == ERunMode::Endless)
	{
		Boss->PlayClip(TEXT("dead"), false);
	}
}

void ATrackDirector::PlayBomb()
{
	if (Game)
	{
		Game->PlayBomb();
	}
}

void ATrackDirector::PlaySplash(float VolumeScale)
{
	if (Game)
	{
		Game->PlaySplash(VolumeScale);
	}
}
