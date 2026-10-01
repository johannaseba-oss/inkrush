#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CircuitLayout.h"
#include "RunnerCat.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class ULaneMovementComponent;
class UBuffComponent;
class UItemSlotComponent;
class ATrackDirector;

/**
 * Die Spielfigur. Modell und Animationen sind austauschbar (BP_RunnerCat -> Kategorie "Katze").
 * Fehlt das Modell, wird automatisch eine schwarze Platzhalter-Katze aus Grundformen gebaut.
 * Die Katze laeuft entlang +X; Y ergibt sich aus der Spur.
 */
UCLASS(Blueprintable)
class SHADOWCAT_API ARunnerCat : public APawn
{
	GENERATED_BODY()

public:
	ARunnerCat();

	/** Skelett-Modell der Katze (leer = Platzhalter). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	TSoftObjectPtr<USkeletalMesh> CatMesh;

	/** Lauf-Schleife (In-Place bzw. mit Root-Lock). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	TSoftObjectPtr<UAnimSequence> RunLoopAnim;

	/** Optional: Anlauf beim Start (erstes Bild dient als Pose im Startbildschirm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	TSoftObjectPtr<UAnimSequence> RunStartAnim;

	/** Optional: Abbremsen/Stolpern bei Game Over. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	TSoftObjectPtr<UAnimSequence> StopAnim;

	/** Optional: Warte-Animation im Startbildschirm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	TSoftObjectPtr<UAnimSequence> IdleAnim;

	/** Animation waehrend eines Sprungs (wird auf die Flugzeit gestreckt). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	TSoftObjectPtr<UAnimSequence> JumpAnim;

	/** Kriechen unter Balken (Schleife; fehlt sie, wird die Katze flach gedrueckt). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	TSoftObjectPtr<UAnimSequence> CrawlAnim;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	float CrawlPlayRate = 1.6f;

	/** Material fuer das Modell (leer = Materialien des Modells behalten). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	TSoftObjectPtr<UMaterialInterface> CatMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	float MeshScale = 1.5f;

	/** Drehung des Modells, damit es entlang +X blickt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	float MeshYaw = -90.f;

	/** Strecke (cm), die die Lauf-Schleife bei Abspielrate 1 und Skalierung 1 pro Sekunde zuruecklegt. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	float AnimRootSpeed = 427.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	float MinPlayRate = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Katze")
	float MaxPlayRate = 1.7f;

	/** Sprunghoehe der Fuesse in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprung")
	float JumpHeight = 230.f;

	/** Flugzeit eines normalen Sprungs in Sekunden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprung")
	float JumpDuration = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kollision")
	float CollisionHalfWidth = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kollision")
	float CollisionHalfLength = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kollision")
	float CollisionHeight = 130.f;

	/** Startbildschirm-Pose an Streckenposition A auf Fahrbahn StartLane. */
	void ResetForMenu(float StartA, int32 StartLane);
	void BeginRun();
	/** Ein Lauf-Schritt: vorwaerts, Spur, Buffs, Animation. */
	void StepRun(float DeltaTime, float Speed);
	/** Ausserhalb des Laufs (Menue, Sturz, Game Over). */
	void StepIdle(float DeltaTime);
	void Die();
	/** Treffer ueberlebt (Leben verloren): Stolpern + kurze Unverwundbarkeit mit Blinken. */
	void Stumble(float InvulnSeconds);
	bool IsInvulnerable() const { return InvulnTime > 0.f; }
	/** Unverwundbar fuer mindestens Seconds (z. B. nach der Landung aus der Luft-Ebene). */
	void GrantInvulnerability(float Seconds) { InvulnTime = FMath::Max(InvulnTime, Seconds); }
	/** Kurzer Aufblitz-Effekt, wenn ein Buff einen Treffer abgefangen hat. */
	void OnHitAbsorbed();
	/** 0..1: weisses Aufleuchten (Unbesiegbarkeit). */
	void SetFlash(float Amount);
	/** 0 = nicht moeglich, 1 = Fahrbahn gewechselt, 2 = Kurs gewechselt. */
	int32 RequestLaneShift(int32 Dir, bool bAllowCircuitChange);
	/** Sprung (nur vom Boden). */
	bool RequestJump();
	/** In der Luft: schnell zurueck auf den Boden. */
	void RequestDrop();
	/** Wisch nach unten: am Boden kriechen (CrawlDuration), in der Luft schnell landen und dann kriechen. */
	void RequestCrawl();
	bool IsCrawling() const { return CrawlTime > 0.f; }
	/** Shop-Upgrades (vom GameMode zu Laufbeginn gesetzt): laengerer Flug, Flug unverwundbar, Doppelsprung. */
	float FlyBonus = 0.f;

	/** Shop: zusaetzliche Reichweite der Tintenbombe (cm) */
	float BombRangeBonus = 0.f;
	bool bFlyInvulnerable = false;
	bool bDoubleJump = false;

	/** Spraydose: steigt auf die Luft-Ebene (Height = Hoehe ueber der Strecke), bis StopFly; danach normal landen. */
	void StartFly(float Height);
	void StopFly();
	bool IsFlying() const { return bFlying; }
	/** In ein Loch gefallen: faellt FallDuration lang, danach ConsumeFallEnd (Director setzt sie hinter das Loch). */
	void StartFall();
	bool IsFalling() const { return bFalling; }
	bool ConsumeFallEnd() { const bool b = bFallEnd; bFallEnd = false; return b; }
	/** Katze entlang der Strecke versetzen (hinter ein Loch). */
	void WarpToA(float NewA);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bewegung")
	float FallDuration = 0.7f;
	/** Ereignisse fuer Effekte/Sound (einmal abholen): Absprung, Landung. */
	bool ConsumeJumpEvent() { const bool b = bJumpEvent; bJumpEvent = false; return b; }
	bool ConsumeLandEvent() { const bool b = bLandEvent; bLandEvent = false; return b; }

	/** Dauer des Kriechens (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bewegung")
	float CrawlDuration = 0.8f;
	/** Hoehe der Fuesse ueber dem Boden. */
	float GetFeetZ() const { return JumpZ; }
	/** Streckenposition (Parameter A des Rundkurses). */
	float GetA() const { return A; }
	/** Insgesamt gelaufene Weltstrecke dieses Laufs. */
	float GetTravel() const { return Travel; }
	FVector GetForward() const { return Forward; }
	void SetLayout(const FCircuitLayout& InLayout);
	bool IsJumping() const { return bJumping; }
	/** Im Sprung auf dem Weg nach oben. */
	bool IsRising() const { return bJumping && VelZ > 0.f; }
	/** Traegt die Katze (Oberkante darunter, 0 = Boden), z. B. ein Sargdeckel. */
	void SetSupportZ(float Z) { SupportZ = Z; }
	/** Fuesse auf Boden oder Deckel (Schritt-Geraeusche). */
	bool IsGrounded() const { return !bJumping && JumpZ <= SupportZ + 1.f; }

	ULaneMovementComponent* GetLanes() const { return Lanes; }
	UBuffComponent* GetBuffs() const { return Buffs; }
	UItemSlotComponent* GetSlot() const { return Slot; }
	ATrackDirector* GetDirector() const;
	void SetDirector(ATrackDirector* InDirector);
	bool UsesPlaceholder() const { return bPlaceholder; }

protected:
	virtual void BeginPlay() override;

private:
	void ApplyAssets();
	void BuildPlaceholder();
	void PlayAnim(UAnimSequence* Anim, bool bLoop, float Rate);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	/** Neigung beim Spurwechsel/Sturz, traegt Modell und Platzhalter. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Tilt;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> PlaceholderRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BlobShadow;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ULaneMovementComponent> Lanes;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBuffComponent> Buffs;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UItemSlotComponent> Slot;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CatMid;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlaceholderMid;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> LoopAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> StartAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> EndAnim;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> IdleClip;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> JumpClip;

	void StepVertical(float DeltaTime);
	void ApplyTransform();
	FCircuitLayout Layout;
	float A = 0.f;
	float Travel = 0.f;
	float JumpZ = 0.f;
	FVector Forward = FVector::ForwardVector;
	void Land();
	bool bJumping = false;
	float InvulnTime = 0.f;
	float StumbleTime = -1.f;
	float VelZ = 0.f;
	float SupportZ = 0.f;
	/** Sprung kurz vor der Landung getippt: wird bei der Landung ausgefuehrt. */
	float JumpBuffer = 0.f;
	float CrawlTime = 0.f;
	float CrawlBlend = 0.f;
	bool bCrawlOnLand = false;
	bool bCrawlAnimOn = false;
	bool bFalling = false;
	bool bFlying = false;
	bool bUsedDoubleJump = false;
	float FlyHeight = 200.f;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> FlyClip;
	bool bFallEnd = false;
	float FallTime = 0.f;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> CrawlClip;
	bool bJumpEvent = false;
	bool bLandEvent = false;

	TWeakObjectPtr<ATrackDirector> Director;
	bool bPlaceholder = false;
	bool bInStartClip = false;
	float StartClipLeft = 0.f;
	float Time = 0.f;
	float DeathTime = -1.f;
	float AbsorbFlash = 0.f;
	float BuffFlash = 0.f;
	float RunSpeed = 0.f;
};
