#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossGiant.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMediaPlayer;
class UMediaTexture;
class UFileMediaSource;

/** Ein Wurf des Riesen: Zielbereich auf dem Boden und Abwurfverzoegerung nach dem Ausholen. */
struct FGiantShot
{
	FVector Target = FVector::ZeroVector;
	/** halbe Groesse der Zielmarkierung (Laenge, Breite) */
	FVector2D Extent = FVector2D(100.f, 100.f);
	float Delay = 0.f;
};

/**
 * Riesiger Gegner am Horizont (Endlos-Modus). Geht in grosser Entfernung vor der Katze mit und wirft ab und zu
 * Klumpen weisser Tinte auf die Strecke. Ablauf eines Angriffs: Ausholen (WindUp) -> ein oder mehrere Wuerfe
 * (Salve) mit Bodenmarkierung der Zielfahrbahnen -> Aufschlag: weisse Pfuetze bleibt liegen (der Director prueft sie).
 *
 * Darstellung: Videos (Content/Movies/Enemy_idle(_2), Enemy_prepares_salve, Enemy_salve, Cat_got_hit, Player_dead .mp4) auf einer grossen additiven
 * Tafel am Himmel (schwarzer Hintergrund verschwindet); zwischen zwei Videos wird ueber Schwarz ueberblendet.
 * Fehlen die Videos: GiantMesh + IdleAnim/AttackAnim (BP_BossGiant), sonst ein Platzhalter aus Grundformen.
 */
UCLASS(Blueprintable)
class SHADOWCAT_API ABossGiant : public AActor
{
	GENERATED_BODY()

public:
	ABossGiant();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Riese")
	TSoftObjectPtr<USkeletalMesh> GiantMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Riese")
	TSoftObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Riese")
	TSoftObjectPtr<UAnimSequence> AttackAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Riese")
	float MeshScale = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Riese")
	float MeshYaw = -90.f;

	/** Entfernung vor der Katze und seitlicher Versatz (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Riese")
	float Distance = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Riese")
	float SideOffset = 0.f;

	/** Ausholen bis zum (ersten) Wurf und Flugzeit eines Tintenklumpens (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Angriff")
	float WindUp = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Angriff")
	float FlightTime = 1.6f;

	/** Wurfhand relativ zum Riesen (Platzhalter) bzw. Abwurfhoehe (Modell). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Angriff")
	FVector HandOffset = FVector(-300.f, -1100.f, 3300.f);

	/** Videotafel: Kantenlaenge und Unterkante ueber dem Boden (cm), Ueberblendzeit (s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Video")
	float VideoSize = 7000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Video")
	float VideoBottom = -200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Video")
	float FadeTime = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Video")
	float VideoGain = 1.0f;

	void SetActive(bool bActive);
	/**
	 * Video wechseln (ueber Schwarz): "idle"/"idle2" (Ruhe, im Wechsel), "prepare" (Wurf vorbereiten), "salve",
	 * "cathit" (Katze verliert ein Leben), "dead" (Katze tot). Danach folgt Then, sonst wieder Ruhe.
	 */
	void PlayClip(FName Clip, bool bLoop = false, FName Then = NAME_None);
	void PlayIdle();
	/** Jeden Frame: folgt der Katze in der Ferne. */
	void StepGiant(float DeltaTime, const FVector& CatLoc);
	/** Einzelwurf auf den Bodenbereich Center (Groesse Extent X/Y). */
	void StartAttack(const FVector& Center, const FVector2D& Extent);
	/** Salve: mehrere Wuerfe nach einem Ausholen (Reihenfolge = Index fuer ConsumeImpacts). */
	void StartAttack(const TArray<FGiantShot>& Shots);
	bool IsAttacking() const { return Phase != 0; }
	/** Indizes der Wuerfe, die seit dem letzten Aufruf aufgeschlagen sind. */
	TArray<int32> ConsumeImpacts();
	/** Zeit bis zum naechsten Aufschlag (999 = keiner unterwegs). */
	float TimeToImpact() const;

private:
	void BuildPlaceholder();
	FVector HandWorld() const;
	UStaticMeshComponent* NewBlob();
	UStaticMeshComponent* NewMarker(UMaterialInstanceDynamic*& OutMat);
	void SetupVideo();
	void StepVideo(float DeltaTime);

	UPROPERTY(Transient) TObjectPtr<UMediaPlayer> Player;
	UPROPERTY(Transient) TObjectPtr<UMediaTexture> VideoTex;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UFileMediaSource>> Clips;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Screen;
	/** Wolkenbank vor dem Djinn */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BankFog;
	TArray<FVector> BankBase;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ScreenMat;
	bool bVideo = false;
	FName CurClip;
	FName PendingClip;
	FName QueuedClip;
	bool bCurLoop = true;
	bool bPendingLoop = true;
	/** 0 = laeuft, 1 = blendet aus, 2 = wartet auf das neue Video, 3 = blendet ein */
	int32 FadeState = 0;
	float Fade = 0.f;
	float FadeWait = 0.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<USceneComponent> Body;

	UPROPERTY()
	TObjectPtr<USceneComponent> ThrowArm;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> AttackClip;

	/** Ein Klumpen der laufenden Salve (Bauteile werden wiederverwendet). */
	struct FFlight
	{
		FGiantShot Shot;
		FVector Start = FVector::ZeroVector;
		/** <0: noch nicht geworfen, 0..1 Flug, >1 gelandet */
		float T = -1.f;
		TObjectPtr<UStaticMeshComponent> Blob;
		TObjectPtr<UStaticMeshComponent> Marker;
		TObjectPtr<UMaterialInstanceDynamic> MarkerMat;
	};
	TArray<FFlight> Flights;
	/** Pool: Klumpen und Markierungen (Anzahl = groesste bisherige Salve). */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> BlobPool;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> MarkerPool;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> MarkerMats;
	TArray<int32> Impacts;

	bool bPlaceholder = true;
	bool bActive = false;
	bool bBuilt = false;
	/** 0 = ruhig, 1 = holt aus, 2 = wirft / Klumpen fliegen */
	int32 Phase = 0;
	float PhaseTime = 0.f;
	float Time = 0.f;
	float LastThrow = -10.f;
};
