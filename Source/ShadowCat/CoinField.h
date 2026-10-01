#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoinField.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Schwarze, schwebende Muenzen (Endlos-Modus). Alle Muenzen sind zwei instanzierte Meshes (schwarze Scheibe +
 * heller Rand, 2 Draw Calls); feste Anzahl Plaetze, die wiederverwendet werden. Der Track-Director legt Reihen
 * und einzelne Muenzen auf freie Fahrbahnen, dieses Actor dreht sie, prueft das Einsammeln und spielt den Sammel-Effekt.
 */
UCLASS()
class SHADOWCAT_API ACoinField : public AActor
{
	GENERATED_BODY()

public:
	ACoinField();

	/** Schwebehoehe der Muenzmitte ueber der Fahrbahn (cm). */
	UPROPERTY(EditAnywhere, Category = "Muenzen")
	float FloatHeight = 75.f;

	UPROPERTY(EditAnywhere, Category = "Muenzen")
	float Diameter = 62.f;

	void ClearAll();
	/** Muenze ueber dem Bodenpunkt Ground auf Fahrbahn Lane (seitliche Lage Lat) bei Streckenposition A. */
	bool AddCoin(const FVector& Ground, int32 Lane, float Lat, float A);
	/** Muenzen im Bereich A0..A1 auf den Fahrbahnen in LaneMask entfernen (Tinte des Riesen, Salve). */
	void RemoveRange(float A0, float A1, int32 LaneMask = -1);
	/** Drehen/Schweben, Einsammeln pruefen (nur bei bCollect). Liefert die Anzahl neu eingesammelter Muenzen. */
	int32 StepCoins(float DeltaTime, float CatA, float CatLat, float FeetZ, bool bCollect);
	int32 NumLive() const;
	/** Fahrbahn der naechsten Muenze voraus (Test-Autopilot), INDEX_NONE = keine. */
	int32 CoinLaneAhead(float CatA, float MaxAhead) const;

private:
	void Build();
	void SetInstance(int32 I, const FVector& Loc, float Yaw, float Scale);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Face;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Rim;

	struct FCoin
	{
		FVector Base = FVector::ZeroVector;
		float A = 0.f;
		float Lat = 0.f;
		int32 Lane = 0;
		bool bLive = false;
		/** <0: schwebt, >=0: Sammel-Animation laeuft (s) */
		float Pop = -1.f;
	};
	TArray<FCoin> Coins;
	float Time = 0.f;
	bool bBuilt = false;
	static constexpr int32 Capacity = 72;
};
