#include "RunCamera.h"
#include "RunTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CommandLine.h"

ARunCamera::ARunCamera()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->bConstrainAspectRatio = false;
	// Engine-Standard ist MaintainYFOV; unsere FOV-Rechnung (Spuren muessen ins Hochformat passen) ist horizontal
	Camera->bOverrideAspectRatioAxisConstraint = true;
	Camera->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV);
	Camera->SetFieldOfView(60.f);
	// Feste Belichtung (Leuchtdichte 1 = Weiss): dunkle Werte bleiben dunkel, gleiches Bild auf Desktop und iPhone
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = 0;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = 0.f;
	Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
	Camera->PostProcessSettings.VignetteIntensity = 0.55f;
	// kein Lens-Flare: der helle Mond (fest im Bild) warf sonst einen runden Geisterfleck unten links
	Camera->PostProcessSettings.bOverride_LensFlareIntensity = true;
	Camera->PostProcessSettings.LensFlareIntensity = 0.f;
	// Sicherheitsnetz: alles in Graustufen, auch falls ein Asset farbig ist
	Camera->PostProcessSettings.bOverride_ColorSaturation = true;
	Camera->PostProcessSettings.ColorSaturation = FVector4(0.f, 0.f, 0.f, 1.f);

	Moon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Moon"));
	Moon->SetupAttachment(RootComponent);
	Moon->SetUsingAbsoluteLocation(true);
	Moon->SetUsingAbsoluteRotation(true);
	RunAssets::MakeCheap(Moon);
}

void ARunCamera::SetMenuMode(bool bInMenu, bool bInstant)
{
	bMenu = bInMenu;
	if (bInstant)
	{
		Blend = bMenu ? 0.f : 1.f;
	}
	if (!bCelAdded)
	{
		// monochromes Cel Shading (Stufen + Konturen), siehe PP_CelMono in Tools/ue_setup.py
		if (UMaterialInterface* Cel = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/PP_CelMono.PP_CelMono"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Cel));
		}
		bCelAdded = true;
	}
	if (!Moon->GetStaticMesh())
	{
		Moon->SetStaticMesh(RunAssets::Shape(TEXT("Sphere")));
		Moon->SetMaterial(0, RunAssets::Glow(0.92f, 6.f));
	}
}

float ARunCamera::ComputeFov(float VFov, float MinHalfWidth, float Dist) const
{
	FVector2D Size(1080.f, 2340.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(Size);
	}
	const float Aspect = Size.Y > 1.f ? Size.X / Size.Y : 0.5f;
	float HFov = 2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VFov * 0.5f)) * Aspect);
	// Hochformat: mindestens so breit, dass alle Spuren sichtbar sind
	if (MinHalfWidth > 0.f)
	{
		HFov = FMath::Max(HFov, 2.f * FMath::Atan(MinHalfWidth / FMath::Max(1.f, Dist)));
	}
	return FMath::Clamp(FMath::RadiansToDegrees(HFov), 15.f, 100.f);
}

void ARunCamera::ComputeRunView(const FVector& CatLoc, FVector& OutLoc, FRotator& OutRot) const
{
	static const bool bSide = FParse::Param(FCommandLine::Get(), TEXT("CatSideCam"));
	const FVector F = SmoothFwd;
	const FVector Right(-F.Y, F.X, 0.f);
	if (bSide)
	{
		// Test: Seitenansicht zum Pruefen der Animation
		OutLoc = CatLoc - Right * 420.f + FVector(0.f, 0.f, 80.f);
		OutRot = (CatLoc + FVector(0.f, 0.f, 75.f) - OutLoc).Rotation();
		return;
	}
	// geglaettete Bodenposition: Spurwechsel und Kurven wirken ruhiger, Spruenge heben die Kamera nur leicht
	const FVector Base(SmoothPos.X, SmoothPos.Y, CatLoc.Z * 0.35f);
	OutLoc = Base - F * BackDistance + FVector(0.f, 0.f, Height);
	const FVector Look = Base + F * LookAhead + FVector(0.f, 0.f, LookHeight);
	OutRot = (Look - OutLoc).Rotation();
}

void ARunCamera::ComputeMenuView(const FVector& CatLoc, FVector& OutLoc, FRotator& OutRot) const
{
	// Vor der Katze, leicht seitlich, Blick zurueck: Katze im unteren Bilddrittel, Titel darueber
	const FVector F = SmoothFwd;
	const FVector Right(-F.Y, F.X, 0.f);
	OutLoc = CatLoc + F * 440.f - Right * 110.f + FVector(0.f, 0.f, 100.f);
	const FVector Look = CatLoc + FVector(0.f, 0.f, 62.f);
	OutRot = (Look - OutLoc).Rotation();
}

void ARunCamera::StepCamera(float DeltaTime, const FVector& CatLoc, const FVector& CatForward)
{
	Time += DeltaTime;
	const FVector Ground(CatLoc.X, CatLoc.Y, 0.f);
	if (DeltaTime <= 0.f || SmoothFwd.IsNearlyZero() || FVector::DistSquared(SmoothPos, Ground) > 1500.f * 1500.f)
	{
		SmoothPos = Ground;
		SmoothFwd = CatForward.GetSafeNormal2D();
	}
	else
	{
		SmoothPos = FMath::VInterpTo(SmoothPos, Ground, DeltaTime, 9.f);
		SmoothFwd = FMath::VInterpNormalRotationTo(SmoothFwd, CatForward.GetSafeNormal2D(), DeltaTime, 120.f).GetSafeNormal2D();
	}
	Blend = FMath::FInterpConstantTo(Blend, bMenu ? 0.f : 1.f, DeltaTime, 1.f / 0.9f);
	const float A = Blend * Blend * (3.f - 2.f * Blend);

	FVector RunLoc, MenuLoc;
	FRotator RunRot, MenuRot;
	ComputeRunView(CatLoc, RunLoc, RunRot);
	ComputeMenuView(CatLoc, MenuLoc, MenuRot);

	// Bogen statt gerader Linie, damit die Kamera beim Start nicht durch die Katze faehrt
	const FVector Right(-SmoothFwd.Y, SmoothFwd.X, 0.f);
	FVector Loc = FMath::Lerp(MenuLoc, RunLoc, A);
	Loc += Right * (FMath::Sin(A * PI) * -260.f) + FVector(0.f, 0.f, FMath::Sin(A * PI) * 120.f);
	FRotator Rot = FQuat::Slerp(MenuRot.Quaternion(), RunRot.Quaternion(), A).Rotator();
	const float RunFov = ComputeFov(VerticalFov, MinVisibleHalfWidth, FMath::Sqrt(BackDistance * BackDistance + Height * Height));
	const float MenuFov = ComputeFov(MenuVerticalFov, 0.f, 450.f);
	Camera->SetFieldOfView(FMath::Lerp(MenuFov, RunFov, A));

	if (Shake > 0.f)
	{
		Loc += FVector(0.f, FMath::Sin(Time * 71.f), FMath::Sin(Time * 53.f + 1.f)) * Shake * 14.f;
		Shake = FMath::Max(0.f, Shake - DeltaTime * 2.5f);
	}
	SetActorLocationAndRotation(Loc, Rot);

	// Mond: fest am Himmel, weit weg
	Moon->SetWorldLocation(FVector(52000.f, -20000.f, 16000.f));
	Moon->SetWorldScale3D(FVector(40.f));
}
