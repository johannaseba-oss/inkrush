#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrackPlatform.generated.h"

class UStaticMeshComponent;

/**
 * Gelaende auf einer Fahrbahn (Endlos-Modus): Rampe hoch auf ein Plateau oder eine Erhoehung ohne Rampe (draufspringen).
 * Die Katze laeuft oben weiter (ARunnerCat::SetSupportZ), am Ende faellt sie wieder auf die Strecke.
 * Wer seitlich oder vorn gegen die Wand laeuft (zu tief), verliert ein Leben.
 */
UCLASS()
class SHADOWCAT_API ATrackPlatform : public AActor
{
	GENERATED_BODY()

public:
	ATrackPlatform();

	/** Fahrbahn G, Beginn A0 (Rampenfuss), Rampenlaenge (0 = Erhoehung ohne Rampe), Plateaulaenge, Hoehe. */
	void Setup(int32 InLane, float InA0, float InRampLen, float InPlateauLen, float InHeight, float LaneLat, float LaneWidth);
	void Retire();

	/** Hoehe der Oberflaeche an Streckenposition A (0 ausserhalb). */
	float HeightAt(float InA) const;
	bool IsLive() const { return bLive; }
	bool HasRamp() const { return RampLen > 1.f; }

	int32 Lane = 0;
	float A0 = 0.f;
	float A1 = 0.f;
	float RampLen = 0.f;
	float Height = 0.f;

private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RampTop;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RampFill;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Cap;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FrontRim;
	bool bLive = false;
};
