#include "HazardBase.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

AHazardBase::AHazardBase()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetActorEnableCollision(false);
}

void AHazardBase::Activate(const FVector& Location, const FRotator& Rotation)
{
	SetActorLocationAndRotation(Location, Rotation);
	SetActorHiddenInGame(false);
	bLive = true;
}

void AHazardBase::Retire()
{
	bLive = false;
	SetActorHiddenInGame(true);
	SetActorLocation(FVector(0.f, 0.f, -5000.f));
}

AObstacle::AObstacle()
{
	Kind = EHazardKind::Obstacle;
}

void AObstacle::Build(EObstacleType InType)
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	Type = InType;

	auto Add = [this](const TCHAR* Shape, const FVector& L, const FVector& S, const FRotator& R, UMaterialInterface* M)
	{
		return RunAssets::AddShape(this, Root, Shape, L, S, R, M);
	};
	// gezeichnetes Bild als senkrechte Tafel, Blick zur Katze (Tools/Import/Nature, CAT_SETUP=sprites)
	auto AddSprite = [&](const TCHAR* TexName, float W, float H)
	{
		UMaterialInstanceDynamic* Mat = RunAssets::NewMID(TEXT("M_Sprite"), this);
		const FString Path = FString::Printf(TEXT("/Game/Nature/%s.%s"), TexName, TexName);
		if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (Mat)
			{
				Mat->SetTextureParameterValue(TEXT("Tex"), Tex);
			}
		}
		Add(TEXT("Plane"), FVector(0.f, 0.f, H * 0.5f), FVector(W / 100.f, H / 100.f, 1.f), FRotator(0.f, 90.f, 90.f), Mat);
	};

	switch (Type)
	{
	case EObstacleType::Fence:
	{
		// Absperrung (3D, schlichte Formen): helle Latte mit dunklen Schraegstreifen auf zwei Pfosten -> drueberspringen
		HalfLength = 15.f;
		HalfWidth = 60.f;
		Height = 80.f;
		UMaterialInterface* Light = RunAssets::Mono(0.92f, 0.75f);
		UMaterialInterface* Dark = RunAssets::Mono(0.03f, 0.f);
		Add(TEXT("Cube"), FVector(0.f, 0.f, 62.f), FVector(0.1f, 1.3f, 0.3f), FRotator::ZeroRotator, Light);
		for (int32 I = -2; I <= 2; ++I)
		{
			Add(TEXT("Cube"), FVector(-6.f, I * 26.f, 62.f), FVector(0.02f, 0.09f, 0.34f), FRotator(0.f, 0.f, 35.f), Dark);
		}
		Add(TEXT("Cube"), FVector(0.f, 0.f, 30.f), FVector(0.08f, 1.2f, 0.1f), FRotator::ZeroRotator, Light);
		for (const float Y : { -56.f, 56.f })
		{
			Add(TEXT("Cube"), FVector(0.f, Y, 40.f), FVector(0.12f, 0.1f, 0.8f), FRotator::ZeroRotator, Dark);
			Add(TEXT("Cube"), FVector(0.f, Y, 3.f), FVector(0.4f, 0.16f, 0.06f), FRotator::ZeroRotator, Dark);
		}
		break;
	}

	case EObstacleType::Crate:
	{
		// Kiste aus den Natur-Bildern (Holzkiste oder Karton, zufaellig) -> drueberspringen oder ausweichen
		HalfLength = 30.f;
		HalfWidth = 58.f;
		Height = 108.f;
		if (FMath::RandBool())
		{
			AddSprite(TEXT("T_Sprite_Crate"), 138.f, 108.f);
		}
		else
		{
			AddSprite(TEXT("T_Sprite_CardboardBox"), 108.f * 639.f / 703.f, 108.f);
		}
		break;
	}

	case EObstacleType::Lamp:
		// Laterne (Bild 256x925): zu hoch zum Springen -> ausweichen
		HalfLength = 20.f;
		HalfWidth = 36.f;
		Height = 999.f;
		AddSprite(TEXT("T_Sprite_StreetLamp"), 110.f, 400.f);
		break;

	case EObstacleType::Pit:
	{
		// Echtes Loch: die Fahrbahn-Kacheln darueber blendet der Director aus (AInkCanvas::SetHole). Hier nur der Schacht:
		// Waende oben hell, nach unten dunkel, schwarzer Grund in 5 m Tiefe, helle Kanten am Rand.
		HalfLength = 150.f;
		HalfWidth = 50.f;
		Height = 5.f;
		UMaterialInterface* WallTop = RunAssets::Mono(0.62f, 0.4f);
		UMaterialInterface* WallLow = RunAssets::Mono(0.1f, 0.05f);
		UMaterialInterface* Rim = RunAssets::Mono(0.95f, 0.85f);
		// hintere Wand (Innenseite zur Katze), zwei Baender
		PitParts.Add(Add(TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator(90.f, 0.f, 0.f), WallTop));
		PitParts.Add(Add(TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator(90.f, 0.f, 0.f), WallLow));
		// Seitenwaende (Innenseiten)
		PitParts.Add(Add(TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator(0.f, 0.f, -90.f), WallTop));
		PitParts.Add(Add(TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator(0.f, 0.f, -90.f), WallLow));
		PitParts.Add(Add(TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator(0.f, 0.f, 90.f), WallTop));
		PitParts.Add(Add(TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator(0.f, 0.f, 90.f), WallLow));
		// Grund und Kanten
		PitParts.Add(Add(TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, RunAssets::Glow(0.f, 0.f)));
		PitParts.Add(Add(TEXT("Cube"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, Rim));
		PitParts.Add(Add(TEXT("Cube"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, Rim));
		SetPitLength(300.f);
		break;
	}

	case EObstacleType::Beam:
	{
		// Balken zum Drunterkriechen: helle Tafel mit dunklen Warnstreifen zwischen zwei Pfosten, Unterkante 75 cm
		HalfLength = 20.f;
		HalfWidth = 60.f;
		Height = 999.f;
		UMaterialInterface* Light = RunAssets::Mono(0.9f, 0.7f);
		UMaterialInterface* Dark = RunAssets::Mono(0.03f, 0.f);
		Add(TEXT("Cube"), FVector(0.f, 0.f, 128.f), FVector(0.12f, 1.36f, 1.06f), FRotator::ZeroRotator, Light);
		for (int32 I = -2; I <= 2; ++I)
		{
			Add(TEXT("Cube"), FVector(-7.f, I * 28.f, 128.f), FVector(0.02f, 0.1f, 1.2f), FRotator(0.f, 0.f, 35.f), Dark);
		}
		Add(TEXT("Cylinder"), FVector(0.f, -66.f, 90.f), FVector(0.1f, 0.1f, 1.8f), FRotator::ZeroRotator, Dark);
		Add(TEXT("Cylinder"), FVector(0.f, 66.f, 90.f), FVector(0.1f, 0.1f, 1.8f), FRotator::ZeroRotator, Dark);
		break;
	}
	}
	Retire();
}

void AObstacle::SetPitLength(float Length)
{
	if (Type != EObstacleType::Pit || PitParts.Num() < 9)
	{
		return;
	}
	HalfLength = Length * 0.5f;
	const float W = 140.f;      // Fahrbahnbreite
	const float Top = 70.f;     // helles Band
	const float Depth = 500.f;  // Tiefe bis zum Grund
	const float Low = Depth - Top;
	// Plane: 100 x 100 in XY; nach Pitch 90 zeigt die Flaeche zur Katze (-X), lokale X-Achse = Hoehe
	PitParts[0]->SetRelativeLocationAndRotation(FVector(HalfLength, 0.f, -Top * 0.5f), FRotator(90.f, 0.f, 0.f));
	PitParts[0]->SetRelativeScale3D(FVector(Top / 100.f, W / 100.f, 1.f));
	PitParts[1]->SetRelativeLocationAndRotation(FVector(HalfLength, 0.f, -Top - Low * 0.5f), FRotator(90.f, 0.f, 0.f));
	PitParts[1]->SetRelativeScale3D(FVector(Low / 100.f, W / 100.f, 1.f));
	// Seitenwaende: Roll -90 -> Flaeche zeigt nach +Y (linke Wand), Roll 90 -> nach -Y (rechte Wand); lokale Y = Hoehe
	PitParts[2]->SetRelativeLocation(FVector(0.f, -W * 0.5f, -Top * 0.5f));
	PitParts[2]->SetRelativeScale3D(FVector(Length / 100.f, Top / 100.f, 1.f));
	PitParts[3]->SetRelativeLocation(FVector(0.f, -W * 0.5f, -Top - Low * 0.5f));
	PitParts[3]->SetRelativeScale3D(FVector(Length / 100.f, Low / 100.f, 1.f));
	PitParts[4]->SetRelativeLocation(FVector(0.f, W * 0.5f, -Top * 0.5f));
	PitParts[4]->SetRelativeScale3D(FVector(Length / 100.f, Top / 100.f, 1.f));
	PitParts[5]->SetRelativeLocation(FVector(0.f, W * 0.5f, -Top - Low * 0.5f));
	PitParts[5]->SetRelativeScale3D(FVector(Length / 100.f, Low / 100.f, 1.f));
	PitParts[6]->SetRelativeLocation(FVector(0.f, 0.f, -Depth));
	PitParts[6]->SetRelativeScale3D(FVector(Length / 100.f, W / 100.f, 1.f));
	// helle Kanten vorn und hinten (auch auf schwarz gefaerbten Fahrbahnen sichtbar)
	PitParts[7]->SetRelativeLocation(FVector(-HalfLength, 0.f, -2.f));
	PitParts[7]->SetRelativeScale3D(FVector(0.08f, W / 100.f, 0.06f));
	PitParts[8]->SetRelativeLocation(FVector(HalfLength, 0.f, -2.f));
	PitParts[8]->SetRelativeScale3D(FVector(0.08f, W / 100.f, 0.06f));
	SetPitSideWalls(true, true);
	SetPitEnds(true, true);
}

void AObstacle::SetPitEnds(bool bNear, bool bFar)
{
	if (Type != EObstacleType::Pit || PitParts.Num() < 9)
	{
		return;
	}
	// [0],[1] = hintere Wand, [7] = vordere Kante, [8] = hintere Kante
	PitParts[0]->SetVisibility(bFar);
	PitParts[1]->SetVisibility(bFar);
	PitParts[8]->SetVisibility(bFar);
	PitParts[7]->SetVisibility(bNear);
}

void AObstacle::SetPitSideWalls(bool bLeft, bool bRight)
{
	if (Type != EObstacleType::Pit || PitParts.Num() < 9)
	{
		return;
	}
	// [2],[3] = Wand zur Seite -Y, [4],[5] = Wand zur Seite +Y
	PitParts[2]->SetVisibility(bLeft);
	PitParts[3]->SetVisibility(bLeft);
	PitParts[4]->SetVisibility(bRight);
	PitParts[5]->SetVisibility(bRight);
}

void AObstacle::Place(const FVector& Location, const FRotator& Rotation, int32 InLane, float InA)
{
	Lane = InLane;
	A = InA;
	Activate(Location, Rotation);
}
