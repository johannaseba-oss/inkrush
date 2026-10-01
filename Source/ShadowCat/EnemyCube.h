#pragma once

#include "CoreMinimal.h"
#include "HazardBase.h"
#include "EnemyCube.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Schwebender Wuerfel-Gegner: wartet seitlich hoch ueber der Strecke, fliegt rechtzeitig auf seine Spur
 * und schwebt dort. Schwarz mit leuchtenden Kanten und Auge; eine pulsierende Bodenmarkierung zeigt die Zielspur.
 * Gefaehrlich erst, wenn er ueber seiner Spur angekommen ist (fairer Vorlauf).
 */
UCLASS()
class SHADOWCAT_API AEnemyCube : public AHazardBase
{
	GENERATED_BODY()

public:
	AEnemyCube();

	void Build();
	/** TargetLoc = Bodenpunkt auf der Fahrbahn, Forward = Laufrichtung dort. */
	void Place(const FVector& TargetLoc, const FVector& Forward, int32 InLane, float InA, bool bFromLeft);
	/** Bewegt den Wuerfel abhaengig vom Abstand der Katze (DistAhead > 0 = Wuerfel liegt vor ihr). */
	void StepEnemy(float DeltaTime, float DistAhead, float Speed, float FlyTime, float SettleLead);

	virtual bool IsDangerous() const override { return bLive && !bDoomed && Flight >= 0.85f; }
	virtual void Retire() override;

	bool IsTargetable() const { return bLive && !bDoomed; }
	bool IsDoomed() const { return bDoomed; }
	/** Wird gleich von einem Effekt besiegt: ab sofort harmlos. */
	void MarkDoomed() { bDoomed = true; }

	static constexpr float HoverZ = 95.f;

private:
	FVector Target = FVector::ZeroVector;
	FVector Ground = FVector::ZeroVector;
	FVector Fwd = FVector::ForwardVector;
	FVector Start = FVector::ZeroVector;
	float Flight = 0.f;
	bool bFlying = false;
	bool bDoomed = false;
	float Time = 0.f;
	bool bBuilt = false;

	UPROPERTY()
	TObjectPtr<USceneComponent> Body;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Marker;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Shadow;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MarkerMat;
};
