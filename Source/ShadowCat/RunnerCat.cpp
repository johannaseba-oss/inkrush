#include "RunnerCat.h"
#include "ShadowCat.h"
#include "RunTypes.h"
#include "LaneMovementComponent.h"
#include "BuffComponent.h"
#include "ItemSlotComponent.h"
#include "TrackDirector.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Misc/CommandLine.h"

ARunnerCat::ARunnerCat()
{
	PrimaryActorTick.bCanEverTick = false;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Tilt = CreateDefaultSubobject<USceneComponent>(TEXT("Tilt"));
	Tilt->SetupAttachment(Root);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CatMesh"));
	Mesh->SetupAttachment(Tilt);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;

	PlaceholderRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PlaceholderRoot"));
	PlaceholderRoot->SetupAttachment(Tilt);

	BlobShadow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlobShadow"));
	BlobShadow->SetupAttachment(Root);
	BlobShadow->SetRelativeLocation(FVector(0.f, 0.f, 1.2f));
	BlobShadow->SetRelativeScale3D(FVector(0.95f, 0.75f, 1.f));
	RunAssets::MakeCheap(BlobShadow);

	Lanes = CreateDefaultSubobject<ULaneMovementComponent>(TEXT("Lanes"));
	Buffs = CreateDefaultSubobject<UBuffComponent>(TEXT("Buffs"));
	Slot = CreateDefaultSubobject<UItemSlotComponent>(TEXT("ItemSlot"));

	// Standard-Assets aus Tools/ue_setup.py (austauschbar im Blueprint BP_RunnerCat)
	CatMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Cat/SK_BlackCat.SK_BlackCat")));
	RunLoopAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Cat/A_CatRun.A_CatRun")));
	RunStartAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Cat/A_CatRunStart.A_CatRunStart")));
	StopAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Cat/A_CatRunStop.A_CatRunStop")));
	IdleAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Cat/A_CatIdle.A_CatIdle")));
	JumpAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Cat/A_CatRoll.A_CatRoll")));
	CrawlAnim = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Cat/A_CatCrawl.A_CatCrawl")));
	CatMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_Cat.M_Cat")));
}

void ARunnerCat::BeginPlay()
{
	Super::BeginPlay();
	BlobShadow->SetStaticMesh(RunAssets::Shape(TEXT("Plane")));
	if (UMaterialInstanceDynamic* Shadow = RunAssets::NewMID(TEXT("M_Disc"), this))
	{
		Shadow->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.f, 0.f, 0.f));
		Shadow->SetScalarParameterValue(TEXT("Ring"), 0.f);
		Shadow->SetScalarParameterValue(TEXT("Opacity"), 0.6f);
		BlobShadow->SetMaterial(0, Shadow);
	}
	ApplyAssets();
}

void ARunnerCat::ApplyAssets()
{
	USkeletalMesh* SkMesh = CatMesh.IsNull() ? nullptr : CatMesh.LoadSynchronous();
	bPlaceholder = SkMesh == nullptr;
	if (bPlaceholder)
	{
		UE_LOG(LogShadowCat, Warning, TEXT("Katzenmodell nicht gefunden (%s) - Platzhalter wird verwendet"), *CatMesh.ToString());
		Mesh->SetVisibility(false);
		BuildPlaceholder();
		return;
	}

	Mesh->SetSkeletalMesh(SkMesh);
	Mesh->SetVisibility(true);
	Mesh->SetRelativeScale3D(FVector(MeshScale));
	Mesh->SetRelativeRotation(FRotator(0.f, MeshYaw, 0.f));
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->bEnableUpdateRateOptimizations = false;

	if (UMaterialInterface* Mat = CatMaterial.IsNull() ? nullptr : CatMaterial.LoadSynchronous())
	{
		CatMid = UMaterialInstanceDynamic::Create(Mat, this);
		for (int32 I = 0; I < Mesh->GetNumMaterials(); ++I)
		{
			Mesh->SetMaterial(I, CatMid);
		}
	}
	LoopAnim = RunLoopAnim.IsNull() ? nullptr : RunLoopAnim.LoadSynchronous();
	StartAnim = RunStartAnim.IsNull() ? nullptr : RunStartAnim.LoadSynchronous();
	EndAnim = StopAnim.IsNull() ? nullptr : StopAnim.LoadSynchronous();
	IdleClip = IdleAnim.IsNull() ? nullptr : IdleAnim.LoadSynchronous();
	JumpClip = JumpAnim.IsNull() ? nullptr : JumpAnim.LoadSynchronous();
	CrawlClip = CrawlAnim.IsNull() ? nullptr : CrawlAnim.LoadSynchronous();
	// "flying idle" aus den AccuRig-Clips fuer den Flug auf der Tintenwolke
	FlyClip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Cat/A_CatJump.A_CatJump"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!LoopAnim)
	{
		UE_LOG(LogShadowCat, Warning, TEXT("Laufanimation fehlt (%s)"), *RunLoopAnim.ToString());
	}
	UE_LOG(LogShadowCat, Log, TEXT("Katze: %s, Loop %s, Start %s, Stop %s"), *SkMesh->GetName(), LoopAnim ? *LoopAnim->GetName() : TEXT("-"), StartAnim ? *StartAnim->GetName() : TEXT("-"), EndAnim ? *EndAnim->GetName() : TEXT("-"));
}

void ARunnerCat::BuildPlaceholder()
{
	PlaceholderMid = RunAssets::NewMID(TEXT("M_Mono"), this);
	if (PlaceholderMid)
	{
		PlaceholderMid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.025f, 0.025f, 0.025f));
		PlaceholderMid->SetScalarParameterValue(TEXT("Emissive"), 0.f);
	}
	UMaterialInterface* Body = PlaceholderMid;
	UMaterialInterface* Eye = RunAssets::Glow(0.8f, 1.8f);
	auto Add = [this](const TCHAR* S, const FVector& L, const FVector& Sc, const FRotator& R, UMaterialInterface* M)
	{
		RunAssets::AddShape(this, PlaceholderRoot, S, L, Sc, R, M);
	};
	// Blickrichtung +X, ca. 150 cm hoch wie das skalierte Modell
	Add(TEXT("Sphere"), FVector(0.f, 0.f, 62.f), FVector(0.55f, 0.46f, 0.62f), FRotator::ZeroRotator, Body);
	Add(TEXT("Sphere"), FVector(6.f, 0.f, 112.f), FVector(0.55f), FRotator::ZeroRotator, Body);
	Add(TEXT("Cone"), FVector(6.f, -16.f, 148.f), FVector(0.16f, 0.16f, 0.28f), FRotator(0.f, 0.f, -12.f), Body);
	Add(TEXT("Cone"), FVector(6.f, 16.f, 148.f), FVector(0.16f, 0.16f, 0.28f), FRotator(0.f, 0.f, 12.f), Body);
	Add(TEXT("Sphere"), FVector(30.f, -11.f, 116.f), FVector(0.06f, 0.09f, 0.11f), FRotator::ZeroRotator, Eye);
	Add(TEXT("Sphere"), FVector(30.f, 11.f, 116.f), FVector(0.06f, 0.09f, 0.11f), FRotator::ZeroRotator, Eye);
	Add(TEXT("Cylinder"), FVector(-30.f, 0.f, 60.f), FVector(0.07f, 0.07f, 0.75f), FRotator(-55.f, 0.f, 0.f), Body);
	Add(TEXT("Cylinder"), FVector(0.f, -12.f, 18.f), FVector(0.13f, 0.13f, 0.38f), FRotator::ZeroRotator, Body);
	Add(TEXT("Cylinder"), FVector(0.f, 12.f, 18.f), FVector(0.13f, 0.13f, 0.38f), FRotator::ZeroRotator, Body);
}

void ARunnerCat::PlayAnim(UAnimSequence* Anim, bool bLoop, float Rate)
{
	if (bPlaceholder || !Anim)
	{
		return;
	}
	Mesh->PlayAnimation(Anim, bLoop);
	Mesh->SetPlayRate(Rate);
}

void ARunnerCat::SetLayout(const FCircuitLayout& InLayout)
{
	Layout = InLayout;
	Lanes->SetLayout(InLayout);
}

void ARunnerCat::ApplyTransform()
{
	FVector P;
	Layout.Sample(A, Lanes->GetLateralOffset(), P, Forward);
	SetActorLocationAndRotation(P + FVector(0.f, 0.f, JumpZ), Forward.Rotation());
	// Schatten auf dem Untergrund (Strecke oder Plateau); im Loch keiner
	BlobShadow->SetRelativeLocation(FVector(0.f, 0.f, 1.2f - JumpZ + SupportZ));
	BlobShadow->SetVisibility(JumpZ > -5.f);
	BlobShadow->SetRelativeScale3D(FVector(0.95f, 0.75f, 1.f) * FMath::Lerp(1.f, 0.55f, FMath::Clamp((JumpZ - SupportZ) / 250.f, 0.f, 1.f)));
}

void ARunnerCat::ResetForMenu(float StartA, int32 StartLane)
{
	Buffs->ClearAll();
	Lanes->ResetLanes(StartLane);
	A = Layout.WrapA(StartA);
	Travel = 0.f;
	JumpZ = 0.f;
	SupportZ = 0.f;
	JumpBuffer = 0.f;
	CrawlTime = 0.f;
	CrawlBlend = 0.f;
	bCrawlOnLand = false;
	bCrawlAnimOn = false;
	bFalling = false;
	bFallEnd = false;
	bFlying = false;
	bUsedDoubleJump = false;
	bJumpEvent = false;
	bLandEvent = false;
	Tilt->SetRelativeScale3D(FVector(1.f));
	ApplyTransform();
	Tilt->SetRelativeLocation(FVector::ZeroVector);
	Tilt->SetRelativeRotation(FRotator::ZeroRotator);
	DeathTime = -1.f;
	AbsorbFlash = 0.f;
	InvulnTime = 0.f;
	StumbleTime = -1.f;
	Tilt->SetVisibility(true, true);
	bInStartClip = false;
	SetFlash(0.f);
	bJumping = false;
	VelZ = 0.f;
	// Startbildschirm: Warte-Animation, sonst erstes Bild des Anlaufs (aufrecht)
	if (IdleClip)
	{
		PlayAnim(IdleClip, true, 1.f);
	}
	else if (StartAnim)
	{
		PlayAnim(StartAnim, false, 0.f);
	}
	else
	{
		PlayAnim(LoopAnim, true, 0.f);
	}
}

void ARunnerCat::BeginRun()
{
	DeathTime = -1.f;
	if (StartAnim)
	{
		const float Rate = 1.3f;
		PlayAnim(StartAnim, false, Rate);
		bInStartClip = true;
		// Anlauf nur bis in den Laufschritt, dann in die Schleife wechseln
		StartClipLeft = StartAnim->GetPlayLength() / Rate * 0.55f;
	}
	else
	{
		PlayAnim(LoopAnim, true, 1.f);
	}
}

int32 ARunnerCat::RequestLaneShift(int32 Dir, bool bAllowCircuitChange)
{
	return DeathTime < 0.f ? Lanes->RequestShift(Dir, bAllowCircuitChange) : 0;
}

bool ARunnerCat::RequestJump()
{
	if (DeathTime >= 0.f)
	{
		return false;
	}
	if (bJumping)
	{
		// Doppelsprung (Shop): in der Luft noch einmal abspringen (nicht knapp ueber dem Boden, da wird vorgemerkt)
		if (bDoubleJump && !bUsedDoubleJump && !bFlying && (VelZ > 0.f || JumpZ - SupportZ >= 70.f))
		{
			bUsedDoubleJump = true;
			VelZ = 4.f * JumpHeight / FMath::Max(0.2f, JumpDuration) * 0.9f;
			bJumpEvent = true;
			if (JumpClip)
			{
				PlayAnim(JumpClip, false, JumpClip->GetPlayLength() / FMath::Max(0.2f, JumpDuration));
			}
			return true;
		}
		// kurz vor der Landung getippt: merken und bei der Landung springen
		if (VelZ < 0.f && JumpZ - SupportZ < 70.f)
		{
			JumpBuffer = 0.18f;
		}
		return false;
	}
	JumpBuffer = 0.f;
	bJumping = true;
	bInStartClip = false;
	// Sprung beendet das Kriechen; Ereignis fuer Tropfen-Effekt und Sprung-Sound
	if (bFalling)
	{
		return false;
	}
	CrawlTime = 0.f;
	bCrawlOnLand = false;
	bCrawlAnimOn = false;
	bJumpEvent = true;
	// Wurfparabel: Hoehe H in der Zeit T/2 -> v0 = 4H/T, g = 8H/T^2
	VelZ = 4.f * JumpHeight / FMath::Max(0.2f, JumpDuration);
	UE_LOG(LogShadowCat, Verbose, TEXT("Sprung (Clip %s)"), JumpClip ? *JumpClip->GetName() : TEXT("-"));
	if (JumpClip)
	{
		PlayAnim(JumpClip, false, JumpClip->GetPlayLength() / FMath::Max(0.2f, JumpDuration));
	}
	return true;
}

void ARunnerCat::RequestDrop()
{
	if (bJumping)
	{
		VelZ = FMath::Min(VelZ, -4.f * JumpHeight / FMath::Max(0.2f, JumpDuration) * 1.6f);
	}
}

void ARunnerCat::RequestCrawl()
{
	if (DeathTime >= 0.f || bFlying)
	{
		return;
	}
	if (bJumping)
	{
		// in der Luft: schnell runter und direkt bei der Landung kriechen
		RequestDrop();
		bCrawlOnLand = true;
		return;
	}
	CrawlTime = CrawlDuration;
}

void ARunnerCat::StartFall()
{
	bFalling = true;
	bFallEnd = false;
	FallTime = 0.f;
	bJumping = false;
	JumpBuffer = 0.f;
	CrawlTime = 0.f;
	bCrawlOnLand = false;
	VelZ = -150.f;
}

void ARunnerCat::StartFly(float Height)
{
	if (DeathTime >= 0.f || bFalling)
	{
		return;
	}
	FlyHeight = Height;
	if (!bFlying)
	{
		bFlying = true;
		bJumping = true;
		JumpBuffer = 0.f;
		CrawlTime = 0.f;
		bCrawlOnLand = false;
		bCrawlAnimOn = false;
		if (FlyClip && !bPlaceholder)
		{
			PlayAnim(FlyClip, true, 1.f);
		}
	}
}

void ARunnerCat::StopFly()
{
	if (!bFlying)
	{
		return;
	}
	// sanft herunter: faellt normal und landet mit Laufanimation
	bFlying = false;
	bJumping = true;
	VelZ = 0.f;
}

void ARunnerCat::WarpToA(float NewA)
{
	Travel += NewA - A;
	A = Layout.WrapA(NewA);
	ApplyTransform();
}

void ARunnerCat::StepVertical(float DeltaTime)
{
	JumpBuffer -= DeltaTime;
	if (bFlying)
	{
		// auf der Tintenwolke: weich auf Flughoehe, leichtes Schaukeln
		const float Target = SupportZ + FlyHeight + 12.f * FMath::Sin(Time * 3.f);
		JumpZ = FMath::FInterpTo(JumpZ, Target, DeltaTime, 5.f);
		VelZ = 0.f;
		return;
	}
	if (bFalling)
	{
		// ins Loch: frei fallen, dann (lebend) wieder oben hinter dem Loch weiter
		const float T = FMath::Max(0.2f, JumpDuration);
		VelZ -= 8.f * JumpHeight / (T * T) * DeltaTime;
		JumpZ += VelZ * DeltaTime;
		FallTime += DeltaTime;
		if (FallTime >= FallDuration)
		{
			if (DeathTime >= 0.f)
			{
				// letztes Leben: bleibt unten verschwunden
				Tilt->SetVisibility(false, true);
				VelZ = 0.f;
				return;
			}
			bFalling = false;
			bFallEnd = true;
			JumpZ = 0.f;
			VelZ = 0.f;
			if (LoopAnim && !bPlaceholder)
			{
				PlayAnim(LoopAnim, true, 1.f);
			}
		}
		return;
	}
	if (!bJumping && JumpZ <= SupportZ)
	{
		// auf dem Boden bzw. Deckel (kleine Stufen werden hochgestiegen)
		JumpZ = SupportZ;
		VelZ = 0.f;
		return;
	}
	const float T = FMath::Max(0.2f, JumpDuration);
	const float Gravity = 8.f * JumpHeight / (T * T);
	VelZ -= Gravity * DeltaTime;
	JumpZ += VelZ * DeltaTime;
	if (JumpZ <= SupportZ && VelZ <= 0.f)
	{
		JumpZ = SupportZ;
		Land();
		bLandEvent = true;
		if (bCrawlOnLand && DeathTime < 0.f)
		{
			bCrawlOnLand = false;
			CrawlTime = CrawlDuration;
		}
		else if (JumpBuffer > 0.f && DeathTime < 0.f)
		{
			JumpBuffer = 0.f;
			RequestJump();
		}
	}
}

void ARunnerCat::Land()
{
	const bool bWasJumping = bJumping;
	bJumping = false;
	bUsedDoubleJump = false;
	VelZ = 0.f;
	if (bWasJumping && DeathTime < 0.f)
	{
		const float Rate = FMath::Clamp(RunSpeed / FMath::Max(1.f, AnimRootSpeed * MeshScale), MinPlayRate, MaxPlayRate);
		PlayAnim(LoopAnim, true, Rate);
	}
}

void ARunnerCat::StepRun(float DeltaTime, float Speed)
{
	Time += DeltaTime;
	RunSpeed = Speed;
	Lanes->StepLanes(DeltaTime);
	// konstante Weltgeschwindigkeit: in Kurven aussen mehr Weg pro A
	A = Layout.WrapA(A + Speed * DeltaTime / Layout.Stretch(A, Lanes->GetLateralOffset()));
	Travel += Speed * DeltaTime;
	StepVertical(DeltaTime);
	ApplyTransform();

	const float Lat = Lanes->GetLateralVelocityNorm();
	float StumblePitch = 0.f;
	if (StumbleTime >= 0.f)
	{
		StumbleTime += DeltaTime;
		const float T = FMath::Clamp(StumbleTime / 0.5f, 0.f, 1.f);
		StumblePitch = -28.f * FMath::Sin(T * PI);
		if (T >= 1.f)
		{
			StumbleTime = -1.f;
		}
	}
	Tilt->SetRelativeRotation(FRotator(StumblePitch, Lat * 18.f, Lat * 12.f));
	// Kriechen: eigene Animation (A_CatCrawl); ohne sie flach gedrueckt
	CrawlTime = FMath::Max(0.f, CrawlTime - DeltaTime);
	const bool bCrawlNow = CrawlTime > 0.f && !bJumping && !bFalling;
	if (CrawlClip && !bPlaceholder)
	{
		if (bCrawlNow && !bCrawlAnimOn)
		{
			bCrawlAnimOn = true;
			bInStartClip = false;
			PlayAnim(CrawlClip, true, CrawlPlayRate);
		}
		else if (!bCrawlNow && bCrawlAnimOn)
		{
			bCrawlAnimOn = false;
			if (!bJumping)
			{
				PlayAnim(LoopAnim, true, FMath::Clamp(Speed / FMath::Max(1.f, AnimRootSpeed * MeshScale), MinPlayRate, MaxPlayRate));
			}
		}
	}
	else
	{
		CrawlBlend = FMath::FInterpConstantTo(CrawlBlend, bCrawlNow ? 1.f : 0.f, DeltaTime, 9.f);
		const float Cb = CrawlBlend * CrawlBlend * (3.f - 2.f * CrawlBlend);
		Tilt->SetRelativeScale3D(FVector(FMath::Lerp(1.f, 1.25f, Cb), FMath::Lerp(1.f, 1.12f, Cb), FMath::Lerp(1.f, 0.42f, Cb)));
	}
	if (InvulnTime > 0.f)
	{
		InvulnTime -= DeltaTime;
		const bool bShow = InvulnTime <= 0.f || FMath::Fmod(InvulnTime, 0.2f) > 0.08f;
		Tilt->SetVisibility(bShow, true);
	}

	const float Rate = FMath::Clamp(Speed / FMath::Max(1.f, AnimRootSpeed * MeshScale), MinPlayRate, MaxPlayRate);
	if (bInStartClip)
	{
		StartClipLeft -= DeltaTime;
		if (StartClipLeft <= 0.f)
		{
			bInStartClip = false;
			PlayAnim(LoopAnim, true, Rate);
		}
	}
	else if (!bPlaceholder && !bJumping && !bCrawlAnimOn)
	{
		Mesh->SetPlayRate(Rate);
	}

	if (bPlaceholder)
	{
		const float Ph = Time * Rate * 2.f * PI * 1.7f;
		PlaceholderRoot->SetRelativeLocation(FVector(0.f, 0.f, FMath::Abs(FMath::Sin(Ph)) * 10.f));
		PlaceholderRoot->SetRelativeRotation(FRotator(-6.f + 4.f * FMath::Sin(Ph * 2.f), 0.f, 0.f));
	}

	static const bool bAnimLog = FParse::Param(FCommandLine::Get(), TEXT("CatAnimLog"));
	if (bAnimLog && !bPlaceholder && Time > 3.f && Time < 4.2f)
	{
		const FQuat Q = Mesh->GetBoneQuaternion(TEXT("CC_Base_L_Thigh"), EBoneSpaces::ComponentSpace);
		UE_LOG(LogShadowCat, Log, TEXT("ANIM t %.3f dt %.4f anim %s pos %.3f rate %.2f playing %d thigh %s"), Time, DeltaTime, Mesh->GetSingleNodeInstance() && Mesh->GetSingleNodeInstance()->GetAnimationAsset() ? *Mesh->GetSingleNodeInstance()->GetAnimationAsset()->GetName() : TEXT("-"), Mesh->GetPosition(), Mesh->GetPlayRate(), Mesh->IsPlaying() ? 1 : 0, *Q.Rotator().ToString());
	}
	Buffs->StepBuffs(DeltaTime);
	AbsorbFlash = FMath::Max(0.f, AbsorbFlash - DeltaTime * 4.f);
	SetFlash(BuffFlash);
}

void ARunnerCat::StepIdle(float DeltaTime)
{
	Time += DeltaTime;
	StepVertical(DeltaTime);
	ApplyTransform();
	if (DeathTime >= 0.f)
	{
		DeathTime += DeltaTime;
		// Rueckstoss mit kleinem Hopser und Kippen nach hinten
		const float T = FMath::Clamp(DeathTime / 0.45f, 0.f, 1.f);
		A = Layout.WrapA(A - 420.f * (1.f - T) * DeltaTime / Layout.Stretch(A, Lanes->GetLateralOffset()));
		Tilt->SetRelativeLocation(FVector(0.f, 0.f, 45.f * FMath::Sin(T * PI)));
		Tilt->SetRelativeRotation(FRotator(-22.f * T, 0.f, 0.f));
	}
	else if (bPlaceholder)
	{
		PlaceholderRoot->SetRelativeLocation(FVector(0.f, 0.f, 2.f * FMath::Sin(Time * 2.f)));
	}
	AbsorbFlash = FMath::Max(0.f, AbsorbFlash - DeltaTime * 2.5f);
	SetFlash(BuffFlash);
}

void ARunnerCat::Die()
{
	DeathTime = 0.f;
	InvulnTime = 0.f;
	Tilt->SetVisibility(true, true);
	bJumping = false;
	VelZ = FMath::Min(VelZ, 0.f);
	SupportZ = 0.f;
	CrawlTime = 0.f;
	CrawlBlend = 0.f;
	bCrawlOnLand = false;
	bCrawlAnimOn = false;
	bFlying = false;
	Tilt->SetRelativeScale3D(FVector(1.f));
	Buffs->ClearAll();
	BuffFlash = 0.f;
	AbsorbFlash = 1.f;
	bInStartClip = false;
	if (EndAnim)
	{
		PlayAnim(EndAnim, false, 1.7f);
	}
	else if (!bPlaceholder)
	{
		Mesh->SetPlayRate(0.f);
	}
	SetFlash(0.f);
}

void ARunnerCat::Stumble(float InvulnSeconds)
{
	InvulnTime = InvulnSeconds;
	StumbleTime = 0.f;
	AbsorbFlash = 1.f;
}

void ARunnerCat::OnHitAbsorbed()
{
	AbsorbFlash = 0.8f;
}

void ARunnerCat::SetFlash(float Amount)
{
	BuffFlash = Amount;
	const float F = FMath::Clamp(FMath::Max(BuffFlash, AbsorbFlash), 0.f, 1.f);
	if (CatMid)
	{
		CatMid->SetScalarParameterValue(TEXT("Flash"), F);
	}
	if (PlaceholderMid)
	{
		const float G = FMath::Lerp(0.025f, 0.9f, F);
		PlaceholderMid->SetVectorParameterValue(TEXT("Color"), FLinearColor(G, G, G));
		PlaceholderMid->SetScalarParameterValue(TEXT("Emissive"), F * 1.5f);
	}
}

ATrackDirector* ARunnerCat::GetDirector() const
{
	return Director.Get();
}

void ARunnerCat::SetDirector(ATrackDirector* InDirector)
{
	Director = InDirector;
}
