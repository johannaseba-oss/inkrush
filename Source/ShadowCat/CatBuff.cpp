#include "CatBuff.h"
#include "RunnerCat.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

void UCatBuff::Begin(ARunnerCat* InCat)
{
	Cat = InCat;
	Remaining = Duration;
	Age = 0.f;
	if (Cat)
	{
		VisualRoot = NewObject<USceneComponent>(Cat);
		VisualRoot->SetupAttachment(Cat->GetRootComponent());
		VisualRoot->RegisterComponent();
	}
	OnBegin();
}

void UCatBuff::End()
{
	OnEnd();
	for (USceneComponent* C : Visuals)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	Visuals.Reset();
	if (VisualRoot)
	{
		VisualRoot->DestroyComponent();
		VisualRoot = nullptr;
	}
	Remaining = 0.f;
}

bool UCatBuff::Step(float DeltaTime)
{
	Age += DeltaTime;
	Remaining -= DeltaTime;
	OnTick(DeltaTime);
	return Remaining > 0.f;
}

float UCatBuff::GetFade() const
{
	return FMath::Clamp(FMath::Min(Age / 0.25f, Remaining / 0.5f), 0.f, 1.f);
}

float UCatBuff::GetEndingBlink() const
{
	if (Remaining > 1.5f)
	{
		return 1.f;
	}
	return FMath::Sin(Age * 28.f) > -0.2f ? 1.f : 0.25f;
}

UStaticMeshComponent* UCatBuff::AddVisual(const TCHAR* Shape, const FVector& Loc, const FVector& Scale, const FRotator& Rot, UMaterialInterface* Mat, USceneComponent* Parent)
{
	if (!Cat)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = RunAssets::AddShape(Cat, Parent ? Parent : VisualRoot.Get(), Shape, Loc, Scale, Rot, Mat);
	C->SetTranslucentSortPriority(2);
	Visuals.Add(C);
	return C;
}

USceneComponent* UCatBuff::AddPivot(const FVector& Loc, USceneComponent* Parent)
{
	if (!Cat)
	{
		return nullptr;
	}
	USceneComponent* P = NewObject<USceneComponent>(Cat);
	P->SetupAttachment(Parent ? Parent : VisualRoot.Get());
	P->SetRelativeLocation(Loc);
	P->RegisterComponent();
	Visuals.Add(P);
	return P;
}

ATrackDirector* UCatBuff::GetDirector() const
{
	return Cat ? Cat->GetDirector() : nullptr;
}

void UCatBuff::BuildPickupVisual(AActor* Pickup, USceneComponent* Root) const
{
	RunAssets::AddShape(Pickup, Root, TEXT("Sphere"), FVector::ZeroVector, FVector(0.5f), FRotator::ZeroRotator, RunAssets::Glow(1.f, 2.f));
}

void UCatBuff::AnimatePickup(float Time, USceneComponent* Root) const
{
	if (Root)
	{
		Root->SetRelativeLocation(FVector(0.f, 0.f, 90.f + 12.f * FMath::Sin(Time * 3.f)));
		Root->SetRelativeRotation(FRotator(0.f, Time * 90.f, 0.f));
	}
}
