#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CatRunPlayerController.generated.h"

class UCatRunWidget;

/**
 * Eingabe: Wisch nach oben = Sprung, nach unten = schnell landen; Pfeil hoch/W/Leertaste bzw. Pfeil runter/S.
 * Wisch nach links/rechts (Touch; im Editor auch Maus-Ziehen, da bUseMouseForTouch aktiv ist)
 * sowie Pfeiltasten bzw. A/D. Leertaste/Enter startet bzw. startet neu, E/Shift setzt das Item ein, F1 = Leistungsanzeige.
 */
UCLASS()
class SHADOWCAT_API ACatRunPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACatRunPlayerController();

	/** Mindestweg eines Wischers als Anteil der Bildschirmbreite. */
	UPROPERTY(EditAnywhere, Category = "Eingabe")
	float SwipeMinFraction = 0.045f;

	/** Wischer muss innerhalb dieser Zeit erkannt werden (sonst gilt er als Halten). */
	UPROPERTY(EditAnywhere, Category = "Eingabe")
	float SwipeMaxTime = 0.6f;

	UCatRunWidget* GetRunWidget() const { return RunWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void PollTouch(float DeltaTime);
	void PollKeys();
	void Shift(int32 Dir);

	UPROPERTY(Transient)
	TObjectPtr<UCatRunWidget> RunWidget;

	bool bTouchDown = false;
	bool bSwipeUsed = false;
	FVector2D TouchStart = FVector2D::ZeroVector;
	float TouchTime = 0.f;
};
