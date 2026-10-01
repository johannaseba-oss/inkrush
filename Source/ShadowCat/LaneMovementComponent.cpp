#include "LaneMovementComponent.h"

ULaneMovementComponent::ULaneMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULaneMovementComponent::ResetLanes(int32 StartLane)
{
	TargetLane = FMath::Clamp(StartLane, 0, Layout.NumLanesTotal() - 1);
	CurY = FromY = LaneToY(TargetLane);
	T = 1.f;
	LateralVel = 0.f;
}

int32 ULaneMovementComponent::RequestShift(int32 Dir, bool bAllowCircuitChange)
{
	const int32 NewLane = FMath::Clamp(TargetLane + FMath::Sign(Dir), 0, Layout.NumLanesTotal() - 1);
	if (NewLane == TargetLane)
	{
		return 0;
	}
	const bool bCross = Layout.CircuitOf(NewLane) != Layout.CircuitOf(TargetLane);
	if (bCross && !bAllowCircuitChange)
	{
		return 0;
	}
	// Auch mitten im Wechsel sofort umlenken
	TargetLane = NewLane;
	FromY = CurY;
	T = 0.f;
	const float Dist = FMath::Abs(LaneToY(TargetLane) - FromY);
	Duration = SwitchDuration * FMath::Sqrt(FMath::Max(0.3f, Dist / FMath::Max(1.f, Layout.LaneWidth)));
	return bCross ? 2 : 1;
}

void ULaneMovementComponent::StepLanes(float DeltaTime)
{
	const float PrevY = CurY;
	if (T < 1.f)
	{
		T = FMath::Min(1.f, T + DeltaTime / FMath::Max(0.01f, Duration));
		const float A = T * T * (3.f - 2.f * T);
		CurY = FMath::Lerp(FromY, LaneToY(TargetLane), A);
	}
	const float Vel = DeltaTime > 0.f ? (CurY - PrevY) / DeltaTime : 0.f;
	const float Norm = FMath::Clamp(Vel / (Layout.LaneWidth / FMath::Max(0.01f, SwitchDuration)), -1.f, 1.f);
	LateralVel = FMath::FInterpTo(LateralVel, Norm, DeltaTime, 14.f);
}
