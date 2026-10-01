#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InkDrops.generated.h"

class UStaticMeshComponent;

/**
 * Kurzer Spritzer aus kleinen schwarzen Tintentropfen (Absprung/Landung der Katze). Wird gepoolt: Fire startet ihn
 * neu, StepDrops bewegt die Tropfen (Schwerkraft, schrumpfen) bis sie nach ~0.45 s verschwinden.
 */
UCLASS()
class SHADOWCAT_API AInkDrops : public AActor
{
	GENERATED_BODY()

public:
	AInkDrops();

	/** Loc = Fusspunkt, Forward = Laufrichtung (Tropfen bleiben etwas zurueck), Size 1 = Absprung, ~0.6 = Landung. */
	void Fire(const FVector& Loc, const FVector& Forward, float Size);
	void StepDrops(float DeltaTime);
	bool IsBusy() const { return Life > 0.f; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Drops;

	TArray<FVector> Vel;
	TArray<float> Radius;
	float Life = 0.f;
	float Scale = 1.f;
};
