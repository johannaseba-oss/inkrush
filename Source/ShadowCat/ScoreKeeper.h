#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ScoreKeeper.generated.h"

/** Lokaler Speicherstand (SaveGame-Slot "ShadowCat"): Highscore + Grafikeinstellungen. */
UCLASS()
class SHADOWCAT_API UCatRunSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Version = 1;

	UPROPERTY()
	int32 HighScore = 0;

	UPROPERTY()
	int32 BestMeters = 0;

	/** Schnellste Zeit fuer das komplette Einfaerben (Sekunden, 0 = noch nie geschafft). */
	UPROPERTY()
	float BestTime = 0.f;

	/** 0 = Niedrig, 1 = Mittel, 2 = Hoch */
	UPROPERTY()
	int32 Quality = 1;

	UPROPERTY()
	bool bShowStats = false;

	/** Muenzen (gesammelt minus im Shop ausgegeben). */
	UPROPERTY()
	int32 Coins = 0;

	/** Shop-Upgrades */
	UPROPERTY()
	int32 UpgExtraLives = 0;

	UPROPERTY()
	int32 UpgFlyTime = 0;

	UPROPERTY()
	bool bUpgFlyInvulnerable = false;

	UPROPERTY()
	bool bUpgDoubleJump = false;

	UPROPERTY()
	int32 UpgStartBombs = 0;

	/** Shop: Stufen der Bomben-Reichweite (je +6 m) */
	UPROPERTY()
	int32 UpgBombRange = 0;

	/** 0 = Tutorial, 1 = Endlos */
	UPROPERTY()
	int32 LastMode = 1;
};

/** Score eines Laufs (Strecke + Boni) und dauerhafter Highscore. */
UCLASS()
class SHADOWCAT_API UScoreKeeper : public UObject
{
	GENERATED_BODY()

public:
	static const TCHAR* SlotName;

	/** Punkte pro Meter ueberlebter Strecke. */
	float PointsPerMeter = 1.f;

	/** Testlaeufe (Kommandozeilen-Tests) schreiben nicht in den Spielstand. */
	bool bReadOnly = false;

	void Load();
	void Save();

	void BeginRun();
	void SetMeters(float Meters);
	void AddBonus(int32 Points) { Bonus += Points; }
	int32 GetScore() const { return FMath::FloorToInt(Meters * PointsPerMeter) + Bonus; }
	int32 GetHighScore() const { return Data ? Data->HighScore : 0; }
	/** Lauf abschliessen: true bei neuem Rekord (wird sofort gespeichert). */
	bool CommitRun();
	/** Level geschafft: Zeit eintragen; true bei neuer Bestzeit. */
	bool CommitTime(float Seconds);
	float GetBestTime() const { return Data ? Data->BestTime : 0.f; }
	/** Muenzen eines Laufs gutschreiben (gespeichert mit dem naechsten Save). */
	void AddCoins(int32 N) { if (Data) Data->Coins += N; }
	int32 GetTotalCoins() const { return Data ? Data->Coins : 0; }

	UCatRunSaveGame* GetData() const { return Data; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UCatRunSaveGame> Data;

	float Meters = 0.f;
	int32 Bonus = 0;
};
