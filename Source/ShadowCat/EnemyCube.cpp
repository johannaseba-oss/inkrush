#include "EnemyCube.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AEnemyCube::AEnemyCube()
{
	Kind = EHazardKind::Enemy;
	HalfLength = 40.f;
	HalfWidth = 42.f;
}

void AEnemyCube::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	Body = NewObject<USceneComponent>(this);
	Body->SetupAttachment(Root);
	Body->RegisterComponent();

	const float S = 0.7f;           // Kantenlaenge 70 cm
	const float H = 50.f * S;       // halbe Kante
	UMaterialInterface* Black = RunAssets::Mono(0.02f, 0.f);
	UMaterialInterface* Edge = RunAssets::Glow(1.f, 2.4f);
	RunAssets::AddShape(this, Body, TEXT("Cube"), FVector::ZeroVector, FVector(S), FRotator::ZeroRotator, Black);

	// 12 leuchtende Kanten
	const float T = 0.07f;
	const float L = S + 0.04f;
	for (int32 SA = -1; SA <= 1; SA += 2)
	{
		for (int32 SB = -1; SB <= 1; SB += 2)
		{
			RunAssets::AddShape(this, Body, TEXT("Cube"), FVector(0.f, SA * H, SB * H), FVector(L, T, T), FRotator::ZeroRotator, Edge);
			RunAssets::AddShape(this, Body, TEXT("Cube"), FVector(SA * H, 0.f, SB * H), FVector(T, L, T), FRotator::ZeroRotator, Edge);
			RunAssets::AddShape(this, Body, TEXT("Cube"), FVector(SA * H, SB * H, 0.f), FVector(T, T, L), FRotator::ZeroRotator, Edge);
		}
	}
	// Auge auf der Vorderseite (-X, zur Katze)
	RunAssets::AddShape(this, Body, TEXT("Sphere"), FVector(-H - 1.f, 0.f, 4.f), FVector(0.05f, 0.34f, 0.2f), FRotator::ZeroRotator, RunAssets::Glow(1.f, 3.f));
	RunAssets::AddShape(this, Body, TEXT("Sphere"), FVector(-H - 3.f, 0.f, 4.f), FVector(0.04f, 0.1f, 0.17f), FRotator::ZeroRotator, Black);

	// Bodenmarkierung der Zielspur + weicher Schatten
	MarkerMat = RunAssets::NewMID(TEXT("M_Disc"), this);
	if (MarkerMat)
	{
		MarkerMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 1.f, 1.f));
		MarkerMat->SetScalarParameterValue(TEXT("Ring"), 1.f);
		MarkerMat->SetScalarParameterValue(TEXT("Intensity"), 1.5f);
	}
	Marker = RunAssets::AddShape(this, Root, TEXT("Plane"), FVector::ZeroVector, FVector(1.15f), FRotator::ZeroRotator, MarkerMat);
	UMaterialInstanceDynamic* ShadowMat = RunAssets::NewMID(TEXT("M_Disc"), this);
	if (ShadowMat)
	{
		ShadowMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.f, 0.f, 0.f));
		ShadowMat->SetScalarParameterValue(TEXT("Ring"), 0.f);
		ShadowMat->SetScalarParameterValue(TEXT("Opacity"), 0.7f);
	}
	Shadow = RunAssets::AddShape(this, Root, TEXT("Plane"), FVector::ZeroVector, FVector(0.9f), FRotator::ZeroRotator, ShadowMat);
	Retire();
}

void AEnemyCube::Place(const FVector& TargetLoc, const FVector& Forward, int32 InLane, float InA, bool bFromLeft)
{
	Lane = InLane;
	A = InA;
	Target = TargetLoc + FVector(0.f, 0.f, HoverZ);
	Ground = FVector(TargetLoc.X, TargetLoc.Y, 1.5f);
	Fwd = Forward.GetSafeNormal2D();
	const FVector Right(-Fwd.Y, Fwd.X, 0.f);
	// Wartet seitlich hoch ueber dem Rand, leicht voraus
	Start = TargetLoc + Fwd * 350.f + Right * (bFromLeft ? -1.f : 1.f) * 750.f + FVector(0.f, 0.f, 560.f);
	Flight = 0.f;
	bFlying = false;
	bDoomed = false;
	Time = FMath::FRand() * 5.f;
	Activate(Start, Fwd.Rotation());
	if (Marker)
	{
		Marker->SetWorldLocation(Ground);
	}
	if (Shadow)
	{
		Shadow->SetWorldLocation(Ground - FVector(0.f, 0.f, 0.5f));
		Shadow->SetVisibility(false);
	}
}

void AEnemyCube::StepEnemy(float DeltaTime, float DistAhead, float Speed, float FlyTime, float SettleLead)
{
	if (!bLive)
	{
		return;
	}
	Time += DeltaTime;
	if (!bFlying && DistAhead <= Speed * (FlyTime + SettleLead))
	{
		bFlying = true;
	}
	if (bFlying && Flight < 1.f)
	{
		Flight = FMath::Min(1.f, Flight + DeltaTime / FMath::Max(0.1f, FlyTime));
	}

	// Flugbahn: erst zur Spur hinueber, dann hinab (quadratische Bezierkurve)
	const float Ease = Flight * Flight * (3.f - 2.f * Flight);
	const FVector Ctrl = FVector(Target.X, Target.Y, Start.Z + 60.f) + Fwd * 120.f;
	const FVector P = FMath::Lerp(FMath::Lerp(Start, Ctrl, Ease), FMath::Lerp(Ctrl, Target, Ease), Ease);
	const float Bob = 10.f * FMath::Sin(Time * 3.4f);
	SetActorLocation(P + FVector(0.f, 0.f, Bob));
	if (Marker)
	{
		Marker->SetWorldLocation(Ground);
	}

	if (Body)
	{
		Body->SetRelativeRotation(FRotator(8.f * FMath::Sin(Time * 2.1f), 12.f * FMath::Sin(Time * 1.3f) + (1.f - Ease) * 180.f, 6.f * FMath::Sin(Time * 2.7f)));
	}
	if (MarkerMat)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 9.f);
		MarkerMat->SetScalarParameterValue(TEXT("Opacity"), bDoomed ? 0.f : (0.35f + 0.45f * Pulse) * (Flight >= 1.f ? 0.7f : 1.f));
	}
	if (Shadow)
	{
		Shadow->SetVisibility(Flight > 0.6f);
		Shadow->SetWorldLocation(Ground - FVector(0.f, 0.f, 0.5f));
	}
}

void AEnemyCube::Retire()
{
	Super::Retire();
	bDoomed = false;
	bFlying = false;
	Flight = 0.f;
}
