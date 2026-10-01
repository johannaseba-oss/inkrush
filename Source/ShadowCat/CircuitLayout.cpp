#include "CircuitLayout.h"

// Aufbau einer Runde (Parameter A, Kurs-0-Mittellinie), Lauf gegen den Uhrzeigersinn von oben gesehen so,
// dass "rechts" immer nach aussen zeigt:
//   [0, S)            untere Gerade, Richtung -X
//   [S, S+PiR)        linker Halbkreis
//   [S+PiR, 2S+PiR)   obere Gerade, Richtung +X
//   [2S+PiR, 2S+2PiR) rechter Halbkreis

int32 FCircuitLayout::NearestLane(float Lat) const
{
	int32 Best = 0;
	float BestD = TNumericLimits<float>::Max();
	for (int32 G = 0; G < NumLanesTotal(); ++G)
	{
		const float D = FMath::Abs(LaneLat(G) - Lat);
		if (D < BestD)
		{
			BestD = D;
			Best = G;
		}
	}
	return Best;
}

float FCircuitLayout::WrapA(float A) const
{
	if (bStraight)
	{
		return A;
	}
	const float L = LoopLength();
	A = FMath::Fmod(A, L);
	return A < 0.f ? A + L : A;
}

float FCircuitLayout::DeltaA(float A, float B) const
{
	if (bStraight)
	{
		return B - A;
	}
	const float L = LoopLength();
	float D = WrapA(B - A);
	if (D > L * 0.5f)
	{
		D -= L;
	}
	return D;
}

bool FCircuitLayout::IsOnCurve(float A) const
{
	if (bStraight)
	{
		return false;
	}
	A = WrapA(A);
	const float S = StraightLength;
	const float C = PI * BaseRadius;
	return (A >= S && A < S + C) || (A >= 2.f * S + C);
}

float FCircuitLayout::Stretch(float A, float Lat) const
{
	return IsOnCurve(A) ? FMath::Max(10.f, BaseRadius + Lat) / BaseRadius : 1.f;
}

void FCircuitLayout::Sample(float A, float Lat, FVector& OutPos, FVector& OutForward) const
{
	if (bStraight)
	{
		// Laufrichtung +X, rechts = +Y
		OutPos = FVector(A, Lat, 0.f);
		OutForward = FVector::ForwardVector;
		return;
	}
	A = WrapA(A);
	const float S = StraightLength;
	const float R = BaseRadius;
	const float C = PI * R;
	FVector2D P, F;
	if (A < S)
	{
		P = FVector2D(S * 0.5f - A, -R);
		F = FVector2D(-1.f, 0.f);
	}
	else if (A < S + C)
	{
		const float Phi = -HALF_PI - (A - S) / R;
		P = FVector2D(-S * 0.5f + R * FMath::Cos(Phi), R * FMath::Sin(Phi));
		F = FVector2D(FMath::Sin(Phi), -FMath::Cos(Phi));
	}
	else if (A < 2.f * S + C)
	{
		P = FVector2D(-S * 0.5f + (A - S - C), R);
		F = FVector2D(1.f, 0.f);
	}
	else
	{
		const float Phi = HALF_PI - (A - 2.f * S - C) / R;
		P = FVector2D(S * 0.5f + R * FMath::Cos(Phi), R * FMath::Sin(Phi));
		F = FVector2D(FMath::Sin(Phi), -FMath::Cos(Phi));
	}
	// rechts von F (UE, Z nach oben): (-F.Y, F.X)
	const FVector2D Right(-F.Y, F.X);
	const FVector2D W = P + Right * Lat;
	OutPos = FVector(W.X, W.Y, 0.f);
	OutForward = FVector(F.X, F.Y, 0.f);
}

bool FCircuitLayout::IsConnection(float A) const
{
	if (bStraight)
	{
		return false;
	}
	A = WrapA(A);
	const float S = StraightLength;
	const float C = PI * BaseRadius;
	float T = -1.f;
	if (A < S)
	{
		T = A / S;
	}
	else if (A >= S + C && A < 2.f * S + C)
	{
		T = (A - S - C) / S;
	}
	return T >= ConnectionFrom && T <= ConnectionTo;
}

float FCircuitLayout::LaneArc(int32 G, float A) const
{
	if (bStraight)
	{
		return A;
	}
	A = WrapA(A);
	const float S = StraightLength;
	const float C = PI * BaseRadius;
	const float K = FMath::Max(10.f, BaseRadius + LaneLat(G)) / BaseRadius;
	if (A < S)
	{
		return A;
	}
	if (A < S + C)
	{
		return S + (A - S) * K;
	}
	if (A < 2.f * S + C)
	{
		return S + C * K + (A - S - C);
	}
	return 2.f * S + C * K + (A - 2.f * S - C) * K;
}

float FCircuitLayout::AFromLaneArc(int32 G, float Arc) const
{
	if (bStraight)
	{
		return Arc;
	}
	const float S = StraightLength;
	const float C = PI * BaseRadius;
	const float K = FMath::Max(10.f, BaseRadius + LaneLat(G)) / BaseRadius;
	Arc = FMath::Fmod(Arc, LaneLength(G));
	if (Arc < 0.f)
	{
		Arc += LaneLength(G);
	}
	if (Arc < S)
	{
		return Arc;
	}
	if (Arc < S + C * K)
	{
		return S + (Arc - S) / K;
	}
	if (Arc < 2.f * S + C * K)
	{
		return S + C + (Arc - S - C * K);
	}
	return 2.f * S + C + (Arc - 2.f * S - C * K) / K;
}

int32 FCircuitLayout::SectionAt(int32 G, float A) const
{
	if (bStraight)
	{
		return FMath::Max(0, FMath::FloorToInt(A / SectionLength));
	}
	const int32 N = NumSections(G);
	return FMath::Clamp(FMath::FloorToInt(LaneArc(G, A) / LaneLength(G) * N), 0, N - 1);
}

float FCircuitLayout::SectionStartA(int32 G, int32 Index) const
{
	return AFromLaneArc(G, LaneLength(G) * Index / NumSections(G));
}

void FCircuitLayout::SectionTransform(int32 G, int32 Index, FVector& OutCenter, FRotator& OutRot, float& OutLength) const
{
	if (bStraight)
	{
		OutCenter = FVector((Index + 0.5f) * SectionLength, LaneLat(G), 0.f);
		OutRot = FRotator::ZeroRotator;
		OutLength = SectionLength;
		return;
	}
	const int32 N = NumSections(G);
	const float L = LaneLength(G);
	const float A0 = AFromLaneArc(G, L * Index / N);
	const float A1 = AFromLaneArc(G, L * (Index + 1) / N);
	const float Lat = LaneLat(G);
	const FVector P0 = Position(A0, Lat);
	const FVector P1 = Position(A1, Lat);
	OutCenter = (P0 + P1) * 0.5f;
	const FVector D = P1 - P0;
	OutRot = D.Rotation();
	OutLength = D.Size2D();
}
