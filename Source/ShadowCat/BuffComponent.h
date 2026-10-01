#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RunTypes.h"
#include "BuffComponent.generated.h"

class UCatBuff;

/** Verwaltet die aktiven Buffs der Katze (mehrere gleichzeitig moeglich, gleiche Art wird erneuert). */
UCLASS(ClassGroup = (ShadowCat), meta = (BlueprintSpawnableComponent))
class SHADOWCAT_API UBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBuffComponent();

	UCatBuff* AddBuff(TSubclassOf<UCatBuff> BuffClass);
	/** Faengt ein aktiver Buff die Kollision ab? */
	bool TryAbsorb(EHazardKind Kind) const;
	/** Ist ein Buff dieser Klasse gerade aktiv? (z. B. Fluch der Gegner-Wuerfel) */
	bool HasBuff(TSubclassOf<UCatBuff> BuffClass) const;
	void StepBuffs(float DeltaTime);
	void ClearAll();

	const TArray<TObjectPtr<UCatBuff>>& GetActiveBuffs() const { return Active; }

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCatBuff>> Active;
};
