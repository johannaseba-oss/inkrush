#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RunCamera.generated.h"

class UCameraComponent;
class UStaticMeshComponent;

/**
 * Kamera leicht erhoeht hinter der Katze. Das Sichtfeld passt sich dem Seitenverhaeltnis an:
 * im Hochformat wird der horizontale Winkel so gewaehlt, dass alle Spuren sichtbar bleiben.
 * Im Startbildschirm steht sie vor der Katze und blendet beim Start weich nach hinten.
 */
UCLASS(Blueprintable)
class SHADOWCAT_API ARunCamera : public AActor
{
	GENERATED_BODY()

public:
	ARunCamera();

	UPROPERTY(EditAnywhere, Category = "Kamera")
	float BackDistance = 420.f;

	UPROPERTY(EditAnywhere, Category = "Kamera")
	float Height = 330.f;

	/** Blickpunkt vor der Katze. */
	UPROPERTY(EditAnywhere, Category = "Kamera")
	float LookAhead = 1100.f;

	UPROPERTY(EditAnywhere, Category = "Kamera")
	float LookHeight = 0.f;

	/** Gewuenschter vertikaler Blickwinkel (Grad). */
	UPROPERTY(EditAnywhere, Category = "Kamera")
	float VerticalFov = 62.f;

	/** Halbe Breite (cm) auf Hoehe der Katze, die immer sichtbar sein muss (Spuren + Rand). */
	UPROPERTY(EditAnywhere, Category = "Kamera")
	float MinVisibleHalfWidth = 240.f;

	/** Vertikaler Blickwinkel im Startbildschirm (enger, Katze gross im Bild). */
	UPROPERTY(EditAnywhere, Category = "Kamera")
	float MenuVerticalFov = 44.f;

	void SetMenuMode(bool bMenu, bool bInstant);
	void StepCamera(float DeltaTime, const FVector& CatLoc, const FVector& CatForward);
	void AddShake(float Amount) { Shake = FMath::Max(Shake, Amount); }
	UCameraComponent* GetCamera() const { return Camera; }

private:
	void ComputeRunView(const FVector& CatLoc, FVector& OutLoc, FRotator& OutRot) const;
	void ComputeMenuView(const FVector& CatLoc, FVector& OutLoc, FRotator& OutRot) const;
	/** Horizontaler Blickwinkel (Grad) fuer einen gewuenschten vertikalen, mindestens MinHalfWidth auf Distanz Dist. */
	float ComputeFov(float VFov, float MinHalfWidth, float Dist) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Moon;

	bool bMenu = true;
	/** 0 = Menue-Ansicht, 1 = Lauf-Ansicht. */
	float Blend = 0.f;
	FVector SmoothPos = FVector::ZeroVector;
	FVector SmoothFwd = FVector::ZeroVector;
	float Shake = 0.f;
	bool bCelAdded = false;
	float Time = 0.f;
};
