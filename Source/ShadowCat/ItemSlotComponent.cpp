#include "ItemSlotComponent.h"
#include "CatBuff.h"
#include "BuffComponent.h"
#include "TrackDirector.h"
#include "ShadowCat.h"
#include "Engine/Texture2D.h"

UItemSlotComponent::UItemSlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UItemSlotComponent::SetPool(const TArray<FItemSpawnEntry>& InPool)
{
	PoolClasses.Reset();
	PoolWeights.Reset();
	for (const FItemSpawnEntry& E : InPool)
	{
		if (E.Buff && E.Weight > 0.f)
		{
			PoolClasses.Add(E.Buff);
			PoolWeights.Add(E.Weight);
		}
	}
	Counts.SetNumZeroed(PoolClasses.Num());
}

int32 UItemSlotComponent::PickRandom()
{
	if (PoolClasses.IsValidIndex(ForcedResult))
	{
		return ForcedResult;
	}
	float Total = 0.f;
	for (float W : PoolWeights) Total += W;
	float R = FMath::FRand() * Total;
	for (int32 I = 0; I < PoolWeights.Num(); ++I)
	{
		if (R < PoolWeights[I])
		{
			return I;
		}
		R -= PoolWeights[I];
	}
	return 0;
}

void UItemSlotComponent::Grant(int32 Index)
{
	if (Counts.IsValidIndex(Index))
	{
		Counts[Index] = FMath::Min(Counts[Index] + 1, MaxStack);
		LastResult = Index;
		UE_LOG(LogShadowCat, Log, TEXT("Item im Slot: %s (x%d)"), *GetName(Index).ToString(), Counts[Index]);
	}
}

bool UItemSlotComponent::TryCollect()
{
	if (PoolClasses.Num() == 0)
	{
		return false;
	}
	if (bRolling)
	{
		// zweite Box waehrend der Auslosung: sofort gutschreiben
		Grant(PickRandom());
		return true;
	}
	ResultIndex = PickRandom();
	bRolling = true;
	RollTime = 0.f;
	TickAcc = 0.f;
	return true;
}

void UItemSlotComponent::StepSlot(float DeltaTime)
{
	if (!bRolling)
	{
		return;
	}
	RollTime += DeltaTime;
	// Walzen-Effekt: schnell wechselnde Symbole, die immer langsamer werden
	const float Alpha = FMath::Clamp(RollTime / RollDuration, 0.f, 1.f);
	const float Interval = FMath::Lerp(0.05f, 0.3f, Alpha * Alpha);
	TickAcc += DeltaTime;
	if (TickAcc >= Interval && PoolClasses.Num() > 0)
	{
		TickAcc = 0.f;
		ShowIndex = (ShowIndex + 1) % PoolClasses.Num();
	}
	if (RollTime >= RollDuration)
	{
		bRolling = false;
		ShowIndex = ResultIndex;
		Grant(ResultIndex);
		ResultIndex = INDEX_NONE;
	}
}

UCatBuff* UItemSlotComponent::UseItem(int32 Index)
{
	if (!Counts.IsValidIndex(Index) || Counts[Index] <= 0)
	{
		return nullptr;
	}
	UBuffComponent* Buffs = GetOwner() ? GetOwner()->FindComponentByClass<UBuffComponent>() : nullptr;
	UCatBuff* B = Buffs ? Buffs->AddBuff(PoolClasses[Index]) : nullptr;
	--Counts[Index];
	UE_LOG(LogShadowCat, Log, TEXT("Item eingesetzt: %s (noch %d)"), B ? *B->DisplayName.ToString() : TEXT("-"), Counts[Index]);
	return B;
}

UCatBuff* UItemSlotComponent::UseAny()
{
	for (int32 I = 0; I < Counts.Num(); ++I)
	{
		if (Counts[I] > 0)
		{
			return UseItem(I);
		}
	}
	return nullptr;
}

void UItemSlotComponent::GrantClass(TSubclassOf<UCatBuff> BuffClass, int32 N)
{
	const int32 Index = PoolClasses.IndexOfByKey(BuffClass);
	if (Counts.IsValidIndex(Index))
	{
		Counts[Index] = FMath::Clamp(Counts[Index] + N, 0, MaxStack);
	}
}

bool UItemSlotComponent::HasItem() const
{
	for (int32 C : Counts)
	{
		if (C > 0)
		{
			return true;
		}
	}
	return false;
}

void UItemSlotComponent::Clear()
{
	for (int32& C : Counts)
	{
		C = 0;
	}
	ResultIndex = INDEX_NONE;
	LastResult = INDEX_NONE;
	bRolling = false;
}

UTexture2D* UItemSlotComponent::GetIcon(int32 Index) const
{
	if (!PoolClasses.IsValidIndex(Index) || !PoolClasses[Index])
	{
		return nullptr;
	}
	const TSoftObjectPtr<UTexture2D>& Icon = PoolClasses[Index]->GetDefaultObject<UCatBuff>()->Icon;
	return Icon.IsNull() ? nullptr : Icon.LoadSynchronous();
}

FText UItemSlotComponent::GetName(int32 Index) const
{
	if (!PoolClasses.IsValidIndex(Index) || !PoolClasses[Index])
	{
		return FText::GetEmpty();
	}
	return PoolClasses[Index]->GetDefaultObject<UCatBuff>()->DisplayName;
}

FText UItemSlotComponent::GetDisplayName() const
{
	if (bRolling)
	{
		return GetName(ShowIndex);
	}
	for (int32 I = 0; I < Counts.Num(); ++I)
	{
		if (Counts[I] > 0)
		{
			return GetName(I);
		}
	}
	return FText::FromString(TEXT("LEER"));
}
