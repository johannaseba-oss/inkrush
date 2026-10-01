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

/** Tintenwolke: die Katze schwebt kurz auf einer Tintenwolke ueber allem und kann nicht getroffen werden. */
UCLASS()
class SHADOWCAT_API UBuff_InkCloud : public UCatBuff
{
	GENERATED_BODY()

public:
	UBuff_InkCloud();

	/** Flughoehe (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Tinte")
	float FlyHeight = 210.f;

	/** Unverwundbar nur mit dem Shop-Upgrade "Flug unverwundbar" (hoch genug fliegt sie ohnehin ueber vieles hinweg). */
	virtual bool AbsorbsHit(EHazardKind Kind) const override;

protected:
	virtual void OnBegin() override;
	virtual void OnTick(float DeltaTime) override;
	virtual void OnEnd() override;
	virtual void OnRefreshed() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PuffMat;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Puffs;
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
