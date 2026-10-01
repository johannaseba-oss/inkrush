#include "ItemBox.h"
#include "RunTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AItemBox::AItemBox()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	SetActorEnableCollision(false);
}

void AItemBox::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	Spin = NewObject<USceneComponent>(this);
	Spin->SetupAttachment(Root);
	Spin->RegisterComponent();

	BoxMat = RunAssets::NewMID(TEXT("M_Mono"), this);
	if (BoxMat)
	{
		BoxMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.9f, 0.9f));
		BoxMat->SetScalarParameterValue(TEXT("Emissive"), 0.7f);
	}
	RunAssets::AddShape(this, Spin, TEXT("Cube"), FVector::ZeroVector, FVector(0.7f), FRotator::ZeroRotator, BoxMat);
	// schwarzes Fragezeichen auf allen vier Seiten
	for (int32 I = 0; I < 4; ++I)
	{
		const FRotator Face(0.f, I * 90.f, 0.f);
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(Spin);
		T->SetText(FText::FromString(TEXT("?")));
		T->SetHorizontalAlignment(EHTA_Center);
		T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(62.f);
		T->SetTextRenderColor(FColor::Black);
		T->SetRelativeLocation(Face.RotateVector(FVector(36.f, 0.f, 0.f)));
		T->SetRelativeRotation(Face);
		T->SetCastShadow(false);
		T->RegisterComponent();
	}
	UMaterialInstanceDynamic* RingMat = RunAssets::NewMID(TEXT("M_Disc"), this);
	if (RingMat)
	{
		RingMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 1.f, 1.f));
		RingMat->SetScalarParameterValue(TEXT("Ring"), 1.f);
		RingMat->SetScalarParameterValue(TEXT("Opacity"), 0.7f);
		RingMat->SetScalarParameterValue(TEXT("Intensity"), 1.3f);
	}
	Ring = RunAssets::AddShape(this, Root, TEXT("Plane"), FVector(0.f, 0.f, 2.f), FVector(1.2f), FRotator::ZeroRotator, RingMat);
}

void AItemBox::Place(const FVector& Ground, const FVector& Forward, int32 InLane, float InA)
{
	Build();
	Lane = InLane;
	A = InA;
	SetActorLocationAndRotation(Ground, Forward.Rotation());
	bAvailable = true;
	Cooldown = 0.f;
	SetActorHiddenInGame(false);
}

void AItemBox::Collect(float RespawnTime)
{
	bAvailable = false;
	Cooldown = RespawnTime;
	SetActorHiddenInGame(true);
}

void AItemBox::StepBox(float DeltaTime, bool bSlotFull)
{
	Time += DeltaTime;
	if (!bAvailable)
	{
		Cooldown -= DeltaTime;
		if (Cooldown <= 0.f)
		{
			bAvailable = true;
			SetActorHiddenInGame(false);
		}
		return;
	}
	if (Spin)
	{
		Spin->SetRelativeLocation(FVector(0.f, 0.f, 95.f + 10.f * FMath::Sin(Time * 3.f)));
		Spin->SetRelativeRotation(FRotator(0.f, Time * 70.f, 0.f));
		// Slot voll: Box wirkt "gesperrt" (kleiner, dunkler)
		Spin->SetRelativeScale3D(FVector(bSlotFull ? 0.75f : 1.f));
	}
	if (BoxMat)
	{
		BoxMat->SetScalarParameterValue(TEXT("Emissive"), bSlotFull ? 0.1f : 0.6f + 0.3f * FMath::Sin(Time * 5.f));
	}
}
