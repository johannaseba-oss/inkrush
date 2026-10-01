#include "RunTypes.h"
#include "ShadowCat.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Package.h"

namespace
{
	// Statische Caches muessen vor der Garbage Collection geschuetzt werden
	template <typename T>
	T* Keep(T* Obj)
	{
		if (Obj && !Obj->IsRooted())
		{
			Obj->AddToRoot();
		}
		return Obj;
	}

	TMap<FString, UStaticMesh*>& ShapeCache() { static TMap<FString, UStaticMesh*> C; return C; }
	TMap<FString, UMaterialInterface*>& MatCache() { static TMap<FString, UMaterialInterface*> C; return C; }
	TMap<uint32, UMaterialInstanceDynamic*>& MidCache() { static TMap<uint32, UMaterialInstanceDynamic*> C; return C; }

	UMaterialInstanceDynamic* CachedMid(const TCHAR* Material, uint32 Kind, float Gray, float Value)
	{
		const uint32 Key = (Kind << 28) | (uint32(FMath::Clamp(Gray, 0.f, 1.f) * 255.f) << 16) | uint32(FMath::Clamp(Value, 0.f, 60.f) * 100.f);
		if (UMaterialInstanceDynamic** Found = MidCache().Find(Key))
		{
			return *Found;
		}
		UMaterialInstanceDynamic* Mid = Keep(RunAssets::NewMID(Material, GetTransientPackage()));
		if (Mid)
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(Gray, Gray, Gray, 1.f));
			Mid->SetScalarParameterValue(Kind == 0 ? TEXT("Emissive") : TEXT("Intensity"), Value);
		}
		MidCache().Add(Key, Mid);
		return Mid;
	}
}

UStaticMesh* RunAssets::Shape(const TCHAR* Name)
{
	if (UStaticMesh** Found = ShapeCache().Find(Name))
	{
		return *Found;
	}
	const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name);
	UStaticMesh* Mesh = Keep(LoadObject<UStaticMesh>(nullptr, *Path));
	if (!Mesh)
	{
		UE_LOG(LogShadowCat, Warning, TEXT("Grundform fehlt: %s"), *Path);
	}
	ShapeCache().Add(Name, Mesh);
	return Mesh;
}

UMaterialInterface* RunAssets::Material(const TCHAR* Name)
{
	if (UMaterialInterface** Found = MatCache().Find(Name))
	{
		return *Found;
	}
	const FString Path = FString::Printf(TEXT("/Game/Materials/%s.%s"), Name, Name);
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Mat)
	{
		UE_LOG(LogShadowCat, Warning, TEXT("Material fehlt (Tools/ue_setup.py ausfuehren): %s"), *Path);
		Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	Keep(Mat);
	MatCache().Add(Name, Mat);
	return Mat;
}

UMaterialInstanceDynamic* RunAssets::NewMID(const TCHAR* Material, UObject* Outer)
{
	UMaterialInterface* Base = RunAssets::Material(Material);
	return Base ? UMaterialInstanceDynamic::Create(Base, Outer ? Outer : GetTransientPackage()) : nullptr;
}

UMaterialInstanceDynamic* RunAssets::Mono(float Gray, float Emissive)
{
	return CachedMid(TEXT("M_Mono"), 0, Gray, Emissive);
}

UMaterialInstanceDynamic* RunAssets::Glow(float Gray, float Intensity)
{
	return CachedMid(TEXT("M_Glow"), 1, Gray, Intensity);
}

void RunAssets::MakeCheap(UStaticMeshComponent* C)
{
	if (!C)
	{
		return;
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(false);
	C->bReceivesDecals = false;
	C->SetCanEverAffectNavigation(false);
}

UStaticMeshComponent* RunAssets::AddShape(AActor* Owner, USceneComponent* Parent, const TCHAR* ShapeName, const FVector& Loc, const FVector& Scale, const FRotator& Rot, UMaterialInterface* Mat)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
	MakeCheap(C);
	C->SetStaticMesh(Shape(ShapeName));
	C->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
	C->SetRelativeLocation(Loc);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(Scale);
	if (Mat)
	{
		C->SetMaterial(0, Mat);
	}
	C->RegisterComponent();
	Owner->AddInstanceComponent(C);
	return C;
}
