#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RunTypes.h"
#include "CatRunGameMode.generated.h"

class ARunnerCat;
class ATrackDirector;
class ARunCamera;
class AHazardBase;
class UCatBuff;
class UScoreKeeper;
class UCatRunWidget;
class ACatRunPlayerController;
class ACatAudio;
struct FShopRow;

/**
 * Ablauf von Inkrush: Startbildschirm -> Lauf (alle 9 Fahrbahnen einfaerben) -> Erfolg bzw. Sturz/Game Over -> Neustart.
 * Baut beim Start Licht, Nebel, Level, Katze und Kamera auf und verbindet UI, Score und Speicherstand.
 *
 * Test-Parameter (Kommandozeile):
 *   -CatAutoStart            Lauf sofort starten
 *   -CatAuto                 Autopilot: weicht aus, faerbt ein, wechselt Kurse, setzt Items ein
 *   -CatNoHazards            ohne Hindernisse/Gegner
 *   -CatShot=a.png@3,b.png@8 Screenshots zu Spielzeitpunkten (Sekunden)
 *   -CatWinShot=a.png        Screenshot des Erfolgsbildschirms
 *   -CatGameOverShot=a.png   Screenshot des ersten Game-Over-Bildschirms
 *   -CatPilotOff=20          Autopilot nach Sekunden abschalten
 *   -CatJumpEvery=3          Test: regelmaessig springen
 *   -CatEnemies=0.6          Gegner-Chance pro Reihe ab 20 m
 *   -CatQuit=60              Beenden nach Sekunden
 */
UCLASS()
class SHADOWCAT_API ACatRunGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACatRunGameMode();

	/** Blueprint der Katze (falls vorhanden), sonst ARunnerCat. */
	UPROPERTY(EditAnywhere, Category = "Klassen")
	TSoftClassPtr<ARunnerCat> CatClass;

	/** Blueprint des Track-Directors (falls vorhanden), sonst ATrackDirector. */
	UPROPERTY(EditAnywhere, Category = "Klassen")
	TSoftClassPtr<ATrackDirector> DirectorClass;

	/** Leben pro Lauf. */
	UPROPERTY(EditAnywhere, Category = "Ablauf")
	int32 MaxLives = 9;

	/** Unverwundbarkeit nach einem Treffer (s). */
	UPROPERTY(EditAnywhere, Category = "Ablauf")
	float HitInvulnerability = 2.f;

	/** Zeit zwischen Treffer und Game-Over-Bildschirm. */
	UPROPERTY(EditAnywhere, Category = "Ablauf")
	float DeathDelay = 1.1f;

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void RestartPlayer(AController* NewPlayer) override {}

	void RegisterController(ACatRunPlayerController* PC);

	// Von UI und Eingabe
	void RequestStart();
	/** Menue: Modus waehlen und sofort starten. */
	void RequestStartMode(ERunMode InMode);
	void RequestMenu();
	/** Shop im Startbildschirm: Upgrades und Leben mit Muenzen kaufen. */
	void RequestShop();
	void RequestBuy(int32 Index);
	/** Shop "ZURUECK": in die Pause (waehrend des Laufs) bzw. ins Menue */
	void RequestShopBack();
	/** Nach Button-Tipps im Lauf: Eingabe zuruecksetzen (siehe ACatRunPlayerController::ResetInputAfterUi) */
	void AfterUiTap();
	/** Pause-Taste oben links: Spiel anhalten, Pause-Seite mit Shop */
	void RequestPause();
	void RequestResume();
	void TogglePause();
	/** Pause: Lauf beenden (wie Game Over, Muenzen werden gutgeschrieben) */
	void RequestQuitRun();
	bool IsRunPaused() const { return bPaused; }
	void RequestConfirm();
	void RequestLaneShift(int32 Dir);
	void RequestJump();
	void RequestDrop();
	/** Item einsetzen: Index = Sorte (Symbol am Rand), -1 = irgendein vorhandenes. */
	void RequestUseItem(int32 Index = -1);
	void CycleQuality();
	void ToggleStats();

	// Von der Strecke
	/** Treffer: kostet ein Leben. true = keine Leben mehr (Lauf vorbei). */
	bool OnCatHit(const FString& Reason);
	void OnEnemyDefeated(int32 Bonus);
	void OnInkPainted(int32 Points);
	void OnLevelComplete();
	void OnCircuitChanged(int32 Circuit);
	void OnBoxCollected();
	void OnSlotFull();
	void OnCoinsCollected(int32 Count);
	/** Der Riese beginnt eine Salve. */
	void OnSalvo();
	/** Tintenklatscher (Riese, Tintenbombe). */
	void PlaySplash(float VolumeScale = 1.f);
	void PlayBomb();
	void PlayJump();
	void PlayCarHonk();
	/** Gegner-Wuerfel beruehrt: Fluch (Name fuer die Anzeige) */
	void OnCurse(const FText& Name);
	void PlayTrain();
	/** Naechster entgegenkommender Zug (0..1, 1 = ganz nah) -> Lautstaerke des Zuggeraeuschs */
	void SetTrainProximity(float Near);
	/** Einstellungen: Lautstaerke Musik bzw. Soundeffekte (0..1), wird gespeichert */
	void SetVolume(bool bMusic, float Value);
	/** Spraydose: Spruehgeraeusch an/aus */
	void SetSpraySound(bool bOn);

	ERunPhase GetPhase() const { return Phase; }

private:
	void SetupWorld();
	void EnterMenu();
	void BeginRun();
	void EnterGameOver();
	void ApplyQuality(int32 Level, bool bRebuild);
	void UpdateStats(float DeltaTime);
	void UpdateHud();
	void ParseTestOptions();
	void TickTests(float DeltaTime);
	void TickScenario();
	void TickCoffinTest();
	/** Shop-Angebote mit aktuellen Preisen (-1 = ausgereizt). */
	TArray<FShopRow> BuildShopRows(TArray<int32>* OutPrices) const;
	int32 RunMaxLives = 9;
	FString QualityName() const;
	/** Gekaufte Upgrades auf die Katze uebertragen (Lauf-Start und Kauf in der Pause). */
	void ApplyUpgrades();
	/** Muenzen dieses Laufs dem Konto gutschreiben (vor dem Shop in der Pause und bei Game Over). */
	void BankRunCoins();
	/** Guthaben inkl. noch nicht gutgeschriebener Muenzen dieses Laufs. */
	int32 WalletCoins() const;
	bool bPaused = false;
	/** Spruehnebel-Overlay (Spraydose): Deckkraft und Nachlaufzeit */
	float SprayFade = 0.f;
	/** Bildspiegelung (Fluch) 0..1 */
	float MirrorAmount = 0.f;
	float SprayHold = 0.f;
	int32 RunCoinsBanked = 0;

	UPROPERTY(Transient) TObjectPtr<ARunnerCat> Cat;
	UPROPERTY(Transient) TObjectPtr<ATrackDirector> Director;
	UPROPERTY(Transient) TObjectPtr<ARunCamera> RunCamera;
	UPROPERTY(Transient) TObjectPtr<UScoreKeeper> Score;
	UPROPERTY(Transient) TObjectPtr<UCatRunWidget> Widget;
	UPROPERTY(Transient) TObjectPtr<ACatRunPlayerController> Controller;
	UPROPERTY(Transient) TObjectPtr<ACatAudio> Audio;

	ERunPhase Phase = ERunPhase::Menu;
	ERunMode Mode = ERunMode::Endless;
	int32 Lives = 9;
	float PhaseTime = 0.f;
	float RunClock = 0.f;
	bool bNewRecord = false;
	int32 RunCoins = 0;
	bool bPendingMenu = false;
	float EdgeMsgCooldown = 0.f;

	// Leistungsanzeige
	float StatTime = 0.f;
	int32 StatFrames = 0;
	float StatLogTime = 0.f;

	// Tests
	bool bAutoStart = false;
	bool bAutopilot = false;
	bool bNoHazards = false;
	TArray<TPair<float, FString>> Shots;
	float QuitAt = -1.f;
	float PlayTime = 0.f;
	int32 RunCount = 0;
	float PilotOffAt = -1.f;
	float JumpEvery = -1.f;
	float NextTestJump = 0.f;
	FString GameOverShot;
	FString WinShot;

	// Szenario "bombgap"
	FString Scenario;
	FString ModeArg;
	int32 ScenarioStep = 0;
	float ScenarioStartArc = 0.f;
	float ScenarioGapArc = 0.f;
	/** Szenario "coffin": Sarg-Sprungtest */
	float CoffinA = -1.f;
	float CoffinLead = 0.f;
	int32 CoffinIndex = 0;
	bool bCoffinJumped = false;
	float SalvoAt = -1.f;
	FString ScenarioShotDir;
};
