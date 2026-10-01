#include "ShardBurst.h"
#include "RunTypes.h"
#include "Components/StaticMeshComponent.h"

static constexpr int32 NumShards = 8;
static constexpr float BurstLife = 0.7f;

AShardBurst::AShardBurst()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetActorEnableCollision(false);
}

void AShardBurst::Fire(const FVector& Location, float Gray, bool bGlow, float Size)
{
	if (Shards.Num() == 0)
	{
		for (int32 I = 0; I < NumShards; ++I)
		{
			Shards.Add(RunAssets::AddShape(this, Root, TEXT("Cube"), FVector::ZeroVector, FVector(0.1f), FRotator::ZeroRotator, nullptr));
		}
		Vel.SetNum(NumShards);
		Spin.SetNum(NumShards);
	}
	UMaterialInterface* Mat = bGlow ? static_cast<UMaterialInterface*>(RunAssets::Glow(Gray, 2.f)) : static_cast<UMaterialInterface*>(RunAssets::Mono(Gray, 0.3f));
	Scale = Size;
	for (int32 I = 0; I < NumShards; ++I)
	{
		Shards[I]->SetMaterial(0, I % 2 == 0 && bGlow ? RunAssets::Mono(0.02f, 0.f) : Mat);
		Shards[I]->SetRelativeLocation(FMath::VRand() * 15.f * Size);
		Vel[I] = FVector(FMath::FRandRange(-150.f, 450.f), FMath::FRandRange(-420.f, 420.f), FMath::FRandRange(250.f, 650.f)) * Size;
		Spin[I] = FRotator(FMath::FRandRange(-700.f, 700.f), FMath::FRandRange(-700.f, 700.f), 0.f);
	}
	SetActorLocation(Location);
	SetActorHiddenInGame(false);
	Life = BurstLife;
	StepBurst(0.f);
}

void AShardBurst::StepBurst(float DeltaTime)
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
	const float K = Life / BurstLife;
	for (int32 I = 0; I < Shards.Num(); ++I)
	{
		Vel[I].Z -= 1400.f * DeltaTime;
		Shards[I]->AddRelativeLocation(Vel[I] * DeltaTime);
		Shards[I]->AddRelativeRotation(Spin[I] * DeltaTime);
		Shards[I]->SetRelativeScale3D(FVector(0.16f * Scale * K));
	}
}
