#include "BossGiant.h"
#include "RunTypes.h"
#include "ShadowCat.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "FileMediaSource.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

ABossGiant::ABossGiant()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GiantMesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	SetActorEnableCollision(false);
	GiantMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Boss/SK_Giant.SK_Giant")));
	IdleAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Boss/A_GiantIdle.A_GiantIdle")));
	AttackAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Boss/A_GiantAttack.A_GiantAttack")));
}

void ABossGiant::BuildPlaceholder()
{
	if (Body)
	{
		return;
	}
	// Platzhalter: verhuellte Riesengestalt aus Tinte (ca. 36 m), leuchtende Augen, ein Wurfarm
	Body = NewObject<USceneComponent>(this);
	Body->SetupAttachment(Root);
	Body->RegisterComponent();
	// tiefschwarze Silhouette (hebt sich vom hellen Nebelhorizont ab)
	UMaterialInterface* Ink = RunAssets::Glow(0.f, 0.f);
	UMaterialInterface* Eye = RunAssets::Glow(1.f, 6.f);
	auto Add = [this](USceneComponent* P, const TCHAR* S, const FVector& L, const FVector& Sc, const FRotator& R, UMaterialInterface* M)
	{
		return RunAssets::AddShape(this, P, S, L, Sc, R, M);
	};
	Add(Body, TEXT("Cone"), FVector(0.f, 0.f, 1400.f), FVector(16.f, 16.f, 28.f), FRotator::ZeroRotator, Ink);
	Add(Body, TEXT("Sphere"), FVector(0.f, 0.f, 2600.f), FVector(10.f, 12.f, 9.f), FRotator::ZeroRotator, Ink);
	Add(Body, TEXT("Sphere"), FVector(0.f, 0.f, 3250.f), FVector(8.f), FRotator::ZeroRotator, Ink);
	Add(Body, TEXT("Cone"), FVector(0.f, -250.f, 3750.f), FVector(2.5f, 2.5f, 5.f), FRotator(0.f, 0.f, -18.f), Ink);
	Add(Body, TEXT("Cone"), FVector(0.f, 250.f, 3750.f), FVector(2.5f, 2.5f, 5.f), FRotator(0.f, 0.f, 18.f), Ink);
	Add(Body, TEXT("Sphere"), FVector(360.f, -170.f, 3300.f), FVector(0.9f, 1.3f, 0.7f), FRotator::ZeroRotator, Eye);
	Add(Body, TEXT("Sphere"), FVector(360.f, 170.f, 3300.f), FVector(0.9f, 1.3f, 0.7f), FRotator::ZeroRotator, Eye);
	// ruhender Arm links
	Add(Body, TEXT("Cylinder"), FVector(0.f, 650.f, 2000.f), FVector(2.5f, 2.5f, 16.f), FRotator(0.f, 0.f, -12.f), Ink);
	// Wurfarm rechts (Drehpunkt an der Schulter)
	ThrowArm = NewObject<USceneComponent>(this);
	ThrowArm->SetupAttachment(Body);
	ThrowArm->SetRelativeLocation(FVector(0.f, -650.f, 2750.f));
	ThrowArm->RegisterComponent();
	Add(ThrowArm, TEXT("Cylinder"), FVector(0.f, 0.f, -800.f), FVector(2.5f, 2.5f, 16.f), FRotator(0.f, 0.f, 12.f), Ink);
}

UStaticMeshComponent* ABossGiant::NewBlob()
{
	UStaticMeshComponent* B = RunAssets::AddShape(this, Root, TEXT("Sphere"), FVector::ZeroVector, FVector(3.f), FRotator::ZeroRotator, RunAssets::Glow(1.f, 3.f));
	B->SetUsingAbsoluteLocation(true);
	B->SetUsingAbsoluteScale(true);
	B->SetVisibility(false);
	return B;
}

UStaticMeshComponent* ABossGiant::NewMarker(UMaterialInstanceDynamic*& OutMat)
{
	OutMat = RunAssets::NewMID(TEXT("M_Disc"), this);
	if (OutMat)
	{
		// grau: auf weissen wie auf schwarz gefaerbten Fahrbahnen sichtbar
		OutMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.28f, 0.28f, 0.28f));
		OutMat->SetScalarParameterValue(TEXT("Ring"), 1.f);
		OutMat->SetScalarParameterValue(TEXT("Intensity"), 1.f);
	}
	UStaticMeshComponent* M = RunAssets::AddShape(this, Root, TEXT("Plane"), FVector::ZeroVector, FVector(1.f), FRotator::ZeroRotator, OutMat);
	M->SetUsingAbsoluteLocation(true);
	M->SetUsingAbsoluteScale(true);
	M->SetUsingAbsoluteRotation(true);
	M->SetVisibility(false);
	return M;
}

void ABossGiant::SetActive(bool bInActive)
{
	bActive = bInActive;
	SetActorHiddenInGame(!bActive);
	Phase = 0;
	Impacts.Reset();
	for (FFlight& F : Flights)
	{
		if (F.Blob) F.Blob->SetVisibility(false);
		if (F.Marker) F.Marker->SetVisibility(false);
	}
	Flights.Reset();
	if (!bActive)
	{
		// Tutorial: kein Riese, Video anhalten
		if (Player)
		{
			Player->Close();
		}
		CurClip = NAME_None;
		FadeState = 0;
		Fade = 0.f;
		return;
	}
	if (!bBuilt)
	{
		bBuilt = true;
		SetupVideo();
	}
	if (bVideo)
	{
		if (CurClip == NAME_None || (CurClip != TEXT("idle") && CurClip != TEXT("idle2")))
		{
			PlayIdle();
		}
		return;
	}
	if (Body || !bPlaceholder)
	{
		return;
	}
	USkeletalMesh* SK = GiantMesh.IsNull() ? nullptr : GiantMesh.LoadSynchronous();
	bPlaceholder = SK == nullptr;
	if (SK)
	{
		Mesh->SetSkeletalMesh(SK);
		Mesh->SetRelativeScale3D(FVector(MeshScale));
		Mesh->SetRelativeRotation(FRotator(0.f, MeshYaw, 0.f));
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		IdleClip = IdleAnim.IsNull() ? nullptr : IdleAnim.LoadSynchronous();
		AttackClip = AttackAnim.IsNull() ? nullptr : AttackAnim.LoadSynchronous();
		if (IdleClip)
		{
			Mesh->PlayAnimation(IdleClip, true);
		}
		UE_LOG(LogShadowCat, Log, TEXT("Riese: Modell %s"), *SK->GetName());
	}
	else
	{
		Mesh->SetVisibility(false);
		BuildPlaceholder();
	}
}

void ABossGiant::SetupVideo()
{
	static const TCHAR* Names[][2] = {
		{ TEXT("idle"), TEXT("Enemy_idle") }, { TEXT("idle2"), TEXT("Enemy_idle_2") },
		{ TEXT("prepare"), TEXT("Enemy_prepares_salve") }, { TEXT("salve"), TEXT("Enemy_salve") },
		{ TEXT("cathit"), TEXT("Cat_got_hit") }, { TEXT("dead"), TEXT("Player_dead") } };
	for (const auto& N : Names)
	{
		const FString Full = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies") / (FString(N[1]) + TEXT(".mp4")));
		if (!IFileManager::Get().FileExists(*Full))
		{
			UE_LOG(LogShadowCat, Warning, TEXT("Riese: Video fehlt %s"), *Full);
			continue;
		}
		UFileMediaSource* S = NewObject<UFileMediaSource>(this);
		S->SetFilePath(Full);
		Clips.Add(FName(N[0]), S);
	}
	if (!Clips.Contains(TEXT("idle")))
	{
		return;
	}
	ScreenMat = RunAssets::NewMID(TEXT("M_Video"), this);
	if (!ScreenMat)
	{
		return;
	}
	Player = NewObject<UMediaPlayer>(this);
	Player->PlayOnOpen = true;
	VideoTex = NewObject<UMediaTexture>(this);
	VideoTex->AutoClear = true;
	VideoTex->ClearColor = FLinearColor::Black;
	VideoTex->SetMediaPlayer(Player);
	VideoTex->UpdateResource();
	ScreenMat->SetTextureParameterValue(TEXT("Video"), VideoTex);
	ScreenMat->SetScalarParameterValue(TEXT("Fade"), 0.f);
	ScreenMat->SetScalarParameterValue(TEXT("Gain"), VideoGain);
	Screen = RunAssets::AddShape(this, Root, TEXT("Plane"), FVector(0.f, 0.f, VideoBottom + VideoSize * 0.5f), FVector(VideoSize / 100.f, VideoSize / 100.f, 1.f), FRotator::ZeroRotator, ScreenMat);
	Screen->SetUsingAbsoluteRotation(true);
	// Wolkenbank vor dem Fuss des Djinns (weiss, dazwischen dunkle Schwaden): er steigt aus dem Nebel auf
	for (int32 I = 0; I < 10; ++I)
	{
		const bool bDark = I % 4 == 3;
		UMaterialInstanceDynamic* FM = RunAssets::NewMID(TEXT("M_FogPuff"), this);
		if (!FM)
		{
			break;
		}
		FM->SetScalarParameterValue(TEXT("Variant"), float(I % 4));
		FM->SetScalarParameterValue(TEXT("Opacity"), bDark ? 0.75f : 0.85f);
		FM->SetScalarParameterValue(TEXT("Intensity"), 1.1f);
		FM->SetVectorParameterValue(TEXT("Color"), bDark ? FLinearColor::Black : FLinearColor(0.85f, 0.85f, 0.88f));
		const float Y = (float(I) / 9.f - 0.5f) * 8000.f;
		const FVector Loc(bDark ? 700.f : 900.f + 150.f * (I % 2), Y, bDark ? 1000.f : 250.f + 220.f * (I % 3));
		const FVector2D Size(bDark ? 3400.f : 4400.f, bDark ? 1600.f : 1800.f);
		UStaticMeshComponent* Q = RunAssets::AddShape(this, Root, TEXT("Plane"), Loc, FVector(Size.X / 100.f, Size.Y / 100.f, 1.f), FRotator::ZeroRotator, FM);
		Q->SetUsingAbsoluteRotation(true);
		Q->SetTranslucentSortPriority(2);
		BankFog.Add(Q);
		BankBase.Add(Loc);
	}
	Mesh->SetVisibility(false);
	bVideo = true;
	UE_LOG(LogShadowCat, Log, TEXT("Riese: Videos (%d Clips)"), Clips.Num());
}

void ABossGiant::PlayClip(FName Clip, bool bLoop, FName Then)
{
	if (!bVideo || !Clips.Contains(Clip))
	{
		return;
	}
	if (FadeState == 0 && Clip == CurClip && bCurLoop && bLoop)
	{
		return; // laeuft schon
	}
	PendingClip = Clip;
	bPendingLoop = bLoop;
	QueuedClip = Clips.Contains(Then) ? Then : NAME_None;
	FadeState = 1;
}

void ABossGiant::PlayIdle()
{
	// Ruhe: "Enemy idle" und "Enemy idle 2" im Wechsel (bei nur einem Idle-Video: Schleife)
	const bool bTwo = Clips.Contains(TEXT("idle2"));
	if (!bTwo)
	{
		PlayClip(TEXT("idle"), true);
		return;
	}
	PlayClip(CurClip == TEXT("idle") ? FName(TEXT("idle2")) : FName(TEXT("idle")), false);
}

void ABossGiant::StepVideo(float DeltaTime)
{
	switch (FadeState)
	{
	case 1:
		// Ueberblendung: erst nach Schwarz ...
		Fade = FMath::Max(0.f, Fade - DeltaTime / FMath::Max(0.01f, FadeTime));
		if (Fade <= 0.f)
		{
			CurClip = PendingClip;
			bCurLoop = bPendingLoop;
			Player->SetLooping(bCurLoop);
			Player->OpenSource(Clips[CurClip]);
			UE_LOG(LogShadowCat, Log, TEXT("Riese-Video: %s"), *CurClip.ToString());
			FadeState = 2;
			FadeWait = 0.f;
		}
		break;
	case 2:
		// ... warten, bis das neue Video wirklich laeuft ...
		FadeWait += DeltaTime;
		if ((Player->IsPlaying() && Player->GetTime().GetTotalSeconds() > 0.04) || FadeWait > 1.5f)
		{
			FadeState = 3;
		}
		break;
	case 3:
		// ... dann wieder aufblenden
		Fade = FMath::Min(1.f, Fade + DeltaTime / FMath::Max(0.01f, FadeTime));
		if (Fade >= 1.f)
		{
			FadeState = 0;
		}
		break;
	default:
		if (!bCurLoop && CurClip != NAME_None)
		{
			// Einmal-Clip kurz vor dem Ende: zurueck zu idle
			const double Dur = Player->GetDuration().GetTotalSeconds();
			const double T = Player->GetTime().GetTotalSeconds();
			if ((Dur > 0.0 && T >= Dur - FadeTime) || (!Player->IsPlaying() && !Player->IsPreparing()))
			{
				if (QueuedClip != NAME_None)
				{
					PlayClip(QueuedClip, false);
				}
				else
				{
					PlayIdle();
				}
			}
		}
		break;
	}
	ScreenMat->SetScalarParameterValue(TEXT("Fade"), Fade);
}

FVector ABossGiant::HandWorld() const
{
	if (bVideo && Screen)
	{
		// Wurf aus der Brust des Geistes (etwas vor der Tafel)
		return Screen->GetComponentLocation() + FVector(-150.f, 0.f, -VideoSize * 0.08f);
	}
	if (!bPlaceholder)
	{
		return GetActorLocation() + GetActorRotation().RotateVector(HandOffset);
	}
	return ThrowArm ? ThrowArm->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, -1600.f)) : GetActorLocation();
}

float ABossGiant::TimeToImpact() const
{
	float Best = 999.f;
	for (const FFlight& F : Flights)
	{
		if (F.T > 1.f)
		{
			continue;
		}
		const float Left = F.T < 0.f
			? FMath::Max(0.f, WindUp + F.Shot.Delay - PhaseTime) + FlightTime
			: (1.f - F.T) * FlightTime;
		Best = FMath::Min(Best, Left);
	}
	return Best;
}

void ABossGiant::StartAttack(const FVector& Center, const FVector2D& Extent)
{
	FGiantShot S;
	S.Target = Center;
	S.Extent = Extent;
	StartAttack(TArray<FGiantShot>{ S });
}

void ABossGiant::StartAttack(const TArray<FGiantShot>& Shots)
{
	if (!bActive || Phase != 0 || Shots.Num() == 0)
	{
		return;
	}
	Phase = 1;
	PhaseTime = 0.f;
	Impacts.Reset();
	Flights.Reset();
	for (int32 I = 0; I < Shots.Num(); ++I)
	{
		while (BlobPool.Num() <= I)
		{
			BlobPool.Add(NewBlob());
			UMaterialInstanceDynamic* Mat = nullptr;
			MarkerPool.Add(NewMarker(Mat));
			MarkerMats.Add(Mat);
		}
		FFlight F;
		F.Shot = Shots[I];
		F.Blob = BlobPool[I];
		F.Marker = MarkerPool[I];
		F.MarkerMat = MarkerMats[I];
		// alle Zielmarkierungen sofort zeigen: der Spieler kann den freien Weg vorausplanen
		F.Marker->SetWorldLocation(F.Shot.Target + FVector(0.f, 0.f, 3.f));
		F.Marker->SetWorldScale3D(FVector(F.Shot.Extent.X / 50.f, F.Shot.Extent.Y / 50.f, 1.f));
		F.Marker->SetVisibility(true);
		Flights.Add(F);
	}
	if (!bPlaceholder && AttackClip)
	{
		Mesh->PlayAnimation(AttackClip, false);
	}
	// Einzelwurf: Tinte vorbereiten; Salve: vorbereiten, dann Salven-Video
	PlayClip(TEXT("prepare"), false, Shots.Num() > 1 ? FName(TEXT("salve")) : NAME_None);
}

TArray<int32> ABossGiant::ConsumeImpacts()
{
	TArray<int32> Out = MoveTemp(Impacts);
	Impacts.Reset();
	return Out;
}

void ABossGiant::StepGiant(float DeltaTime, const FVector& CatLoc)
{
	if (!bActive)
	{
		return;
	}
	Time += DeltaTime;
	// geht in der Ferne mit, blickt zur Katze
	SetActorLocationAndRotation(FVector(CatLoc.X + Distance, SideOffset, 0.f), FRotator(0.f, 180.f, 0.f));
	if (bVideo)
	{
		// Videotafel senkrecht, zur Katze gedreht
		const FVector To = CatLoc - Screen->GetComponentLocation();
		const FRotator Face(0.f, FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X)) - 90.f, 90.f);
		Screen->SetWorldRotation(Face);
		for (int32 I = 0; I < BankFog.Num(); ++I)
		{
			// langsames Wabern der Wolkenbank
			const FVector Off(0.f, 260.f * FMath::Sin(Time * 0.13f + I * 1.9f), 80.f * FMath::Sin(Time * 0.21f + I * 1.3f));
			BankFog[I]->SetRelativeLocation(BankBase[I] + Off);
			BankFog[I]->SetWorldRotation(Face);
		}
		StepVideo(DeltaTime);
	}
	if (Body)
	{
		Body->SetRelativeLocation(FVector(0.f, 0.f, 60.f * FMath::Sin(Time * 0.6f)));
		Body->SetRelativeRotation(FRotator(0.f, 6.f * FMath::Sin(Time * 0.35f), 2.f * FMath::Sin(Time * 0.5f)));
	}

	if (Phase == 0)
	{
		if (ThrowArm)
		{
			ThrowArm->SetRelativeRotation(FRotator(8.f * FMath::Sin(Time * 0.8f), 0.f, 0.f));
		}
		return;
	}
	PhaseTime += DeltaTime;
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 12.f);
	if (Phase == 1)
	{
		// ausholen: Arm nach hinten oben
		const float T = FMath::Clamp(PhaseTime / WindUp, 0.f, 1.f);
		if (ThrowArm)
		{
			ThrowArm->SetRelativeRotation(FRotator(-150.f * T * T, 0.f, 0.f));
		}
		if (PhaseTime >= WindUp)
		{
			Phase = 2;
		}
	}

	bool bAllDone = Phase == 2;
	for (int32 I = 0; I < Flights.Num(); ++I)
	{
		FFlight& F = Flights[I];
		if (F.T > 1.f)
		{
			continue;
		}
		bAllDone = false;
		if (F.MarkerMat)
		{
			F.MarkerMat->SetScalarParameterValue(TEXT("Opacity"), 0.4f + 0.5f * Pulse);
		}
		if (F.T < 0.f)
		{
			if (Phase == 2 && PhaseTime >= WindUp + F.Shot.Delay)
			{
				// Abwurf
				F.T = 0.f;
				F.Start = HandWorld();
				LastThrow = Time;
				F.Blob->SetVisibility(true);
			}
			continue;
		}
		F.T += DeltaTime / FMath::Max(0.1f, FlightTime);
		// Flug: Bogen von der Hand zum Ziel (das Ziel bewegt sich nicht)
		const float T = FMath::Clamp(F.T, 0.f, 1.f);
		FVector P = FMath::Lerp(F.Start, F.Shot.Target, T);
		P.Z += FMath::Sin(T * PI) * 2500.f;
		F.Blob->SetWorldLocation(P);
		F.Blob->SetWorldScale3D(FVector(FMath::Lerp(4.f, 2.2f, T)));
		if (F.T >= 1.f)
		{
			F.T = 1.01f;
			F.Blob->SetVisibility(false);
			F.Marker->SetVisibility(false);
			Impacts.Add(I);
		}
	}
	if (Phase == 2 && ThrowArm)
	{
		// Arm schnellt bei jedem Wurf nach vorn und holt fuer den naechsten wieder aus
		const float S = FMath::Clamp((Time - LastThrow) / 0.25f, 0.f, 1.f);
		ThrowArm->SetRelativeRotation(FRotator(FMath::Lerp(60.f, -110.f, S * S), 0.f, 0.f));
	}
	if (bAllDone)
	{
		Phase = 0;
		Flights.Reset();
		if (!bPlaceholder && IdleClip)
		{
			Mesh->PlayAnimation(IdleClip, true);
		}
	}
}
