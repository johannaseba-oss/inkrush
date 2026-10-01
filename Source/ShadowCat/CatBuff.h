#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RunTypes.h"
#include "CatBuff.generated.h"

class ARunnerCat;
class ATrackDirector;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UTexture2D;

/**
 * Basisklasse fuer zeitlich begrenzte Item-Effekte.
 * Neues Item = neue Unterklasse (C++ oder Blueprint) + Eintrag in ATrackDirector::Items.
 * Die Klasse beschreibt beides: das schwebende Symbol auf der Strecke (BuildPickupVisual, auf dem CDO)
 * und den aktiven Effekt an der Katze (OnBegin/OnTick/OnEnd, AbsorbsHit).
 */
UCLASS(Abstract, Blueprintable)
class SHADOWCAT_API UCatBuff : public UObject
{
	GENERATED_BODY()

public:
	/** Name in der Buff-Anzeige. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff")
	FText DisplayName;

	/** Symbol im Item-Button (Tools/Import/UI -> /Game/UI, CAT_SETUP=ui). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Wirkungsdauer in Sekunden. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff")
	float Duration = 6.f;

	void Begin(ARunnerCat* InCat);
	void End();
	/** Erneutes Einsammeln desselben Items setzt die Dauer zurueck. */
	void Refresh() { Remaining = Duration; OnRefreshed(); }
	/** false, sobald die Wirkung abgelaufen ist. */
	bool Step(float DeltaTime);

	float GetRemaining() const { return Remaining; }
	float GetFraction() const { return Duration > 0.f ? FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 0.f; }

	/** true = eine Kollision mit dieser Gefahrenart wird abgefangen (Gegner/Hindernis wird zerstoert, Katze lebt). */
	virtual bool AbsorbsHit(EHazardKind Kind) const { return false; }

	/** Baut das Einsammel-Symbol (wird auf dem Klassen-Standardobjekt aufgerufen). */
	virtual void BuildPickupVisual(AActor* Pickup, USceneComponent* Root) const;
	/** Laufende Animation des Symbols. */
	virtual void AnimatePickup(float Time, USceneComponent* Root) const;

protected:
	virtual void OnBegin() {}
	virtual void OnTick(float DeltaTime) {}
	virtual void OnEnd() {}
	/** Gleiches Item erneut eingesetzt, waehrend es noch wirkt (gestapelt). */
	virtual void OnRefreshed() {}

	/** 0..1 Ein-/Ausblendfaktor (erste 0.25 s, letzte 0.5 s). */
	float GetFade() const;
	/** Beschleunigtes Blinken in den letzten 1.5 s als Warnung vor dem Ablauf. */
	float GetEndingBlink() const;

	UStaticMeshComponent* AddVisual(const TCHAR* Shape, const FVector& Loc, const FVector& Scale, const FRotator& Rot, UMaterialInterface* Mat, USceneComponent* Parent = nullptr);
	USceneComponent* AddPivot(const FVector& Loc, USceneComponent* Parent = nullptr);
	ATrackDirector* GetDirector() const;

	UPROPERTY(Transient)
	TObjectPtr<ARunnerCat> Cat;

	/** Alle Effekt-Bauteile haengen hierunter und werden beim Ende entfernt. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Visuals;

	float Remaining = 0.f;
	float Age = 0.f;
};
