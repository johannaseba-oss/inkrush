#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RunPlanner.h"
#include "CircuitLayout.h"
#include "TrackDirector.generated.h"

class ARunnerCat;
class ACatRunGameMode;
class AObstacle;
class AEnemyCube;
class AHazardBase;
class AItemBox;
class AShardBurst;
class AInkCanvas;
class ALevelScenery;
class ABossGiant;
class ACoinField;
class AInkDrops;
class ASlidingProp;
class ATrackPlatform;
class AInkMarks;
class UTexture2D;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UCatBuff;

/** Ein Item im Pool der Fragezeichen-Boxen: welcher Effekt und wie haeufig. */
USTRUCT(BlueprintType)
struct FItemSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TSubclassOf<UCatBuff> Buff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	float MinMeters = 0.f;
};

/**
 * Level-Regeln des Tinten-Rundkurses: Geometrie (FCircuitLayout), Einfaerbung (AInkCanvas), Kulisse (ALevelScenery),
 * Fragezeichen-Boxen, Hindernis-/Gegner-Planung entlang des aktuellen Kurses (FRunPlanner), Kollision, Tempo.
 * Einstellbar im Blueprint BP_TrackDirector.
 */
UCLASS(Blueprintable)
class SHADOWCAT_API ATrackDirector : public AActor
{
	GENERATED_BODY()

public:
	ATrackDirector();

	/** Endlos-Modus: gerade Strecke mit 5 Fahrbahnen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	FCircuitLayout EndlessLayout;

	/** Aktives Layout (Kopie des Endlos-Layouts). */
	FCircuitLayout Layout;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	FRunDifficulty Difficulty;

	/** Inhalt der Fragezeichen-Boxen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
	TArray<FItemSpawnEntry> Items;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
	int32 BoxesPerCircuit = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
	float BoxRespawnTime = 15.f;

	/** Hindernisse/Gegner (aus = reines Einfaerben). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	bool bHazards = true;

	/** So weit voraus werden Hindernisse geplant (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	float ViewAhead = 6500.f;

	/** Startfahrbahn (global) und -position (Anteil der ersten Geraden). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	int32 StartLane = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	float StartFraction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punkte")
	int32 EnemyBonus = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punkte")
	int32 PointsPerSection = 5;

	/** Endlos: Deko (Baum/Haus/Laterne) gleitet quer ueber die Strecke - ab so vielen Metern, Abstand (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float SlideStartMeters = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	FVector2D SlideSpacing = FVector2D(3500.f, 7000.f);

	/** Endlos: Gelaende (Rampen mit Plateau, Erhoehungen) auf 1-3 Fahrbahnen - ab so vielen Metern, Abstand (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float TerrainStartMeters = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	FVector2D TerrainSpacing = FVector2D(2500.f, 5000.f);

	/** Schluchten mit Mittel-Insel: ab so vielen Metern, Abstand (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	float ChasmStartMeters = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	FVector2D ChasmSpacing = FVector2D(18000.f, 32000.f);

	/** So weit hinter der Katze beginnt die Tintenspur (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
	float TrailLag = 90.f;

	/** Riese (Endlos-Modus): erster Angriff nach, Abstand zwischen Angriffen (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	float BossFirstAttack = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	FVector2D BossInterval = FVector2D(9.f, 15.f);

	/** Laenge und Dauer der weissen Tintenpfuetze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	float PuddleLength = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	float PuddleDuration = 2.8f;

	/** Endlos-Modus: Abstand zwischen Fragezeichen-Boxen (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Items")
	FVector2D EndlessBoxSpacing = FVector2D(9000.f, 15000.f);

	/** Salve des Riesen: erste nach, dann alle (s); Reihen, Abstand der Reihen (cm), Abstand der Wuerfe (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	float SalvoFirst = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	FVector2D SalvoInterval = FVector2D(45.f, 60.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	int32 SalvoRows = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	float SalvoRowGap = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	float SalvoThrowGap = 0.3f;

	/** Muenzen (Endlos): Anteil Zehnerreihen, Abstand in der Reihe, Luecke bis zur naechsten Gruppe (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Muenzen")
	float CoinRowChance = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Muenzen")
	float CoinSpacing = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Muenzen")
	FVector2D CoinGap = FVector2D(1200.f, 3500.f);

	void Initialize(ARunnerCat* InCat, ACatRunGameMode* InGame);
	/** Modus wechseln: baut Strecke, Leinwand und Kulisse neu. */
	void SetMode(ERunMode InMode);
	ERunMode GetMode() const { return Mode; }
	/** Level fuer einen neuen Lauf (oder den Startbildschirm) zuruecksetzen. */
	void ResetTrack();
	void BeginRun();
	void StopRun();
	void StepDirector(float DeltaTime);
	void RebuildScenery(float InDensity);
	void SetDensity(float InDensity) { Density = InDensity; }

	float GetSpeed() const { return Speed; }
	float GetMeters() const;
	bool IsRunning() const { return bRunning; }
	float GetStartA() const { return Layout.bStraight ? 0.f : Layout.StraightLength * StartFraction; }
	int32 GetStartLane() const { return Layout.bStraight ? Layout.LanesPerCircuit / 2 : StartLane; }
	int32 GetCatCircuit() const;
	bool CanChangeCircuit() const;
	AInkCanvas* GetCanvas() const { return Canvas; }

	/** Von Items/Spur gemeldete neu eingefaerbte Abschnitte (Punkte, Levelende). */
	void ReportPainted(int32 Count);
	void PlaySplash(float VolumeScale = 1.f);
	void PlayBomb();
	/** Tintenbombe: aufquellende Kleckse ueber alle Fahrbahnen des Kurses um CenterA (+-HalfLen). */
	void InkSmearArea(int32 Circuit, float CenterA, float HalfLen, bool bFromBack = false);
	/** Tintenbombe: sprengt Hindernisse, Gegner, Deko-Hindernisse und weisse Tinte bis Range vor der Katze weg. */
	int32 BombBlast(float Range);
	/** Tintenwolke: Kleckse hinter der Katze auf allen Fahrbahnen des Kurses (fortlaufend aufrufen). */
	void InkSmearTrail(int32 Circuit, float CatA);
	/** Spraydose: Flug beginnt (Muenzreihen in der Luft-Ebene, Spruehgeraeusch) bzw. endet (Reste entfernen). */
	void OnSprayFlight(bool bStart, float Duration);
	/** Hoehe der Luft-Ebene (Fuesse der Katze) */
	float AirLevelZ = 620.f;
	/** Abschnitt nur mit Zuegen (sonst Parkour) und wo der naechste Wechsel kommt */
	bool bTrainSection = false;
	/** Tempo-Faktor durch den Fluch "doppeltes Tempo" */
	float SpeedCurse = 1.f;
	float NextSectionA = 0.f;

	/** Pfotenabdruecke: Abstand zwischen zwei Abdruecken, so weit hinter der Katze erscheinen sie, Groesse (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spur")
	float PawStep = 46.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spur")
	float PawLag = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spur")
	float PawSize = 30.f;
	/** Katze hat alle Leben verloren (Riese spielt "Player dead"). */
	void OnPlayerDead();
	/** Katze hat ein Leben verloren (Riese spielt "Cat got hit"). */
	void OnCatLostLife();

	/** Salve: zusaetzliche Zeit vor dem ersten Wurf, damit das Video "prepares salve" zuerst laufen kann (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Riese")
	float SalvoPrepareTime = 1.6f;

	TArray<AEnemyCube*> GetLiveEnemies() const;
	void DefeatEnemy(AEnemyCube* Enemy);
	int32 DefeatEnemiesNear(const FVector& Center, float Radius);

	/** Test-Autopilot: weicht aus und faerbt systematisch ein (wechselt die Kurse selbst). */
	bool bAutopilot = false;
	FString LastHitReason;
	/** Test: Box vor die Katze legen. */
	void DebugPlaceBox(float AheadCm, int32 Lane);
	/** Test: Hindernis auf die Fahrbahn der Katze legen; liefert seine Streckenposition A. */
	float DebugPlaceObstacle(EObstacleType Type, float AheadCm);

private:
	void PaintTrail();
	void CheckCircuitChange();
	void PlanAhead();
	void SpawnRow(const FPlannedRow& Row);
	void CheckCollisions();
	void CheckBoxes(float DeltaTime);
	void RecycleBehind();
	void RetireAllHazards();
	void RunAutopilot();
	void StepEndlessBoxes();
	void StepBoss(float DeltaTime);
	bool StartSalvo();
	void CheckPuddles();
	void StepEndlessCoins(float DeltaTime);
	/** Hindernis oder Gegner auf Fahrbahn Lane zwischen A0 und A1? */
	bool LaneBusy(int32 Lane, float A0, float A1) const;
	void PlaceBoxes();
	float WorldAhead(float HazardA) const;
	/** Planer-Strecke (Weltstrecke) -> A auf der Mittelspur des aktuellen Kurses. */
	float TravelToA(float Travel) const;

	AObstacle* AcquireObstacle(EObstacleType Type);
	AEnemyCube* AcquireEnemy();
	void Burst(const FVector& Loc, float Gray, bool bGlow, float Size = 1.f);

	UPROPERTY(Transient) TObjectPtr<ARunnerCat> Cat;
	UPROPERTY(Transient) TObjectPtr<ACatRunGameMode> Game;
	UPROPERTY(Transient) TObjectPtr<AInkCanvas> Canvas;
	UPROPERTY(Transient) TObjectPtr<ALevelScenery> Scenery;
	UPROPERTY(Transient) TArray<TObjectPtr<AObstacle>> Obstacles;
	UPROPERTY(Transient) TArray<TObjectPtr<AEnemyCube>> Enemies;
	UPROPERTY(Transient) TArray<TObjectPtr<AItemBox>> Boxes;
	UPROPERTY(Transient) TArray<TObjectPtr<AShardBurst>> Bursts;
	UPROPERTY(Transient) TObjectPtr<ABossGiant> Boss;
	UPROPERTY(Transient) TObjectPtr<ACoinField> Coins;
	UPROPERTY(Transient) TArray<TObjectPtr<AInkDrops>> DropPool;
	UPROPERTY(Transient) TArray<TObjectPtr<ASlidingProp>> Sliders;
	UPROPERTY(Transient) TArray<TObjectPtr<ATrackPlatform>> Platforms;
	UPROPERTY(Transient) TObjectPtr<AInkMarks> Marks;
	/** Fusspositionen der letzten Meter (fuer Pfotenabdruecke mit Abstand hinter der Katze) */
	struct FFootSample
	{
		float Travel = 0.f;
		FVector Pos = FVector::ZeroVector;
		float Yaw = 0.f;
		bool bGround = true;
	};
	TArray<FFootSample> FootHist;
	float NextPrintTravel = 0.f;
	bool bPrintLeft = false;
	float LastSmearA = -1.0e9f;
	void StepPawPrints();
	float NextTerrainA = 0.f;
	void StepTerrain();
	/** Schlucht: Abgrund ueber alle Fahrbahnen, nur in der Mitte eine Insel zum Zwischenlanden. */
	TArray<FVector2D> ChasmZones;
	float NextChasmA = 0.f;
	void StepChasms();
	bool ChasmBusy(float InA0, float InA1) const;
	/** Abgrund aus ganzen Abschnitten (Kacheln verschwinden) auf Fahrbahn G ab Abschnitt FirstSec. */
	void SpawnPit(int32 G, int32 FirstSec, int32 NumSec);
	/** Seitenwaende der Abgruende: zwischen zwei gleichen Abgruenden nebeneinander keine Wand. */
	void UpdatePitWalls();
	/** Hoehe des Gelaendes auf Fahrbahn G bei A (0 = Strecke). */
	float GroundAt(int32 G, float InA) const;
	/** Liegt auf Fahrbahn G zwischen A0 und A1 Gelaende? */
	bool TerrainBusy(int32 G, float InA0, float InA1) const;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> SlideTex;
	float NextSlideA = 0.f;
	void StepSliders(float DeltaTime);
	void SpawnDrops(const FVector& Loc, float Size);

	/** Weisse Tintenpfuetze des Riesen (Gefahr fuer einige Sekunden). */
	struct FPuddle
	{
		float A0 = 0.f;
		float A1 = 0.f;
		int32 Mask = 0;
		float TimeLeft = 0.f;
		TObjectPtr<UStaticMeshComponent> Visual;
		TObjectPtr<UMaterialInstanceDynamic> Mat;
		/** aufgewoelbte Kleckse (Cel-Konturen machen sie auf weissen Fahrbahnen sichtbar) */
		TArray<TObjectPtr<UStaticMeshComponent>> Blobs;
	};
	TArray<FPuddle> Puddles;
	/** Laufender Angriff (Einzelwurf oder Salve): je Wurf Ziel, Fahrbahnen, Liegedauer der Pfuetze. */
	struct FIncoming
	{
		float A = 0.f;
		int32 Mask = 0;
		float Duration = 0.f;
	};
	TArray<FIncoming> Incoming;
	void SpawnPuddle(const FIncoming& In);
	float NextBossAttack = 0.f;
	float NextSalvo = 0.f;
	/** Nach einem Sturz in ein Loch: hier geht es weiter */
	float FallRespawnA = 0.f;
	/** Anzahl Salven in diesem Lauf (jede weitere ist schwerer). */
	int32 SalvoCount = 0;
	float NextCoinA = 0.f;
	float NextBoxA = 0.f;
	float Density = 1.f;
	ERunMode Mode = ERunMode::Endless;

	FRunPlanner Planner;
	FRandomStream Rng;
	float Speed = 0.f;
	float RunTime = 0.f;
	bool bRunning = false;
	int32 PlanCircuit = 0;
	int32 LastPaintLane = INDEX_NONE;
	int32 LastSection = 0;
	float LastPaintA = 0.f;
	float SlotFullMsgCooldown = 0.f;
};
