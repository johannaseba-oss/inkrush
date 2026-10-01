#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrackPlatform.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/** Art des Gelaende-Stuecks. */
enum class EPlatformKind : uint8
{
	/** Rampe/Plateau/Erhoehung (hell, begehbar) */
	Terrain,
	/** Zug: Lok + Waggons oder nur Waggons, ein Gleis breit, zu hoch -> ausweichen; kann entgegenkommen */
	Train
};

/**
 * Gelaende auf einer Fahrbahn (Endlos-Modus): Rampe hoch auf ein Plateau, Erhoehung ohne Rampe (draufspringen),
 * Zug aus schlichten Waggons (stehend oder entgegenkommend) oder ein Haus als Hindernis.
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
	/** Zug ueber zwei Gleise (InLane und InLane + 1, LaneLat = Mitte dazwischen): Lok (optional) + Cars Waggons, 3D-Modelle
	 *  (/Game/Models); InSpeed > 0 = faehrt der Katze entgegen. */
	void SetupTrain(int32 InLane, float InA0, bool bLoco, int32 Cars, float InSpeed, float LaneLat, float LaneWidth);
	/** Gesamtlaenge eines Zuges (cm). */
	static float TrainLength(bool bLoco, int32 Cars);
	/** Liegt Fahrbahn G unter diesem Stueck? (Zuege belegen zwei Fahrbahnen) */
	bool Covers(int32 G) const { return G >= Lane && G < Lane + Span; }
	void Retire();
	/** Bewegung (entgegenkommender Zug). */
	void StepPlatform(float DeltaTime);

	/** Hoehe der Oberflaeche an Streckenposition A (0 ausserhalb). */
	float HeightAt(float InA) const;
	bool IsLive() const { return bLive; }
	bool HasRamp() const { return RampLen > 1.f; }
	/** Zu hoch zum Draufspringen (ohne Rampe): nur ausweichen. */
	bool IsWall() const { return (!HasRamp() && Height > 200.f) || Kind == EPlatformKind::Train; }

	int32 Lane = 0;
	float A0 = 0.f;
	float A1 = 0.f;
	float RampLen = 0.f;
	float Height = 0.f;
	/** Fahrt entgegen der Laufrichtung (cm/s), 0 = steht */
	float Speed = 0.f;
	/** Fahrender Zug: bis hierher (A) faehrt er voraussichtlich -> auf dieser Fahrbahn bis dahin nichts anderes planen */
	float SweepA = 0.f;
	EPlatformKind Kind = EPlatformKind::Terrain;
	/** Anzahl belegter Fahrbahnen ab Lane */
	int32 Span = 1;
	/** Zug: Laenge der Lok vorn (0 = keine) und ihre Hoehe (niedriger als die Waggons) */
	float LocoPart = 0.f;
	float LocoTop = 0.f;
	/** Zuggeraeusch schon gespielt */
	bool bSounded = false;

private:
	void ClearExtras();
	UStaticMeshComponent* AddPart(const TCHAR* Shape, const FVector& Loc, const FVector& Size, const FRotator& Rot, UMaterialInterface* Mat);
	/** Rampe (helle Schraege + Keil) von 0 bis RampLen auf Hoehe H. */
	void BuildRamp(float W, float H);
	UStaticMeshComponent* AddModel(class UStaticMesh* Mesh, float X0, float Len, float W, float H, bool bFlip);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RampTop;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RampFill;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Cap;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FrontRim;
	/** Zusatzteile von Zug/Haus (werden bei jedem Setup neu gebaut) */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Extras;
	/** Bild der Zugfront (Tools/Import/Nature/Train front.png -> T_Sprite_TrainFront) */
	UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> FrontMat;
	float LaneLatitude = 0.f;
	bool bLive = false;
};
