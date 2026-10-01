#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemBox.generated.h"

class UTextRenderComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Fragezeichen-Box auf einer Fahrbahn. Nach dem Einsammeln verschwindet sie und kommt nach einer Weile zurueck. */
UCLASS()
class SHADOWCAT_API AItemBox : public AActor
{
	GENERATED_BODY()

public:
	AItemBox();

	void Build();
	void Place(const FVector& Ground, const FVector& Forward, int32 InLane, float InA);
	void Collect(float RespawnTime);
	void StepBox(float DeltaTime, bool bSlotFull);

	bool IsAvailable() const { return bAvailable; }
	int32 Lane = 0;
	float A = 0.f;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<USceneComponent> Spin;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BoxMat;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ring;

	bool bAvailable = true;
	bool bBuilt = false;
	float Cooldown = 0.f;
	float Time = 0.f;
};
