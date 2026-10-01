#include "InkMarks.h"
#include "RunTypes.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FTransform HiddenXform(FRotator::ZeroRotator, FVector(0.f, 0.f, -5000.f), FVector(0.001f));
}

AInkMarks::AInkMarks()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Prints = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Prints"));
	Prints->SetupAttachment(RootComponent);
	Blobs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Blobs"));
	Blobs->SetupAttachment(RootComponent);
	Sheets = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Sheets"));
	Sheets->SetupAttachment(RootComponent);
	for (UInstancedStaticMeshComponent* C : { Prints.Get(), Blobs.Get(), Sheets.Get() })
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetMobility(EComponentMobility::Movable);
	}
	SetActorEnableCollision(false);
}

void AInkMarks::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	Prints->SetStaticMesh(RunAssets::Shape(TEXT("Plane")));
	if (UMaterialInstanceDynamic* M = RunAssets::NewMID(TEXT("M_Sprite"), this))
	{
		if (UTexture2D* Paw = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Fx/T_Item_CatPawPrint.T_Item_CatPawPrint"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			M->SetTextureParameterValue(TEXT("Tex"), Paw);
		}
		M->SetScalarParameterValue(TEXT("Brightness"), 1.f);
		Prints->SetMaterial(0, M);
	}
	// Kleckse: glaenzend schwarz, leicht gewoelbt (Cel-Konturen machen sie auch auf Weiss plastisch)
	Blobs->SetStaticMesh(RunAssets::Shape(TEXT("Sphere")));
	Blobs->SetMaterial(0, RunAssets::Mono(0.015f, 0.f));
	BlobData.SetNum(BlobCap);
	TArray<FTransform> X;
	X.Init(HiddenXform, BlobCap);
	Blobs->AddInstances(X, false);
	// Tintenflaeche der Bombe
	Sheets->SetStaticMesh(RunAssets::Shape(TEXT("Plane")));
	Sheets->SetNumCustomDataFloats(2);
	if (UMaterialInstanceDynamic* SM = RunAssets::NewMID(TEXT("M_InkSheet"), this))
	{
		Sheets->SetMaterial(0, SM);
	}
	SheetData.SetNum(SheetCap);
	TArray<FTransform> SX;
	SX.Init(HiddenXform, SheetCap);
	Sheets->AddInstances(SX, false);
	if (PrintCap == 0)
	{
		SetPrintCapacity(400);
	}
}

void AInkMarks::SetPrintCapacity(int32 Capacity)
{
	Build();
	Capacity = FMath::Max(16, Capacity);
	if (Capacity == PrintCap)
	{
		return;
	}
	Prints->ClearInstances();
	TArray<FTransform> X;
	X.Init(HiddenXform, Capacity);
	Prints->AddInstances(X, false);
	PrintCap = Capacity;
	NextPrint = 0;
}

void AInkMarks::ClearAll()
{
	Build();
	for (int32 I = 0; I < PrintCap; ++I)
	{
		Prints->UpdateInstanceTransform(I, HiddenXform, false, false, true);
	}
	for (int32 I = 0; I < BlobData.Num(); ++I)
	{
		BlobData[I].bLive = false;
		Blobs->UpdateInstanceTransform(I, HiddenXform, false, false, true);
	}
	for (int32 I = 0; I < SheetData.Num(); ++I)
	{
		SheetData[I].bLive = false;
		Sheets->UpdateInstanceTransform(I, HiddenXform, false, false, true);
	}
	NextPrint = 0;
	Prints->MarkRenderStateDirty();
	Blobs->MarkRenderStateDirty();
	Sheets->MarkRenderStateDirty();
}

void AInkMarks::AddPrint(const FVector& Pos, float Yaw, float Size, bool bMirror)
{
	Build();
	// Plane liegt flach (100 x 100), Zehen zeigen in Laufrichtung
	const FVector S(Size / 100.f * (bMirror ? -1.f : 1.f), Size / 100.f, 1.f);
	Prints->UpdateInstanceTransform(NextPrint, FTransform(FRotator(0.f, Yaw + 90.f, 0.f), Pos, S), false, true, true);
	NextPrint = (NextPrint + 1) % PrintCap;
}

void AInkMarks::AddBlob(const FVector& Pos, float Size, float Life, float Delay)
{
	Build();
	int32 Use = INDEX_NONE;
	float Oldest = -1.f;
	for (int32 I = 0; I < BlobData.Num(); ++I)
	{
		if (!BlobData[I].bLive)
		{
			Use = I;
			break;
		}
		if (BlobData[I].Age > Oldest)
		{
			Oldest = BlobData[I].Age;
			Use = I;
		}
	}
	FBlob& B = BlobData[Use];
	B.Pos = Pos;
	B.Size = Size;
	B.Stretch = FMath::FRandRange(1.f, 1.6f);
	B.Yaw = FMath::FRandRange(0.f, 360.f);
	B.Age = -Delay;
	B.Life = Life;
	B.bLive = true;
}

void AInkMarks::AddSheet(const FVector& P0, const FVector& P1, float Width, float Life, float Delay, bool bOpenLo, bool bOpenHi)
{
	Build();
	// Material: Instanzdaten 0/1 = linke/rechte Seite offen (lokales -Y/+Y)
	Sheets->SetCustomDataValue(NextSheet, 0, bOpenLo ? 1.f : 0.f, false);
	Sheets->SetCustomDataValue(NextSheet, 1, bOpenHi ? 1.f : 0.f, false);
	FSheet& S = SheetData[NextSheet];
	NextSheet = (NextSheet + 1) % SheetCap;
	const FVector D = P1 - P0;
	S.Mid = (P0 + P1) * 0.5f;
	S.Rot = D.Rotation();
	// etwas laenger als der Abstand: Stuecke ueberlappen, keine Fugen
	S.Len = D.Size() + 8.f;
	S.Width = Width;
	S.Age = -Delay;
	S.Life = Life;
	S.bLive = true;
}

void AInkMarks::StepMarks(float DeltaTime)
{
	if (!bBuilt)
	{
		return;
	}
	bool bSheets = false;
	for (int32 I = 0; I < SheetData.Num(); ++I)
	{
		FSheet& S = SheetData[I];
		if (!S.bLive)
		{
			continue;
		}
		bSheets = true;
		S.Age += DeltaTime;
		if (S.Age < 0.f)
		{
			continue;
		}
		if (S.Age >= S.Life)
		{
			S.bLive = false;
			Sheets->UpdateInstanceTransform(I, HiddenXform, false, false, true);
			continue;
		}
		// Tinte laeuft seitlich aus (mit leichtem Ueberschwingen) und zieht sich am Ende zusammen
		const float T = FMath::Clamp(S.Age / 0.3f, 0.f, 1.f);
		const float G = 1.f - FMath::Pow(1.f - T, 3.f) + 0.06f * FMath::Sin(T * PI);
		const float K = FMath::Clamp((S.Life - S.Age) / 0.6f, 0.f, 1.f);
		Sheets->UpdateInstanceTransform(I, FTransform(S.Rot, S.Mid, FVector(S.Len / 100.f, S.Width / 100.f * G * FMath::Sqrt(K), 1.f)), false, false, true);
	}
	if (bSheets)
	{
		Sheets->MarkRenderStateDirty();
	}
	bool bAny = false;
	for (int32 I = 0; I < BlobData.Num(); ++I)
	{
		FBlob& B = BlobData[I];
		if (!B.bLive)
		{
			continue;
		}
		bAny = true;
		B.Age += DeltaTime;
		if (B.Age < 0.f)
		{
			Blobs->UpdateInstanceTransform(I, HiddenXform, false, false, true);
			continue;
		}
		if (B.Age >= B.Life)
		{
			B.bLive = false;
			Blobs->UpdateInstanceTransform(I, HiddenXform, false, false, true);
			continue;
		}
		// aufquellen (mit Ueberschwingen), wabern, am Ende einsinken
		const float G = B.Age < 0.2f ? FMath::Sin(B.Age / 0.2f * PI * 0.6f) / FMath::Sin(PI * 0.6f) : 1.f;
		const float K = FMath::Clamp((B.Life - B.Age) / 0.7f, 0.f, 1.f);
		const float W = 1.f + 0.07f * FMath::Sin(B.Age * 8.f + I);
		const float D = B.Size / 100.f * G * FMath::Sqrt(K) * W;
		Blobs->UpdateInstanceTransform(I, FTransform(FRotator(0.f, B.Yaw, 0.f), B.Pos, FVector(D * B.Stretch, D, 0.2f * B.Size / 100.f * G * K)), false, false, true);
	}
	if (bAny)
	{
		Blobs->MarkRenderStateDirty();
	}
}
