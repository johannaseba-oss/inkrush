#pragma once

#include "CoreMinimal.h"
#include "CatBuff.h"
#include "Buffs.generated.h"

class AEnemyCube;
class UMaterialInstanceDynamic;

/** Schwarze Nebelwolke: umhuellt die Katze und zerstoert Gegner, die sie beruehren. Hindernisse bleiben toedlich. */
UCLASS()
class SHADOWCAT_API UBuff_ShadowMist : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_ShadowMist();

	/** Kontaktradius um die Katze (cm), in dem Gegner vergehen. */
	UPROPERTY(EditDefaultsOnly, Category = "Nebel")
	float ContactRadius = 170.f;

	virtual bool AbsorbsHit(EHazardKind Kind) const override { return Kind == EHazardKind::Enemy; }
	virtual void BuildPickupVisual(AActor* Pickup, USceneComponent* Root) const override;

protected:
	virtual void OnBegin() override;
	virtual void OnTick(float DeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PuffMat;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WispMat;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Puffs;

	TArray<FVector> PuffBase;
};

/** Shuriken-Wolke: kleine dunkle Wurfsterne kreisen um die Katze und erledigen nahe Gegner automatisch. */
UCLASS()
class SHADOWCAT_API UBuff_ShurikenStorm : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_ShurikenStorm();

	UPROPERTY(EditDefaultsOnly, Category = "Shuriken")
	int32 Count = 5;

	/** Reichweite nach vorn (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Shuriken")
	float Range = 750.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shuriken")
	float OrbitRadius = 125.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shuriken")
	float ThrowTime = 0.14f;

	virtual bool AbsorbsHit(EHazardKind Kind) const override { return Kind == EHazardKind::Enemy; }
	virtual void BuildPickupVisual(AActor* Pickup, USceneComponent* Root) const override;
	virtual void AnimatePickup(float Time, USceneComponent* Root) const override;

protected:
	virtual void OnBegin() override;
	virtual void OnTick(float DeltaTime) override;

private:
	struct FStar
	{
		TObjectPtr<USceneComponent> Pivot;
		TWeakObjectPtr<AEnemyCube> Target;
		/** 0 = kreist, 0..1 = Flug zum Ziel, 1..2 = Rueckflug. */
		float Throw = 0.f;
		bool bThrowing = false;
		FVector ReleasePos = FVector::ZeroVector;
	};
	TArray<FStar> Stars;
	float Orbit = 0.f;
};

/** Unbesiegbarkeit: kurze Zeit uebersteht die Katze jeden Treffer; sie leuchtet deutlich weiss auf. */
UCLASS()
class SHADOWCAT_API UBuff_Invincible : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_Invincible();

	virtual bool AbsorbsHit(EHazardKind Kind) const override { return true; }
	virtual void BuildPickupVisual(AActor* Pickup, USceneComponent* Root) const override;

protected:
	virtual void OnBegin() override;
	virtual void OnTick(float DeltaTime) override;
	virtual void OnEnd() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AuraMat;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Aura;
};

namespace BuffVisuals
{
	/** Flacher 8-zackiger Wurfstern in der XY-Ebene. */
	void AddStar(AActor* Owner, USceneComponent* Parent, float Size, UMaterialInterface* Mat, TArray<TObjectPtr<USceneComponent>>* Track = nullptr);
}

/**
 * Spraydose: kleine Dose auf dem Ruecken sprueht schwarze Tinte nach hinten, die Katze steigt in die Luft-Ebene auf
 * (5 Luft-Fahrbahnen ueber den Fahrbahnen am Boden). Dort gibt es keine Hindernisse, nur Muenzreihen (bis 20).
 */
UCLASS()
class SHADOWCAT_API UBuff_SprayPaint : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_SprayPaint();

	/** Hoehe der Luft-Ebene (cm ueber der Strecke, ueber allen Zuegen und Rampen). */
	UPROPERTY(EditDefaultsOnly, Category = "Spray")
	float FlyHeight = 620.f;

	/** Oben ist nichts im Weg: Treffer werden immer abgefangen. */
	virtual bool AbsorbsHit(EHazardKind Kind) const override;

protected:
	virtual void OnBegin() override;
	virtual void OnTick(float DeltaTime) override;
	virtual void OnEnd() override;
	virtual void OnRefreshed() override;

private:
	/** Dose auf dem Ruecken (relativ zu den Fuessen der Katze) */
	FVector CanOffset = FVector(-44.f, 0.f, 78.f);
	/** Stuetzpunkte des Fluessigkeitsstrahls */
	static constexpr int32 StreamPoints = 15;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MistMat;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Mist;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Segs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Joints;
};
/** Tintenbombe: sprengt alle Hindernisse, Gegner und weisse Tinte vor der Katze weg (alle Fahrbahnen), Kleckse vorn. */
UCLASS()
class SHADOWCAT_API UBuff_InkBomb : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_InkBomb();

	/** Grund-Reichweite nach vorn (cm); Shop-Upgrade legt RunnerCat::BombRangeBonus drauf. */
	UPROPERTY(EditDefaultsOnly, Category = "Tinte")
	float Range = 1800.f;

	/** Halbe Laenge des gewerteten Bereichs um die Katze (Tutorial-Luecken schliessen). */
	UPROPERTY(EditDefaultsOnly, Category = "Tinte")
	float HalfLength = 480.f;

protected:
	virtual void OnBegin() override;
	virtual void OnRefreshed() override;

private:
	void Blast();
};

/**
 * Fluch der schwarzen Gegner-Wuerfel (statt Lebensverlust): 10 s Nachteil. Abgefragt ueber UBuffComponent::HasBuff
 * (Steuerung im GameMode, Bildspiegelung ueber ARunCamera, Tempo im TrackDirector).
 */
/** Steuerung links/rechts vertauscht */
UCLASS()
class SHADOWCAT_API UBuff_CurseControls : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_CurseControls() { DisplayName = NSLOCTEXT("ShadowCatBuffs", "CurseControls", "STEUERUNG VERDREHT"); Duration = 10.f; }
};

/** Bild horizontal gespiegelt */
UCLASS()
class SHADOWCAT_API UBuff_CurseMirror : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_CurseMirror() { DisplayName = NSLOCTEXT("ShadowCatBuffs", "CurseMirror", "BILD GESPIEGELT"); Duration = 10.f; }
};

/** doppeltes Tempo */
UCLASS()
class SHADOWCAT_API UBuff_CurseSpeed : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_CurseSpeed() { DisplayName = NSLOCTEXT("ShadowCatBuffs", "CurseSpeed", "DOPPELTES TEMPO"); Duration = 10.f; }
};