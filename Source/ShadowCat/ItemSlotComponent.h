#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CatBuff.h"
#include "ItemSlotComponent.generated.h"

class UCatBuff;
class UTexture2D;
struct FItemSpawnEntry;

/**
 * Item-Vorrat mit Stapeln: jede Item-Sorte (Tintenbombe, Tintenwolke, ...) hat einen Zaehler (bis MaxStack).
 * Fragezeichen-Box -> kurze Slot-Machine-Auslosung -> Zaehler der ausgelosten Sorte +1. Einsetzen per Antippen des
 * jeweiligen Symbols (UseItem(Index)); gleiches Item mehrfach einsetzen verlaengert bzw. wiederholt die Wirkung.
 */
UCLASS(ClassGroup = (ShadowCat), meta = (BlueprintSpawnableComponent))
class SHADOWCAT_API UItemSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemSlotComponent();

	/** Dauer der Slot-Machine-Animation. */
	UPROPERTY(EditAnywhere, Category = "Item")
	float RollDuration = 0.8f;

	/** Hoechstens so viele Items pro Sorte. */
	UPROPERTY(EditAnywhere, Category = "Item")
	int32 MaxStack = 9;

	void SetPool(const TArray<FItemSpawnEntry>& InPool);
	/** Box beruehrt: startet die Auslosung (laeuft schon eine, wird sofort gelost). */
	bool TryCollect();
	/** Item der Sorte Index einsetzen (aktiviert den Buff an der Katze). */
	UCatBuff* UseItem(int32 Index);
	/** Irgendein vorhandenes Item einsetzen (Tastatur, Test-Autopilot). */
	UCatBuff* UseAny();
	void StepSlot(float DeltaTime);
	void Clear();
	/** Direkt N Items einer Sorte gutschreiben (Shop: Start-Bomben). */
	void GrantClass(TSubclassOf<UCatBuff> BuffClass, int32 N);

	int32 NumKinds() const { return PoolClasses.Num(); }
	int32 GetCount(int32 Index) const { return Counts.IsValidIndex(Index) ? Counts[Index] : 0; }
	UTexture2D* GetIcon(int32 Index) const;
	FText GetName(int32 Index) const;
	bool HasItem() const;
	bool IsEmpty() const { return !HasItem() && !bRolling; }
	bool IsRolling() const { return bRolling; }
	/** Waehrend der Auslosung wechselndes Symbol/Name. */
	UTexture2D* GetRollIcon() const { return bRolling ? GetIcon(ShowIndex) : nullptr; }
	/** Name fuer Meldungen/Log: waehrend der Auslosung wechselnd, sonst erstes vorhandenes Item. */
	FText GetDisplayName() const;
	/** Sorte der gerade beendeten Auslosung (einmal abholen), sonst INDEX_NONE. */
	int32 ConsumeRollResult() { const int32 R = LastResult; LastResult = INDEX_NONE; return R; }

	/** Test: naechste Auslosung liefert diesen Pool-Eintrag. */
	int32 ForcedResult = INDEX_NONE;

private:
	int32 PickRandom();
	void Grant(int32 Index);

	UPROPERTY(Transient)
	TArray<TSubclassOf<UCatBuff>> PoolClasses;

	TArray<float> PoolWeights;
	TArray<int32> Counts;

	bool bRolling = false;
	float RollTime = 0.f;
	float TickAcc = 0.f;
	int32 ShowIndex = 0;
	int32 ResultIndex = INDEX_NONE;
	int32 LastResult = INDEX_NONE;
};
