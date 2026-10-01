#pragma once

#include "CoreMinimal.h"
#include "RunTypes.h"
#include "RunPlanner.generated.h"

/**
 * Hindernisarten (je eine Fahrbahn). Parkour: Zaun und Abgrund ueberspringen, unter dem Balken durchkriechen
 * (Wisch nach unten); die Laterne ist zu hoch -> ausweichen.
 */
UENUM(BlueprintType)
enum class EObstacleType : uint8
{
	Fence,
	Lamp,
	Pit,
	Beam,
	/** Kiste (Crate.png): drueberspringen oder ausweichen */
	Crate
};

/** Kann man das Hindernis mit einer Aktion (Sprung/Kriechen) passieren? */
inline bool IsActionObstacle(EObstacleType T) { return T != EObstacleType::Lamp; }

/** Schwierigkeits- und Tempokurve. Alle Zeiten in Sekunden, Strecken in Metern, Tempo in cm/s. */
USTRUCT(BlueprintType)
struct FRunDifficulty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tempo")
	float StartSpeed = 950.f;

	/** Vorlaeufige Obergrenze des Tempos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tempo")
	float MaxSpeed = 1500.f;

	/** Tempozuwachs pro Sekunde Laufzeit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tempo")
	float SpeedGainPerSecond = 6.f;

	/** Angenommene Reaktionszeit eines Spielers bis zum ersten Wisch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fairness")
	float ReactionTime = 0.45f;

	/** Zeitabstand zwischen Hindernisreihen am Anfang bzw. nach voller Steigerung. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fairness")
	float MaxGapTime = 1.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fairness")
	float MinGapTime = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fairness")
	float GapRampMeters = 2500.f;

	/** Erste Hindernisreihe nach so vielen Metern. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fairness")
	float FirstRowMeters = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hindernisse")
	float DoubleBlockChanceStart = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hindernisse")
	float DoubleBlockChanceMax = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hindernisse")
	float DoubleRampMeters = 1800.f;

	/** Parkour: Anteil "Wand"-Reihen ueber alle Fahrbahnen (Abgrund, Balken oder Zaun: nur mit Sprung/Kriechen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float WallChanceStart = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float WallChanceMax = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float WallStartMeters = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float WallRampMeters = 2000.f;

	/** Mindestzeit nach einer Sprung-/Kriech-Reihe bis zur naechsten Reihe (Aktion + Reaktion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float ActionRecoverTime = 1.25f;

	/** Laenge eines Abgrunds (cm): mit einem Sprung sicher ueberwindbar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	FVector2D PitLength = FVector2D(300.f, 680.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gegner")
	float EnemyStartMeters = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gegner")
	float EnemyChanceStart = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gegner")
	float EnemyChanceMax = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gegner")
	float EnemyRampMeters = 2500.f;

	/** Flugdauer eines Wuerfels bis zu seiner Spur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gegner")
	float EnemyFlyTime = 1.0f;

	/** So lange vor Ankunft der Katze steht der Wuerfel bereits sichtbar auf seiner Spur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gegner")
	float EnemySettleLead = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
	float ItemStartMeters = 80.f;

	/** Chance pro Hindernisreihe, dass danach ein Item liegt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
	float ItemChance = 0.1f;

	/** Mindestabstand zwischen zwei Items. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
	float ItemCooldownMeters = 220.f;
};

struct FPlannedHazard
{
	int32 Lane = 0;
	EHazardKind Kind = EHazardKind::Obstacle;
	EObstacleType Type = EObstacleType::Lamp;
	float Length = 60.f;
};

/** Eine Hindernisreihe quer ueber die Spuren (plus optional ein Item dahinter). */
struct FPlannedRow
{
	float StartX = 0.f;
	float EndX = 0.f;
	uint8 BlockedMask = 0;
	/** Spuren, in denen der Spieler diese Reihe nachweislich rechtzeitig sicher passieren kann. */
	uint8 SafeMask = 0;
	TArray<FPlannedHazard> Hazards;
	int32 ItemLane = INDEX_NONE;
	float ItemX = 0.f;
};

/**
 * Plant Hindernisreihen so, dass immer ein erreichbarer, sicherer Weg offen bleibt:
 * SafeMask = Spuren, die an der letzten Reihe sicher sind. Fuer die naechste Reihe zaehlen nur Spuren als erreichbar,
 * die mit (Abstand/Tempo - Reaktionszeit) / Spurwechselzeit Wechseln von einer sicheren Spur erreicht werden koennen.
 * Mindestens eine erreichbare Spur bleibt frei. Nach einem Item bleibt dessen Spur in der naechsten Reihe frei und es folgt kein Gegner.
 */
class SHADOWCAT_API FRunPlanner
{
public:
	void Reset(int32 InNumLanes, float InLaneSwitchTime, float FirstRowX, int32 StartLane, int32 Seed);
	FPlannedRow Next(const FRunDifficulty& D, float SpeedAtRow, float Meters, bool bAllowItem);
	float GetFrontierX() const { return bFirst ? FirstX : LastEndX; }
	uint8 GetSafeMask() const { return SafeMask; }
	void Shift(float Dx) { LastEndX += Dx; FirstX += Dx; }
	/** Abgruende nur auf der geraden Strecke (echte Loecher); sonst werden daraus Zaeune. */
	bool bAllowPits = true;

	static float Ramp(float Value, float Range) { return Range > 0.f ? FMath::Clamp(Value / Range, 0.f, 1.f) : 1.f; }

private:
	uint8 Dilate(uint8 Mask, int32 Steps) const;
	uint8 AllMask() const { return uint8((1 << NumLanes) - 1); }
	int32 RandomBit(uint8 Mask);

	FRandomStream Rng;
	int32 NumLanes = 3;
	float LaneSwitchTime = 0.15f;
	uint8 SafeMask = 0;
	float LastEndX = 0.f;
	float FirstX = 0.f;
	bool bFirst = true;
	int32 PendingItemLane = INDEX_NONE;
	/** Letzte Reihe verlangte eine Aktion (Sprung/Kriechen) -> naechste Reihe mit Erholungsabstand. */
	bool bLastAction = false;
};
