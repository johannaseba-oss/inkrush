#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InkMarks.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Tintenspuren auf dem Boden, alles instanziert (2 Draw Calls):
 *  - Pfotenabdruecke der Katze (Bild Tools/Import/Items/Cat Paw print.png -> /Game/Fx/T_Item_CatPawPrint), Ringpuffer
 *  - animierte schwarze Tintenkleckse (Tintenwolke): quellen auf, wabern, schrumpfen wieder weg
 *  - Tintenflaeche der Bombe: flache Stuecke (M_InkSheet, glaenzend schwarz, gewellter Rand aus Weltkoordinaten),
 *    die nahtlos ineinander uebergehen und auch Rampen und Plateaus folgen
 */
UCLASS()
class SHADOWCAT_API AInkMarks : public AActor
{
	GENERATED_BODY()

public:
	AInkMarks();

	/** Anzahl gleichzeitiger Pfotenabdruecke (Rundkurs: viel, damit gefaerbte Fahrbahnen sichtbar bleiben). */
	void SetPrintCapacity(int32 Capacity);
	void ClearAll();
	/** Abdruck flach auf dem Boden: Position, Blickrichtung (Grad), Groesse (cm), gespiegelt. */
	void AddPrint(const FVector& Pos, float Yaw, float Size, bool bMirror);
	/** Klecks: erscheint nach Delay, lebt Life Sekunden. Size = Durchmesser (cm). */
	void AddBlob(const FVector& Pos, float Size, float Life, float Delay = 0.f);
	/** Stueck Tintenflaeche von P0 nach P1 (Mitte der Breite, darf geneigt sein), Breite in cm.
	 *  bOpenLo/bOpenHi: Seite geht nahtlos in die Nachbarflaeche ueber (kein ausgefranster Rand). */
	void AddSheet(const FVector& P0, const FVector& P1, float Width, float Life, float Delay, bool bOpenLo, bool bOpenHi);
	void StepMarks(float DeltaTime);

private:
	void Build();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Prints;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Blobs;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Sheets;

	int32 PrintCap = 0;
	int32 NextPrint = 0;

	struct FBlob
	{
		FVector Pos = FVector::ZeroVector;
		float Size = 100.f;
		float Stretch = 1.f;
		float Yaw = 0.f;
		float Age = 0.f;
		float Life = 0.f;
		bool bLive = false;
	};
	TArray<FBlob> BlobData;

	struct FSheet
	{
		FVector Mid = FVector::ZeroVector;
		FRotator Rot = FRotator::ZeroRotator;
		float Len = 100.f;
		float Width = 100.f;
		float Age = 0.f;
		float Life = 0.f;
		bool bLive = false;
	};
	TArray<FSheet> SheetData;
	int32 NextSheet = 0;
	static constexpr int32 SheetCap = 900;
	bool bBuilt = false;
	static constexpr int32 BlobCap = 460;
};
