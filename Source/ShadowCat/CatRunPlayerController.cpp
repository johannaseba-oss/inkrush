#include "CatRunPlayerController.h"
#include "CatRunGameMode.h"
#include "CatRunWidget.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

ACatRunPlayerController::ACatRunPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableTouchEvents = true;
	// Pause: Tasten (P/Esc) weiter abfragen, damit man wieder weiterspielen kann
	bShouldPerformFullTickWhenPaused = true;
}

void ACatRunPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	RunWidget = CreateWidget<UCatRunWidget>(this, UCatRunWidget::StaticClass());
	if (RunWidget)
	{
		RunWidget->AddToViewport(0);
	}
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	if (ACatRunGameMode* GM = GetWorld()->GetAuthGameMode<ACatRunGameMode>())
	{
		GM->RegisterController(this);
	}
}

void ACatRunPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	PollTouch(DeltaTime);
	PollKeys();
}

void ACatRunPlayerController::Shift(int32 Dir)
{
	if (ACatRunGameMode* GM = GetWorld()->GetAuthGameMode<ACatRunGameMode>())
	{
		GM->RequestLaneShift(Dir);
	}
}

void ACatRunPlayerController::PollTouch(float DeltaTime)
{
	float X = 0.f;
	float Y = 0.f;
	bool bPressed = false;
	GetInputTouchState(ETouchIndex::Touch1, X, Y, bPressed);

	if (bPressed && !bTouchDown)
	{
		bTouchDown = true;
		bSwipeUsed = false;
		TouchStart = FVector2D(X, Y);
		TouchTime = 0.f;
	}
	else if (bPressed && bTouchDown)
	{
		TouchTime += DeltaTime;
		// Wischer schon waehrend der Bewegung ausloesen (nicht erst beim Loslassen): spuerbar schneller
		if (!bSwipeUsed && TouchTime <= SwipeMaxTime)
		{
			int32 SizeX = 1080;
			int32 SizeY = 2340;
			GetViewportSize(SizeX, SizeY);
			const FVector2D D = FVector2D(X, Y) - TouchStart;
			const float Min = FMath::Max(24.f, SizeX * SwipeMinFraction);
			if (FMath::Abs(D.X) >= Min && FMath::Abs(D.X) > FMath::Abs(D.Y) * 1.1f)
			{
				Shift(D.X > 0.f ? 1 : -1);
				bSwipeUsed = true;
			}
			else if (FMath::Abs(D.Y) >= Min && FMath::Abs(D.Y) > FMath::Abs(D.X) * 1.1f)
			{
				// Bildschirm-Y waechst nach unten: nach oben wischen = springen
				if (ACatRunGameMode* GM = GetWorld()->GetAuthGameMode<ACatRunGameMode>())
				{
					if (D.Y < 0.f) GM->RequestJump(); else GM->RequestDrop();
				}
				bSwipeUsed = true;
			}
		}
	}
	else if (!bPressed && bTouchDown)
	{
		bTouchDown = false;
	}
}

void ACatRunPlayerController::PollKeys()
{
	ACatRunGameMode* GM = GetWorld()->GetAuthGameMode<ACatRunGameMode>();
	if (!GM)
	{
		return;
	}
	if (WasInputKeyJustPressed(EKeys::Left) || WasInputKeyJustPressed(EKeys::A))
	{
		GM->RequestLaneShift(-1);
	}
	if (WasInputKeyJustPressed(EKeys::Right) || WasInputKeyJustPressed(EKeys::D))
	{
		GM->RequestLaneShift(1);
	}
	if (WasInputKeyJustPressed(EKeys::Up) || WasInputKeyJustPressed(EKeys::W))
	{
		GM->RequestJump();
	}
	if (WasInputKeyJustPressed(EKeys::Down) || WasInputKeyJustPressed(EKeys::S))
	{
		GM->RequestDrop();
	}
	if (WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Enter))
	{
		GM->RequestConfirm();
	}
	// Items: E = Tintenbombe (Sorte 1), Q/Shift = Tintenwolke (Sorte 0)
	if (WasInputKeyJustPressed(EKeys::Q) || WasInputKeyJustPressed(EKeys::LeftShift))
	{
		GM->RequestUseItem(0);
	}
	if (WasInputKeyJustPressed(EKeys::E))
	{
		GM->RequestUseItem(1);
	}
	if (WasInputKeyJustPressed(EKeys::F1))
	{
		GM->ToggleStats();
	}
	if (WasInputKeyJustPressed(EKeys::P) || WasInputKeyJustPressed(EKeys::Escape))
	{
		GM->TogglePause();
	}
}
