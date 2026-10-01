#include "CoinField.h"
#include "RunTypes.h"
#include "Components/InstancedStaticMeshComponent.h"

ACoinField::ACoinField()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Face = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Face"));
	Face->SetupAttachment(RootComponent);
	Rim = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rim"));
	Rim->SetupAttachment(RootComponent);
	for (UInstancedStaticMeshComponent* C : { Face.Get(), Rim.Get() })
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetMobility(EComponentMobility::Movable);
	}
	SetActorEnableCollision(false);
}

void ACoinField::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	UStaticMesh* Cyl = RunAssets::Shape(TEXT("Cylinder"));
	Face->SetStaticMesh(Cyl);
	Rim->SetStaticMesh(Cyl);
	// schwarze Scheibe (etwas dicker, damit beide Seiten schwarz sind) + heller Rand: auf weissen und
	// schwarz gefaerbten Fahrbahnen gleich gut zu erkennen
	Face->SetMaterial(0, RunAssets::Mono(0.02f, 0.f));
	Rim->SetMaterial(0, RunAssets::Mono(0.9f, 0.75f));
	Coins.SetNum(Capacity);
	for (int32 I = 0; I < Capacity; ++I)
	{
		const FTransform Hidden(FRotator::ZeroRotator, FVector(0.f, 0.f, -5000.f), FVector(0.001f));
		Face->AddInstance(Hidden, false);
		Rim->AddInstance(Hidden, false);
	}
}

void ACoinField::SetInstance(int32 I, const FVector& Loc, float Yaw, float Scale)
{
	// Zylinder (Achse Z) hochkant stellen, dann um die Hochachse drehen
	const FRotator R(90.f, Yaw, 0.f);
	const float D = Diameter / 100.f * Scale;
	Face->UpdateInstanceTransform(I, FTransform(R, Loc, FVector(D * 0.86f, D * 0.86f, 0.11f * FMath::Max(Scale, 0.01f))), false, false, true);
	Rim->UpdateInstanceTransform(I, FTransform(R, Loc, FVector(D, D, 0.07f * FMath::Max(Scale, 0.01f))), false, false, true);
}

void ACoinField::ClearAll()
{
	Build();
	for (int32 I = 0; I < Coins.Num(); ++I)
	{
		if (Coins[I].bLive)
		{
			Coins[I].bLive = false;
			SetInstance(I, FVector(0.f, 0.f, -5000.f), 0.f, 0.001f);
		}
	}
	Face->MarkRenderStateDirty();
	Rim->MarkRenderStateDirty();
}

bool ACoinField::AddCoin(const FVector& Ground, int32 Lane, float Lat, float A)
{
	Build();
	for (int32 I = 0; I < Coins.Num(); ++I)
	{
		if (!Coins[I].bLive)
		{
			FCoin& C = Coins[I];
			C.Base = Ground + FVector(0.f, 0.f, FloatHeight);
			C.A = A;
			C.Lat = Lat;
			C.Lane = Lane;
			C.bLive = true;
			C.Pop = -1.f;
			return true;
		}
	}
	return false;
}

void ACoinField::RemoveRange(float A0, float A1, int32 LaneMask)
{
	for (int32 I = 0; I < Coins.Num(); ++I)
	{
		FCoin& C = Coins[I];
		if (C.bLive && C.Pop < 0.f && C.A >= A0 && C.A <= A1 && (LaneMask & (1 << C.Lane)))
		{
			C.bLive = false;
			SetInstance(I, FVector(0.f, 0.f, -5000.f), 0.f, 0.001f);
		}
	}
}

void ACoinField::RemoveAbove(float Z)
{
	for (int32 I = 0; I < Coins.Num(); ++I)
	{
		FCoin& C = Coins[I];
		if (C.bLive && C.Pop < 0.f && C.Base.Z > Z)
		{
			C.bLive = false;
			SetInstance(I, FVector(0.f, 0.f, -5000.f), 0.f, 0.001f);
		}
	}
}

int32 ACoinField::NumLive() const
{
	int32 N = 0;
	for (const FCoin& C : Coins) { N += C.bLive ? 1 : 0; }
	return N;
}

int32 ACoinField::CoinLaneAhead(float CatA, float MaxAhead) const
{
	float Best = MaxAhead;
	int32 Lane = INDEX_NONE;
	for (const FCoin& C : Coins)
	{
		const float Ahead = C.A - CatA;
		if (C.bLive && C.Pop < 0.f && Ahead > 60.f && Ahead < Best)
		{
			Best = Ahead;
			Lane = C.Lane;
		}
	}
	return Lane;
}

int32 ACoinField::StepCoins(float DeltaTime, float CatA, float CatLat, float FeetZ, bool bCollect, float Reach)
{
	Build();
	Time += DeltaTime;
	int32 Got = 0;
	bool bAny = false;
	for (int32 I = 0; I < Coins.Num(); ++I)
	{
		FCoin& C = Coins[I];
		if (!C.bLive)
		{
			continue;
		}
		bAny = true;
		const float Ahead = C.A - CatA;
		if (Ahead < -700.f)
		{
			C.bLive = false;
			SetInstance(I, FVector(0.f, 0.f, -5000.f), 0.f, 0.001f);
			continue;
		}
		if (C.Pop < 0.f)
		{
			// Katze beruehrt die Muenze (Sprung ueber die Reihe = verpasst); Hoehe: Koerpermitte der Katze nahe der Muenze
			// (Boden-Muenzen beim Laufen, Luft-Muenzen beim Fliegen mit der Spraydose)
			if (bCollect && FMath::Abs(Ahead) < 70.f && FMath::Abs(C.Lat - CatLat) < 60.f + Reach && FMath::Abs(FeetZ + 40.f - C.Base.Z) < 110.f)
			{
				C.Pop = 0.f;
				++Got;
			}
			const float Bob = 7.f * FMath::Sin(Time * 3.f + C.A * 0.004f);
			SetInstance(I, C.Base + FVector(0.f, 0.f, Bob), Time * 160.f + C.A * 0.2f, 1.f);
			continue;
		}
		// Sammel-Effekt: schnell drehen, hochsteigen, kurz groesser, dann weg
		C.Pop += DeltaTime;
		const float T = FMath::Clamp(C.Pop / 0.28f, 0.f, 1.f);
		const float S = T < 0.3f ? FMath::Lerp(1.f, 1.35f, T / 0.3f) : FMath::Lerp(1.35f, 0.f, (T - 0.3f) / 0.7f);
		SetInstance(I, C.Base + FVector(0.f, 0.f, 90.f * T), Time * 900.f, FMath::Max(S, 0.001f));
		if (T >= 1.f)
		{
			C.bLive = false;
			SetInstance(I, FVector(0.f, 0.f, -5000.f), 0.f, 0.001f);
		}
	}
	if (bAny)
	{
		Face->MarkRenderStateDirty();
		Rim->MarkRenderStateDirty();
	}
	return Got;
}
