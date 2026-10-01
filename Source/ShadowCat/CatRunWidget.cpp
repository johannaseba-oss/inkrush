#include "CatRunWidget.h"
#include "CatRunGameMode.h"
#include "CatBuff.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Engine/Texture2D.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SafeZone.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor White(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Gray(0.55f, 0.55f, 0.55f, 1.f);
	const FLinearColor Black(0.f, 0.f, 0.f, 1.f);

	FSlateBrush RoundBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.Color = FSlateColor(Outline);
		B.OutlineSettings.Width = OutlineWidth;
		return B;
	}

	/** Item-Button ohne Hintergrund: sichtbar ist nur das Symbol. */
	FButtonStyle ItemStyle()
	{
		FButtonStyle IS;
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		IS.Normal = None;
		IS.Hovered = None;
		IS.Pressed = None;
		IS.Disabled = None;
		IS.NormalPadding = FMargin(0.f);
		IS.PressedPadding = FMargin(0.f);
		return IS;
	}

	FProgressBarStyle BarStyle(float Radius)
	{
		FProgressBarStyle PS;
		// weiss = noch zu faerben, schwarz = eingefaerbt (wie auf der Strecke)
		PS.BackgroundImage = RoundBrush(FLinearColor(0.9f, 0.9f, 0.9f, 0.9f), Radius);
		PS.FillImage = RoundBrush(FLinearColor(0.02f, 0.02f, 0.02f, 1.f), Radius, FLinearColor(1.f, 1.f, 1.f, 0.9f), 2.f);
		PS.MarqueeImage = PS.FillImage;
		return PS;
	}
}

FString UCatRunWidget::FormatTime(float Seconds)
{
	const int32 S = FMath::Max(0, FMath::FloorToInt(Seconds));
	return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
}

TSharedRef<SWidget> UCatRunWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

UTextBlock* UCatRunWidget::MakeText(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
	T->SetText(FText::FromString(Text));
	T->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
	T->SetColorAndOpacity(FSlateColor(Color));
	T->SetShadowOffset(FVector2D(0.f, 3.f));
	T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	T->SetJustification(ETextJustify::Center);
	return T;
}

UWidget* UCatRunWidget::MakeButton(const FString& Label, float Width, float Height, int32 FontSize, bool bPrimary, UButton*& OutButton, UTextBlock*& OutLabel)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(Width);
	Box->SetHeightOverride(Height);

	UButton* Btn = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle Style;
	const FLinearColor Fill = bPrimary ? FLinearColor(0.93f, 0.93f, 0.93f, 1.f) : FLinearColor(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor Line = bPrimary ? FLinearColor::Transparent : FLinearColor(1.f, 1.f, 1.f, 0.75f);
	Style.Normal = RoundBrush(Fill, 28.f, Line, 3.f);
	Style.Hovered = RoundBrush(bPrimary ? White : FLinearColor(0.15f, 0.15f, 0.15f, 0.7f), 28.f, Line, 3.f);
	Style.Pressed = RoundBrush(bPrimary ? FLinearColor(0.65f, 0.65f, 0.65f, 1.f) : FLinearColor(0.3f, 0.3f, 0.3f, 0.8f), 28.f, Line, 3.f);
	Style.Disabled = Style.Normal;
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = FMargin(0.f);
	Btn->SetStyle(Style);
	Btn->SetClickMethod(EButtonClickMethod::MouseDown);
	Btn->SetTouchMethod(EButtonTouchMethod::PreciseTap);

	UTextBlock* Text = MakeText(Label, FontSize, bPrimary ? Black : White);
	if (bPrimary)
	{
		Text->SetShadowOffset(FVector2D::ZeroVector);
	}
	Btn->AddChild(Text);
	Box->AddChild(Btn);
	OutButton = Btn;
	OutLabel = Text;
	return Box;
}

UWidget* UCatRunWidget::Spacer(float Height)
{
	USpacer* S = WidgetTree->ConstructWidget<USpacer>();
	S->SetSize(FVector2D(1.f, Height));
	return S;
}

UWidget* UCatRunWidget::FillSpacer()
{
	return WidgetTree->ConstructWidget<USpacer>();
}

UWidget* UCatRunWidget::MakeEndPage(const FString& Title, TObjectPtr<UTextBlock>& OutLine1, TObjectPtr<UTextBlock>& OutBig, TObjectPtr<UTextBlock>& OutLine2, TObjectPtr<UTextBlock>& OutLine3)
{
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	auto Row = [Box](UWidget* W, bool bFill = false)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W);
		S->SetHorizontalAlignment(HAlign_Center);
		if (bFill)
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};
	Row(FillSpacer(), true);
	Row(MakeText(Title, Title.Len() > 10 ? 66 : 84, White));
	Row(Spacer(50.f));
	OutLine1 = MakeText(TEXT(""), 34, Gray, false);
	Row(OutLine1);
	OutBig = MakeText(TEXT("0"), 120, White);
	Row(OutBig);
	OutLine2 = MakeText(TEXT(""), 44, White);
	Row(OutLine2);
	Row(Spacer(14.f));
	OutLine3 = MakeText(TEXT(""), 38, Gray);
	Row(OutLine3);
	Row(Spacer(80.f));
	UButton* Retry = nullptr;
	UButton* Menu = nullptr;
	UTextBlock* L1 = nullptr;
	UTextBlock* L2 = nullptr;
	Row(MakeButton(TEXT("NOCHMAL"), 620.f, 170.f, 58, true, Retry, L1));
	Row(Spacer(32.f));
	Row(MakeButton(TEXT("MENÜ"), 420.f, 110.f, 36, false, Menu, L2));
	Retry->OnClicked.AddDynamic(this, &UCatRunWidget::HandleRestart);
	Menu->OnClicked.AddDynamic(this, &UCatRunWidget::HandleMenu);
	Row(FillSpacer(), true);
	return Box;
}

void UCatRunWidget::BuildTree()
{
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = RootOverlay;

	Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* S = RootOverlay->AddChildToOverlay(Dim))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	USafeZone* Safe = WidgetTree->ConstructWidget<USafeZone>();
	if (UOverlaySlot* S = RootOverlay->AddChildToOverlay(Safe))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	UOverlay* Inner = WidgetTree->ConstructWidget<UOverlay>();
	Safe->AddChild(Inner);
	auto AddPage = [Inner](UWidget* W, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Pad)
	{
		if (UOverlaySlot* S = Inner->AddChildToOverlay(W))
		{
			S->SetHorizontalAlignment(H);
			S->SetVerticalAlignment(V);
			S->SetPadding(Pad);
		}
	};
	auto AddRow = [](UVerticalBox* Box, UWidget* W, bool bFill = false)
	{
		UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W);
		S->SetHorizontalAlignment(HAlign_Center);
		if (bFill)
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};

	// ---------------- Startbildschirm ----------------
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, Spacer(150.f));
		AddRow(Box, MakeText(TEXT("INKRUSH"), 124, White));
		AddRow(Box, FillSpacer(), true);
		MenuRecord = MakeText(TEXT("REKORD  0"), 40, White);
		AddRow(Box, MenuRecord);
		MenuBest = MakeText(TEXT("TUTORIAL  -"), 30, Gray, false);
		AddRow(Box, MenuBest);
		MenuCoins = MakeText(TEXT("MÜNZEN  0"), 30, Gray, false);
		AddRow(Box, MenuCoins);
		AddRow(Box, Spacer(30.f));
		UButton* Endless = nullptr;
		UTextBlock* EndlessLabel = nullptr;
		AddRow(Box, MakeButton(TEXT("ENDLOS"), 620.f, 160.f, 62, true, Endless, EndlessLabel));
		Endless->OnClicked.AddDynamic(this, &UCatRunWidget::HandleEndless);
		AddRow(Box, Spacer(22.f));
		// Tutorial und Shop nebeneinander
		UHorizontalBox* Second = WidgetTree->ConstructWidget<UHorizontalBox>();
		UButton* Tut = nullptr;
		UTextBlock* TutLabel = nullptr;
		UButton* Shop = nullptr;
		UTextBlock* ShopLabel = nullptr;
		Second->AddChildToHorizontalBox(MakeButton(TEXT("TUTORIAL"), 300.f, 110.f, 36, false, Tut, TutLabel));
		Second->AddChildToHorizontalBox(Spacer(1.f))->SetPadding(FMargin(10.f, 0.f));
		Second->AddChildToHorizontalBox(MakeButton(TEXT("SHOP"), 300.f, 110.f, 36, false, Shop, ShopLabel));
		Tut->OnClicked.AddDynamic(this, &UCatRunWidget::HandleTutorial);
		Shop->OnClicked.AddDynamic(this, &UCatRunWidget::HandleShop);
		AddRow(Box, Second);
		AddRow(Box, Spacer(34.f));
		UHorizontalBox* Opts = WidgetTree->ConstructWidget<UHorizontalBox>();
		UButton* Q = nullptr;
		UButton* St = nullptr;
		UTextBlock* QL = nullptr;
		UTextBlock* SL = nullptr;
		Opts->AddChildToHorizontalBox(MakeButton(TEXT("GRAFIK: MITTEL"), 330.f, 104.f, 28, false, Q, QL));
		Opts->AddChildToHorizontalBox(Spacer(1.f))->SetPadding(FMargin(12.f, 0.f));
		Opts->AddChildToHorizontalBox(MakeButton(TEXT("LEISTUNG: AUS"), 330.f, 104.f, 28, false, St, SL));
		QualityLabel = QL;
		StatsLabel = SL;
		Q->OnClicked.AddDynamic(this, &UCatRunWidget::HandleQuality);
		St->OnClicked.AddDynamic(this, &UCatRunWidget::HandleStats);
		AddRow(Box, Opts);
		AddRow(Box, Spacer(70.f));
		MenuPage = Box;
		AddPage(Box, HAlign_Fill, VAlign_Fill, FMargin(40.f, 0.f));
	}

	// ---------------- Lauf-HUD ----------------
	// oben links Pause-Taste, daneben Score/Zeit/Leben; oben rechts grosse Muenzanzeige;
	// rechts mittig ein abgerundetes Inventar mit Wolke und Bombe untereinander (antippen = einsetzen)
	{
		UOverlay* Hud = WidgetTree->ConstructWidget<UOverlay>();
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		Box->SetVisibility(ESlateVisibility::HitTestInvisible);
		UHorizontalBox* Top = WidgetTree->ConstructWidget<UHorizontalBox>();
		UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
		HudScore = MakeText(TEXT("0"), 72, White);
		HudScore->SetJustification(ETextJustify::Left);
		HudTime = MakeText(TEXT("0:00"), 28, Gray, false);
		HudTime->SetJustification(ETextJustify::Left);
		Left->AddChildToVerticalBox(HudScore);
		Left->AddChildToVerticalBox(HudTime);
		UHorizontalBox* Pips = WidgetTree->ConstructWidget<UHorizontalBox>();
		// bis zu 15 Leben (9 + im Shop gekaufte)
		for (int32 I = 0; I < 15; ++I)
		{
			USizeBox* PipBox = WidgetTree->ConstructWidget<USizeBox>();
			PipBox->SetWidthOverride(26.f);
			PipBox->SetHeightOverride(26.f);
			UBorder* Pip = WidgetTree->ConstructWidget<UBorder>();
			Pip->SetBrush(RoundBrush(White, 13.f, White, 2.f));
			PipBox->AddChild(Pip);
			Pips->AddChildToHorizontalBox(PipBox)->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
			LifePips.Add(Pip);
		}
		Left->AddChildToVerticalBox(Pips)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		// Platz fuer die Pause-Taste links daneben
		Top->AddChildToHorizontalBox(Left)->SetPadding(FMargin(132.f, 0.f, 0.f, 0.f));
		Top->AddChildToHorizontalBox(FillSpacer())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBox* Prog = WidgetTree->ConstructWidget<UVerticalBox>();
		// Muenzen (Guthaben): grosse schwarze Muenze mit hellem Rand + Anzahl
		{
			UHorizontalBox* CoinH = WidgetTree->ConstructWidget<UHorizontalBox>();
			USizeBox* CoinSize = WidgetTree->ConstructWidget<USizeBox>();
			CoinSize->SetWidthOverride(64.f);
			CoinSize->SetHeightOverride(64.f);
			UBorder* CoinDot = WidgetTree->ConstructWidget<UBorder>();
			CoinDot->SetBrush(RoundBrush(FLinearColor(0.02f, 0.02f, 0.02f, 1.f), 32.f, White, 6.f));
			CoinSize->AddChild(CoinDot);
			CoinH->AddChildToHorizontalBox(CoinSize)->SetVerticalAlignment(VAlign_Center);
			HudCoins = MakeText(TEXT("0"), 72, White);
			CoinH->AddChildToHorizontalBox(HudCoins)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
			CoinRow = CoinH;
			Prog->AddChildToVerticalBox(CoinH)->SetHorizontalAlignment(HAlign_Right);
		}
		// Tutorial: Fortschritt in Prozent und je Kurs eine Zeile mit L / M / R
		HudTotal = MakeText(TEXT("0%"), 30, White);
		HudTotal->SetJustification(ETextJustify::Right);
		Prog->AddChildToVerticalBox(HudTotal)->SetHorizontalAlignment(HAlign_Right);
		UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>();
		ProgRows = Rows;
		Prog->AddChildToVerticalBox(Rows)->SetHorizontalAlignment(HAlign_Right);
		for (int32 C = 0; C < 3; ++C)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
			UTextBlock* Label = MakeText(FString::Printf(TEXT("K%d"), C + 1), 28, White);
			USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
			LabelBox->SetWidthOverride(64.f);
			LabelBox->AddChild(Label);
			Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
			for (int32 L = 0; L < 3; ++L)
			{
				USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
				BarBox->SetWidthOverride(110.f);
				BarBox->SetHeightOverride(22.f);
				UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
				Bar->SetWidgetStyle(BarStyle(6.f));
				Bar->SetFillColorAndOpacity(White);
				BarBox->AddChild(Bar);
				Row->AddChildToHorizontalBox(BarBox)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
				LaneBars.Add(Bar);
			}
			Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			CircuitLabels.Add(Label);
			CircuitRows.Add(Row);
		}
		UTextBlock* Legend = MakeText(TEXT("L          M          R   "), 20, Gray, false);
		Rows->AddChildToVerticalBox(Legend)->SetHorizontalAlignment(HAlign_Right);
		Top->AddChildToHorizontalBox(Prog)->SetVerticalAlignment(VAlign_Top);
		Box->AddChildToVerticalBox(Top);

		UVerticalBox* BuffBox = WidgetTree->ConstructWidget<UVerticalBox>();
		for (int32 I = 0; I < 2; ++I)
		{
			UBorder* Row = WidgetTree->ConstructWidget<UBorder>();
			Row->SetBrush(RoundBrush(FLinearColor(0.f, 0.f, 0.f, 0.55f), 18.f, FLinearColor(1.f, 1.f, 1.f, 0.35f), 2.f));
			Row->SetPadding(FMargin(22.f, 12.f));
			UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
			UTextBlock* Name = MakeText(TEXT("BUFF"), 30, White);
			Name->SetJustification(ETextJustify::Left);
			UTextBlock* Secs = MakeText(TEXT("0.0 s"), 30, White, false);
			H->AddChildToHorizontalBox(Name)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			H->AddChildToHorizontalBox(Secs);
			V->AddChildToVerticalBox(H);
			USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
			BarBox->SetWidthOverride(420.f);
			BarBox->SetHeightOverride(18.f);
			UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
			FProgressBarStyle PS;
			PS.BackgroundImage = RoundBrush(FLinearColor(1.f, 1.f, 1.f, 0.15f), 9.f);
			PS.FillImage = RoundBrush(White, 9.f);
			PS.MarqueeImage = PS.FillImage;
			Bar->SetWidgetStyle(PS);
			BarBox->AddChild(Bar);
			V->AddChildToVerticalBox(BarBox)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			Row->SetContent(V);
			BuffBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
			Row->SetVisibility(ESlateVisibility::Collapsed);
			BuffRows.Add(Row);
			BuffNames.Add(Name);
			BuffTimes.Add(Secs);
			BuffBars.Add(Bar);
		}
		Box->AddChildToVerticalBox(BuffBox)->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
		Box->AddChildToVerticalBox(FillSpacer())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Message = MakeText(TEXT(""), 40, White);
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Message))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 330.f));
		}
		if (UOverlaySlot* S = Hud->AddChildToOverlay(Box))
		{
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Fill);
		}

		// Pause-Taste oben links (eigene Ebene, damit sie antippbar bleibt)
		{
			UButton* PauseBtn = nullptr;
			UTextBlock* PauseLabel = nullptr;
			UWidget* PauseW = MakeButton(TEXT("II"), 108.f, 108.f, 44, false, PauseBtn, PauseLabel);
			PauseBtn->OnClicked.AddDynamic(this, &UCatRunWidget::HandlePause);
			if (UOverlaySlot* S = Hud->AddChildToOverlay(PauseW))
			{
				S->SetHorizontalAlignment(HAlign_Left);
				S->SetVerticalAlignment(VAlign_Top);
				S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
			}
		}

		// Inventar rechts: abgerundetes Feld, darin je Sorte ein Kaestchen mit Symbol und Anzahl
		UBorder* Inv = WidgetTree->ConstructWidget<UBorder>();
		Inv->SetBrush(RoundBrush(FLinearColor(0.f, 0.f, 0.f, 0.5f), 30.f, FLinearColor(1.f, 1.f, 1.f, 0.4f), 3.f));
		Inv->SetPadding(FMargin(12.f));
		UVerticalBox* ItemCol = WidgetTree->ConstructWidget<UVerticalBox>();
		Inv->SetContent(ItemCol);
		auto MakeIcon = [this](float Size, UImage*& OutImage)
		{
			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
			IconBox->SetWidthOverride(Size);
			IconBox->SetHeightOverride(Size);
			// ScaleBox: Symbol mit richtigem Seitenverhaeltnis einpassen
			UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
			Fit->SetStretch(EStretch::ScaleToFit);
			OutImage = WidgetTree->ConstructWidget<UImage>();
			Fit->AddChild(OutImage);
			IconBox->AddChild(Fit);
			return IconBox;
		};
		// waehrend der Auslosung: wechselndes Symbol oben im Inventar
		UImage* RollImg = nullptr;
		RollBox = MakeIcon(118.f, RollImg);
		RollImage = RollImg;
		RollBox->SetVisibility(ESlateVisibility::Collapsed);
		ItemCol->AddChildToVerticalBox(RollBox)->SetHorizontalAlignment(HAlign_Center);
		FButtonStyle Cell;
		Cell.Normal = RoundBrush(FLinearColor(1.f, 1.f, 1.f, 0.08f), 22.f, FLinearColor(1.f, 1.f, 1.f, 0.25f), 2.f);
		Cell.Hovered = RoundBrush(FLinearColor(1.f, 1.f, 1.f, 0.16f), 22.f, FLinearColor(1.f, 1.f, 1.f, 0.4f), 2.f);
		Cell.Pressed = RoundBrush(FLinearColor(1.f, 1.f, 1.f, 0.3f), 22.f, FLinearColor(1.f, 1.f, 1.f, 0.6f), 2.f);
		Cell.Disabled = Cell.Normal;
		Cell.NormalPadding = FMargin(0.f);
		Cell.PressedPadding = FMargin(0.f);
		for (int32 I = 0; I < 3; ++I)
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>();
			Btn->SetStyle(Cell);
			Btn->SetClickMethod(EButtonClickMethod::MouseDown);
			Btn->SetTouchMethod(EButtonTouchMethod::PreciseTap);
			USizeBox* CellBox = WidgetTree->ConstructWidget<USizeBox>();
			CellBox->SetWidthOverride(140.f);
			CellBox->SetHeightOverride(140.f);
			UOverlay* Ov = WidgetTree->ConstructWidget<UOverlay>();
			UImage* Img = nullptr;
			if (UOverlaySlot* IS = Ov->AddChildToOverlay(MakeIcon(112.f, Img)))
			{
				IS->SetHorizontalAlignment(HAlign_Center);
				IS->SetVerticalAlignment(VAlign_Center);
			}
			UTextBlock* Cnt = MakeText(TEXT(""), 34, White);
			if (UOverlaySlot* CS = Ov->AddChildToOverlay(Cnt))
			{
				CS->SetHorizontalAlignment(HAlign_Right);
				CS->SetVerticalAlignment(VAlign_Bottom);
				CS->SetPadding(FMargin(0.f, 0.f, 8.f, 2.f));
			}
			CellBox->AddChild(Ov);
			Btn->AddChild(CellBox);
			if (I == 0) Btn->OnClicked.AddDynamic(this, &UCatRunWidget::HandleItem0);
			if (I == 1) Btn->OnClicked.AddDynamic(this, &UCatRunWidget::HandleItem1);
			if (I == 2) Btn->OnClicked.AddDynamic(this, &UCatRunWidget::HandleItem2);
			Btn->SetVisibility(ESlateVisibility::Collapsed);
			ItemCol->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, I == 0 ? 0.f : 12.f, 0.f, 0.f));
			ItemSlots.Add(Btn);
			ItemImages.Add(Img);
			ItemCountTexts.Add(Cnt);
			ShownIcons.Add(nullptr);
		}
		if (UOverlaySlot* S = Hud->AddChildToOverlay(Inv))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Center);
		}
		Hud->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		HudPage = Hud;
		AddPage(Hud, HAlign_Fill, VAlign_Fill, FMargin(36.f, 24.f));
	}

	// ---------------- Pause ----------------
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		AddRow(Box, FillSpacer(), true);
		AddRow(Box, MakeText(TEXT("PAUSE"), 110, White));
		PauseCoins = MakeText(TEXT("MÜNZEN  0"), 40, White);
		AddRow(Box, PauseCoins);
		AddRow(Box, Spacer(80.f));
		UButton* Resume = nullptr;
		UButton* Shop = nullptr;
		UButton* Quit = nullptr;
		UTextBlock* L1 = nullptr;
		UTextBlock* L2 = nullptr;
		UTextBlock* L3 = nullptr;
		AddRow(Box, MakeButton(TEXT("WEITER"), 620.f, 160.f, 60, true, Resume, L1));
		AddRow(Box, Spacer(28.f));
		AddRow(Box, MakeButton(TEXT("SHOP"), 460.f, 120.f, 40, false, Shop, L2));
		AddRow(Box, Spacer(22.f));
		AddRow(Box, MakeButton(TEXT("LAUF BEENDEN"), 460.f, 110.f, 32, false, Quit, L3));
		Resume->OnClicked.AddDynamic(this, &UCatRunWidget::HandleResume);
		Shop->OnClicked.AddDynamic(this, &UCatRunWidget::HandleShop);
		Quit->OnClicked.AddDynamic(this, &UCatRunWidget::HandleQuitRun);
		AddRow(Box, FillSpacer(), true);
		PausePage = Box;
		AddPage(Box, HAlign_Fill, VAlign_Fill, FMargin(40.f, 0.f));
	}

	// ---------------- Game Over / Erfolg ----------------
	OverPage = MakeEndPage(TEXT("GAME OVER"), OverLine1, OverBig, OverLine2, OverLine3);
	AddPage(OverPage, HAlign_Fill, VAlign_Fill, FMargin(40.f, 0.f));
	WinPage = MakeEndPage(TEXT("LEVEL GESCHAFFT!"), WinLine1, WinBig, WinLine2, WinLine3);
	AddPage(WinPage, HAlign_Fill, VAlign_Fill, FMargin(40.f, 0.f));

	// ---------------- Shop ----------------
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		auto Row = [Box](UWidget* W, EHorizontalAlignment H = HAlign_Center)
		{
			UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W);
			S->SetHorizontalAlignment(H);
			return S;
		};
		Row(Spacer(130.f));
		Row(MakeText(TEXT("SHOP"), 96, White));
		ShopCoins = MakeText(TEXT("MÜNZEN  0"), 40, White);
		Row(ShopCoins);
		Row(Spacer(40.f));
		for (int32 I = 0; I < 6; ++I)
		{
			UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
			Card->SetBrush(RoundBrush(FLinearColor(0.f, 0.f, 0.f, 0.55f), 22.f, FLinearColor(1.f, 1.f, 1.f, 0.35f), 2.f));
			Card->SetPadding(FMargin(26.f, 16.f));
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
			UVerticalBox* Txt = WidgetTree->ConstructWidget<UVerticalBox>();
			UTextBlock* Name = MakeText(TEXT(""), 34, White);
			Name->SetJustification(ETextJustify::Left);
			UTextBlock* Desc = MakeText(TEXT(""), 24, Gray, false);
			Desc->SetJustification(ETextJustify::Left);
			Txt->AddChildToVerticalBox(Name);
			Txt->AddChildToVerticalBox(Desc);
			H->AddChildToHorizontalBox(Txt)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			UButton* Buy = nullptr;
			UTextBlock* Price = nullptr;
			UHorizontalBoxSlot* BS = H->AddChildToHorizontalBox(MakeButton(TEXT("0"), 230.f, 92.f, 32, true, Buy, Price));
			BS->SetVerticalAlignment(VAlign_Center);
			Card->SetContent(H);
			USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
			CardSize->SetWidthOverride(900.f);
			CardSize->AddChild(Card);
			Row(CardSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
			switch (I)
			{
			case 0: Buy->OnClicked.AddDynamic(this, &UCatRunWidget::HandleBuy0); break;
			case 1: Buy->OnClicked.AddDynamic(this, &UCatRunWidget::HandleBuy1); break;
			case 2: Buy->OnClicked.AddDynamic(this, &UCatRunWidget::HandleBuy2); break;
			case 3: Buy->OnClicked.AddDynamic(this, &UCatRunWidget::HandleBuy3); break;
			case 4: Buy->OnClicked.AddDynamic(this, &UCatRunWidget::HandleBuy4); break;
			default: Buy->OnClicked.AddDynamic(this, &UCatRunWidget::HandleBuy5); break;
			}
			ShopRows.Add(CardSize);
			ShopNames.Add(Name);
			ShopDescs.Add(Desc);
			ShopPrices.Add(Price);
			ShopButtons.Add(Buy);
		}
		Row(FillSpacer())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UButton* Back = nullptr;
		UTextBlock* BackLabel = nullptr;
		Row(MakeButton(TEXT("ZURÜCK"), 420.f, 110.f, 36, false, Back, BackLabel));
		Back->OnClicked.AddDynamic(this, &UCatRunWidget::HandleShopBack);
		Row(Spacer(80.f));
		ShopPage = Box;
		AddPage(Box, HAlign_Fill, VAlign_Fill, FMargin(40.f, 0.f));
	}

	StatsText = MakeText(TEXT(""), 22, White, false);
	StatsText->SetJustification(ETextJustify::Left);
	StatsText->SetVisibility(ESlateVisibility::Collapsed);
	AddPage(StatsText, HAlign_Left, VAlign_Bottom, FMargin(10.f, 0.f, 0.f, 60.f));

	MenuPage->SetVisibility(ESlateVisibility::Collapsed);
	HudPage->SetVisibility(ESlateVisibility::Collapsed);
	OverPage->SetVisibility(ESlateVisibility::Collapsed);
	WinPage->SetVisibility(ESlateVisibility::Collapsed);
	ShopPage->SetVisibility(ESlateVisibility::Collapsed);
	PausePage->SetVisibility(ESlateVisibility::Collapsed);
}

void UCatRunWidget::ShowPause(int32 Coins)
{
	PauseCoins->SetText(FText::FromString(FString::Printf(TEXT("MÜNZEN  %d"), Coins)));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	HudPage->SetVisibility(ESlateVisibility::Collapsed);
	ShopPage->SetVisibility(ESlateVisibility::Collapsed);
	PausePage->SetVisibility(ESlateVisibility::Visible);
}

void UCatRunWidget::HidePause()
{
	Dim->SetVisibility(ESlateVisibility::Collapsed);
	ShopPage->SetVisibility(ESlateVisibility::Collapsed);
	PausePage->SetVisibility(ESlateVisibility::Collapsed);
	HudPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UCatRunWidget::ShowShop(int32 Coins, const TArray<FShopRow>& Rows)
{
	ShopCoins->SetText(FText::FromString(FString::Printf(TEXT("MÜNZEN  %d"), Coins)));
	for (int32 I = 0; I < ShopRows.Num(); ++I)
	{
		if (!Rows.IsValidIndex(I))
		{
			ShopRows[I]->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		ShopRows[I]->SetVisibility(ESlateVisibility::Visible);
		ShopNames[I]->SetText(FText::FromString(Rows[I].Name));
		ShopDescs[I]->SetText(FText::FromString(Rows[I].Desc));
		ShopPrices[I]->SetText(FText::FromString(Rows[I].Price));
		ShopButtons[I]->SetIsEnabled(Rows[I].bCanBuy);
		ShopButtons[I]->SetRenderOpacity(Rows[I].bCanBuy ? 1.f : 0.45f);
	}
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	MenuPage->SetVisibility(ESlateVisibility::Collapsed);
	HudPage->SetVisibility(ESlateVisibility::Collapsed);
	OverPage->SetVisibility(ESlateVisibility::Collapsed);
	WinPage->SetVisibility(ESlateVisibility::Collapsed);
	ShopPage->SetVisibility(ESlateVisibility::Visible);
	PausePage->SetVisibility(ESlateVisibility::Collapsed);
}

void UCatRunWidget::ShowMenu(float BestTime, int32 HighScore, bool bEndlessLast, const FString& InQualityLabel, bool bStatsOn, int32 TotalCoins)
{
	MenuCoins->SetText(FText::FromString(FString::Printf(TEXT("MÜNZEN  %d"), TotalCoins)));
	MenuRecord->SetText(FText::FromString(FString::Printf(TEXT("ENDLOS-REKORD  %d"), HighScore)));
	MenuBest->SetText(FText::FromString(BestTime > 0.f ? FString::Printf(TEXT("TUTORIAL-BESTZEIT  %s"), *FormatTime(BestTime)) : TEXT("TUTORIAL NOCH NICHT GESCHAFFT")));
	QualityLabel->SetText(FText::FromString(FString::Printf(TEXT("GRAFIK: %s"), *InQualityLabel)));
	StatsLabel->SetText(FText::FromString(bStatsOn ? TEXT("LEISTUNG: AN") : TEXT("LEISTUNG: AUS")));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	MenuPage->SetVisibility(ESlateVisibility::Visible);
	HudPage->SetVisibility(ESlateVisibility::Collapsed);
	OverPage->SetVisibility(ESlateVisibility::Collapsed);
	WinPage->SetVisibility(ESlateVisibility::Collapsed);
	ShopPage->SetVisibility(ESlateVisibility::Collapsed);
	PausePage->SetVisibility(ESlateVisibility::Collapsed);
}

void UCatRunWidget::ShowRunHud(bool bEndless)
{
	Dim->SetVisibility(ESlateVisibility::Collapsed);
	MenuPage->SetVisibility(ESlateVisibility::Collapsed);
	HudPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	OverPage->SetVisibility(ESlateVisibility::Collapsed);
	WinPage->SetVisibility(ESlateVisibility::Collapsed);
	ShopPage->SetVisibility(ESlateVisibility::Collapsed);
	ProgRows->SetVisibility(bEndless ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	// Endlos: statt Metern nur die Muenzen; Tutorial: Fortschritt in Prozent
	HudTotal->SetVisibility(bEndless ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	CoinRow->SetVisibility(bEndless ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Message->SetText(FText::GetEmpty());
	MessageTime = 0.f;
	PausePage->SetVisibility(ESlateVisibility::Collapsed);
}

void UCatRunWidget::ShowGameOver(int32 Score, int32 Painted, int32 Total, bool bEndless, int32 HighScore, bool bNewRecord, float Meters, int32 Coins)
{
	OverLine1->SetText(FText::FromString(TEXT("SCORE")));
	OverBig->SetText(FText::AsNumber(Score));
	if (bEndless)
	{
		OverLine2->SetText(FText::FromString(bNewRecord ? TEXT("NEUER REKORD!") : FString::Printf(TEXT("REKORD  %d"), HighScore)));
		OverLine3->SetText(FText::FromString(FString::Printf(TEXT("%d m  ·  +%d MÜNZEN"), FMath::FloorToInt(Meters), Coins)));
	}
	else
	{
		OverLine2->SetText(FText::FromString(FString::Printf(TEXT("TINTE  %d%%"), Total > 0 ? FMath::FloorToInt(100.f * Painted / Total) : 0)));
		OverLine3->SetText(FText::FromString(TEXT("ALLE LEBEN VERBRAUCHT")));
	}
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	HudPage->SetVisibility(ESlateVisibility::Collapsed);
	OverPage->SetVisibility(ESlateVisibility::Visible);
	WinPage->SetVisibility(ESlateVisibility::Collapsed);
	ShopPage->SetVisibility(ESlateVisibility::Collapsed);
	PausePage->SetVisibility(ESlateVisibility::Collapsed);
}

void UCatRunWidget::ShowWin(int32 Score, float Seconds, float BestTime, bool bNewBest)
{
	WinLine1->SetText(FText::FromString(TEXT("ALLE 9 FAHRBAHNEN SCHWARZ")));
	WinBig->SetText(FText::FromString(FormatTime(Seconds)));
	WinLine2->SetText(FText::FromString(bNewBest ? TEXT("NEUE BESTZEIT!") : FString::Printf(TEXT("BESTZEIT  %s"), *FormatTime(BestTime))));
	WinLine3->SetText(FText::FromString(FString::Printf(TEXT("SCORE  %d"), Score)));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	HudPage->SetVisibility(ESlateVisibility::Collapsed);
	OverPage->SetVisibility(ESlateVisibility::Collapsed);
	WinPage->SetVisibility(ESlateVisibility::Visible);
	ShopPage->SetVisibility(ESlateVisibility::Collapsed);
	PausePage->SetVisibility(ESlateVisibility::Collapsed);
}

void UCatRunWidget::UpdateHud(int32 Score, float Seconds, const TArray<float>& LaneProgress, int32 LanesPerCircuit, int32 CurrentCircuit,
	const TArray<UTexture2D*>& ItemIcons, const TArray<int32>& ItemCounts, UTexture2D* RollIcon, const TArray<UCatBuff*>& Buffs, int32 Lives, int32 MaxLives, bool bEndless, float Meters, int32 Coins)
{
	HudScore->SetText(FText::AsNumber(Score));
	HudCoins->SetText(FText::AsNumber(Coins));
	HudTime->SetText(FText::FromString(FormatTime(Seconds)));
	for (int32 I = 0; I < LifePips.Num(); ++I)
	{
		LifePips[I]->SetVisibility(I < MaxLives ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		LifePips[I]->SetRenderOpacity(I < Lives ? 1.f : 0.22f);
	}
	float Sum = 0.f;
	for (int32 I = 0; I < LaneBars.Num(); ++I)
	{
		const float P = LaneProgress.IsValidIndex(I) ? LaneProgress[I] : 0.f;
		LaneBars[I]->SetPercent(P);
		Sum += P;
	}
	HudTotal->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::FloorToInt(100.f * Sum / FMath::Max(1, LaneBars.Num())))));
	for (int32 C = 0; C < CircuitRows.Num(); ++C)
	{
		// aktueller Kurs hervorgehoben
		CircuitRows[C]->SetRenderOpacity(C == CurrentCircuit ? 1.f : 0.55f);
		CircuitLabels[C]->SetText(FText::FromString((C == CurrentCircuit ? FString::Printf(TEXT(">K%d"), C + 1) : FString::Printf(TEXT("K%d"), C + 1))));
	}

	// Items rechts: je Sorte Symbol + Anzahl (ab 2), ausgeblendet bei 0; Auslosung darueber mit wechselndem Symbol
	for (int32 I = 0; I < ItemSlots.Num(); ++I)
	{
		UTexture2D* Tex = ItemIcons.IsValidIndex(I) ? ItemIcons[I] : nullptr;
		const int32 N = ItemCounts.IsValidIndex(I) ? ItemCounts[I] : 0;
		if (ShownIcons[I].Get() != Tex && Tex)
		{
			ShownIcons[I] = Tex;
			ItemImages[I]->SetBrushFromTexture(Tex, true);
		}
		// Inventar: Kaestchen immer da; leer = Symbol blass, sonst Anzahl
		ItemSlots[I]->SetVisibility(Tex ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ItemImages[I]->SetRenderOpacity(N > 0 ? 1.f : 0.22f);
		ItemCountTexts[I]->SetText(FText::FromString(N > 0 ? FString::Printf(TEXT("%d"), N) : FString()));
	}
	if (ShownRollIcon.Get() != RollIcon && RollIcon)
	{
		ShownRollIcon = RollIcon;
		RollImage->SetBrushFromTexture(RollIcon, true);
	}
	RollBox->SetVisibility(RollIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const float RollPulse = 1.f + 0.08f * FMath::Sin(Time * 30.f);
	RollBox->SetRenderScale(FVector2D(RollPulse, RollPulse));
	RollBox->SetRenderOpacity(0.8f);

	for (int32 I = 0; I < BuffRows.Num(); ++I)
	{
		if (Buffs.IsValidIndex(I) && Buffs[I])
		{
			const UCatBuff* B = Buffs[I];
			BuffRows[I]->SetVisibility(ESlateVisibility::HitTestInvisible);
			BuffNames[I]->SetText(B->DisplayName);
			BuffTimes[I]->SetText(FText::FromString(FString::Printf(TEXT("%.1f s"), FMath::Max(0.f, B->GetRemaining()))));
			BuffBars[I]->SetPercent(B->GetFraction());
			const bool bBlink = B->GetRemaining() < 1.5f && FMath::Fmod(B->GetRemaining(), 0.3f) < 0.15f;
			BuffRows[I]->SetRenderOpacity(bBlink ? 0.45f : 1.f);
		}
		else
		{
			BuffRows[I]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UCatRunWidget::SetStats(const FString& Text, bool bVisible)
{
	StatsText->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		StatsText->SetText(FText::FromString(Text));
	}
}

void UCatRunWidget::FlashMessage(const FString& Text)
{
	Message->SetText(FText::FromString(Text));
	MessageTime = 1.4f;
	Message->SetRenderOpacity(1.f);
}

void UCatRunWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;
	if (MessageTime > 0.f)
	{
		MessageTime -= InDeltaTime;
		Message->SetRenderOpacity(FMath::Clamp(MessageTime / 0.5f, 0.f, 1.f));
	}
}

void UCatRunWidget::HandleStart() { if (Game.IsValid()) Game->RequestStart(); }
void UCatRunWidget::HandleRestart() { if (Game.IsValid()) Game->RequestStart(); }
void UCatRunWidget::HandleMenu() { if (Game.IsValid()) Game->RequestMenu(); }
void UCatRunWidget::HandleQuality() { if (Game.IsValid()) Game->CycleQuality(); }
void UCatRunWidget::HandleStats() { if (Game.IsValid()) Game->ToggleStats(); }
void UCatRunWidget::HandleShop() { if (Game.IsValid()) Game->RequestShop(); }
void UCatRunWidget::HandleShopBack() { if (Game.IsValid()) Game->RequestShopBack(); }
void UCatRunWidget::HandlePause() { if (Game.IsValid()) Game->RequestPause(); }
void UCatRunWidget::HandleResume() { if (Game.IsValid()) Game->RequestResume(); }
void UCatRunWidget::HandleQuitRun() { if (Game.IsValid()) Game->RequestQuitRun(); }
void UCatRunWidget::HandleBuy0() { if (Game.IsValid()) Game->RequestBuy(0); }
void UCatRunWidget::HandleBuy1() { if (Game.IsValid()) Game->RequestBuy(1); }
void UCatRunWidget::HandleBuy2() { if (Game.IsValid()) Game->RequestBuy(2); }
void UCatRunWidget::HandleBuy3() { if (Game.IsValid()) Game->RequestBuy(3); }
void UCatRunWidget::HandleBuy4() { if (Game.IsValid()) Game->RequestBuy(4); }
void UCatRunWidget::HandleBuy5() { if (Game.IsValid()) Game->RequestBuy(5); }
void UCatRunWidget::HandleItem0() { if (Game.IsValid()) Game->RequestUseItem(0); }
void UCatRunWidget::HandleItem1() { if (Game.IsValid()) Game->RequestUseItem(1); }
void UCatRunWidget::HandleItem2() { if (Game.IsValid()) Game->RequestUseItem(2); }
void UCatRunWidget::HandleEndless() { if (Game.IsValid()) Game->RequestStartMode(ERunMode::Endless); }
void UCatRunWidget::HandleTutorial() { if (Game.IsValid()) Game->RequestStartMode(ERunMode::Tutorial); }
