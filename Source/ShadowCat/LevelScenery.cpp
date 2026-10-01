#include "LevelScenery.h"
#include "RunTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"

void ALevelScenery::LoadSpriteTextures()
{
	if (TreeTex.Num() > 0 || HouseTex.Num() > 0)
	{
		return;
	}
	// T_Sprite_Tree1..9 / T_Sprite_House1..9 (weitere Bilder einfach nach Tools/Import/Nature legen)
	for (int32 I = 1; I <= 9; ++I)
	{
		for (int32 K = 0; K < 2; ++K)
		{
			const TCHAR* Kind = K == 0 ? TEXT("Tree") : TEXT("House");
			const FString Path = FString::Printf(TEXT("/Game/Nature/T_Sprite_%s%d.T_Sprite_%s%d"), Kind, I, Kind, I);
			if (UTexture2D* T = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				(K == 0 ? TreeTex : HouseTex).Add(T);
			}
		}
	}
	LampTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Nature/T_Sprite_StreetLamp.T_Sprite_StreetLamp"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	FenceTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Nature/T_Sprite_Zaun.T_Sprite_Zaun"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto Load = [](const TCHAR* Name) { return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Nature/T_Sprite_%s.T_Sprite_%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet); };
	if (UTexture2D* Pine = Load(TEXT("Pinetree")))
	{
		TreeTex.Add(Pine);
	}
	const TPair<const TCHAR*, float> Props[] = { { TEXT("Bush"), 210.f }, { TEXT("Car"), 230.f }, { TEXT("CardboardBox"), 130.f }, { TEXT("Stopsign"), 340.f }, { TEXT("Crate"), 120.f } };
	for (const TPair<const TCHAR*, float>& Pr : Props)
	{
		if (UTexture2D* T = Load(Pr.Key))
		{
			PropTex.Add(T);
			PropHeight.Add(Pr.Value);
		}
	}
#if WITH_EDITOR
	// Seitenverhaeltnis wird beim Verteilen gebraucht (Testfenster: Texturen evtl. noch Platzhalter)
	TArray<UTexture*> All;
	for (UTexture2D* T : PropTex)
	{
		All.Add(T);
	}
	FTextureCompilingManager::Get().FinishCompilation(All);
#endif
	UE_LOG(LogTemp, Log, TEXT("Kulisse: %d Baum-, %d Haus-, %d Kleinkram-Bilder, Laterne %s, Zaun %s"), TreeTex.Num(), HouseTex.Num(), PropTex.Num(), LampTex ? TEXT("ja") : TEXT("nein"), FenceTex ? TEXT("ja") : TEXT("nein"));
}

void ALevelScenery::AddSprite(FRandomStream& R, bool bHouse, const FVector& P, float Height)
{
	const TArray<TObjectPtr<UTexture2D>>& Kinds = bHouse ? HouseTex : TreeTex;
	if (Kinds.Num() == 0)
	{
		return;
	}
	UTexture2D* Tex = Kinds[R.RandHelper(Kinds.Num())];
	AddSpriteTex(Tex, P, Height, R.FRand() < 0.5f ? -1.f : 1.f, true);
}

void ALevelScenery::AddFenceRun(float Lat, float A0, int32 Count)
{
	if (!FenceTex)
	{
		return;
	}
	const float H = 110.f;
	const float W = H * float(FenceTex->GetSizeX()) / float(FMath::Max(1, FenceTex->GetSizeY()));
	float A = A0;
	for (int32 I = 0; I < Count; ++I)
	{
		// Mitte des Stuecks, Ausrichtung entlang der Strecke (auch in Kurven)
		const float Step = W * 0.96f / Layout.Stretch(A, Lat);
		FVector P, F;
		Layout.Sample(Layout.WrapA(A + Step * 0.5f), Lat, P, F);
		AddSpriteTex(FenceTex, P, H, 1.f, false, F.Rotation().Yaw);
		A += Step;
	}
}

void ALevelScenery::AddSpriteTex(UTexture2D* Tex, const FVector& P, float Height, float Flip, bool bFace, float Yaw)
{
	if (!Tex)
	{
		return;
	}
	TObjectPtr<UMaterialInstanceDynamic>& Mat = SpriteMats.FindOrAdd(Tex);
	if (!Mat)
	{
		Mat = RunAssets::NewMID(TEXT("M_Sprite"), this);
		if (Mat)
		{
			Mat->SetTextureParameterValue(TEXT("Tex"), Tex);
		}
#if WITH_EDITOR
		// Editor/Testfenster: Textur evtl. noch in Arbeit (Platzhalter 32x32) -> fuer das Seitenverhaeltnis fertigstellen
		FTextureCompilingManager::Get().FinishCompilation({ Tex });
#endif
	}
	const float Aspect = Tex->GetSizeY() > 0 ? float(Tex->GetSizeX()) / float(Tex->GetSizeY()) : 1.f;
	FSprite S;
	S.Ism = IsmFor(RunAssets::Shape(TEXT("Plane")), Mat);
	S.Base = P;
	S.Height = Height;
	// Flip -1 = gespiegelt (negative Breite)
	S.Width = Height * Aspect * Flip;
	S.bFace = bFace;
	S.Instance = S.Ism->AddInstance(FTransform(FRotator(0.f, bFace ? -90.f : Yaw, 90.f), P + FVector(0.f, 0.f, Height * 0.5f), FVector(S.Width / 100.f, Height / 100.f, 1.f)), false);
	Sprites.Add(S);
}

ALevelScenery::ALevelScenery()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetActorEnableCollision(false);
}

UInstancedStaticMeshComponent* ALevelScenery::IsmFor(UStaticMesh* Mesh, UMaterialInterface* Mat)
{
	const FString Key = FString::Printf(TEXT("%p|%p"), Mesh, Mat);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Isms.Find(Key))
	{
		return *Found;
	}
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetStaticMesh(Mesh);
	if (Mat)
	{
		C->SetMaterial(0, Mat);
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->RegisterComponent();
	Isms.Add(Key, C);
	return C;
}

UInstancedStaticMeshComponent* ALevelScenery::Ism(const TCHAR* Shape, UMaterialInterface* Mat)
{
	return IsmFor(RunAssets::Shape(Shape), Mat);
}

void ALevelScenery::Add(const TCHAR* Shape, UMaterialInterface* Mat, const FVector& Loc, const FRotator& Rot, const FVector& Scale)
{
	Ism(Shape, Mat)->AddInstance(FTransform(Rot, Loc, Scale), false);
}

void ALevelScenery::AddStrip(const TCHAR* Shape, UMaterialInterface* Mat, float Lat, float Z, float Width, float Height, float StepLen, float FromA, float ToA)
{
	const float L = Layout.LoopLength();
	if (ToA < 0.f)
	{
		ToA = L;
	}
	// Schrittweite in A so waehlen, dass die Weltlaenge etwa StepLen betraegt; in Kurven kuerzer (glatter)
	float A = FromA;
	while (A < ToA - 1.f)
	{
		const float Len = Layout.IsOnCurve(A) ? FMath::Min(StepLen, 120.f) : StepLen;
		const float Step = Len / Layout.Stretch(A, Lat);
		const float B = FMath::Min(A + Step, ToA);
		const FVector P0 = Layout.Position(A, Lat);
		const FVector P1 = Layout.Position(B, Lat);
		const FVector D = P1 - P0;
		Add(Shape, Mat, (P0 + P1) * 0.5f + FVector(0.f, 0.f, Z), D.Rotation(), FVector((D.Size2D() + 4.f) / 100.f, Width / 100.f, Height / 100.f));
		A = B;
	}
}

void ALevelScenery::Clear()
{
	for (TPair<FString, TObjectPtr<UInstancedStaticMeshComponent>>& P : Isms)
	{
		P.Value->DestroyComponent();
	}
	Isms.Reset();
	Sprites.Reset();
	SpriteMats.Reset();
	LastCam = FVector(1.0e9);
	for (UStaticMeshComponent* F : Fog)
	{
		F->DestroyComponent();
	}
	Fog.Reset();
	FogSize.Reset();
	FogBase.Reset();
	FogPhase.Reset();
	SetActorLocation(FVector::ZeroVector);
}

void ALevelScenery::Build(const FCircuitLayout& InLayout, float Density, int32 Seed)
{
	Clear();
	Layout = InLayout;
	LoadSpriteTextures();
	FRandomStream R(Seed);
	const bool bFog = !FParse::Param(FCommandLine::Get(), TEXT("CatNoFog"));

	if (Layout.bStraight)
	{
		BuildStraight(R, Density, bFog);
	}
	else
	{
		// grosser dunkler Boden unter allem
		Add(TEXT("Cube"), RunAssets::Mono(0.045f), FVector(0.f, 0.f, -30.f), FRotator::ZeroRotator, FVector(700.f, 700.f, 0.2f));
		BuildTrackEdges();
		BuildGaps();
		BuildDecor(R, Density);
		if (bFog)
		{
			BuildFog(R, Density);
		}
	}
	for (TPair<FString, TObjectPtr<UInstancedStaticMeshComponent>>& P : Isms)
	{
		P.Value->MarkRenderStateDirty();
	}
}

void ALevelScenery::BuildTrackEdges()
{
	// Unterbau und Randsteine jedes Kurses
	const float Half = Layout.LanesPerCircuit * Layout.LaneWidth * 0.5f;
	for (int32 C = 0; C < Layout.NumCircuits; ++C)
	{
		const float Mid = C * Layout.CircuitSpacing;
		AddStrip(TEXT("Cube"), RunAssets::Mono(0.1f), Mid, -14.f, Half * 2.f + 30.f, 20.f, 400.f);
		AddStrip(TEXT("Cube"), RunAssets::Mono(0.3f, 0.12f), Mid - Half - 8.f, 2.f, 14.f, 14.f, 300.f);
		AddStrip(TEXT("Cube"), RunAssets::Mono(0.3f, 0.12f), Mid + Half + 8.f, 2.f, 14.f, 14.f, 300.f);
	}
}

void ALevelScenery::BuildGaps()
{
	const float S = Layout.StraightLength;
	const float C = PI * Layout.BaseRadius;
	const float Half = Layout.LanesPerCircuit * Layout.LaneWidth * 0.5f;
	const float GapWidth = Layout.CircuitSpacing - Half * 2.f - 30.f;
	const float Conn[2][2] = {
		{ S * Layout.ConnectionFrom, S * Layout.ConnectionTo },
		{ S + C + S * Layout.ConnectionFrom, S + C + S * Layout.ConnectionTo } };
	UMaterialInterface* Post = RunAssets::Mono(0.14f);
	UMaterialInterface* Plank = RunAssets::Mono(0.32f, 0.15f);
	UMaterialInterface* Glow = RunAssets::Glow(1.f, 2.2f);

	for (int32 G = 0; G + 1 < Layout.NumCircuits; ++G)
	{
		const float Lat = G * Layout.CircuitSpacing + Layout.CircuitSpacing * 0.5f;
		const float L = Layout.LoopLength();
		const float Ranges[3][2] = { { Conn[0][1], Conn[1][0] }, { Conn[1][1], L }, { 0.f, Conn[0][0] } };
		for (const auto& Rg : Ranges)
		{
			for (float A = Rg[0]; A < Rg[1]; A += 260.f / Layout.Stretch(A, Lat))
			{
				Add(TEXT("Cylinder"), Post, Layout.Position(A, Lat) + FVector(0.f, 0.f, 45.f), FRotator::ZeroRotator, FVector(0.08f, 0.08f, 0.9f));
			}
			AddStrip(TEXT("Cube"), Post, Lat, 80.f, 6.f, 6.f, 300.f, Rg[0], Rg[1]);
		}
		for (const auto& Cn : Conn)
		{
			for (float A = Cn[0]; A < Cn[1]; A += 55.f)
			{
				FVector P, F;
				Layout.Sample(A, Lat, P, F);
				Add(TEXT("Cube"), Plank, P + FVector(0.f, 0.f, -4.f), F.Rotation(), FVector(0.42f, (GapWidth + 40.f) / 100.f, 0.08f));
			}
			AddStrip(TEXT("Cube"), Glow, Lat - GapWidth * 0.5f, 1.f, 5.f, 3.f, 300.f, Cn[0], Cn[1]);
			AddStrip(TEXT("Cube"), Glow, Lat + GapWidth * 0.5f, 1.f, 5.f, 3.f, 300.f, Cn[0], Cn[1]);
			for (float A = Cn[0] + 150.f; A < Cn[1]; A += 450.f)
			{
				FVector P, F;
				Layout.Sample(A, Lat, P, F);
				Add(TEXT("Cube"), Glow, P + FVector(0.f, 0.f, 2.f), FRotator(0.f, F.Rotation().Yaw + 45.f, 0.f), FVector(0.45f, 0.45f, 0.03f));
			}
		}
	}
}

void ALevelScenery::BuildDecor(FRandomStream& R, float Density)
{
	// nur gezeichnete Bildtafeln (Baeume, Haeuser), keine 3D-Modelle
	const float L = Layout.LoopLength();
	const float Half = Layout.LanesPerCircuit * Layout.LaneWidth * 0.5f;
	const float InnerMin = -Layout.BaseRadius + 350.f;
	const float InnerMax = -Half - 300.f;
	const float OuterMin = (Layout.NumCircuits - 1) * Layout.CircuitSpacing + Half + 350.f;
	auto Count = [Density](int32 N) { return FMath::Max(1, FMath::RoundToInt(N * Density)); };
	// belegte Stellen (X, Y, Radius): nichts steht in oder auf etwas anderem
	TArray<FVector> Placed;
	auto Place = [&](bool bHouse, float LatMin, float LatMax, float HMin, float HMax)
	{
		const float H = R.FRandRange(HMin, HMax);
		const float Rad = H * (bHouse ? 0.42f : 0.3f);
		for (int32 Try = 0; Try < 10; ++Try)
		{
			const FVector P = Layout.Position(R.FRandRange(0.f, L), R.FRandRange(LatMin, LatMax));
			if (IsFreeSpot(Placed, P, Rad, 0.f))
			{
				Placed.Add(FVector(P.X, P.Y, Rad));
				AddSprite(R, bHouse, P, H);
				return;
			}
		}
	};

	// Laternen am Innenrand zuerst (feste Plaetze)
	for (int32 I = 0; I < Count(12); ++I)
	{
		const FVector P = Layout.Position(L * (I + 0.5f) / Count(12), -Half - 140.f);
		Placed.Add(FVector(P.X, P.Y, 90.f));
		AddSpriteTex(LampTex, P, 420.f, I % 2 ? -1.f : 1.f, true);
	}
	for (int32 I = 0; I < Count(12); ++I)
	{
		Place(true, OuterMin + 500.f, OuterMin + 3800.f, 600.f, 900.f);
	}
	for (int32 I = 0; I < Count(44); ++I)
	{
		const bool bInner = I % 5 == 0;
		Place(false, bInner ? InnerMin : OuterMin + 100.f, bInner ? InnerMax : OuterMin + 4200.f, 550.f, 950.f);
	}
	// ferne grosse Baeume rundherum
	for (int32 I = 0; I < 14; ++I)
	{
		Place(false, OuterMin + 5000.f, OuterMin + 9000.f, 1600.f, 2600.f);
	}
}

bool ALevelScenery::IsFreeSpot(const TArray<FVector>& Placed, const FVector& P, float Radius, float InPeriod)
{
	for (const FVector& Q : Placed)
	{
		float Dx = P.X - Q.X;
		if (InPeriod > 0.f)
		{
			// periodische Kulisse: auch mit der Nachbarperiode vergleichen
			Dx = FMath::Fmod(Dx, InPeriod);
			if (Dx > InPeriod * 0.5f) Dx -= InPeriod;
			if (Dx < -InPeriod * 0.5f) Dx += InPeriod;
		}
		const float Dy = P.Y - Q.Y;
		if (Dx * Dx + Dy * Dy < FMath::Square(Radius + Q.Z))
		{
			return false;
		}
	}
	return true;
}

void ALevelScenery::BuildStraight(FRandomStream& R, float Density, bool bFog)
{
	// Endlose Gerade: Inhalt einer Periode wird dreimal hintereinander gebaut; der Actor springt in StepScenery
	// jeweils um eine Periode mit der Katze mit (unsichtbar, da periodisch).
	const float P = Period;
	const float Half = Layout.LanesPerCircuit * Layout.LaneWidth * 0.5f;
	auto Count = [Density](int32 N) { return FMath::Max(1, FMath::RoundToInt(N * Density)); };

	// Deko einer Periode wie auf einer Filmkulisse: links und rechts dicht an der Strasse grosse Haeuser, davor Laternen
	// und Kleinkram (Busch, Auto, Karton, Schild), dahinter Baeume; die Mitte hinten bleibt frei fuer den Djinn.
	// Kind 0 = Baum, 1 = Haus, 2 = Laterne, 3 = Kleinkram (Seed waehlt das Bild); belegte Stellen werden gemerkt
	struct FItem { int32 Kind; FVector Pos; double Size; int32 Seed; };
	TArray<FItem> Items;
	TArray<FVector> Placed;
	auto RandSide = [&R]() { return R.FRand() < 0.5f ? -1.f : 1.f; };
	// wenige Laternen, versetzt links/rechts direkt am Rand
	const int32 NL = 2;
	for (int32 I = 0; I < NL; ++I)
	{
		for (int32 S = 0; S < 2; ++S)
		{
			const FVector Pos(P * (I + 0.1f + S * 0.5f) / NL, (S ? 1.f : -1.f) * (Half + 105.f), 0.f);
			Items.Add({ 2, Pos, 480.0, I });
			Placed.Add(FVector(Pos.X, Pos.Y, 80.f));
		}
	}
	// Lat: Mitte der Tafel; bei festem Abstand zum Rand haengt sie von der Bildbreite ab (Edge > 0: Rand + halbe Breite)
	auto Place = [&](int32 Kind, float XMin, float XMax, float LatMin, float LatMax, float Side, float HMin, float HMax, float Aspect = 0.f)
	{
		const float H = R.FRandRange(HMin, HMax);
		const float Rad = Aspect > 0.f ? H * Aspect * 0.5f : H * (Kind == 1 ? 0.42f : 0.3f);
		for (int32 Try = 0; Try < 12; ++Try)
		{
			float Lat = R.FRandRange(LatMin, LatMax);
			if (Aspect > 0.f)
			{
				Lat += H * Aspect * 0.5f;
			}
			const FVector Pos(R.FRandRange(XMin, XMax), (Side != 0.f ? Side : RandSide()) * Lat, 0.f);
			if (IsFreeSpot(Placed, Pos, Rad, P))
			{
				Placed.Add(FVector(Pos.X, Pos.Y, Rad));
				Items.Add({ Kind, Pos, H, R.RandHelper(1 << 20) });
				return;
			}
		}
	};
	// grosse Haeuser nah an der Strasse, abwechselnd links und rechts versetzt
	const int32 NH = FMath::Max(3, Count(5));
	for (int32 I = 0; I < NH; ++I)
	{
		for (int32 S = 0; S < 2; ++S)
		{
			const float X0 = P * (I + S * 0.5f) / NH;
			Place(1, X0, X0 + P * 0.35f / NH, Half + 60.f, Half + 260.f, S ? 1.f : -1.f, 1050.f, 1400.f, 0.95f);
		}
	}
	// Kleinkram zwischen Laternen und Haeusern
	if (PropTex.Num() > 0)
	{
		const int32 NP = Count(30);
		for (int32 I = 0; I < NP; ++I)
		{
			const int32 Seed = R.RandHelper(1 << 20);
			const int32 Which = Seed % PropTex.Num();
			UTexture2D* T = PropTex[Which];
			const float Aspect = T->GetSizeY() > 0 ? float(T->GetSizeX()) / float(T->GetSizeY()) : 1.f;
			const float H = PropHeight[Which] * R.FRandRange(0.9f, 1.15f);
			const float Rad = H * Aspect * 0.5f + 20.f;
			for (int32 Try = 0; Try < 12; ++Try)
			{
				const FVector Pos(R.FRandRange(0.f, P), RandSide() * (Half + 60.f + H * Aspect * 0.5f + R.FRandRange(0.f, 120.f)), 0.f);
				if (IsFreeSpot(Placed, Pos, Rad, P))
				{
					Placed.Add(FVector(Pos.X, Pos.Y, Rad));
					Items.Add({ 3, Pos, H, Seed });
					break;
				}
			}
		}
	}
	// Baeume hinter und zwischen den Haeusern (nur so weit draussen, wie das Hochformat-Bild reicht)
	const int32 NT = Count(14);
	for (int32 I = 0; I < NT; ++I) Place(0, P * I / NT, P * (I + 0.9f) / NT, Half + 450.f, Half + 1900.f, 0.f, 800.f, 1300.f);

	for (int32 K = 0; K < 3; ++K)
	{
		const float X0 = K * P;
		// Randsteine und dunkler Seitenboden links/rechts. Unter den Fahrbahnen liegt bewusst nichts:
		// Abgruende sind echte Loecher (Kacheln ausgeblendet, darunter der Schacht des Hindernisses).
		Add(TEXT("Cube"), RunAssets::Mono(0.3f, 0.12f), FVector(X0 + P * 0.5f, -Half - 8.f, -6.f), FRotator::ZeroRotator, FVector(P / 100.f + 0.05f, 0.14f, 0.3f));
		Add(TEXT("Cube"), RunAssets::Mono(0.3f, 0.12f), FVector(X0 + P * 0.5f, Half + 8.f, -6.f), FRotator::ZeroRotator, FVector(P / 100.f + 0.05f, 0.14f, 0.3f));
		// tiefschwarzer Boden neben der Strecke
		Add(TEXT("Cube"), RunAssets::Mono(0.f), FVector(X0 + P * 0.5f, -(Half + 15.f + 10000.f), -30.f), FRotator::ZeroRotator, FVector(P / 100.f + 0.05f, 200.f, 0.2f));
		Add(TEXT("Cube"), RunAssets::Mono(0.f), FVector(X0 + P * 0.5f, Half + 15.f + 10000.f, -30.f), FRotator::ZeroRotator, FVector(P / 100.f + 0.05f, 200.f, 0.2f));
		for (const FItem& It : Items)
		{
			// gleicher Seed in allen drei Kopien -> gleiches Bild, gleiche Spiegelung (Periode nahtlos)
			FRandomStream IR(It.Seed);
			const FVector Pos = It.Pos + FVector(X0, 0.f, 0.f);
			if (It.Kind == 2)
			{
				// Laterne am Strassenrand (Zaeune sind jetzt Hindernisse auf der Strecke)
				AddSpriteTex(LampTex, Pos, It.Size, It.Pos.Y > 0.f ? 1.f : -1.f, true);
			}
			else if (It.Kind == 3)
			{
				// nur Busch und Auto spiegeln (Schrift auf Schild/Karton bliebe sonst spiegelverkehrt)
				UTexture2D* T = PropTex[It.Seed % PropTex.Num()];
				const bool bMirror = (T->GetName().Contains(TEXT("Bush")) || T->GetName().Contains(TEXT("Car"))) && (It.Seed >> 4) % 2;
				AddSpriteTex(T, Pos, It.Size, bMirror ? -1.f : 1.f, true);
			}
			else
			{
				AddSprite(IR, It.Kind == 1, Pos, It.Size);
			}
		}
	}
	if (!bFog)
	{
		return;
	}
	// Nebel in Schwarz und Weiss: helle Schwaden am Strassenrand und als Bodennebel ueber der Strecke,
	// dunkle Schwaden weiter draussen zwischen den Haeusern (verdunkeln die Kulisse stellenweise)
	struct FFogSpec { FVector Pos; double W, H; int32 Var; double Opacity; FLinearColor Color; };
	TArray<FFogSpec> Specs;
	const FLinearColor White(1.35f, 1.35f, 1.4f);
	const FLinearColor Black(0.f, 0.f, 0.f);
	for (int32 I = 0; I < Count(14); ++I)
	{
		const float H = R.FRandRange(380.f, 650.f);
		Specs.Add({ FVector(P * (I + R.FRandRange(0.f, 0.7f)) / Count(14), (I % 2 ? 1.f : -1.f) * R.FRandRange(Half + 900.f, Half + 1900.f), H * 0.45f),
			R.FRandRange(1600.f, 2800.f), H, R.RandHelper(4), 0.7f, White });
	}
	for (int32 I = 0; I < Count(8); ++I)
	{
		const float H = R.FRandRange(500.f, 900.f);
		Specs.Add({ FVector(P * (I + R.FRandRange(0.f, 0.8f)) / Count(8), (I % 2 ? -1.f : 1.f) * R.FRandRange(Half + 900.f, Half + 3500.f), H * 0.35f),
			R.FRandRange(2200.f, 3800.f), H, R.RandHelper(4), 0.8f, Black });
	}
	for (int32 I = 0; I < Count(7); ++I)
	{
		// flacher Bodennebel quer ueber die Fahrbahnen (nahe der Kamera blendet das Material ihn aus)
		const float H = R.FRandRange(150.f, 240.f);
		Specs.Add({ FVector(P * (I + R.FRandRange(0.f, 0.8f)) / Count(7), R.FRandRange(-Half, Half), H * 0.25f),
			R.FRandRange(900.f, 1500.f), H, R.RandHelper(4), 0.4f, White });
	}
	for (int32 K = 0; K < 3; ++K)
	{
		for (int32 I = 0; I < Specs.Num(); ++I)
		{
			const FFogSpec& F = Specs[I];
			AddFog(F.Pos + FVector(K * P, 0.f, 0.f), F.W, F.H, F.Var, F.Opacity, F.Color);
			// gleiche Bewegung in allen drei Kopien, sonst springen die Schwaden beim Mitziehen
			FogPhase.Last() = FogPhase[I];
		}
	}
}

void ALevelScenery::AddFog(const FVector& Loc, float SizeX, float SizeY, int32 Variant, float Opacity, const FLinearColor& Color)
{
	// senkrechtes Quad, das sich in StepScenery zur Kamera dreht (zylindrisches Billboard)
	UMaterialInstanceDynamic* Mat = RunAssets::NewMID(TEXT("M_FogPuff"), this);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Color);
		Mat->SetScalarParameterValue(TEXT("Variant"), float(Variant % 4));
		Mat->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		FogMats.Add(Mat);
	}
	UStaticMeshComponent* Q = RunAssets::AddShape(this, RootComponent, TEXT("Plane"), Loc, FVector(SizeX / 100.f, SizeY / 100.f, 1.f), FRotator(0.f, 0.f, 90.f), Mat);
	Q->SetTranslucentSortPriority(1);
	Fog.Add(Q);
	FogSize.Add(FVector2D(SizeX, SizeY));
	FogBase.Add(Loc);
	FogPhase.Add(FMath::FRand() * 10.f);
}

void ALevelScenery::BuildFog(FRandomStream& R, float Density)
{
	const float L = Layout.LoopLength();
	const float Half = Layout.LanesPerCircuit * Layout.LaneWidth * 0.5f;
	const float OuterMin = (Layout.NumCircuits - 1) * Layout.CircuitSpacing + Half + 250.f;
	auto Count = [Density](int32 N) { return FMath::Max(2, FMath::RoundToInt(N * Density)); };

	for (int32 G = 0; G + 1 < Layout.NumCircuits; ++G)
	{
		const float Lat = G * Layout.CircuitSpacing + Layout.CircuitSpacing * 0.5f;
		for (int32 I = 0; I < Count(14); ++I)
		{
			const FVector P = Layout.Position(L * (I + R.FRandRange(0.f, 0.8f)) / Count(14), Lat);
			const float H = R.FRandRange(180.f, 260.f);
			AddFog(P + FVector(0.f, 0.f, H * 0.3f), R.FRandRange(700.f, 1000.f), H, R.RandHelper(4), 0.55f);
		}
	}
	for (int32 I = 0; I < Count(16); ++I)
	{
		const FVector P = Layout.Position(R.FRandRange(0.f, L), R.FRandRange(-Layout.BaseRadius + 150.f, -Half - 350.f));
		const float H = R.FRandRange(320.f, 520.f);
		AddFog(P + FVector(0.f, 0.f, H * 0.3f), R.FRandRange(1400.f, 2400.f), H, R.RandHelper(4), 0.7f);
	}
	for (int32 I = 0; I < Count(44); ++I)
	{
		const FVector P = Layout.Position(L * (I + R.FRandRange(0.f, 1.f)) / Count(44), R.FRandRange(OuterMin + 150.f, OuterMin + 3800.f));
		const float H = R.FRandRange(420.f, 700.f);
		AddFog(P + FVector(0.f, 0.f, H * 0.3f), R.FRandRange(2400.f, 4000.f), H, R.RandHelper(4), 0.8f);
	}
}

void ALevelScenery::StepScenery(float DeltaTime, const FVector& CatLoc)
{
	Time += DeltaTime;
	if (Layout.bStraight)
	{
		// periodische Kulisse mitziehen: beginnt knapp hinter der Kamera, die drei Kopien reichen weit voraus
		// (hinter der Kamera wird nichts mehr vorgehalten)
		SetActorLocation(FVector(FMath::FloorToFloat((CatLoc.X - 1500.f) / Period) * Period, 0.f, 0.f));
	}
	FVector Cam = FVector::ZeroVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			Cam = PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	const FVector Origin = GetActorLocation();
	// Bildtafeln zur Kamera drehen (nur um die Hochachse, bleiben senkrecht stehen)
	if (Sprites.Num() > 0 && FVector::DistSquared(Cam - Origin, LastCam) > 4.f)
	{
		LastCam = Cam - Origin;
		TSet<UInstancedStaticMeshComponent*> Dirty;
		for (const FSprite& S : Sprites)
		{
			if (!S.bFace)
			{
				continue;
			}
			const FVector ToCam = LastCam - S.Base;
			float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToCam.Y, ToCam.X));
			if (Layout.bStraight)
			{
				// Gerade: Tafeln bleiben fast parallel zum Bild (wie Kulissenwaende), statt sich beim Vorbeiziehen seitlich wegzudrehen
				Yaw = 180.f + FMath::Clamp(FMath::FindDeltaAngleDegrees(180.f, Yaw), -28.f, 28.f);
			}
			S.Ism->UpdateInstanceTransform(S.Instance, FTransform(FRotator(0.f, Yaw - 90.f, 90.f), S.Base + FVector(0.f, 0.f, S.Height * 0.5f), FVector(S.Width / 100.f, S.Height / 100.f, 1.f)), false, false, true);
			Dirty.Add(S.Ism);
		}
		for (UInstancedStaticMeshComponent* C : Dirty)
		{
			C->MarkRenderStateDirty();
		}
	}
	// langsames Treiben und "Atmen" der Nebelschwaden, Quads zur Kamera gedreht
	for (int32 I = 0; I < Fog.Num(); ++I)
	{
		const float Ph = FogPhase[I];
		const FVector Off(FMath::Sin(Time * 0.11f + Ph) * 120.f, FMath::Cos(Time * 0.09f + Ph * 1.7f) * 120.f, FMath::Sin(Time * 0.2f + Ph) * 12.f);
		const FVector P = FogBase[I] + Off;
		const FVector ToCam = Cam - (Origin + P);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(ToCam.Y, ToCam.X));
		const float Breath = 1.f + 0.06f * FMath::Sin(Time * 0.35f + Ph * 2.f);
		Fog[I]->SetRelativeLocationAndRotation(P, FRotator(0.f, Yaw - 90.f, 90.f));
		Fog[I]->SetRelativeScale3D(FVector(FogSize[I].X / 100.f * Breath, FogSize[I].Y / 100.f, 1.f));
	}
}
