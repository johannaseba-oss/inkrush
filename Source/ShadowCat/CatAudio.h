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
	/** Jeden Frame: Schritte an/aus und an das Tempo angepasst. */
	void SetRunning(bool bRunning, float Speed);
	void PlaySplash(float VolumeScale = 1.f);
	/** Muenze eingesammelt: Tonhoehe steigt innerhalb einer Reihe leicht an. */
	void PlayCoin();
	/** Tintenbombe gezuendet. */
	void PlayBomb();
	/** Kurze Effekte: "jump", "change_lane", "Item_Pickup" (= SFX_<Name> aus Tools/Import/Soundeffects). */
	void PlaySfx(FName Name, float VolumeScale = 1.f, float PitchJitter = 0.04f);

	UPROPERTY(EditAnywhere, Category = "Audio")
	float SfxVolume = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Audio")
	float CoinVolume = 0.55f;

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

	double LastCoinTime = -10.0;
	int32 CoinCombo = 0;

	int32 Current = INDEX_NONE;
	bool bStepsOn = false;
	bool bShuttingDown = false;
};
