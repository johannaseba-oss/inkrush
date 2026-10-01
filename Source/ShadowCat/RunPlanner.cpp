#include "RunPlanner.h"

void FRunPlanner::Reset(int32 InNumLanes, float InLaneSwitchTime, float FirstRowX, int32 StartLane, int32 Seed)
{
	NumLanes = FMath::Clamp(InNumLanes, 1, 7);
	LaneSwitchTime = InLaneSwitchTime;
	Rng.Initialize(Seed);
	SafeMask = uint8(1 << FMath::Clamp(StartLane, 0, NumLanes - 1));
	FirstX = FirstRowX;
	LastEndX = FirstRowX;
	bFirst = true;
	PendingItemLane = INDEX_NONE;
	bLastAction = false;
}

uint8 FRunPlanner::Dilate(uint8 Mask, int32 Steps) const
{
	uint8 Out = Mask;
	for (int32 S = 0; S < Steps; ++S)
	{
		Out = uint8((Out | (Out << 1) | (Out >> 1)) & AllMask());
	}
	return Out;
}

int32 FRunPlanner::RandomBit(uint8 Mask)
{
	TArray<int32, TInlineAllocator<8>> Lanes;
	for (int32 L = 0; L < NumLanes; ++L)
	{
		if (Mask & (1 << L))
		{
			Lanes.Add(L);
		}
	}
	return Lanes.Num() ? Lanes[Rng.RandHelper(Lanes.Num())] : INDEX_NONE;
}

FPlannedRow FRunPlanner::Next(const FRunDifficulty& D, float SpeedAtRow, float Meters, bool bAllowItem)
{
	FPlannedRow Row;
	const float Speed = FMath::Max(100.f, SpeedAtRow);

	// Zeitabstand zur vorigen Reihe: wird mit der Strecke langsam kuerzer, nie unter MinGapTime
	float GapTime = FMath::Lerp(D.MaxGapTime, D.MinGapTime, Ramp(Meters, D.GapRampMeters)) * Rng.FRandRange(0.9f, 1.35f);
	GapTime = FMath::Max(GapTime, D.MinGapTime);

	// Parkour-"Wand": alle Fahrbahnen mit Abgrund, Balken oder Zaun -> nur mit Sprung/Kriechen zu schaffen
	const float PWall = Meters >= D.WallStartMeters ? FMath::Lerp(D.WallChanceStart, D.WallChanceMax, Ramp(Meters - D.WallStartMeters, D.WallRampMeters)) : 0.f;
	const bool bWall = !bFirst && PendingItemLane == INDEX_NONE && Rng.FRand() < PWall;
	if (bLastAction || bWall)
	{
		// nach einer Aktion (Sprung dauert ~0.85 s) bzw. vor einer Wand: Zeit zum Landen, Sehen und Reagieren
		GapTime = FMath::Max(GapTime, D.ActionRecoverTime);
	}

	Row.StartX = bFirst ? FirstX : LastEndX + GapTime * Speed;

	// Erreichbare Spuren: so viele Wechsel, wie nach der Reaktionszeit in den Abstand passen
	uint8 Reach = SafeMask;
	if (!bFirst)
	{
		const int32 Moves = FMath::Clamp(FMath::FloorToInt((GapTime - D.ReactionTime) / FMath::Max(0.05f, LaneSwitchTime)), 0, NumLanes - 1);
		Reach = Dilate(SafeMask, Moves);
	}
	else
	{
		// Vor der ersten Reihe liegen mehrere Sekunden freie Strecke: alles erreichbar
		Reach = AllMask();
	}
	bFirst = false;

	if (bWall)
	{
		const float R = Rng.FRand();
		EObstacleType Type = R < 0.35f ? EObstacleType::Pit : (R < 0.6f ? EObstacleType::Beam : (R < 0.8f ? EObstacleType::Fence : EObstacleType::Crate));
		if (Type == EObstacleType::Pit && !bAllowPits)
		{
			Type = EObstacleType::Fence;
		}
		const float Len = Type == EObstacleType::Pit ? Rng.FRandRange(D.PitLength.X, D.PitLength.Y) : (Type == EObstacleType::Beam ? 40.f : (Type == EObstacleType::Crate ? 60.f : 30.f));
		for (int32 L = 0; L < NumLanes; ++L)
		{
			FPlannedHazard H;
			H.Lane = L;
			H.Type = Type;
			H.Length = Len;
			Row.Hazards.Add(H);
		}
		Row.BlockedMask = AllMask();
		// jede erreichbare Fahrbahn ist mit der Aktion passierbar
		Row.SafeMask = Reach;
		Row.EndX = Row.StartX + Len;
		SafeMask = Reach;
		LastEndX = Row.EndX;
		bLastAction = true;
		PendingItemLane = INDEX_NONE;
		return Row;
	}
	bLastAction = false;

	const float PDouble = FMath::Lerp(D.DoubleBlockChanceStart, D.DoubleBlockChanceMax, Ramp(Meters, D.DoubleRampMeters));
	// "Doppelt": bei 3 Fahrbahnen 2 gesperrt, bei 5 Fahrbahnen 2 oder 3 (nie alle bis auf eine)
	const int32 MaxBlocked = NumLanes <= 3 ? NumLanes - 1 : NumLanes - 2;
	const int32 WantBlocked = (NumLanes >= 3 && Rng.FRand() < PDouble) ? Rng.RandRange(2, FMath::Max(2, MaxBlocked)) : 1;

	// Kandidaten mit gewuenschter Anzahl gesperrter Spuren, zufaellig gemischt; erster gueltiger gewinnt
	uint8 Blocked = 0;
	for (int32 Count = WantBlocked; Count >= 1 && Blocked == 0; --Count)
	{
		TArray<uint8, TInlineAllocator<16>> Cands;
		for (uint8 M = 1; M < (1 << NumLanes); ++M)
		{
			if (FMath::CountBits(M) == Count)
			{
				Cands.Add(M);
			}
		}
		for (int32 I = Cands.Num() - 1; I > 0; --I)
		{
			Cands.Swap(I, Rng.RandHelper(I + 1));
		}
		for (uint8 M : Cands)
		{
			const uint8 Open = AllMask() & ~M;
			if ((Open & Reach) == 0)
			{
				continue;
			}
			if (PendingItemLane != INDEX_NONE && (M & (1 << PendingItemLane)))
			{
				continue;
			}
			Blocked = M;
			break;
		}
	}

	Row.BlockedMask = Blocked;
	Row.SafeMask = (AllMask() & ~Blocked) & Reach;

	// Gegner: zunaechst selten, nie direkt nach einem Item, hoechstens einer pro Reihe
	float PEnemy = 0.f;
	if (Meters >= D.EnemyStartMeters)
	{
		PEnemy = FMath::Lerp(D.EnemyChanceStart, D.EnemyChanceMax, Ramp(Meters - D.EnemyStartMeters, D.EnemyRampMeters));
	}
	const int32 EnemyLane = (PendingItemLane == INDEX_NONE && Rng.FRand() < PEnemy) ? RandomBit(Blocked) : INDEX_NONE;

	float MaxLen = 0.f;
	// alle Abgruende einer Reihe gleich lang: nebeneinander ergeben sie ein durchgehendes Loch ohne Zwischenwand
	const float RowPitLen = Rng.FRandRange(D.PitLength.X, D.PitLength.Y);
	for (int32 L = 0; L < NumLanes; ++L)
	{
		if (!(Blocked & (1 << L)))
		{
			continue;
		}
		FPlannedHazard H;
		H.Lane = L;
		if (L == EnemyLane)
		{
			H.Kind = EHazardKind::Enemy;
			H.Length = 80.f;
		}
		else
		{
			// einzelne Fahrbahn: Laterne (ausweichen) oder Parkour-Hindernis (ausweichen ODER springen/kriechen)
			const float R = Rng.FRand();
			if (bAllowPits && Meters > 40.f && R < 0.18f)
			{
				H.Type = EObstacleType::Pit;
				H.Length = RowPitLen;
			}
			else if (Meters > 80.f && R < 0.3f)
			{
				H.Type = EObstacleType::Beam;
				H.Length = 40.f;
			}
			else if (R < 0.5f)
			{
				H.Type = EObstacleType::Fence;
				H.Length = 30.f;
			}
			else if (R < 0.68f)
			{
				H.Type = EObstacleType::Crate;
				H.Length = 60.f;
			}
			else
			{
				H.Type = EObstacleType::Lamp;
				H.Length = 40.f;
			}
		}
		MaxLen = FMath::Max(MaxLen, H.Length);
		Row.Hazards.Add(H);
	}
	Row.EndX = Row.StartX + MaxLen;

	// Item hinter der Reihe, immer in einer sicheren Spur
	PendingItemLane = INDEX_NONE;
	if (bAllowItem && Meters >= D.ItemStartMeters && Row.SafeMask != 0 && Rng.FRand() < D.ItemChance)
	{
		Row.ItemLane = RandomBit(Row.SafeMask);
		Row.ItemX = Row.EndX + Speed * 0.35f;
		PendingItemLane = Row.ItemLane;
	}

	SafeMask = Row.SafeMask != 0 ? Row.SafeMask : Reach;
	LastEndX = Row.ItemLane != INDEX_NONE ? FMath::Max(Row.EndX, Row.ItemX - Speed * 0.2f) : Row.EndX;
	return Row;
}
