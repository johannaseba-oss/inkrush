#include "InkCanvas.h"
#include "RunTypes.h"
#include "Components/InstancedStaticMeshComponent.h"

AInkCanvas::AInkCanvas()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Tiles = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Tiles"));
	Tiles->SetupAttachment(RootComponent);
	Tiles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tiles->SetCastShadow(false);
	Tiles->NumCustomDataFloats = 1;

	Beacons = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Beacons"));
	Beacons->SetupAttachment(RootComponent);
	Beacons->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacons->SetCastShadow(false);
}

static FTransform TileXform(const FVector& P0, const FVector& P1, float LaneWidth)
{
	const FVector D = P1 - P0;
	// leicht ueberlappend (keine Fugen), Oberkante bei Z = 0
	return FTransform(D.Rotation(), (P0 + P1) * 0.5f + FVector(0.f, 0.f, -5.f), FVector((D.Size2D() + 4.f) / 100.f, (LaneWidth - 2.f) / 100.f, 0.1f));
}

void AInkCanvas::Build(const FCircuitLayout& InLayout)
{
	Layout = InLayout;
	Tiles->ClearInstances();
	Tiles->SetStaticMesh(RunAssets::Shape(TEXT("Cube")));
	Tiles->SetMaterial(0, RunAssets::Material(TEXT("M_Ink")));
	Beacons->ClearInstances();
	Beacons->SetStaticMesh(RunAssets::Shape(TEXT("Cylinder")));
	Beacons->SetMaterial(0, RunAssets::Material(TEXT("M_Beacon")));
	Filling.Reset();

	const int32 NumLanes = Layout.NumLanesTotal();
	LaneStart.SetNum(NumLanes);
	LaneCount.Init(0, NumLanes);
	TArray<FTransform> Xforms;
	if (Layout.bStraight)
	{
		Sub = 1;
		TotalSections = NumLanes * WindowRows;
		SlotAbs.SetNum(WindowRows);
		for (int32 G = 0; G < NumLanes; ++G)
		{
			LaneStart[G] = G * WindowRows;
		}
		Xforms.Init(FTransform::Identity, TotalSections);
		Tiles->AddInstances(Xforms, false);
		Painted.Init(0, TotalSections);
		Visual.Init(0.f, TotalSections);
		ResetInk(0.f);
		return;
	}

	Sub = FMath::Max(1, SubTiles);
	TotalSections = 0;
	for (int32 G = 0; G < NumLanes; ++G)
	{
		LaneStart[G] = TotalSections;
		const int32 N = Layout.NumSections(G);
		const float L = Layout.LaneLength(G);
		const float Lat = Layout.LaneLat(G);
		for (int32 I = 0; I < N; ++I)
		{
			for (int32 K = 0; K < Sub; ++K)
			{
				const float Arc0 = L * (I + float(K) / Sub) / N;
				const float Arc1 = L * (I + float(K + 1) / Sub) / N;
				Xforms.Add(TileXform(Layout.Position(Layout.AFromLaneArc(G, Arc0), Lat), Layout.Position(Layout.AFromLaneArc(G, Arc1), Lat), Layout.LaneWidth));
			}
		}
		TotalSections += N;
	}
	Tiles->AddInstances(Xforms, false);
	Painted.Init(0, TotalSections);
	Visual.Init(0.f, TotalSections);
	ResetInk(0.f);
}

int32 AInkCanvas::SectionKey(int32 G, int32 Index) const
{
	if (!LaneStart.IsValidIndex(G))
	{
		return INDEX_NONE;
	}
	if (Layout.bStraight)
	{
		if (Index < 0)
		{
			return INDEX_NONE;
		}
		const int32 Slot = Index % WindowRows;
		return SlotAbs[Slot] == Index ? LaneStart[G] + Slot : INDEX_NONE;
	}
	const int32 N = Layout.NumSections(G);
	return LaneStart[G] + ((Index % N) + N) % N;
}

void AInkCanvas::WriteVisual(int32 Key, bool bDirty)
{
	// Einfaerbung wird gewertet, aber nicht als schwarze Flaeche gezeigt (Spur = Pfotenabdruecke, AInkMarks)
	const float V = bShowCoverage ? Visual[Key] : 0.f;
	for (int32 K = 0; K < Sub; ++K)
	{
		Tiles->SetCustomDataValue(Key * Sub + K, 0, FMath::Clamp(V * Sub - K, 0.f, 1.f), bDirty);
	}
}

void AInkCanvas::PlaceWindowSlot(int32 Slot)
{
	const int32 Abs = SlotAbs[Slot];
	for (int32 G = 0; G < Layout.NumLanesTotal(); ++G)
	{
		const int32 Key = LaneStart[G] + Slot;
		Painted[Key] = 0;
		Visual[Key] = 0.f;
		Filling.Remove(Key);
		const float Lat = Layout.LaneLat(G);
		FTransform X = TileXform(FVector(Abs * Layout.SectionLength, Lat, 0.f), FVector((Abs + 1) * Layout.SectionLength, Lat, 0.f), Layout.LaneWidth);
		if (Holes.Contains(FIntPoint(G, Abs)))
		{
			// Loch: Kachel verschwindet, darunter liegt der Schacht des Abgrunds
			X = FTransform(FRotator::ZeroRotator, FVector(Abs * Layout.SectionLength, Lat, -5000.f), FVector(0.001f));
		}
		Tiles->UpdateInstanceTransform(Key, X, false, false, true);
		Tiles->SetCustomDataValue(Key, 0, 0.f, false);
	}
}

void AInkCanvas::ResetInk(float CatA)
{
	Filling.Reset();
	TotalPainted = 0;
	LaneCount.Init(0, Layout.NumLanesTotal());
	Beacons->ClearInstances();
	Holes.Reset();
	if (Layout.bStraight)
	{
		const int32 First = FMath::Max(0, Layout.SectionAt(0, CatA) - WindowBehind);
		for (int32 R = 0; R < WindowRows; ++R)
		{
			SlotAbs[R] = First + ((R - First % WindowRows) + WindowRows) % WindowRows;
			PlaceWindowSlot(R);
		}
		Tiles->MarkRenderStateDirty();
		return;
	}
	for (int32 I = 0; I < TotalSections; ++I)
	{
		Painted[I] = 0;
		Visual[I] = 0.f;
		WriteVisual(I, false);
	}
	Tiles->MarkRenderStateDirty();
}

void AInkCanvas::SetHole(int32 G, int32 Index)
{
	if (!Layout.bStraight || Index < 0)
	{
		return;
	}
	Holes.Add(FIntPoint(G, Index));
	const int32 Slot = Index % WindowRows;
	if (SlotAbs.IsValidIndex(Slot) && SlotAbs[Slot] == Index && LaneStart.IsValidIndex(G))
	{
		const FTransform X(FRotator::ZeroRotator, FVector(Index * Layout.SectionLength, Layout.LaneLat(G), -5000.f), FVector(0.001f));
		Tiles->UpdateInstanceTransform(LaneStart[G] + Slot, X, false, true, true);
	}
}

void AInkCanvas::ClearHole(int32 G, int32 Index)
{
	if (Holes.Remove(FIntPoint(G, Index)) == 0)
	{
		return;
	}
	const int32 Slot = Index % WindowRows;
	if (SlotAbs.IsValidIndex(Slot) && SlotAbs[Slot] == Index && LaneStart.IsValidIndex(G))
	{
		const float Lat = Layout.LaneLat(G);
		const FTransform X = TileXform(FVector(Index * Layout.SectionLength, Lat, 0.f), FVector((Index + 1) * Layout.SectionLength, Lat, 0.f), Layout.LaneWidth);
		Tiles->UpdateInstanceTransform(LaneStart[G] + Slot, X, false, true, true);
	}
}

void AInkCanvas::UpdateWindow(float CatA)
{
	if (!Layout.bStraight)
	{
		return;
	}
	const int32 First = FMath::Max(0, Layout.SectionAt(0, CatA) - WindowBehind);
	bool bChanged = false;
	for (auto It = Holes.CreateIterator(); It; ++It)
	{
		if (It->Y < First)
		{
			It.RemoveCurrent();
		}
	}
	for (int32 R = 0; R < WindowRows; ++R)
	{
		if (SlotAbs[R] < First)
		{
			// hinter der Katze: als naechste Reihe vorne wiederverwenden (dort wieder weiss)
			while (SlotAbs[R] < First)
			{
				SlotAbs[R] += WindowRows;
			}
			PlaceWindowSlot(R);
			bChanged = true;
		}
	}
	if (bChanged)
	{
		Tiles->MarkRenderStateDirty();
	}
}

bool AInkCanvas::PaintSection(int32 G, int32 Index)
{
	const int32 Key = SectionKey(G, Index);
	if (Key == INDEX_NONE || Painted[Key])
	{
		return false;
	}
	Painted[Key] = 1;
	++LaneCount[G];
	++TotalPainted;
	Filling.Add(Key, 1.f);
	if (!Layout.bStraight && Layout.NumSections(G) - LaneCount[G] <= BeaconThreshold)
	{
		UpdateBeacons();
	}
	return true;
}

int32 AInkCanvas::PaintSpan(int32 G, float FromA, float ToA)
{
	if (!LaneStart.IsValidIndex(G))
	{
		return 0;
	}
	const int32 N = Layout.NumSections(G);
	const int32 S0 = Layout.SectionAt(G, FromA);
	const int32 S1 = Layout.SectionAt(G, ToA);
	int32 Steps = Layout.bStraight ? S1 - S0 : ((S1 - S0) % N + N) % N;
	if (Steps < 0 || Steps > FMath::Min(N / 2, 200))
	{
		Steps = 0; // Rueckwaerts/Sprung (z. B. Neustart): nur aktuellen Abschnitt
	}
	int32 Count = 0;
	for (int32 K = 0; K <= Steps; ++K)
	{
		Count += PaintSection(G, S0 + K) ? 1 : 0;
	}
	return Count;
}

int32 AInkCanvas::PaintArea(int32 Circuit, float CenterA, float HalfLength)
{
	int32 Count = 0;
	for (int32 L = 0; L < Layout.LanesPerCircuit; ++L)
	{
		const int32 G = Circuit * Layout.LanesPerCircuit + L;
		if (!LaneStart.IsValidIndex(G))
		{
			continue;
		}
		const float Arc = Layout.LaneArc(G, CenterA);
		Count += PaintSpan(G, Layout.AFromLaneArc(G, Arc - HalfLength), Layout.AFromLaneArc(G, Arc + HalfLength));
	}
	return Count;
}

bool AInkCanvas::IsPainted(int32 G, int32 Index) const
{
	const int32 Key = SectionKey(G, Index);
	return Key != INDEX_NONE && Painted[Key] != 0;
}

int32 AInkCanvas::CountPainted(int32 G) const
{
	return LaneCount.IsValidIndex(G) ? LaneCount[G] : 0;
}

int32 AInkCanvas::CountSections(int32 G) const
{
	return Layout.bStraight ? 0 : Layout.NumSections(G);
}

float AInkCanvas::LaneProgress(int32 G) const
{
	return CountSections(G) > 0 ? float(CountPainted(G)) / CountSections(G) : 0.f;
}

void AInkCanvas::DebugPaintAll(int32 ExceptLane)
{
	if (Layout.bStraight)
	{
		return;
	}
	for (int32 G = 0; G < Layout.NumLanesTotal(); ++G)
	{
		if (G == ExceptLane)
		{
			continue;
		}
		for (int32 I = 0; I < Layout.NumSections(G); ++I)
		{
			PaintSection(G, I);
		}
	}
}

void AInkCanvas::SetTrailCoverage(int32 G, int32 Index, float Fraction)
{
	const int32 Key = SectionKey(G, Index);
	if (Key == INDEX_NONE)
	{
		return;
	}
	const float F = FMath::Clamp(Fraction, 0.f, 1.f);
	if (F > Visual[Key])
	{
		Visual[Key] = F;
		WriteVisual(Key, true);
	}
	// die Katze bedeckt den Abschnitt gerade selbst -> nicht automatisch auffuellen
	Filling.Remove(Key);
}

void AInkCanvas::FinishCoverage(int32 G, int32 Index)
{
	const int32 Key = SectionKey(G, Index);
	if (Key != INDEX_NONE && Painted[Key] && Visual[Key] < 1.f)
	{
		Filling.Add(Key, 1.f);
	}
}

void AInkCanvas::UpdateBeacons()
{
	// Leuchtende Saeulen ueber allen fehlenden Abschnitten von Fahrbahnen, denen nur noch wenige fehlen
	Beacons->ClearInstances();
	for (int32 L = 0; L < Layout.NumLanesTotal(); ++L)
	{
		const int32 N = Layout.NumSections(L);
		const int32 Missing = N - LaneCount[L];
		if (Missing <= 0 || Missing > BeaconThreshold)
		{
			continue;
		}
		for (int32 I = 0; I < N; ++I)
		{
			if (!Painted[LaneStart[L] + I])
			{
				FVector C;
				FRotator R;
				float Len = 0.f;
				Layout.SectionTransform(L, I, C, R, Len);
				Beacons->AddInstance(FTransform(FRotator::ZeroRotator, C + FVector(0.f, 0.f, 400.f), FVector(0.25f, 0.25f, 8.f)));
			}
		}
	}
}

void AInkCanvas::StepCanvas(float DeltaTime)
{
	Time += DeltaTime;
	if (Filling.Num() == 0)
	{
		return;
	}
	TArray<int32> Done;
	for (const TPair<int32, float>& F : Filling)
	{
		const int32 Key = F.Key;
		Visual[Key] = FMath::Min(F.Value, Visual[Key] + DeltaTime / 0.2f);
		WriteVisual(Key, false);
		if (Visual[Key] >= F.Value)
		{
			Done.Add(Key);
		}
	}
	for (int32 K : Done)
	{
		Filling.Remove(K);
	}
	Tiles->MarkRenderStateDirty();
}
