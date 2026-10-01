#include "BuffComponent.h"
#include "CatBuff.h"
#include "RunnerCat.h"
#include "ShadowCat.h"

UBuffComponent::UBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UCatBuff* UBuffComponent::AddBuff(TSubclassOf<UCatBuff> BuffClass)
{
	if (!BuffClass)
	{
		return nullptr;
	}
	for (UCatBuff* B : Active)
	{
		if (B && B->GetClass() == BuffClass)
		{
			B->Refresh();
			return B;
		}
	}
	UCatBuff* Buff = NewObject<UCatBuff>(this, BuffClass);
	Active.Add(Buff);
	Buff->Begin(Cast<ARunnerCat>(GetOwner()));
	UE_LOG(LogShadowCat, Log, TEXT("Buff aktiv: %s (%.1f s)"), *Buff->DisplayName.ToString(), Buff->Duration);
	return Buff;
}

bool UBuffComponent::TryAbsorb(EHazardKind Kind) const
{
	for (const UCatBuff* B : Active)
	{
		if (B && B->AbsorbsHit(Kind))
		{
			return true;
		}
	}
	return false;
}

void UBuffComponent::StepBuffs(float DeltaTime)
{
	for (int32 I = Active.Num() - 1; I >= 0; --I)
	{
		UCatBuff* B = Active[I];
		if (!B || !B->Step(DeltaTime))
		{
			if (B)
			{
				UE_LOG(LogShadowCat, Log, TEXT("Buff abgelaufen: %s"), *B->DisplayName.ToString());
				B->End();
			}
			Active.RemoveAt(I);
		}
	}
}

void UBuffComponent::ClearAll()
{
	for (UCatBuff* B : Active)
	{
		if (B)
		{
			B->End();
		}
	}
	Active.Reset();
}
