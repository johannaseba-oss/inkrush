#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CatAudio.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Musik und Soundeffekte. Musik: /Game/Audio/Music/MUS_01, MUS_02, ... werden nacheinander gespielt,
 * nach dem letzten Track beginnt die Liste wieder von vorn (Tools/ue_setup.py Schritt "audio").
 * Schritte laufen als Schleife, solange die Katze rennt; Tintenklatscher bei Aufschlaegen.
 */
UCLASS()
class SHADOWCAT_API ACatAudio : public AActor
{
	GENERATED_BODY()

public:
	ACatAudio();

	UPROPERTY(EditAnywhere, Category = "Audio")
	float MusicVolume = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Audio")
	float FootstepVolume = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Audio")
	float SplashVolume = 0.8f;

	void StartMusic();
	/** Lautstaerke aus den Einstellungen (0..1): Musik und alle Soundeffekte. */
	void SetUserVolumes(float InMusic, float InSfx);
	/** Naechster entgegenkommender Zug: 0 = keiner/weit weg, 1 = ganz nah (Lautstaerke des Zuggeraeuschs). */
	void SetTrainProximity(float Near);
	/** Jeden Frame: Schritte an/aus und an das Tempo angepasst. */
	void SetRunning(bool bRunning, float Speed);
	void PlaySplash(float VolumeScale = 1.f);
	/** Muenze eingesammelt: Tonhoehe steigt innerhalb einer Reihe leicht an. */
	void PlayCoin();
	/** Tintenbombe gezuendet. */
	void PlayBomb();
	/** Spraydose: Spruehgeraeusch an/aus (weich ein- und ausgeblendet). */
	void SetSpray(bool bOn);
	/** Treffer: eine der Varianten SFX_take_damage_1.. */
	void PlayDamage();
	/** Entgegenkommender Zug: Geraeusch einige Sekunden, dann ausblenden. */
	void PlayTrain();
	/** Letztes Leben verloren */
	void PlayGameOver();
	/** Kurze Effekte: "jump" (Varianten), "change_lane", "Item_Pickup", "Car_honk" (= SFX_<Name> aus Tools/Import/Soundeffects). */
	void PlaySfx(FName Name, float VolumeScale = 1.f, float PitchJitter = 0.04f);

	UPROPERTY(EditAnywhere, Category = "Audio")
	float SfxVolume = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Audio")
	float CoinVolume = 0.55f;

	/** Anteil von SfxVolume fuer Sprung bzw. Fahrbahnwechsel (kommen sehr oft). */
	UPROPERTY(EditAnywhere, Category = "Audio")
	float JumpVolume = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Audio")
	float LaneVolume = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Audio")
	float BombVolume = 0.9f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION()
	void OnMusicFinished();

	void PlayTrack(int32 Index);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> Music;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> Steps;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> Spray;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> Train;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> DamageSounds;

	FTimerHandle TrainFade;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Tracks;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> Splash;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> Coin;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> Bomb;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> Sfx;

	/** SFX_jump_1, SFX_jump_2, ... */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> JumpSounds;
	int32 LastJump = INDEX_NONE;

	/** Einstellungen (0..1) */
	float UserMusic = 0.8f;
	float UserSfx = 0.8f;
	/** Grundlautstaerke des Zuggeraeuschs (bei voller Naehe) */
	float TrainVolume = 0.45f;
	double LastCoinTime = -10.0;
	int32 CoinCombo = 0;

	int32 Current = INDEX_NONE;
	bool bStepsOn = false;
	bool bShuttingDown = false;
};
