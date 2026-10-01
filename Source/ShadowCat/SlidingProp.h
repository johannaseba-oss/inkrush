#pragma once

#include "CoreMinimal.h"
#include "HazardBase.h"
#include "SlidingProp.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * Bewegtes Deko-Hindernis (Endlos-Modus, nur auf flacher Strecke):
 *  - Pendel (Kiste, Stoppschild): gleitet dauerhaft und gleichmaessig von Rand zu Rand hin und her.
 *  - Einmal quer (Auto): faehrt einmal von einer Strassenseite zur anderen und bleibt dann draussen.
 * Treffen kann nur die Breite HalfWidth; ist Height niedrig genug (Kiste), kann man drueberspringen.
 */
UCLASS()
class SHADOWCAT_API ASlidingProp : public AHazardBase
{
	GENERATED_BODY()

public:
	ASlidingProp();

	/** Bild und Groesse (Hoehe in cm); CollisionFrac = Anteil der Bildbreite, der trifft. */
	/** Flip -1 = Bild gespiegelt (Auto faehrt nach links). */
	void Setup(UTexture2D* Tex, float Height, float CollisionFrac, float Flip = 1.f);
	/** Pendelt zwischen LatMin und LatMax mit SlideSpeed (cm/s); Phase 0..1 = Startpunkt auf der Hin-und-Her-Bahn. */
	void Launch(const FVector& Ground, float InA, float InLatMin, float InLatMax, float InSlideSpeed, float Phase);
	/** Einmal quer von FromLat nach ToLat; bei ArriveTime (s ab jetzt) ist es bei AtLat. */
	void LaunchCross(const FVector& Ground, float InA, float FromLat, float ToLat, float InSpeed, float ArriveTime, float AtLat);
	void StepSlide(float DeltaTime, float SecondsToCat);
	float GetLat() const { return Lat; }
	/** Seitliche Lage in Seconds Sekunden (fuer den Test-Autopiloten). */
	float PredictLat(float SecondsToCat, float Seconds) const;

private:
	/** Lage nach Zeit Tm seit dem Start (Dreieckswelle) und Bewegungsrichtung. */
	float LatAtTime(float Tm, float* OutDir = nullptr) const;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Sprite;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Mat;

	FVector Base = FVector::ZeroVector;
	float LatMin = 0.f;
	float LatMax = 0.f;
	float SlideSpeed = 250.f;
	float Offset = 0.f;
	float Lat = 0.f;
	float Elapsed = 0.f;
	/** Einmal quer statt Pendel; Richtung +1/-1 */
	bool bOneWay = false;
	float CrossDir = 1.f;
};
