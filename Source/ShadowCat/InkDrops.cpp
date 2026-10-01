#include "InkDrops.h"
#include "RunTypes.h"
#include "Components/StaticMeshComponent.h"

static constexpr int32 NumDrops = 12;
static constexpr float DropLife = 0.45f;

AInkDrops::AInkDrops()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetActorEnableCollision(false);
}

void AInkDrops::Fire(const FVector& Loc, const FVector& Forward, float Size)
{
	if (Drops.Num() == 0)
	{
		// tiefschwarze, leicht glaenzende Tropfen (wie die Katze)
		UMaterialInterface* Ink = RunAssets::Mono(0.02f, 0.f);
		for (int32 I = 0; I < NumDrops; ++I)
		{
			Drops.Add(RunAssets::AddShape(this, Root, TEXT("Sphere"), FVector::ZeroVector, FVector(0.1f), FRotator::ZeroRotator, Ink));
		}
		Vel.SetNum(NumDrops);
		Radius.SetNum(NumDrops);
	}
	Scale = Size;
	const FVector F = Forward.GetSafeNormal2D();
	const FVector R(-F.Y, F.X, 0.f);
	for (int32 I = 0; I < NumDrops; ++I)
	{
		// Kranz um die Fuesse: seitlich und nach oben, nach hinten abgebremst (die Katze laeuft weiter)
		const float Ang = (I + FMath::FRandRange(-0.3f, 0.3f)) / NumDrops * 2.f * PI;
		const FVector Out = R * FMath::Cos(Ang) + F * FMath::Sin(Ang) * 0.6f;
		Drops[I]->SetRelativeLocation(Out * 12.f * Size + FVector(0.f, 0.f, 4.f));
		Vel[I] = (Out * FMath::FRandRange(180.f, 360.f) - F * FMath::FRandRange(80.f, 220.f) + FVector(0.f, 0.f, FMath::FRandRange(260.f, 520.f))) * Size;
		Radius[I] = FMath::FRandRange(0.05f, 0.11f) * Size;
		Drops[I]->SetVisibility(true);
	}
	SetActorLocation(Loc);
	SetActorHiddenInGame(false);
	Life = DropLife;
	StepDrops(0.f);
}

void AInkDrops::StepDrops(float DeltaTime)
{
	if (Life <= 0.f)
	{
		return;
	}
	Life -= DeltaTime;
	if (Life <= 0.f)
	{
		SetActorHiddenInGame(true);
		return;
	}
	const float K = FMath::Clamp(Life / DropLife, 0.f, 1.f);
	for (int32 I = 0; I < Drops.Num(); ++I)
	{
		Vel[I].Z -= 1500.f * DeltaTime;
		Drops[I]->AddRelativeLocation(Vel[I] * DeltaTime);
		// Tropfenform: in Flugrichtung etwas gestreckt, zum Ende kleiner
		const float S = Radius[I] * (0.4f + 0.6f * K);
		Drops[I]->SetRelativeScale3D(FVector(S, S, S * 1.4f));
	}
}
