#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RunTypes.h"
#include "RunPlanner.h"
#include "HazardBase.generated.h"

/** Gemeinsame Basis fuer alles, was die Katze auf einer Spur treffen kann. Wird gepoolt statt zerstoert. */
UCLASS(Abstract)
class SHADOWCAT_API AHazardBase : public AActor
{
	GENERATED_BODY()

public:
	AHazardBase();

	EHazardKind GetKind() const { return Kind; }
	bool IsLive() const { return bLive; }
	/** Kann die Katze gerade daran scheitern? */
	virtual bool IsDangerous() const { return bLive; }
	/** Halbe Ausdehnung entlang der Laufrichtung (X) bzw. quer (Y) fuer die Kollisionspruefung. */
	float HalfLength = 30.f;
	float HalfWidth = 50.f;
	/** Oberkante in cm: springt die Katze hoeher, verfehlt sie das Hindernis (Saeule: nicht ueberspringbar). */
	float Height = 999.f;
	/** Globale Fahrbahn und Streckenposition (Rundkurs-Parameter A). */
	int32 Lane = 0;
	float A = 0.f;

	virtual void Retire();

protected:
	void Activate(const FVector& Location, const FRotator& Rotation);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	EHazardKind Kind = EHazardKind::Obstacle;
	bool bLive = false;
};

/** Parkour-Hindernis: Zaun (Bild, springen), Laterne (Bild, ausweichen), Abgrund (springen), Balken (kriechen). */
UCLASS()
class SHADOWCAT_API AObstacle : public AHazardBase
{
	GENERATED_BODY()

public:
	AObstacle();

	/** Baut die Optik fuer einen Typ (einmalig pro Pool-Objekt). */
	void Build(EObstacleType InType);
	void Place(const FVector& Location, const FRotator& Rotation, int32 InLane, float InA);
	EObstacleType GetType() const { return Type; }
	/** Abgrund: Laenge anpassen (vor Place). */
	void SetPitLength(float Length);
	/** Abgrund: Seitenwaende zeigen (aus, wenn daneben ebenfalls Abgrund ist -> keine Zwischenwand). */
	void SetPitSideWalls(bool bLeft, bool bRight);
	/** Abgrund: vordere Kante / hintere Wand + Kante zeigen (aus, wenn in derselben Fahrbahn direkt Abgrund anschliesst). */
	void SetPitEnds(bool bNear, bool bFar);

private:
	EObstacleType Type = EObstacleType::Lamp;
	bool bBuilt = false;

	/** Abgrund: hintere Wand (2 Baender), Seitenwaende (je 2), Grund, 2 Kanten */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PitParts;
};
