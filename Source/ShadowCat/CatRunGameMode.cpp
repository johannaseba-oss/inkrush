#include "CatRunGameMode.h"
#include "ShadowCat.h"
#include "RunnerCat.h"
#include "TrackDirector.h"
#include "RunCamera.h"
#include "ScoreKeeper.h"
#include "CatRunWidget.h"
#include "CatRunPlayerController.h"
#include "HazardBase.h"
#include "CatBuff.h"
#include "BuffComponent.h"
#include "ItemSlotComponent.h"
#include "InkCanvas.h"
#include "LaneMovementComponent.h"
#include "CatAudio.h"
#include "Buffs.h"
#include "CatRunWidget.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "Camera/PlayerCameraManager.h"

ACatRunGameMode::ACatRunGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ACatRunPlayerController::StaticClass();
	CatClass = TSoftClassPtr<ARunnerCat>(FSoftObjectPath(TEXT("/Game/Blueprints/BP_RunnerCat.BP_RunnerCat_C")));
	DirectorClass = TSoftClassPtr<ATrackDirector>(FSoftObjectPath(TEXT("/Game/Blueprints/BP_TrackDirector.BP_TrackDirector_C")));
}

void ACatRunGameMode::StartPlay()
{
	ParseTestOptions();
	SetupWorld();
	Super::StartPlay();
}

void ACatRunGameMode::SetupWorld()
{
	UWorld* W = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Mondlicht: hoch, schraeg; erzeugt Glanzlichter auf Katze und Tinte. Keine Schatten (mobil).
	// Richtung muss zu MoonDir in M_Cat/M_Ink passen (Tools/ue_setup.py).
	if (ADirectionalLight* Moon = W->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform::Identity, P))
	{
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Moon->GetLightComponent()))
		{
			L->SetMobility(EComponentMobility::Movable);
			L->SetWorldRotation(FRotator(-50.f, 165.f, 0.f));
			L->SetIntensity(2.4f);
			L->SetLightColor(FLinearColor::White);
			L->SetCastShadows(false);
		}
	}
	// leichter Hoehennebel fuer Tiefe; die sichtbaren Nebelschwaden kommen aus ALevelScenery
	if (AExponentialHeightFog* Fog = W->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FTransform::Identity, P))
	{
		UExponentialHeightFogComponent* F = Fog->GetComponent();
		F->SetMobility(EComponentMobility::Movable);
		F->SetFogDensity(0.035f);
		F->SetFogHeightFalloff(0.003f);
		F->SetFogInscatteringColor(FLinearColor(0.09f, 0.09f, 0.09f));
		F->SetStartDistance(2500.f);
		F->SetFogMaxOpacity(0.85f);
	}

	UClass* CatCls = CatClass.IsNull() ? nullptr : CatClass.LoadSynchronous();
	Cat = W->SpawnActor<ARunnerCat>(CatCls ? CatCls : ARunnerCat::StaticClass(), FTransform::Identity, P);
	UClass* DirCls = DirectorClass.IsNull() ? nullptr : DirectorClass.LoadSynchronous();
	Director = W->SpawnActor<ATrackDirector>(DirCls ? DirCls : ATrackDirector::StaticClass(), FTransform::Identity, P);
	Cat->SetDirector(Director);
	if (bNoHazards)
	{
		Director->bHazards = false;
	}
	float EnemyChance = -1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("CatEnemies="), EnemyChance))
	{
		Director->Difficulty.EnemyStartMeters = 20.f;
		Director->Difficulty.EnemyChanceStart = EnemyChance;
		Director->Difficulty.EnemyChanceMax = EnemyChance;
	}
	if (Scenario == TEXT("bombgap"))
	{
		Director->bHazards = false;
		Director->StartLane = 2;
	}
	if (Scenario == TEXT("coffin") || Scenario == TEXT("beam") || Scenario == TEXT("pit"))
	{
		Director->bHazards = false;
		Director->BossFirstAttack = 9999.f;
		Director->SalvoFirst = 9999.f;
	}
	if (SalvoAt > 0.f)
	{
		Director->SalvoFirst = SalvoAt;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("CatParkourEarly")))
	{
		// Test: Gelaende und Deko-Hindernisse frueh und oft
		Director->TerrainStartMeters = 12.f;
		Director->TerrainSpacing = FVector2D(1500.f, 3000.f);
		Director->SlideStartMeters = 15.f;
		Director->SlideSpacing = FVector2D(2500.f, 4000.f);
		Director->ChasmStartMeters = 25.f;
		Director->ChasmSpacing = FVector2D(4000.f, 7000.f);
	}
	Director->Initialize(Cat, this);

	RunCamera = W->SpawnActor<ARunCamera>(ARunCamera::StaticClass(), FTransform::Identity, P);
	Audio = W->SpawnActor<ACatAudio>(ACatAudio::StaticClass(), FTransform::Identity, P);

	Score = NewObject<UScoreKeeper>(this);
	Score->PointsPerMeter = 0.f;
	Score->bReadOnly = bAutoStart || bAutopilot || !Scenario.IsEmpty() || Shots.Num() > 0 || QuitAt > 0.f;
	Score->Load();
	// Modus: Kommandozeile > zuletzt gespielt > Endlos
	Mode = Score->GetData()->LastMode == 0 ? ERunMode::Tutorial : ERunMode::Endless;
	if (ModeArg == TEXT("tutorial") || Scenario == TEXT("bombgap"))
	{
		Mode = ERunMode::Tutorial;
	}
	else if (ModeArg == TEXT("endless"))
	{
		Mode = ERunMode::Endless;
	}
	ApplyQuality(Score->GetData()->Quality, false);
	Director->SetMode(Mode);

	UE_LOG(LogShadowCat, Log, TEXT("Welt aufgebaut: Katze %s (%s), Director %s"), *Cat->GetClass()->GetName(), Cat->UsesPlaceholder() ? TEXT("Platzhalter") : TEXT("Modell"), *Director->GetClass()->GetName());
}

void ACatRunGameMode::RegisterController(ACatRunPlayerController* PC)
{
	Controller = PC;
	Widget = PC->GetRunWidget();
	if (Widget)
	{
		Widget->SetGame(this);
	}
	PC->SetViewTarget(RunCamera);
	// erst im naechsten Tick: dann haben alle Actors (auch die Katze) BeginPlay hinter sich
	bPendingMenu = true;
}

void ACatRunGameMode::EnterMenu()
{
	Phase = ERunPhase::Menu;
	PhaseTime = 0.f;
	Director->ResetTrack();
	Cat->ResetForMenu(Director->GetStartA(), Director->GetStartLane());
	RunCamera->SetMenuMode(true, true);
	RunCamera->StepCamera(0.f, Cat->GetActorLocation(), Cat->GetForward());
	if (Widget)
	{
		Widget->ShowMenu(Score->GetBestTime(), Score->GetHighScore(), Mode == ERunMode::Endless, QualityName(), Score->GetData()->bShowStats, Score->GetTotalCoins());
	}
}

void ACatRunGameMode::BeginRun()
{
	Phase = ERunPhase::Running;
	PhaseTime = 0.f;
	RunClock = 0.f;
	// Shop-Upgrades anwenden
	const UCatRunSaveGame* Save = Score->GetData();
	RunMaxLives = MaxLives + Save->UpgExtraLives;
	Lives = RunMaxLives;
	ApplyUpgrades();
	RunCoins = 0;
	RunCoinsBanked = 0;
	++RunCount;
	Score->BeginRun();
	Cat->BeginRun();
	Director->bAutopilot = bAutopilot;
	Director->BeginRun();
	// Shop: mit Bomben starten
	Cat->GetSlot()->GrantClass(UBuff_InkBomb::StaticClass(), Save->UpgStartBombs);
	RunCamera->SetMenuMode(false, false);
	if (Widget)
	{
		Widget->ShowRunHud(Mode == ERunMode::Endless);
	}
	UE_LOG(LogShadowCat, Log, TEXT("Lauf %d gestartet"), RunCount);
	if (Scenario == TEXT("bombgap"))
	{
		ScenarioStep = 0;
		// alles ausser Kurs 1 / rechte Fahrbahn vorfaerben; die faerbt die Katze selbst
		Director->GetCanvas()->DebugPaintAll(2);
		if (UItemSlotComponent* Slot = Cat->GetSlot())
		{
			Slot->ForcedResult = 1; // Tintenbombe
		}
		const FCircuitLayout& L = Director->Layout;
		const float K = (L.BaseRadius + L.LaneLat(2)) / L.BaseRadius;
		// Luecke mitten auf der oberen Geraden (halbe Gerade + Kurve + halbe Gerade voraus)
		ScenarioGapArc = L.StraightLength + PI * L.BaseRadius * K;
		Director->DebugPlaceBox(ScenarioGapArc + 1600.f, 2);
		UE_LOG(LogShadowCat, Log, TEXT("SZENARIO bombgap: Fahrbahn K1-R %d/%d, Luecke bei %.0f m, Rundenlaenge %.0f m"),
			Director->GetCanvas()->CountPainted(2), Director->GetCanvas()->CountSections(2), ScenarioGapArc / 100.f, L.LaneLength(2) / 100.f);
	}
}

void ACatRunGameMode::RequestStart()
{
	if (Phase == ERunPhase::Menu)
	{
		BeginRun();
	}
	else if (Phase == ERunPhase::GameOver || Phase == ERunPhase::Won)
	{
		Director->ResetTrack();
		Cat->ResetForMenu(Director->GetStartA(), Director->GetStartLane());
		RunCamera->SetMenuMode(false, true);
		BeginRun();
	}
}

void ACatRunGameMode::RequestStartMode(ERunMode InMode)
{
	if (Phase != ERunPhase::Menu)
	{
		return;
	}
	if (InMode != Mode)
	{
		Mode = InMode;
		Director->SetMode(Mode);
		Director->ResetTrack();
		Cat->ResetForMenu(Director->GetStartA(), Director->GetStartLane());
		RunCamera->SetMenuMode(true, true);
		RunCamera->StepCamera(0.f, Cat->GetActorLocation(), Cat->GetForward());
	}
	Score->GetData()->LastMode = Mode == ERunMode::Tutorial ? 0 : 1;
	Score->Save();
	BeginRun();
}

// ------------------------------------------------------------------------------------------------
// Shop: Muenzen ausgeben
// ------------------------------------------------------------------------------------------------
namespace
{
	/** Preis, der mit jeder Stufe um Faktor waechst (auf 10 gerundet). */
	int32 LevelPrice(int32 Base, float Growth, int32 Level)
	{
		return FMath::RoundToInt(Base * FMath::Pow(Growth, float(Level)) / 10.f) * 10;
	}
}

TArray<FShopRow> ACatRunGameMode::BuildShopRows(TArray<int32>* OutPrices) const
{
	const UCatRunSaveGame* D = Score->GetData();
	const int32 Coins = D->Coins;
	TArray<FShopRow> Rows;
	TArray<int32> Prices;
	auto Add = [&](const FString& Name, const FString& Desc, int32 Price, bool bMaxed)
	{
		FShopRow R;
		R.Name = Name;
		R.Desc = Desc;
		R.Price = bMaxed ? TEXT("MAX") : FString::Printf(TEXT("%d"), Price);
		R.bCanBuy = !bMaxed && Coins >= Price;
		Rows.Add(R);
		Prices.Add(bMaxed ? -1 : Price);
	};
	// Preis fuer ein Leben verdoppelt sich mit jedem gekauften Leben
	Add(TEXT("+1 LEBEN"), FString::Printf(TEXT("jetzt %d Leben"), MaxLives + D->UpgExtraLives), 150 << D->UpgExtraLives, D->UpgExtraLives >= 6);
	Add(TEXT("WOLKE +1 S"), FString::Printf(TEXT("Flug %d s"), 4 + D->UpgFlyTime), LevelPrice(120, 1.8f, D->UpgFlyTime), D->UpgFlyTime >= 5);
	Add(TEXT("FLUG UNVERWUNDBAR"), TEXT("auf der Wolke kann nichts treffen"), 450, D->bUpgFlyInvulnerable);
	Add(TEXT("DOPPELSPRUNG"), TEXT("in der Luft noch einmal springen"), 600, D->bUpgDoubleJump);
	Add(TEXT("BOMBEN-REICHWEITE"), FString::Printf(TEXT("jetzt %d m nach vorn"), 18 + 6 * D->UpgBombRange), LevelPrice(150, 1.8f, D->UpgBombRange), D->UpgBombRange >= 4);
	Add(TEXT("START-BOMBE"), bPaused ? FString(TEXT("sofort +1 Bombe, danach jeder Start")) : FString::Printf(TEXT("mit %d Bomben starten"), D->UpgStartBombs), LevelPrice(200, 2.f, D->UpgStartBombs), D->UpgStartBombs >= 3);
	if (OutPrices)
	{
		*OutPrices = Prices;
	}
	return Rows;
}

void ACatRunGameMode::ApplyUpgrades()
{
	const UCatRunSaveGame* Save = Score->GetData();
	Cat->FlyBonus = float(Save->UpgFlyTime);
	Cat->bFlyInvulnerable = Save->bUpgFlyInvulnerable;
	Cat->bDoubleJump = Save->bUpgDoubleJump;
	Cat->BombRangeBonus = 600.f * Save->UpgBombRange;
}

void ACatRunGameMode::BankRunCoins()
{
	const int32 New = RunCoins - RunCoinsBanked;
	if (New > 0)
	{
		Score->AddCoins(New);
	}
	RunCoinsBanked = RunCoins;
}

int32 ACatRunGameMode::WalletCoins() const
{
	return Score->GetTotalCoins() + FMath::Max(0, RunCoins - RunCoinsBanked);
}

void ACatRunGameMode::RequestPause()
{
	if (Phase != ERunPhase::Running || bPaused)
	{
		return;
	}
	bPaused = true;
	UGameplayStatics::SetGamePaused(this, true);
	UE_LOG(LogShadowCat, Log, TEXT("Pause"));
	if (Widget)
	{
		Widget->ShowPause(WalletCoins());
	}
}

void ACatRunGameMode::RequestResume()
{
	if (!bPaused)
	{
		return;
	}
	bPaused = false;
	UGameplayStatics::SetGamePaused(this, false);
	if (Widget)
	{
		Widget->HidePause();
	}
}

void ACatRunGameMode::TogglePause()
{
	if (bPaused)
	{
		RequestResume();
	}
	else
	{
		RequestPause();
	}
}

void ACatRunGameMode::RequestQuitRun()
{
	if (!bPaused)
	{
		return;
	}
	RequestResume();
	Director->StopRun();
	Cat->GetBuffs()->ClearAll();
	Cat->GetSlot()->Clear();
	EnterGameOver();
}

void ACatRunGameMode::RequestShopBack()
{
	if (bPaused)
	{
		if (Widget)
		{
			Widget->ShowPause(WalletCoins());
		}
		return;
	}
	RequestMenu();
}

void ACatRunGameMode::RequestShop()
{
	if ((Phase != ERunPhase::Menu && !bPaused) || !Widget)
	{
		return;
	}
	// waehrend des Laufs: gesammelte Muenzen sofort verfuegbar
	if (bPaused)
	{
		BankRunCoins();
	}
	Widget->ShowShop(Score->GetTotalCoins(), BuildShopRows(nullptr));
}

void ACatRunGameMode::RequestBuy(int32 Index)
{
	if (Phase != ERunPhase::Menu && !bPaused)
	{
		return;
	}
	if (bPaused)
	{
		BankRunCoins();
	}
	TArray<int32> Prices;
	BuildShopRows(&Prices);
	UCatRunSaveGame* D = Score->GetData();
	if (!Prices.IsValidIndex(Index) || Prices[Index] < 0 || D->Coins < Prices[Index])
	{
		return;
	}
	D->Coins -= Prices[Index];
	switch (Index)
	{
	case 0: ++D->UpgExtraLives; break;
	case 1: ++D->UpgFlyTime; break;
	case 2: D->bUpgFlyInvulnerable = true; break;
	case 3: D->bUpgDoubleJump = true; break;
	case 4: ++D->UpgBombRange; break;
	case 5: ++D->UpgStartBombs; break;
	default: break;
	}
	Score->Save();
	if (bPaused)
	{
		// Kauf in der Pause wirkt sofort
		ApplyUpgrades();
		if (Index == 0)
		{
			++RunMaxLives;
			++Lives;
		}
		if (Index == 5)
		{
			Cat->GetSlot()->GrantClass(UBuff_InkBomb::StaticClass(), 1);
		}
	}
	UE_LOG(LogShadowCat, Log, TEXT("Shop: Angebot %d gekauft fuer %d Muenzen (Rest %d)"), Index, Prices[Index], D->Coins);
	if (Audio)
	{
		Audio->PlayCoin();
	}
	RequestShop();
}

void ACatRunGameMode::RequestMenu()
{
	if (Phase == ERunPhase::GameOver || Phase == ERunPhase::Menu || Phase == ERunPhase::Won)
	{
		EnterMenu();
	}
}

void ACatRunGameMode::RequestConfirm()
{
	if (bPaused)
	{
		return;
	}
	if (Phase == ERunPhase::Running)
	{
		RequestJump();
		return;
	}
	RequestStart();
}

void ACatRunGameMode::RequestLaneShift(int32 Dir)
{
	if (bPaused)
	{
		return;
	}
	if (Phase != ERunPhase::Running || !Cat)
	{
		return;
	}
	const int32 R = Cat->RequestLaneShift(Dir, Director->CanChangeCircuit());
	if (R != 0 && Audio)
	{
		Audio->PlaySfx(TEXT("change_lane"), 0.8f);
	}
	if (R == 0 && Widget && EdgeMsgCooldown <= 0.f)
	{
		// Rand des Kurses erreicht, aber keine Verbindungsstelle
		const FCircuitLayout& L = Director->Layout;
		const int32 G = Cat->GetLanes()->GetTargetLane();
		const int32 Next = G + FMath::Sign(Dir);
		if (Next >= 0 && Next < L.NumLanesTotal() && L.CircuitOf(Next) != L.CircuitOf(G))
		{
			Widget->FlashMessage(TEXT("KURSWECHSEL NUR AN DEN LEUCHTSTEGEN"));
			EdgeMsgCooldown = 1.5f;
		}
	}
}

void ACatRunGameMode::RequestJump()
{
	if (bPaused)
	{
		return;
	}
	if (Phase == ERunPhase::Running && Cat)
	{
		Cat->RequestJump();
	}
}

void ACatRunGameMode::RequestDrop()
{
	if (bPaused)
	{
		return;
	}
	// Wisch nach unten: kriechen (in der Luft: schnell landen, dann kriechen)
	if (Phase == ERunPhase::Running && Cat)
	{
		Cat->RequestCrawl();
	}
}

void ACatRunGameMode::RequestUseItem(int32 Index)
{
	if (bPaused)
	{
		return;
	}
	if (Phase != ERunPhase::Running || !Cat || !Cat->GetSlot())
	{
		return;
	}
	UItemSlotComponent* Slot = Cat->GetSlot();
	if (UCatBuff* B = Index >= 0 ? Slot->UseItem(Index) : Slot->UseAny())
	{
		if (Widget)
		{
			Widget->FlashMessage(B->DisplayName.ToString());
		}
	}
}

FString ACatRunGameMode::QualityName() const
{
	switch (Score && Score->GetData() ? Score->GetData()->Quality : 1)
	{
	case 0: return TEXT("NIEDRIG");
	case 2: return TEXT("HOCH");
	default: return TEXT("MITTEL");
	}
}

void ACatRunGameMode::ApplyQuality(int32 Level, bool bRebuild)
{
	// Laufzeit-Stufen fuer Messungen auf dem Geraet: Renderaufloesung und Deko-/Nebel-Dichte.
	// Grundaufloesung auf dem iPhone kommt aus Config/DefaultDeviceProfiles.ini (r.MobileContentScaleFactor).
	static const float ScreenPct[3] = { 65.f, 85.f, 100.f };
	static const float Density[3] = { 0.5f, 1.f, 1.f };
	Level = FMath::Clamp(Level, 0, 2);
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		V->Set(ScreenPct[Level], ECVF_SetByCode);
	}
	if (Director)
	{
		Director->SetDensity(Density[Level]);
		if (bRebuild)
		{
			Director->RebuildScenery(Density[Level]);
		}
	}
	UE_LOG(LogShadowCat, Log, TEXT("Grafikstufe %d: ScreenPercentage %.0f, Deko %.1f"), Level, ScreenPct[Level], Density[Level]);
}

void ACatRunGameMode::CycleQuality()
{
	UCatRunSaveGame* D = Score->GetData();
	D->Quality = (D->Quality + 1) % 3;
	ApplyQuality(D->Quality, Phase == ERunPhase::Menu);
	Score->Save();
	if (Phase == ERunPhase::Menu && Widget)
	{
		Widget->ShowMenu(Score->GetBestTime(), Score->GetHighScore(), Mode == ERunMode::Endless, QualityName(), D->bShowStats, Score->GetTotalCoins());
	}
}

void ACatRunGameMode::ToggleStats()
{
	UCatRunSaveGame* D = Score->GetData();
	D->bShowStats = !D->bShowStats;
	Score->Save();
	if (Phase == ERunPhase::Menu && Widget)
	{
		Widget->ShowMenu(Score->GetBestTime(), Score->GetHighScore(), Mode == ERunMode::Endless, QualityName(), D->bShowStats, Score->GetTotalCoins());
	}
}

bool ACatRunGameMode::OnCatHit(const FString& Reason)
{
	if (Phase != ERunPhase::Running)
	{
		return true;
	}
	--Lives;
	RunCamera->AddShake(Lives > 0 ? 0.6f : 1.f);
	UE_LOG(LogShadowCat, Log, TEXT("%sTreffer: %s | Leben %d/%d"), bAutopilot ? TEXT("AUTOPILOT ") : TEXT(""), *Reason, Lives, RunMaxLives);
	if (Lives > 0)
	{
		Cat->Stumble(HitInvulnerability);
		Director->OnCatLostLife();
		if (Widget)
		{
			Widget->FlashMessage(FString::Printf(TEXT("-1 LEBEN  (%d üBRIG)"), Lives));
		}
		return false;
	}
	Phase = ERunPhase::Dying;
	PhaseTime = 0.f;
	Director->StopRun();
	Director->OnPlayerDead();
	Cat->Die();
	Cat->GetSlot()->Clear();
	return true;
}

void ACatRunGameMode::EnterGameOver()
{
	Phase = ERunPhase::GameOver;
	PhaseTime = 0.f;
	const bool bEndless = Mode == ERunMode::Endless;
	// Muenzen gutschreiben (CommitRun speichert)
	BankRunCoins();
	bNewRecord = bEndless ? Score->CommitRun() : false;
	if (!bEndless)
	{
		Score->Save();
	}
	UE_LOG(LogShadowCat, Log, TEXT("Muenzen: %d in diesem Lauf, %d insgesamt"), RunCoins, Score->GetTotalCoins());
	const AInkCanvas* C = Director->GetCanvas();
	if (Widget)
	{
		Widget->ShowGameOver(Score->GetScore(), C->GetTotalPainted(), C->GetTotalSections(), bEndless, Score->GetHighScore(), bNewRecord, Director->GetMeters(), RunCoins);
	}
}

void ACatRunGameMode::OnLevelComplete()
{
	if (Phase != ERunPhase::Running)
	{
		return;
	}
	Phase = ERunPhase::Won;
	PhaseTime = 0.f;
	Director->StopRun();
	Cat->GetBuffs()->ClearAll();
	const bool bBest = Score->CommitTime(RunClock);
	RunCamera->SetMenuMode(true, false);
	UE_LOG(LogShadowCat, Log, TEXT("LEVEL GESCHAFFT nach %.1f s, Score %d%s"), RunClock, Score->GetScore(), bBest ? TEXT(" (neue Bestzeit)") : TEXT(""));
	if (Widget)
	{
		UpdateHud();
		Widget->ShowWin(Score->GetScore(), RunClock, Score->GetBestTime(), bBest);
	}
}

void ACatRunGameMode::OnInkPainted(int32 Points)
{
	Score->AddBonus(Points);
}

void ACatRunGameMode::OnEnemyDefeated(int32 Bonus)
{
	Score->AddBonus(Bonus);
	UE_LOG(LogShadowCat, Log, TEXT("Gegner besiegt (+%d)"), Bonus);
	if (Widget)
	{
		Widget->FlashMessage(FString::Printf(TEXT("+%d"), Bonus));
	}
}

void ACatRunGameMode::OnCircuitChanged(int32 Circuit)
{
	if (Widget)
	{
		Widget->FlashMessage(FString::Printf(TEXT("KURS %d"), Circuit + 1));
	}
}

void ACatRunGameMode::OnBoxCollected()
{
	UE_LOG(LogShadowCat, Log, TEXT("Fragezeichen-Box eingesammelt"));
	if (Audio)
	{
		Audio->PlaySfx(TEXT("Item_Pickup"));
	}
}

void ACatRunGameMode::PlaySplash(float VolumeScale)
{
	if (Audio)
	{
		Audio->PlaySplash(VolumeScale);
	}
}

void ACatRunGameMode::OnCoinsCollected(int32 Count)
{
	RunCoins += Count;
	if (Audio)
	{
		Audio->PlayCoin();
	}
}

void ACatRunGameMode::PlayJump()
{
	if (Audio)
	{
		Audio->PlaySfx(TEXT("jump"));
	}
}

void ACatRunGameMode::PlayBomb()
{
	if (Audio)
	{
		Audio->PlayBomb();
	}
}

void ACatRunGameMode::OnSalvo()
{
	if (Widget)
	{
		Widget->FlashMessage(TEXT("SALVE! AUSWEICHEN"));
	}
}

void ACatRunGameMode::OnSlotFull()
{
	if (Widget)
	{
		Widget->FlashMessage(TEXT("SLOT VOLL - ERST ITEM EINSETZEN"));
	}
}

void ACatRunGameMode::UpdateHud()
{
	if (!Widget)
	{
		return;
	}
	const AInkCanvas* C = Director->GetCanvas();
	const FCircuitLayout& L = Director->Layout;
	TArray<float> Progress;
	for (int32 G = 0; G < L.NumLanesTotal(); ++G)
	{
		Progress.Add(C->LaneProgress(G));
	}
	TArray<UCatBuff*> Buffs;
	for (UCatBuff* B : Cat->GetBuffs()->GetActiveBuffs())
	{
		if (B && B->Duration > 1.f)
		{
			Buffs.Add(B);
		}
	}
	UItemSlotComponent* Slot = Cat->GetSlot();
	// Item-Vorrat: je Sorte Symbol + Anzahl; waehrend der Auslosung ein wechselndes Symbol darueber
	TArray<UTexture2D*> Icons;
	TArray<int32> Counts;
	for (int32 I = 0; I < Slot->NumKinds(); ++I)
	{
		Icons.Add(Slot->GetIcon(I));
		Counts.Add(Slot->GetCount(I));
	}
	const int32 Rolled = Slot->ConsumeRollResult();
	if (Rolled != INDEX_NONE)
	{
		Widget->FlashMessage(FString::Printf(TEXT("%s  x%d"), *Slot->GetName(Rolled).ToString(), Slot->GetCount(Rolled)));
	}
	Widget->UpdateHud(Score->GetScore(), RunClock, Progress, L.LanesPerCircuit, Director->GetCatCircuit(), Icons, Counts, Slot->GetRollIcon(), Buffs, Lives, RunMaxLives, Mode == ERunMode::Endless, Director->GetMeters(), WalletCoins());
}

void ACatRunGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Cat || !Director || !RunCamera)
	{
		return;
	}
	// Grosse Ruckler nicht als Sprung durch Hindernisse durchreichen
	const float Dt = FMath::Min(DeltaSeconds, 1.f / 20.f);
	if (bPendingMenu)
	{
		bPendingMenu = false;
		EnterMenu();
		if (Audio)
		{
			Audio->StartMusic();
		}
	}
	PhaseTime += Dt;
	PlayTime += Dt;
	EdgeMsgCooldown -= Dt;

	Director->StepDirector(Dt);
	if (Phase != ERunPhase::Running)
	{
		Cat->StepIdle(Dt);
	}

	if (Phase == ERunPhase::Running)
	{
		RunClock += Dt;
		Score->SetMeters(Director->GetMeters());
		UpdateHud();
	}
	else if (Phase == ERunPhase::Dying && PhaseTime >= DeathDelay)
	{
		EnterGameOver();
	}

	RunCamera->StepCamera(Dt, Cat->GetActorLocation(), Cat->GetForward());
	if (Audio)
	{
		// Schritte nur, solange die Katze auf dem Boden rennt
		Audio->SetRunning(Phase == ERunPhase::Running && Cat->IsGrounded(), Director->GetSpeed());
	}
	UpdateStats(DeltaSeconds);
	TickTests(Dt);
}

void ACatRunGameMode::UpdateStats(float DeltaTime)
{
	StatTime += DeltaTime;
	++StatFrames;
	const bool bShow = Score && Score->GetData() && Score->GetData()->bShowStats;
	if (StatTime < 0.25f)
	{
		return;
	}
	const float Fps = StatFrames / StatTime;
	const float FrameMs = StatTime * 1000.f / StatFrames;
	const float GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const float RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const float GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	FVector2D Size(0.f, 0.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Size);
	}
	float ScreenPct = 100.f;
	if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		ScreenPct = V->GetFloat();
	}
	const FString Text = FString::Printf(TEXT("%.0f FPS  %.1f ms\nGame %.1f  Draw %.1f  GPU %.1f\n%dx%d  %.0f%%  %s"), Fps, FrameMs, GameMs, RenderMs, GpuMs, int32(Size.X), int32(Size.Y), ScreenPct, *QualityName());
	if (Widget)
	{
		Widget->SetStats(Text, bShow);
	}
	StatLogTime += StatTime;
	if (StatLogTime >= 5.f && Phase == ERunPhase::Running)
	{
		StatLogTime = 0.f;
		const AInkCanvas* C = Director->GetCanvas();
		UE_LOG(LogShadowCat, Log, TEXT("Leistung: %.0f FPS, Frame %.1f ms, Game %.1f, Draw %.1f, GPU %.1f | %.0f m, Tempo %.0f, Tinte %d/%d, Kurs %d"),
			Fps, FrameMs, GameMs, RenderMs, GpuMs, Director->GetMeters(), Director->GetSpeed(), C->GetTotalPainted(), C->GetTotalSections(), Director->GetCatCircuit() + 1);
	}
	StatTime = 0.f;
	StatFrames = 0;
}

void ACatRunGameMode::ParseTestOptions()
{
	const TCHAR* Cmd = FCommandLine::Get();
	bAutoStart = FParse::Param(Cmd, TEXT("CatAutoStart"));
	bAutopilot = FParse::Param(Cmd, TEXT("CatAuto"));
	bNoHazards = FParse::Param(Cmd, TEXT("CatNoHazards"));
	FParse::Value(Cmd, TEXT("CatScenario="), Scenario);
	FParse::Value(Cmd, TEXT("CatMode="), ModeArg);
	FParse::Value(Cmd, TEXT("CatScenarioShots="), ScenarioShotDir, false);
	bAutoStart |= bAutopilot || !Scenario.IsEmpty();
	FString ShotList;
	if (FParse::Value(Cmd, TEXT("CatShot="), ShotList, false))
	{
		TArray<FString> Parts;
		ShotList.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			FString File, At;
			if (Part.Split(TEXT("@"), &File, &At))
			{
				Shots.Add(TPair<float, FString>(FCString::Atof(*At), File));
			}
		}
	}
	FParse::Value(Cmd, TEXT("CatQuit="), QuitAt);
	FParse::Value(Cmd, TEXT("CatSalvoAt="), SalvoAt);
	FParse::Value(Cmd, TEXT("CatPilotOff="), PilotOffAt);
	FParse::Value(Cmd, TEXT("CatJumpEvery="), JumpEvery);
	FParse::Value(Cmd, TEXT("CatGameOverShot="), GameOverShot, false);
	FParse::Value(Cmd, TEXT("CatWinShot="), WinShot, false);
}

void ACatRunGameMode::TickCoffinTest()
{
	// Zaun direkt auf der Fahrbahn der Katze, Sprung bei wechselnden Abstaenden (Katzenmitte bis Zaun-Vorderkante)
	static const float Leads[] = { 5.f, 40.f, 90.f, 180.f, 320.f, 480.f, 640.f, 800.f };
	if (Phase != ERunPhase::Running)
	{
		return;
	}
	if (CoffinA < 0.f || Cat->GetA() > CoffinA + 500.f)
	{
		if (CoffinA >= 0.f)
		{
			UE_LOG(LogShadowCat, Log, TEXT("ZAUNTEST Abstand %.0f cm: vorbei, Leben %d, Tempo %.0f"), CoffinLead, Lives, Director->GetSpeed());
		}
		CoffinLead = Leads[CoffinIndex++ % int32(UE_ARRAY_COUNT(Leads))];
		// -CatScenario=beam: Balken + Kriechen, =pit: Loch ohne Reaktion (Sturz), sonst Zaun + Sprung
		const EObstacleType Type = Scenario == TEXT("beam") ? EObstacleType::Beam : (Scenario == TEXT("pit") ? EObstacleType::Pit : EObstacleType::Fence);
		CoffinA = Director->DebugPlaceObstacle(Type, 1800.f);
		bCoffinJumped = false;
	}
	else if (!bCoffinJumped && CoffinA - 15.f - Cat->GetA() <= FMath::Max(CoffinLead, 60.f))
	{
		bCoffinJumped = true;
		if (Scenario == TEXT("beam"))
		{
			RequestDrop();
		}
		else if (Scenario != TEXT("pit"))
		{
			RequestJump();
		}
	}
}

void ACatRunGameMode::TickScenario()
{
	if (Scenario == TEXT("coffin") || Scenario == TEXT("beam") || Scenario == TEXT("pit"))
	{
		TickCoffinTest();
		return;
	}
	if (Scenario != TEXT("bombgap") || Phase == ERunPhase::Menu)
	{
		return;
	}
	const AInkCanvas* C = Director->GetCanvas();
	const FCircuitLayout& L = Director->Layout;
	const float D = Cat->GetTravel();
	const float Lap = L.LaneLength(2);
	auto Shot = [this](const TCHAR* Name)
	{
		if (!ScenarioShotDir.IsEmpty())
		{
			FScreenshotRequest::RequestScreenshot(ScenarioShotDir / Name, true, false);
		}
	};
	auto Log = [&](const TCHAR* What)
	{
		UE_LOG(LogShadowCat, Log, TEXT("SZENARIO %s | K1-R %d/%d (%.1f%%), gesamt %d/%d, Slot %s, Weg %.0f m"), What,
			C->CountPainted(2), C->CountSections(2), C->LaneProgress(2) * 100.f, C->GetTotalPainted(), C->GetTotalSections(),
			*Cat->GetSlot()->GetDisplayName().ToString(), D / 100.f);
	};
	switch (ScenarioStep)
	{
	case 0:
		if (D >= ScenarioGapArc - 450.f)
		{
			Cat->RequestLaneShift(-1, false);
			Log(TEXT("Runde 1: weiche absichtlich aus (Luecke beginnt)"));
			++ScenarioStep;
		}
		break;
	case 1:
		if (D >= ScenarioGapArc + 250.f)
		{
			Cat->RequestLaneShift(1, false);
			++ScenarioStep;
		}
		break;
	case 2:
		if (D >= ScenarioGapArc + 900.f)
		{
			Log(TEXT("Runde 1: Luecke gelassen"));
			++ScenarioStep;
		}
		break;
	case 3:
		if (Cat->GetSlot()->HasItem())
		{
			Log(TEXT("Tintenbombe im Slot - wird aufgespart"));
			++ScenarioStep;
		}
		break;
	case 4:
		if (D >= Lap + 200.f)
		{
			// Runde 2 auf der mittleren Fahrbahn: die Spur faerbt die Luecke NICHT
			Cat->RequestLaneShift(-1, false);
			Log(TEXT("Runde 1 beendet, Runde 2 auf der Mitte"));
			++ScenarioStep;
		}
		break;
	case 5:
		if (D >= Lap + ScenarioGapArc - 1100.f)
		{
			Log(TEXT("Runde 2: Luecke voraus"));
			Shot(TEXT("scen_gap_ahead.png"));
			++ScenarioStep;
		}
		break;
	case 6:
		if (D >= Lap + ScenarioGapArc - 60.f)
		{
			Log(TEXT("vor Bombe"));
			RequestUseItem();
			Log(TEXT("nach Bombe"));
			++ScenarioStep;
		}
		break;
	case 7:
		if (Phase == ERunPhase::Won)
		{
			UE_LOG(LogShadowCat, Log, TEXT("SZENARIO ERFOLGREICH: Level nach der letzten Luecke beendet"));
			++ScenarioStep;
		}
		else if (D >= Lap + ScenarioGapArc + 800.f)
		{
			Log(TEXT("FEHLER: Level nicht beendet"));
			++ScenarioStep;
		}
		break;
	default:
		break;
	}
}

void ACatRunGameMode::TickTests(float DeltaTime)
{
	TickScenario();
	if (bAutoStart && Phase == ERunPhase::Menu && PlayTime > 1.f)
	{
		RequestStart();
	}
	if (JumpEvery > 0.f && Phase == ERunPhase::Running && PlayTime >= NextTestJump)
	{
		NextTestJump = PlayTime + JumpEvery;
		RequestJump();
	}
	if (PilotOffAt > 0.f && PlayTime >= PilotOffAt)
	{
		PilotOffAt = -1.f;
		bAutopilot = false;
		Director->bAutopilot = false;
	}
	if (bAutopilot && Phase == ERunPhase::GameOver && PhaseTime > 1.5f)
	{
		RequestStart();
	}
	// Test: -CatShopShot=a.png oeffnet den Shop im Startbildschirm und macht ein Bild
	static FString ShopShot = [] { FString S; FParse::Value(FCommandLine::Get(), TEXT("CatShopShot="), S, false); return S; }();
	if (!ShopShot.IsEmpty() && Phase == ERunPhase::Menu && PlayTime > 2.f)
	{
		static bool bOpened = false;
		if (!bOpened)
		{
			bOpened = true;
			RequestShop();
		}
		else if (PlayTime > 3.f)
		{
			FScreenshotRequest::RequestScreenshot(ShopShot, true, false);
			ShopShot.Reset();
		}
	}
	// Test: -CatPauseTest=dir  nach 8 s Pause -> Bild, Shop -> Bild, zurueck, weiter, Bombe -> Bilder (kauft nichts)
	static FString PauseDir = [] { FString S; FParse::Value(FCommandLine::Get(), TEXT("CatPauseTest="), S, false); return S; }();
	if (!PauseDir.IsEmpty() && Phase == ERunPhase::Running && PlayTime > 8.f)
	{
		const FString Dir = PauseDir;
		PauseDir.Reset();
		RequestPause();
		// waehrend der Pause steht Tick still -> Ablauf ueber den Core-Ticker
		TSharedRef<int32> Step = MakeShared<int32>(0);
		TWeakObjectPtr<ACatRunGameMode> Self(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self, Dir, Step](float) -> bool
		{
			ACatRunGameMode* GM = Self.Get();
			if (!GM)
			{
				return false;
			}
			switch ((*Step)++)
			{
			case 0: FScreenshotRequest::RequestScreenshot(Dir / TEXT("pause.png"), true, false); break;
			case 1: GM->RequestShop(); break;
			case 2: FScreenshotRequest::RequestScreenshot(Dir / TEXT("pshop.png"), true, false); break;
			case 3: GM->RequestShopBack(); break;
			case 4: GM->RequestResume(); break;
			case 5: GM->Cat->GetSlot()->GrantClass(UBuff_InkBomb::StaticClass(), 1); GM->RequestUseItem(1); break;
			case 6: FScreenshotRequest::RequestScreenshot(Dir / TEXT("bomb1.png"), true, false); break;
			case 7: FScreenshotRequest::RequestScreenshot(Dir / TEXT("bomb2.png"), true, false); break;
			default:
				FScreenshotRequest::RequestScreenshot(Dir / TEXT("bomb3.png"), true, false);
				return false;
			}
			return true;
		}), 0.6f);
	}
	if (!GameOverShot.IsEmpty() && Phase == ERunPhase::GameOver && PhaseTime > 0.4f)
	{
		FScreenshotRequest::RequestScreenshot(GameOverShot, true, false);
		GameOverShot.Reset();
	}
	if (!WinShot.IsEmpty() && Phase == ERunPhase::Won && PhaseTime > 1.2f)
	{
		FScreenshotRequest::RequestScreenshot(WinShot, true, false);
		UE_LOG(LogShadowCat, Log, TEXT("Screenshot Erfolg %s"), *WinShot);
		WinShot.Reset();
	}
	for (int32 I = Shots.Num() - 1; I >= 0; --I)
	{
		if (PlayTime >= Shots[I].Key)
		{
			FScreenshotRequest::RequestScreenshot(Shots[I].Value, true, false);
			UE_LOG(LogShadowCat, Log, TEXT("Screenshot %s (Phase %d, %.0f m)"), *Shots[I].Value, int32(Phase), Director->GetMeters());
			Shots.RemoveAt(I);
		}
	}
	if (QuitAt > 0.f && PlayTime >= QuitAt)
	{
		QuitAt = -1.f;
		UE_LOG(LogShadowCat, Log, TEXT("Testende nach %.0f s: %d Laeufe"), PlayTime, RunCount);
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	}
}
