#include "SlidingProp.h"
#include "RunTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

ASlidingProp::ASlidingProp()
{
	Kind = EHazardKind::Obstacle;
	HalfLength = 40.f;
	Height = 999.f; // zu hoch zum Springen
}

void ASlidingProp::Setup(UTexture2D* Tex, float InHeight, float CollisionFrac, float Flip)
{
	if (!Tex)
	{
		return;
	}
#if WITH_EDITOR
	FTextureCompilingManager::Get().FinishCompilation({ Tex });
#endif
	if (!Mat)
	{
		Mat = RunAssets::NewMID(TEXT("M_Sprite"), this);
	}
	if (Mat)
	{
		Mat->SetTextureParameterValue(TEXT("Tex"), Tex);
	}
	if (!Sprite)
	{
		Sprite = RunAssets::AddShape(this, Root, TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator(0.f, 90.f, 90.f), Mat);
	}
	Sprite->SetVisibility(true);
	if (Model)
	{
		Model->SetVisibility(false);
	}
	const float Aspect = Tex->GetSizeY() > 0 ? float(Tex->GetSizeX()) / float(Tex->GetSizeY()) : 1.f;
	const float Width = InHeight * Aspect;
	HalfWidth = Width * 0.5f * CollisionFrac;
	Sprite->SetRelativeLocation(FVector(0.f, 0.f, InHeight * 0.5f));
	Sprite->SetRelativeScale3D(FVector(Flip * Width / 100.f, InHeight / 100.f, 1.f));
}

void ASlidingProp::SetupModel(UStaticMesh* Mesh, float Len, float Depth, float InHeight)
{
	if (!Mesh)
	{
		return;
	}
	if (Sprite)
	{
		Sprite->SetVisibility(false);
	}
	if (!Model)
	{
		Model = NewObject<UStaticMeshComponent>(this);
		Model->SetupAttachment(Root);
		Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Model->SetCastShadow(false);
		Model->RegisterComponent();
	}
	Model->SetStaticMesh(Mesh);
	Model->SetVisibility(true);
	// Modell: Ursprung unten mittig, Laenge entlang X -> um 90 Grad gedreht, faehrt quer ueber die Gleise
	const FVector Size = Mesh->GetBoundingBox().GetSize();
	Model->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	Model->SetRelativeScale3D(FVector(Len / FMath::Max(1.f, Size.X), Depth / FMath::Max(1.f, Size.Y), InHeight / FMath::Max(1.f, Size.Z)));
	HalfWidth = Len * 0.5f * 0.9f;
	HalfLength = Depth * 0.5f;
	Height = InHeight;
}

void ASlidingProp::Launch(const FVector& Ground, float InA, float InLatMin, float InLatMax, float InSlideSpeed, float Phase)
{
	A = InA;
	Lane = 0;
	LatMin = FMath::Min(InLatMin, InLatMax);
	LatMax = FMath::Max(InLatMin, InLatMax);
	SlideSpeed = FMath::Max(10.f, InSlideSpeed);
	bOneWay = false;
	Offset = FMath::Frac(Phase) * 2.f * (LatMax - LatMin);
	Elapsed = 0.f;
	Base = Ground;
	Lat = LatAtTime(0.f);
	Activate(Base + FVector(0.f, Lat, 0.f), FRotator::ZeroRotator);
}

void ASlidingProp::LaunchCross(const FVector& Ground, float InA, float FromLat, float ToLat, float InSpeed, float ArriveTime, float AtLat)
{
	A = InA;
	Lane = 0;
	bOneWay = true;
	bHonked = false;
	CrossDir = ToLat >= FromLat ? 1.f : -1.f;
	LatMin = FromLat;
	LatMax = ToLat;
	SlideSpeed = FMath::Max(10.f, InSpeed);
	// Weg bis AtLat minus gefahrene Strecke bis ArriveTime; negativ = wartet noch am Rand
	Offset = FMath::Abs(AtLat - FromLat) - SlideSpeed * ArriveTime;
	Elapsed = 0.f;
	Base = Ground;
	Lat = LatAtTime(0.f);
	Activate(Base + FVector(0.f, Lat, 0.f), FRotator::ZeroRotator);
}

float ASlidingProp::LatAtTime(float Tm, float* OutDir) const
{
	if (bOneWay)
	{
		const float Span = FMath::Abs(LatMax - LatMin);
		const float P = FMath::Clamp(Offset + SlideSpeed * FMath::Max(0.f, Tm), 0.f, Span);
		if (OutDir)
		{
			*OutDir = P > 0.f && P < Span ? CrossDir : 0.f;
		}
		return LatMin + CrossDir * P;
	}
	const float Span = FMath::Max(1.f, LatMax - LatMin);
	const float P = FMath::Fmod(Offset + SlideSpeed * FMath::Max(0.f, Tm), 2.f * Span);
	if (OutDir)
	{
		*OutDir = P < Span ? 1.f : -1.f;
	}
	return P < Span ? LatMin + P : LatMax - (P - Span);
}

float ASlidingProp::PredictLat(float SecondsToCat, float Seconds) const
{
	return LatAtTime(Elapsed + Seconds);
}

void ASlidingProp::StepSlide(float DeltaTime, float SecondsToCat)
{
	if (!bLive)
	{
		return;
	}
	Elapsed += DeltaTime;
	float Dir = 0.f;
	Lat = LatAtTime(Elapsed, &Dir);
	// gleichmaessiges Gleiten, leicht in Fahrtrichtung geneigt; Auto wippt beim Fahren
	const float Bob = bOneWay && Dir != 0.f ? 4.f * FMath::Abs(FMath::Sin(Elapsed * 14.f)) : 0.f;
	SetActorLocationAndRotation(Base + FVector(0.f, Lat, Bob), FRotator(0.f, 0.f, Dir * 3.f));
}
