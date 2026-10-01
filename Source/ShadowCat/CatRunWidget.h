#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CatRunWidget.generated.h"

class ACatRunGameMode;
class UCatBuff;
class UTextBlock;
class UButton;
class UWidget;
class UVerticalBox;
class UProgressBar;
class UBorder;
class UImage;
class UTexture2D;

/** Eine Zeile im Shop. */
struct FShopRow
{
	FString Name;
	FString Desc;
	/** Preis ("250") oder Status ("MAX", "GEKAUFT") */
	FString Price;
	bool bCanBuy = false;
};

/**
 * Gesamte Oberflaeche (Start, HUD, Erfolg, Game Over), komplett in C++ aufgebaut.
 * Alles Bedienbare liegt in einer SafeZone (Notch, Dynamic Island, Home-Indikator).
 * Layout fuer 1080 px Breite (Hochformat), skaliert per DPI-Regel.
 */
UCLASS()
class SHADOWCAT_API UCatRunWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetGame(ACatRunGameMode* InGame) { Game = InGame; }

	void ShowMenu(float BestTime, int32 HighScore, bool bEndlessLast, const FString& QualityLabel, bool bStatsOn, int32 TotalCoins);
	void ShowRunHud(bool bEndless);
	/** Shop: Muenzen und Angebote (Upgrades, Leben). */
	void ShowShop(int32 Coins, const TArray<FShopRow>& Rows);
	/** Pause-Seite (Weiter, Shop, Lauf beenden) bzw. zurueck zum HUD */
	void ShowPause(int32 Coins);
	void HidePause();
	void ShowGameOver(int32 Score, int32 Painted, int32 Total, bool bEndless, int32 HighScore, bool bNewRecord, float Meters, int32 Coins);
	void ShowWin(int32 Score, float Seconds, float BestTime, bool bNewBest);
	/** ItemIcons/ItemCounts: je Item-Sorte Symbol und Anzahl (gestapelt); RollIcon: waehrend der Auslosung, sonst nullptr. */
	void UpdateHud(int32 Score, float Seconds, const TArray<float>& LaneProgress, int32 LanesPerCircuit, int32 CurrentCircuit,
		const TArray<UTexture2D*>& ItemIcons, const TArray<int32>& ItemCounts, UTexture2D* RollIcon, const TArray<UCatBuff*>& Buffs, int32 Lives, int32 MaxLives, bool bEndless, float Meters, int32 Coins);
	void SetStats(const FString& Text, bool bVisible);
	void FlashMessage(const FString& Text);

	static FString FormatTime(float Seconds);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildTree();
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = true);
	UWidget* MakeButton(const FString& Label, float Width, float Height, int32 FontSize, bool bPrimary, UButton*& OutButton, UTextBlock*& OutLabel);
	UWidget* MakeEndPage(const FString& Title, TObjectPtr<UTextBlock>& OutLine1, TObjectPtr<UTextBlock>& OutBig, TObjectPtr<UTextBlock>& OutLine2, TObjectPtr<UTextBlock>& OutLine3);
	UWidget* Spacer(float Height);
	UWidget* FillSpacer();

	UFUNCTION() void HandleStart();
	UFUNCTION() void HandleRestart();
	UFUNCTION() void HandleMenu();
	UFUNCTION() void HandleQuality();
	UFUNCTION() void HandleStats();
	UFUNCTION() void HandleItem0();
	UFUNCTION() void HandleItem1();
	UFUNCTION() void HandleItem2();
	UFUNCTION() void HandleEndless();
	UFUNCTION() void HandleTutorial();
	UFUNCTION() void HandleShop();
	UFUNCTION() void HandleShopBack();
	UFUNCTION() void HandlePause();
	UFUNCTION() void HandleResume();
	UFUNCTION() void HandleQuitRun();
	UFUNCTION() void HandleBuy0();
	UFUNCTION() void HandleBuy1();
	UFUNCTION() void HandleBuy2();
	UFUNCTION() void HandleBuy3();
	UFUNCTION() void HandleBuy4();
	UFUNCTION() void HandleBuy5();

	TWeakObjectPtr<ACatRunGameMode> Game;

	UPROPERTY(Transient) TObjectPtr<UBorder> Dim;
	UPROPERTY(Transient) TObjectPtr<UWidget> MenuPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> HudPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> OverPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> WinPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> ShopPage;
	UPROPERTY(Transient) TObjectPtr<UWidget> PausePage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PauseCoins;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ShopCoins;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> ShopRows;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ShopNames;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ShopDescs;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ShopPrices;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ShopButtons;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> MenuBest;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MenuRecord;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MenuCoins;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HudCoins;
	UPROPERTY(Transient) TObjectPtr<UWidget> CoinRow;
	/** Item-Spalte rechts: je Sorte ein Symbol-Button mit Anzahl, darueber das Auslosungs-Symbol */
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> ItemSlots;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> ItemImages;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ItemCountTexts;
	UPROPERTY(Transient) TObjectPtr<UWidget> RollBox;
	UPROPERTY(Transient) TObjectPtr<UImage> RollImage;
	UPROPERTY(Transient) TObjectPtr<UWidget> ProgRows;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> LifePips;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QualityLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatsLabel;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> HudScore;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HudTime;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HudTotal;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Message;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> CircuitLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> CircuitRows;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> LaneBars;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> BuffRows;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> BuffNames;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> BuffTimes;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> BuffBars;


	UPROPERTY(Transient) TObjectPtr<UTextBlock> OverLine1;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OverBig;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OverLine2;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OverLine3;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WinLine1;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WinBig;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WinLine2;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WinLine3;

	float MessageTime = 0.f;
	float Time = 0.f;
	TArray<TWeakObjectPtr<UTexture2D>> ShownIcons;
	TWeakObjectPtr<UTexture2D> ShownRollIcon;
};
