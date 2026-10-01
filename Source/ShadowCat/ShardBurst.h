#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShardBurst.generated.h"

class UStaticMeshComponent;

/** Sparsamer Splitter-Effekt (8 kleine Wuerfel, kein Partikelsystem). Gepoolt. */
UCLASS()
class SHADOWCAT_API AShardBurst : public AActor
{
	GENERATED_BODY()

public:
	AShardBurst();

	/** Gray = Farbe der Splitter, bGlow = leuchtend (Gegner) statt Stein. */
	void Fire(const FVector& Location, float Gray, bool bGlow, float Size = 1.f);
	void StepBurst(float DeltaTime);
	bool IsBusy() const { return Life > 0.f; }
	void ShiftX(float Dx) { AddActorWorldOffset(FVector(Dx, 0.f, 0.f)); }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Shards;

	TArray<FVector> Vel;
	TArray<FRotator> Spin;
	float Life = 0.f;
	float Scale = 1.f;
};
