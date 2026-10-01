#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CircuitLayout.h"
#include "LaneMovementComponent.generated.h"

/**
 * Fahrbahnwahl ueber alle Kurse: globale Fahrbahn G (0 = innen). Ein Wisch verschiebt um genau eine Fahrbahn;
 * ueber die Kursgrenze hinaus nur, wenn bAllowCircuitChange gesetzt ist (Verbindungsstelle).
 */
UCLASS(ClassGroup = (ShadowCat), meta = (BlueprintSpawnableComponent))
class SHADOWCAT_API ULaneMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULaneMovementComponent();

	/** Dauer eines Wechsels um eine Fahrbahn (Kurswechsel dauern etwas laenger). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spuren")
	float SwitchDuration = 0.15f;

	void SetLayout(const FCircuitLayout& InLayout) { Layout = InLayout; }
	const FCircuitLayout& GetLayout() const { return Layout; }

	void ResetLanes(int32 StartLane);
	/** -1 = links (innen), +1 = rechts (aussen). Ergebnis: 0 = nicht moeglich, 1 = Fahrbahn, 2 = Kurswechsel. */
	int32 RequestShift(int32 Dir, bool bAllowCircuitChange);
	void StepLanes(float DeltaTime);

	float GetLateralOffset() const { return CurY; }
	float GetLateralVelocityNorm() const { return LateralVel; }
	int32 GetTargetLane() const { return TargetLane; }
	/** Fahrbahn, auf der die Katze gerade tatsaechlich ist (naechste zur aktuellen Position). */
	int32 GetCurrentLane() const { return Layout.NearestLane(CurY); }
	int32 GetTargetCircuit() const { return Layout.CircuitOf(TargetLane); }
	float LaneToY(int32 G) const { return Layout.LaneLat(G); }
	bool IsSwitching() const { return T < 1.f; }

private:
	FCircuitLayout Layout;
	int32 TargetLane = 1;
	float FromY = 0.f;
	float CurY = 0.f;
	float T = 1.f;
	float Duration = 0.15f;
	float LateralVel = 0.f;
};
