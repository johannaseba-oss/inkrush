#include "CatAudio.h"
#include "ShadowCat.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

ACatAudio::ACatAudio()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Music = CreateDefaultSubobject<UAudioComponent>(TEXT("Music"));
	Music->SetupAttachment(RootComponent);
	Music->bAutoActivate = false;
	Music->bIsUISound = true;
	Music->bAllowSpatialization = false;
	Steps = CreateDefaultSubobject<UAudioComponent>(TEXT("Steps"));
	Steps->SetupAttachment(RootComponent);
	Steps->bAutoActivate = false;
	Steps->bAllowSpatialization = false;
	Spray = CreateDefaultSubobject<UAudioComponent>(TEXT("Spray"));
	Spray->SetupAttachment(RootComponent);
	Spray->bAutoActivate = false;
	Spray->bAllowSpatialization = false;
	Train = CreateDefaultSubobject<UAudioComponent>(TEXT("Train"));
	Train->SetupAttachment(RootComponent);
	Train->bAutoActivate = false;
	Train->bAllowSpatialization = false;
}

void ACatAudio::BeginPlay()
{
	Super::BeginPlay();
	for (int32 I = 1; I < 100; ++I)
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/Music/MUS_%02d.MUS_%02d"), I, I);
		USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!S)
		{
			break;
		}
		Tracks.Add(S);
	}
	Steps->SetSound(LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Footsteps.SFX_Footsteps"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	Splash = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Ink_splash_impact.SFX_Ink_splash_impact"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	Coin = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Collecting_Coin.SFX_Collecting_Coin"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	Bomb = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Ink_Bomb_activate.SFX_Ink_Bomb_activate"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	for (const TCHAR* N : { TEXT("jump"), TEXT("change_lane"), TEXT("Item_Pickup"), TEXT("Car_honk") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/SFX_%s.SFX_%s"), N, N);
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Sfx.Add(FName(N), S);
		}
	}
	// Sprung: mehrere Varianten (jump 1.wav, jump 2.wav, ...) -> zufaellig, ohne direkte Wiederholung
	for (int32 I = 1; I < 10; ++I)
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/SFX_jump_%d.SFX_jump_%d"), I, I);
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			JumpSounds.Add(S);
		}
	}
	Spray->SetSound(LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_spraypaint.SFX_spraypaint"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	Train->SetSound(LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Train.SFX_Train"), nullptr, LOAD_NoWarn | LOAD_Quiet));
	for (int32 I = 1; I < 10; ++I)
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/SFX_take_damage_%d.SFX_take_damage_%d"), I, I);
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			DamageSounds.Add(S);
		}
	}
	Music->OnAudioFinished.AddDynamic(this, &ACatAudio::OnMusicFinished);
	UE_LOG(LogShadowCat, Log, TEXT("Audio: %d Musiktracks, Schritte %s, Klatscher %s, Muenze %s, Bombe %s, weitere Effekte %d"), Tracks.Num(), Steps->GetSound() ? TEXT("ja") : TEXT("nein"),
		Splash ? TEXT("ja") : TEXT("nein"), Coin ? TEXT("ja") : TEXT("nein"), Bomb ? TEXT("ja") : TEXT("nein"), Sfx.Num());
}

void ACatAudio::StartMusic()
{
	if (Tracks.Num() > 0 && !FParse::Param(FCommandLine::Get(), TEXT("CatNoMusic")))
	{
		PlayTrack(0);
	}
}

void ACatAudio::PlayTrack(int32 Index)
{
	if (Tracks.Num() == 0)
	{
		return;
	}
	Current = Index % Tracks.Num();
	Music->SetSound(Tracks[Current]);
	Music->SetVolumeMultiplier(FMath::Max(0.0001f, MusicVolume * UserMusic));
	// Test: -CatMusicSkip=5 startet jeden Track 5 s vor seinem Ende (Playlist-Wechsel pruefen)
	float Skip = 0.f;
	FParse::Value(FCommandLine::Get(), TEXT("CatMusicSkip="), Skip);
	Music->Play(Skip > 0.f ? FMath::Max(0.f, Tracks[Current]->GetDuration() - Skip) : 0.f);
	UE_LOG(LogShadowCat, Log, TEXT("Musik: Track %d/%d (%s)"), Current + 1, Tracks.Num(), *Tracks[Current]->GetName());
}

void ACatAudio::EndPlay(const EEndPlayReason::Type Reason)
{
	bShuttingDown = true;
	Super::EndPlay(Reason);
}

void ACatAudio::OnMusicFinished()
{
	if (bShuttingDown)
	{
		return;
	}
	// naechster Track, nach dem letzten wieder von vorn
	PlayTrack(Current + 1);
}

void ACatAudio::SetRunning(bool bRunning, float Speed)
{
	if (!Steps->GetSound())
	{
		return;
	}
	if (bRunning != bStepsOn)
	{
		bStepsOn = bRunning;
		if (bRunning)
		{
			Steps->SetVolumeMultiplier(FootstepVolume * UserSfx);
			Steps->FadeIn(0.15f, FootstepVolume * UserSfx, FMath::FRand() * 8.f);
		}
		else
		{
			Steps->FadeOut(0.12f, 0.f);
		}
	}
	if (bRunning)
	{
		Steps->SetPitchMultiplier(FMath::Clamp(Speed / 950.f, 0.9f, 1.25f));
	}
}

void ACatAudio::PlayCoin()
{
	if (!Coin)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	CoinCombo = Now - LastCoinTime < 0.5 ? FMath::Min(CoinCombo + 1, 12) : 0;
	LastCoinTime = Now;
	UGameplayStatics::PlaySound2D(this, Coin, CoinVolume * UserSfx, 1.f + 0.035f * CoinCombo);
}

void ACatAudio::PlaySfx(FName Name, float VolumeScale, float PitchJitter)
{
	// Sprung und Fahrbahnwechsel kommen sehr oft -> deutlich leiser als die anderen Effekte
	if (Name == TEXT("jump"))
	{
		VolumeScale *= JumpVolume;
		if (JumpSounds.Num() > 0)
		{
			int32 Pick = FMath::RandHelper(JumpSounds.Num());
			if (JumpSounds.Num() > 1 && Pick == LastJump)
			{
				Pick = (Pick + 1) % JumpSounds.Num();
			}
			LastJump = Pick;
			UGameplayStatics::PlaySound2D(this, JumpSounds[Pick], SfxVolume * UserSfx * VolumeScale, 1.f + FMath::FRandRange(-PitchJitter, PitchJitter));
			return;
		}
	}
	else if (Name == TEXT("change_lane"))
	{
		VolumeScale *= LaneVolume;
	}
	if (const TObjectPtr<USoundBase>* S = Sfx.Find(Name))
	{
		UGameplayStatics::PlaySound2D(this, *S, SfxVolume * UserSfx * VolumeScale, 1.f + FMath::FRandRange(-PitchJitter, PitchJitter));
	}
}

void ACatAudio::PlayBomb()
{
	if (Bomb)
	{
		UGameplayStatics::PlaySound2D(this, Bomb, BombVolume * UserSfx, 1.f);
	}
	else
	{
		PlaySplash(0.8f);
	}
}

void ACatAudio::SetSpray(bool bOn)
{
	if (!Spray->GetSound())
	{
		return;
	}
	if (bOn)
	{
		Spray->FadeIn(0.15f, SfxVolume * UserSfx * 0.8f, FMath::FRand() * 5.f);
	}
	else if (Spray->IsPlaying())
	{
		Spray->FadeOut(0.35f, 0.f);
	}
}

void ACatAudio::PlayGameOver()
{
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX_Geme_over.SFX_Geme_over"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySound2D(this, S, SfxVolume * UserSfx * 1.1f);
	}
	SetSpray(false);
	Train->FadeOut(0.3f, 0.f);
}

void ACatAudio::PlayDamage()
{
	if (DamageSounds.Num() > 0)
	{
		UGameplayStatics::PlaySound2D(this, DamageSounds[FMath::RandHelper(DamageSounds.Num())], SfxVolume * UserSfx, FMath::FRandRange(0.96f, 1.04f));
	}
}

void ACatAudio::PlayTrain()
{
	// (ersetzt durch SetTrainProximity: Lautstaerke folgt dem Abstand)
}

void ACatAudio::SetTrainProximity(float Near)
{
	if (!Train->GetSound())
	{
		return;
	}
	if (Near > 0.01f)
	{
		if (!Train->IsPlaying())
		{
			Train->Play(FMath::FRand() * 2.f);
		}
		Train->SetVolumeMultiplier(FMath::Max(0.001f, TrainVolume * UserSfx * Near));
	}
	else if (Train->IsPlaying())
	{
		Train->FadeOut(0.6f, 0.f);
	}
}

void ACatAudio::SetUserVolumes(float InMusic, float InSfx)
{
	UserMusic = FMath::Clamp(InMusic, 0.f, 1.f);
	UserSfx = FMath::Clamp(InSfx, 0.f, 1.f);
	Music->SetVolumeMultiplier(FMath::Max(0.0001f, MusicVolume * UserMusic));
	if (bStepsOn)
	{
		Steps->SetVolumeMultiplier(FMath::Max(0.0001f, FootstepVolume * UserSfx));
	}
}

void ACatAudio::PlaySplash(float VolumeScale)
{
	if (Splash)
	{
		UGameplayStatics::PlaySound2D(this, Splash, SplashVolume * UserSfx * VolumeScale, FMath::FRandRange(0.92f, 1.08f));
	}
}
