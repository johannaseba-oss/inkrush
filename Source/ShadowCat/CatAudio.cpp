#include "CatAudio.h"
#include "ShadowCat.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"

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
	for (const TCHAR* N : { TEXT("jump"), TEXT("change_lane"), TEXT("Item_Pickup") })
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/SFX_%s.SFX_%s"), N, N);
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Sfx.Add(FName(N), S);
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
	Music->SetVolumeMultiplier(MusicVolume);
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
			Steps->SetVolumeMultiplier(FootstepVolume);
			Steps->FadeIn(0.15f, FootstepVolume, FMath::FRand() * 8.f);
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
	UGameplayStatics::PlaySound2D(this, Coin, CoinVolume, 1.f + 0.035f * CoinCombo);
}

void ACatAudio::PlaySfx(FName Name, float VolumeScale, float PitchJitter)
{
	if (const TObjectPtr<USoundBase>* S = Sfx.Find(Name))
	{
		UGameplayStatics::PlaySound2D(this, *S, SfxVolume * VolumeScale, 1.f + FMath::FRandRange(-PitchJitter, PitchJitter));
	}
}

void ACatAudio::PlayBomb()
{
	if (Bomb)
	{
		UGameplayStatics::PlaySound2D(this, Bomb, BombVolume, 1.f);
	}
	else
	{
		PlaySplash(0.8f);
	}
}

void ACatAudio::PlaySplash(float VolumeScale)
{
	if (Splash)
	{
		UGameplayStatics::PlaySound2D(this, Splash, SplashVolume * VolumeScale, FMath::FRandRange(0.92f, 1.08f));
	}
}
