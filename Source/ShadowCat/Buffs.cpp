#include "Buffs.h"
#include "EnemyCube.h"
#include "RunnerCat.h"
#include "TrackDirector.h"
#include "InkCanvas.h"
#include "LaneMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#define LOCTEXT_NAMESPACE "ShadowCatBuffs"

void BuffVisuals::AddStar(AActor* Owner, USceneComponent* Parent, float Size, UMaterialInterface* Mat, TArray<TObjectPtr<USceneComponent>>* Track)
{
	const float S = Size / 100.f;
	UStaticMeshComponent* A = RunAssets::AddShape(Owner, Parent, TEXT("Cube"), FVector::ZeroVector, FVector(S * 0.72f, S * 0.72f, S * 0.05f), FRotator(0.f, 0.f, 0.f), Mat);
	UStaticMeshComponent* B = RunAssets::AddShape(Owner, Parent, TEXT("Cube"), FVector::ZeroVector, FVector(S * 0.72f, S * 0.72f, S * 0.05f), FRotator(0.f, 45.f, 0.f), Mat);
	UStaticMeshComponent* Hub = RunAssets::AddShape(Owner, Parent, TEXT("Cylinder"), FVector::ZeroVector, FVector(S * 0.3f, S * 0.3f, S * 0.08f), FRotator::ZeroRotator, RunAssets::Glow(0.85f, 1.6f));
	if (Track)
	{
		Track->Add(A);
		Track->Add(B);
		Track->Add(Hub);
	}
}

// ------------------------------------------------------------------------------------------------
// Nebelwolke
// ------------------------------------------------------------------------------------------------
UBuff_ShadowMist::UBuff_ShadowMist()
{
	DisplayName = LOCTEXT("Mist", "NEBELWOLKE");
	Duration = 6.f;
}

void UBuff_ShadowMist::OnBegin()
{
	PuffMat = RunAssets::NewMID(TEXT("M_Soft"), this);
	WispMat = RunAssets::NewMID(TEXT("M_Soft"), this);
	if (PuffMat)
	{
		PuffMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.01f, 0.01f));
		PuffMat->SetScalarParameterValue(TEXT("Softness"), 1.3f);
	}
	if (WispMat)
	{
		WispMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.5f, 0.5f));
		WispMat->SetScalarParameterValue(TEXT("EdgeMode"), 1.f);
		WispMat->SetScalarParameterValue(TEXT("Softness"), 2.5f);
	}
	const int32 N = 7;
	for (int32 I = 0; I < N; ++I)
	{
		const float A = I * 2.f * PI / N;
		const FVector P(FMath::Cos(A) * 85.f, FMath::Sin(A) * 85.f, 45.f + (I % 3) * 35.f);
		const float Sc = 1.1f + 0.25f * (I % 2);
		Puffs.Add(AddVisual(TEXT("Sphere"), P, FVector(Sc), FRotator::ZeroRotator, PuffMat));
		PuffBase.Add(P);
	}
	// heller Rand-Schleier, damit die Wolke vor dunklem Boden lesbar bleibt
	AddVisual(TEXT("Sphere"), FVector(0.f, 0.f, 80.f), FVector(3.6f, 3.6f, 2.6f), FRotator::ZeroRotator, WispMat);
}

void UBuff_ShadowMist::OnTick(float DeltaTime)
{
	const float Fade = GetFade() * GetEndingBlink();
	if (PuffMat)
	{
		PuffMat->SetScalarParameterValue(TEXT("Opacity"), 0.78f * Fade);
	}
	if (WispMat)
	{
		WispMat->SetScalarParameterValue(TEXT("Opacity"), 0.35f * Fade);
	}
	if (VisualRoot)
	{
		VisualRoot->SetRelativeRotation(FRotator(0.f, Age * 70.f, 0.f));
	}
	for (int32 I = 0; I < Puffs.Num(); ++I)
	{
		if (Puffs[I])
		{
			const float W = 1.f + 0.18f * FMath::Sin(Age * 3.2f + I * 1.7f);
			Puffs[I]->SetRelativeScale3D(FVector((1.1f + 0.25f * (I % 2)) * W));
			Puffs[I]->SetRelativeLocation(PuffBase[I] + FVector(0.f, 0.f, 12.f * FMath::Sin(Age * 2.1f + I)));
		}
	}
	if (ATrackDirector* Dir = GetDirector())
	{
		Dir->DefeatEnemiesNear(Cat->GetActorLocation() + FVector(0.f, 0.f, 80.f), ContactRadius);
	}
}

void UBuff_ShadowMist::BuildPickupVisual(AActor* Pickup, USceneComponent* Root) const
{
	RunAssets::AddShape(Pickup, Root, TEXT("Sphere"), FVector::ZeroVector, FVector(0.5f), FRotator::ZeroRotator, RunAssets::Mono(0.02f, 0.f));
	UMaterialInstanceDynamic* Puff = RunAssets::NewMID(TEXT("M_Soft"), Pickup);
	if (Puff)
	{
		Puff->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.f, 0.f, 0.f));
		Puff->SetScalarParameterValue(TEXT("Opacity"), 0.7f);
	}
	for (int32 I = 0; I < 3; ++I)
	{
		const float A = I * 2.f * PI / 3.f;
		RunAssets::AddShape(Pickup, Root, TEXT("Sphere"), FVector(FMath::Cos(A) * 28.f, FMath::Sin(A) * 28.f, 8.f * (I - 1)), FVector(0.55f), FRotator::ZeroRotator, Puff);
	}
	// Ring aus leuchtenden Punkten
	for (int32 I = 0; I < 8; ++I)
	{
		const float A = I * 2.f * PI / 8.f;
		RunAssets::AddShape(Pickup, Root, TEXT("Sphere"), FVector(FMath::Cos(A) * 55.f, FMath::Sin(A) * 55.f, 0.f), FVector(0.09f), FRotator::ZeroRotator, RunAssets::Glow(1.f, 2.5f));
	}
}

// ------------------------------------------------------------------------------------------------
// Shuriken-Wolke
// ------------------------------------------------------------------------------------------------
UBuff_ShurikenStorm::UBuff_ShurikenStorm()
{
	DisplayName = LOCTEXT("Shuriken", "SHURIKEN");
	Duration = 7.f;
}

void UBuff_ShurikenStorm::OnBegin()
{
	for (int32 I = 0; I < Count; ++I)
	{
		FStar S;
		S.Pivot = AddPivot(FVector::ZeroVector);
		BuffVisuals::AddStar(Cat, S.Pivot, 50.f, RunAssets::Mono(0.1f, 0.3f), &Visuals);
		Stars.Add(S);
	}
}

void UBuff_ShurikenStorm::OnTick(float DeltaTime)
{
	Orbit += DeltaTime * 250.f;
	ATrackDirector* Dir = GetDirector();
	const FVector CatLoc = Cat->GetActorLocation();
	const float Blink = GetEndingBlink();

	for (int32 I = 0; I < Stars.Num(); ++I)
	{
		FStar& S = Stars[I];
		if (!S.Pivot)
		{
			continue;
		}
		const float A = FMath::DegreesToRadians(Orbit + I * 360.f / Stars.Num());
		const FVector OrbitPos(FMath::Cos(A) * OrbitRadius, FMath::Sin(A) * OrbitRadius, 75.f + 18.f * FMath::Sin(A * 2.f + Age));

		// freies Ziel suchen: naechster Gegner voraus in Reichweite, noch von keinem Stern anvisiert
		if (!S.bThrowing && Dir)
		{
			AEnemyCube* Best = nullptr;
			float BestDx = Range;
			for (AEnemyCube* E : Dir->GetLiveEnemies())
			{
				const FVector D = E->GetActorLocation() - CatLoc;
				if (D.X > -80.f && D.X < BestDx && FMath::Abs(D.Y) < 480.f && E->IsTargetable())
				{
					Best = E;
					BestDx = D.X;
				}
			}
			if (Best)
			{
				Best->MarkDoomed();
				S.Target = Best;
				S.bThrowing = true;
				S.Throw = 0.f;
				S.ReleasePos = OrbitPos;
			}
		}

		FVector Pos = OrbitPos;
		if (S.bThrowing)
		{
			S.Throw += DeltaTime / FMath::Max(0.02f, ThrowTime);
			if (S.Throw < 1.f)
			{
				const FVector TargetRel = S.Target.IsValid() ? S.Target->GetActorLocation() - CatLoc : S.ReleasePos;
				Pos = FMath::Lerp(S.ReleasePos, TargetRel, S.Throw);
			}
			else
			{
				if (S.Target.IsValid() && Dir)
				{
					Dir->DefeatEnemy(S.Target.Get());
				}
				S.Target = nullptr;
				const float Back = FMath::Clamp(S.Throw - 1.f, 0.f, 1.f);
				Pos = FMath::Lerp(S.ReleasePos, OrbitPos, Back);
				if (S.Throw >= 2.f)
				{
					S.bThrowing = false;
				}
			}
		}
		S.Pivot->SetRelativeLocation(Pos);
		S.Pivot->SetRelativeRotation(FRotator(0.f, Age * 900.f, 0.f));
		S.Pivot->SetRelativeScale3D(FVector(GetFade() * Blink));
	}
}

void UBuff_ShurikenStorm::BuildPickupVisual(AActor* Pickup, USceneComponent* Root) const
{
	USceneComponent* Upright = NewObject<USceneComponent>(Pickup);
	Upright->SetupAttachment(Root);
	Upright->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	Upright->RegisterComponent();
	BuffVisuals::AddStar(Pickup, Upright, 80.f, RunAssets::Mono(0.55f, 0.6f));
}

void UBuff_ShurikenStorm::AnimatePickup(float Time, USceneComponent* Root) const
{
	if (Root)
	{
		Root->SetRelativeLocation(FVector(0.f, 0.f, 90.f + 12.f * FMath::Sin(Time * 3.f)));
		Root->SetRelativeRotation(FRotator(0.f, 90.f + 25.f * FMath::Sin(Time * 1.3f), Time * 240.f));
	}
}

// ------------------------------------------------------------------------------------------------
// Unbesiegbarkeit
// ------------------------------------------------------------------------------------------------
UBuff_Invincible::UBuff_Invincible()
{
	DisplayName = LOCTEXT("Invincible", "UNBESIEGBAR");
	Duration = 5.f;
}

void UBuff_Invincible::OnBegin()
{
	AuraMat = RunAssets::NewMID(TEXT("M_Soft"), this);
	if (AuraMat)
	{
		AuraMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 1.f, 1.f));
		AuraMat->SetScalarParameterValue(TEXT("EdgeMode"), 1.f);
		AuraMat->SetScalarParameterValue(TEXT("Softness"), 1.6f);
		AuraMat->SetScalarParameterValue(TEXT("Intensity"), 2.f);
	}
	Aura = AddVisual(TEXT("Sphere"), FVector(0.f, 0.f, 78.f), FVector(1.5f, 1.5f, 1.9f), FRotator::ZeroRotator, AuraMat);
}

void UBuff_Invincible::OnTick(float DeltaTime)
{
	const float Blink = GetEndingBlink();
	const float Pulse = 0.75f + 0.25f * FMath::Sin(Age * 10.f);
	if (Cat)
	{
		Cat->SetFlash(Pulse * Blink * GetFade());
	}
	if (AuraMat)
	{
		AuraMat->SetScalarParameterValue(TEXT("Opacity"), 0.55f * Blink * GetFade());
	}
	if (Aura)
	{
		const float S = 1.f + 0.06f * FMath::Sin(Age * 7.f);
		Aura->SetRelativeScale3D(FVector(1.5f * S, 1.5f * S, 1.9f * S));
	}
}

void UBuff_Invincible::OnEnd()
{
	if (Cat)
	{
		Cat->SetFlash(0.f);
	}
}

void UBuff_Invincible::BuildPickupVisual(AActor* Pickup, USceneComponent* Root) const
{
	// Diamant (auf der Spitze stehender Wuerfel) mit hellem Schein
	RunAssets::AddShape(Pickup, Root, TEXT("Cube"), FVector::ZeroVector, FVector(0.42f), FRotator(45.f, 0.f, 35.26f), RunAssets::Glow(1.f, 2.2f));
	UMaterialInstanceDynamic* Halo = RunAssets::NewMID(TEXT("M_Soft"), Pickup);
	if (Halo)
	{
		Halo->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 1.f, 1.f));
		Halo->SetScalarParameterValue(TEXT("Opacity"), 0.35f);
		Halo->SetScalarParameterValue(TEXT("Softness"), 2.f);
	}
	RunAssets::AddShape(Pickup, Root, TEXT("Sphere"), FVector::ZeroVector, FVector(1.25f), FRotator::ZeroRotator, Halo);
}

#undef LOCTEXT_NAMESPACE

// ------------------------------------------------------------------------------------------------
// Spraydose
// ------------------------------------------------------------------------------------------------
namespace
{
	/** Kurs, Fahrbahnen und aktuelle Fahrbahn der Katze. */
	int32 CatCircuit(const ARunnerCat* Cat)
	{
		const ULaneMovementComponent* L = Cat->GetLanes();
		return L->GetLayout().CircuitOf(L->GetCurrentLane());
	}
}

UBuff_SprayPaint::UBuff_SprayPaint()
{
	DisplayName = NSLOCTEXT("ShadowCatBuffs", "SprayPaint", "SPRAYDOSE");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/T_Icon_Spaypaint.T_Icon_Spaypaint")));
	Duration = 5.f;
}

void UBuff_SprayPaint::OnBegin()
{
	// kleine Spraydose auf dem Ruecken (schlichter Zylinder: hell mit dunklem Band, Kappe und Duese unten)
	UMaterialInterface* CanM = RunAssets::Mono(0.88f, 0.6f);
	UMaterialInterface* Dark = RunAssets::Mono(0.02f, 0.f);
	USceneComponent* Can = AddPivot(CanOffset);
	if (Can)
	{
		Can->SetRelativeRotation(FRotator(-25.f, 0.f, 0.f));
		AddVisual(TEXT("Cylinder"), FVector(0.f, 0.f, 0.f), FVector(0.26f, 0.26f, 0.52f), FRotator::ZeroRotator, CanM, Can);
		AddVisual(TEXT("Cylinder"), FVector(0.f, 0.f, 6.f), FVector(0.265f, 0.265f, 0.12f), FRotator::ZeroRotator, Dark, Can);
		AddVisual(TEXT("Sphere"), FVector(0.f, 0.f, 26.f), FVector(0.24f, 0.24f, 0.16f), FRotator::ZeroRotator, CanM, Can);
		AddVisual(TEXT("Cylinder"), FVector(0.f, 0.f, 34.f), FVector(0.1f, 0.1f, 0.1f), FRotator::ZeroRotator, Dark, Can);
		AddVisual(TEXT("Cylinder"), FVector(0.f, 0.f, -29.f), FVector(0.1f, 0.1f, 0.1f), FRotator::ZeroRotator, Dark, Can);
	}
	// Spruehstrahl: zusammenhaengender, schwarz glaenzender Fluessigkeitsstrahl aus der Duese (Bogen aus Rohrstuecken
	// mit runden Gelenken, wellt sich und wird nach hinten breiter), am Ende ein weicher Spruehnebel
	UMaterialInterface* Ink = RunAssets::Mono(0.01f, 0.f);
	for (int32 I = 0; I < StreamPoints; ++I)
	{
		Joints.Add(AddVisual(TEXT("Sphere"), FVector::ZeroVector, FVector(0.05f), FRotator::ZeroRotator, Ink));
		if (I + 1 < StreamPoints)
		{
			Segs.Add(AddVisual(TEXT("Cylinder"), FVector::ZeroVector, FVector(0.05f), FRotator::ZeroRotator, Ink));
		}
	}
	MistMat = RunAssets::NewMID(TEXT("M_Soft"), this);
	if (MistMat)
	{
		MistMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.f, 0.f, 0.f));
		MistMat->SetScalarParameterValue(TEXT("Softness"), 1.6f);
		MistMat->SetScalarParameterValue(TEXT("Opacity"), 0.55f);
	}
	Mist.Add(AddVisual(TEXT("Sphere"), FVector::ZeroVector, FVector(0.3f), FRotator::ZeroRotator, MistMat));
	// Shop: laengerer Flug
	Duration += Cat->FlyBonus;
	Remaining = Duration;
	Cat->StartFly(FlyHeight);
	if (ATrackDirector* Dir = GetDirector())
	{
		Dir->OnSprayFlight(true, Duration);
	}
}

bool UBuff_SprayPaint::AbsorbsHit(EHazardKind Kind) const
{
	// oben in der Luft-Ebene gibt es keine Hindernisse
	return true;
}

void UBuff_SprayPaint::OnRefreshed()
{
	// gestapelt: Flug verlaengert (Dauer wurde schon zurueckgesetzt), neue Muenzreihen
	Cat->StartFly(FlyHeight);
	if (ATrackDirector* Dir = GetDirector())
	{
		Dir->OnSprayFlight(true, Duration);
	}
}

void UBuff_SprayPaint::OnEnd()
{
	Cat->StopFly();
	// Landung abgesichert: kurz unverwundbar (Shop "Sichere Landung": laenger)
	Cat->GrantInvulnerability(Cat->bFlyInvulnerable ? 3.f : 1.2f);
	if (ATrackDirector* Dir = GetDirector())
	{
		Dir->OnSprayFlight(false, 0.f);
	}
}

void UBuff_SprayPaint::OnTick(float DeltaTime)
{
	const float Fade = GetFade() * GetEndingBlink();
	// Duese der Dose (schraeg nach hinten-unten gerichtet)
	const FVector Nozzle = CanOffset + FVector(-16.f, 0.f, -32.f);
	// Punkte des Strahls: Bogen nach hinten-unten, Welle wird nach hinten staerker, Dicke waechst
	TArray<FVector> Pts;
	TArray<float> Rad;
	for (int32 I = 0; I < StreamPoints; ++I)
	{
		const float T = I / float(StreamPoints - 1);
		const float Wob = (1.5f + 15.f * T) * FMath::Sin(Age * 21.f - I * 0.85f);
		const float WobZ = (1.f + 7.f * T) * FMath::Sin(Age * 17.f - I * 0.7f + 1.3f);
		Pts.Add(Nozzle + FVector(-240.f * T, Wob, -26.f * T - 125.f * T * T + WobZ));
		Rad.Add((2.5f + 8.f * T) * (1.f + 0.12f * FMath::Sin(Age * 30.f + I * 1.7f)) * Fade);
	}
	for (int32 I = 0; I < Joints.Num(); ++I)
	{
		if (Joints[I])
		{
			Joints[I]->SetRelativeLocation(Pts[I]);
			Joints[I]->SetRelativeScale3D(FVector(Rad[I] * 2.f / 100.f));
		}
	}
	for (int32 I = 0; I < Segs.Num(); ++I)
	{
		if (!Segs[I])
		{
			continue;
		}
		const FVector D = Pts[I + 1] - Pts[I];
		const float R = (Rad[I] + Rad[I + 1]) * 0.5f;
		Segs[I]->SetRelativeLocationAndRotation((Pts[I] + Pts[I + 1]) * 0.5f, FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator());
		Segs[I]->SetRelativeScale3D(FVector(R * 2.f / 100.f, R * 2.f / 100.f, (D.Size() + 2.f) / 100.f));
	}
	if (Mist.Num() > 0 && Mist[0])
	{
		// Spruehnebel am Ende des Strahls
		const float F = 1.f + 0.2f * FMath::Sin(Age * 19.f);
		Mist[0]->SetRelativeLocation(Pts.Last() + FVector(-25.f, 0.f, -10.f));
		Mist[0]->SetRelativeScale3D(FVector(0.75f, 0.55f, 0.4f) * F * Fade);
	}}
// ------------------------------------------------------------------------------------------------
// Tintenbombe
// ------------------------------------------------------------------------------------------------
UBuff_InkBomb::UBuff_InkBomb()
{
	DisplayName = NSLOCTEXT("ShadowCatBuffs", "InkBomb", "TINTENBOMBE");
	Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/T_Icon_InkBomb.T_Icon_InkBomb")));
	Duration = 0.6f;
}

void UBuff_InkBomb::OnBegin()
{
	Blast();
}

void UBuff_InkBomb::OnRefreshed()
{
	// gestapelt: jede weitere Bombe sprengt erneut
	Blast();
}

void UBuff_InkBomb::Blast()
{
	const int32 Circuit = CatCircuit(Cat);
	if (ATrackDirector* Dir = GetDirector())
	{
		if (AInkCanvas* Canvas = Dir->GetCanvas())
		{
			// Wertung (Tutorial: Luecken schliessen) um die Katze
			Dir->ReportPainted(Canvas->PaintArea(Circuit, Cat->GetA(), HalfLength));
		}
		Dir->PlayBomb();
		// nach vorn: alles im Weg wird weggesprengt (Reichweite per Shop erweiterbar)
		const float R = Range + Cat->BombRangeBonus;
		Dir->BombBlast(R);
		// glaenzende Tintenflaeche nach vorn
		Dir->InkSmearArea(Circuit, Cat->GetA() + R * 0.5f, R * 0.5f, true);
	}
}
