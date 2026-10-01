#include "TrackPlatform.h"
#include "RunTypes.h"
#include "Components/StaticMeshComponent.h"

ATrackPlatform::ATrackPlatform()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetActorEnableCollision(false);
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
	Lane = InLane;
	A0 = InA0;
	RampLen = FMath::Max(0.f, InRampLen);
	Height = InHeight;
	A1 = A0 + RampLen + InPlateauLen;
	const float W = LaneWidth - 4.f;
	const float P0 = RampLen;             // Plateau beginnt (lokal)
	const float PL = InPlateauLen;
	SetActorLocation(FVector(A0, LaneLat, 0.f));
	// Plateau: dunkler Koerper + helle Deckschicht
	Body->SetRelativeLocation(FVector(P0 + PL * 0.5f, 0.f, (Height - 8.f) * 0.5f));
	Body->SetRelativeScale3D(FVector(PL / 100.f, W / 100.f, (Height - 8.f) / 100.f));
	Cap->SetRelativeLocation(FVector(P0 + PL * 0.5f, 0.f, Height - 4.f));
	Cap->SetRelativeScale3D(FVector(PL / 100.f, W / 100.f, 0.08f));
	FrontRim->SetRelativeLocation(FVector(P0 + 2.f, 0.f, Height - (HasRamp() ? 3.f : Height * 0.5f)));
	FrontRim->SetRelativeScale3D(FVector(0.06f, W / 100.f, HasRamp() ? 0.06f : Height / 100.f + 0.02f));
	// Rampe: schraege helle Platte + dunkler Keil darunter (als flacher gedrehter Quader)
	RampTop->SetVisibility(HasRamp());
	RampFill->SetVisibility(HasRamp());
	if (HasRamp())
	{
		const float Len = FMath::Sqrt(RampLen * RampLen + Height * Height);
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Height, RampLen));
		RampTop->SetRelativeLocationAndRotation(FVector(RampLen * 0.5f, 0.f, Height * 0.5f - 3.f), FRotator(Pitch, 0.f, 0.f));
		RampTop->SetRelativeScale3D(FVector(Len / 100.f, W / 100.f, 0.08f));
		// Keil: niedriger Quader unter dem oberen Teil der Rampe (verdeckt die Luecke unter der Platte)
		// (von 0.5 bis 1.0 der Rampe, bis halbe Hoehe: bleibt ueberall unter der Rampenflaeche)
		RampFill->SetRelativeLocation(FVector(RampLen * 0.75f, 0.f, Height * 0.25f));
		RampFill->SetRelativeScale3D(FVector(RampLen * 0.5f / 100.f, (W - 6.f) / 100.f, Height * 0.5f / 100.f));
	}
	SetActorHiddenInGame(false);
	bLive = true;
}

void ATrackPlatform::Retire()
{
	bLive = false;
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
	if (X < RampLen)
	{
		return Height * X / RampLen;
	}
	return Height;
}
