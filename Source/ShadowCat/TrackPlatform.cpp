#include "TrackPlatform.h"
#include "RunTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

ATrackPlatform::ATrackPlatform()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetActorEnableCollision(false);
}

UStaticMeshComponent* ATrackPlatform::AddPart(const TCHAR* Shape, const FVector& Loc, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat)
{
	// Size in cm (Grundformen sind 100 cm gross)
	UStaticMeshComponent* C = RunAssets::AddShape(this, Root, Shape, Loc, Size / 100.f, Rot, Mat);
	Extras.Add(C);
	return C;
}

void ATrackPlatform::ClearExtras()
{
	for (UStaticMeshComponent* C : Extras)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	Extras.Reset();
}

void ATrackPlatform::BuildRamp(float W, float H)
{
	RampTop->SetVisibility(HasRamp());
	RampFill->SetVisibility(HasRamp());
	if (!HasRamp())
	{
		return;
	}
	const float Len = FMath::Sqrt(RampLen * RampLen + H * H);
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(H, RampLen));
	RampTop->SetRelativeLocationAndRotation(FVector(RampLen * 0.5f, 0.f, H * 0.5f - 3.f), FRotator(Pitch, 0.f, 0.f));
	RampTop->SetRelativeScale3D(FVector(Len / 100.f, W / 100.f, 0.08f));
	// Keil: niedriger Quader unter dem oberen Teil der Rampe (verdeckt die Luecke unter der Platte)
	// (von 0.5 bis 1.0 der Rampe, bis halbe Hoehe: bleibt ueberall unter der Rampenflaeche)
	RampFill->SetRelativeLocation(FVector(RampLen * 0.75f, 0.f, H * 0.25f));
	RampFill->SetRelativeScale3D(FVector(RampLen * 0.5f / 100.f, (W - 6.f) / 100.f, H * 0.5f / 100.f));
}

void ATrackPlatform::Setup(int32 InLane, float InA0, float InRampLen, float InPlateauLen, float InHeight, float LaneLat, float LaneWidth)
{
	if (!Body)
	{
		// Seiten dunkel, Oberflaeche hell wie die Fahrbahn, helle Vorderkante (auch auf Tinte sichtbar)
		UMaterialInterface* Side = RunAssets::Mono(0.22f, 0.12f);
		UMaterialInterface* Top = RunAssets::Mono(0.86f, 0.55f);
		RampTop = RunAssets::AddShape(this, Root, TEXT("Cube"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, Top);
		RampFill = RunAssets::AddShape(this, Root, TEXT("Cube"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, Side);
		Body = RunAssets::AddShape(this, Root, TEXT("Cube"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, Side);
		Cap = RunAssets::AddShape(this, Root, TEXT("Cube"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, Top);
		FrontRim = RunAssets::AddShape(this, Root, TEXT("Cube"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, RunAssets::Mono(0.97f, 0.9f));
	}
	ClearExtras();
	Kind = EPlatformKind::Terrain;
	Span = 1;
	Speed = 0.f;
	Lane = InLane;
	LaneLatitude = LaneLat;
	A0 = InA0;
	RampLen = FMath::Max(0.f, InRampLen);
	Height = InHeight;
	A1 = A0 + RampLen + InPlateauLen;
	const float W = LaneWidth - 4.f;
	const float P0 = RampLen;             // Plateau beginnt (lokal)
	const float PL = InPlateauLen;
	SetActorLocation(FVector(A0, LaneLat, 0.f));
	// Plateau: dunkler Koerper + helle Deckschicht
	Body->SetVisibility(true);
	Cap->SetVisibility(true);
	FrontRim->SetVisibility(true);
	Body->SetRelativeLocation(FVector(P0 + PL * 0.5f, 0.f, (Height - 8.f) * 0.5f));
	Body->SetRelativeScale3D(FVector(PL / 100.f, W / 100.f, (Height - 8.f) / 100.f));
	Cap->SetRelativeLocation(FVector(P0 + PL * 0.5f, 0.f, Height - 4.f));
	Cap->SetRelativeScale3D(FVector(PL / 100.f, W / 100.f, 0.08f));
	FrontRim->SetRelativeLocation(FVector(P0 + 2.f, 0.f, Height - (HasRamp() ? 3.f : Height * 0.5f)));
	FrontRim->SetRelativeScale3D(FVector(0.06f, W / 100.f, HasRamp() ? 0.06f : Height / 100.f + 0.02f));
	BuildRamp(W, Height);
	SetActorHiddenInGame(false);
	bLive = true;
}

namespace
{
	// Masse im Spiel (cm): Zuege zwei Gleise breit, Modelle gleichmaessig skaliert (Original-Proportionen).
	// Modell-Proportionen (Laenge 1 : Breite : Hoehe): Lok 1 : 0.742 : 0.855, Waggon 1 : 0.554 : 0.774
	constexpr float Wide = 1.f;
	constexpr float Flat = 1.f;
	constexpr float TrainWidth = 266.f;
	constexpr float LocoLen = TrainWidth / (0.742f * Wide);   // ~358
	constexpr float LocoHeight = LocoLen * 0.855f * Flat;    // ~306
	constexpr float CarLen = TrainWidth / (0.554f * Wide);    // ~480
	constexpr float CarHeight = CarLen * 0.774f * Flat;      // ~372
	constexpr float Gap = 18.f;
}

float ATrackPlatform::TrainLength(bool bLoco, int32 Cars)
{
	const int32 N = FMath::Max(0, Cars);
	return (bLoco ? LocoLen + Gap : 0.f) + N * CarLen + FMath::Max(0, N - 1) * Gap;
}

UStaticMeshComponent* ATrackPlatform::AddModel(UStaticMesh* Mesh, float X0, float Len, float W, float H, bool bFlip)
{
	// Modell (Ursprung unten mittig, Front -X): gleichmaessig auf die Laenge skaliert, von X0 bis X0 + Len
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Root);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	const FVector Size = Mesh->GetBoundingBox().GetSize();
	const float S = Len / FMath::Max(1.f, Size.X);
	C->SetRelativeLocation(FVector(X0 + Len * 0.5f, 0.f, 0.f));
	C->SetRelativeRotation(FRotator(0.f, bFlip ? 180.f : 0.f, 0.f));
	C->SetRelativeScale3D(FVector(S, S * Wide, S * Flat));
	C->RegisterComponent();
	Extras.Add(C);
	return C;
}

void ATrackPlatform::SetupTrain(int32 InLane, float InA0, bool bLoco, int32 Cars, float InSpeed, float LaneLat, float LaneWidth)
{
	const float Len = TrainLength(bLoco, Cars);
	// Grundteile (unsichtbar) und Masse; Waggon-Dach als Oberkante (draufspringen moeglich), Lok etwas niedriger
	Setup(InLane, InA0, 0.f, Len, Cars > 0 ? CarHeight : LocoHeight, LaneLat, LaneWidth * 2.f);
	Kind = EPlatformKind::Train;
	Span = 2;
	Speed = InSpeed;
	bSounded = false;
	LocoPart = bLoco ? LocoLen + Gap * 0.5f : 0.f;
	LocoTop = LocoHeight;
	Body->SetVisibility(false);
	Cap->SetVisibility(false);
	FrontRim->SetVisibility(false);

	static UStaticMesh* LocoMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/SM_TrainLoco.SM_TrainLoco"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	static UStaticMesh* CarMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/SM_TrainCar.SM_TrainCar"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	float X = 0.f;
	if (bLoco)
	{
		if (LocoMesh)
		{
			AddModel(LocoMesh, X, LocoLen, TrainWidth, LocoHeight, false);
		}
		else
		{
			AddPart(TEXT("Cube"), FVector(LocoLen * 0.5f, 0.f, LocoHeight * 0.5f), FVector(LocoLen, TrainWidth, LocoHeight), FRotator::ZeroRotator, RunAssets::Mono(0.05f, 0.f));
		}
		if (Speed > 0.f)
		{
			// fahrende Lok: Scheinwerfer vorn
			AddPart(TEXT("Sphere"), FVector(-6.f, 0.f, LocoHeight * 0.42f), FVector(26.f, 26.f, 26.f), FRotator::ZeroRotator, RunAssets::Glow(1.f, 3.f));
		}
		X += LocoLen + Gap;
	}
	for (int32 K = 0; K < Cars; ++K)
	{
		if (CarMesh)
		{
			AddModel(CarMesh, X, CarLen, TrainWidth, CarHeight, false);
		}
		else
		{
			AddPart(TEXT("Cube"), FVector(X + CarLen * 0.5f, 0.f, CarHeight * 0.5f), FVector(CarLen, TrainWidth, CarHeight), FRotator::ZeroRotator, RunAssets::Mono(0.1f, 0.f));
		}
		X += CarLen + Gap;
	}
}
void ATrackPlatform::StepPlatform(float DeltaTime)
{
	if (!bLive || Speed <= 0.f)
	{
		return;
	}
	const float D = Speed * DeltaTime;
	A0 -= D;
	A1 -= D;
	SetActorLocation(FVector(A0, LaneLatitude, 0.f));
}

void ATrackPlatform::Retire()
{
	bLive = false;
	Speed = 0.f;
	SetActorHiddenInGame(true);
	SetActorLocation(FVector(0.f, 0.f, -5000.f));
}

float ATrackPlatform::HeightAt(float InA) const
{
	if (!bLive || InA < A0 || InA > A1)
	{
		return 0.f;
	}
	const float X = InA - A0;
	if (Kind == EPlatformKind::Train && X < LocoPart)
	{
		return LocoTop;
	}
	if (X < RampLen)
	{
		return Height * X / RampLen;
	}
	return Height;
}
