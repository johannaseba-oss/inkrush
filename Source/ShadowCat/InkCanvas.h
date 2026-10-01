#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CircuitLayout.h"
#include "InkCanvas.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Einfaerbung der Fahrbahnen. Jede Fahrbahn ist in feste Abschnitte (~3 m) geteilt; ein Abschnitt ist weiss oder
 * eingefaerbt. Gerendert als EIN instanziertes Mesh, Custom Data 0 = sichtbare Bedeckung 0..1 in Laufrichtung.
 *
 * Rundkurs: alle Abschnitte existieren; optisch wird jeder Abschnitt in SubTiles Kacheln geteilt (glatte Kurven).
 * Gerade Endlos-Strecke: ein Fenster von WindowRows Abschnittsreihen wandert mit der Katze (Reihen hinter ihr werden
 * vorne wiederverwendet und sind dort wieder weiss).
 * Fehlen einer Rundkurs-Fahrbahn nur noch wenige Abschnitte, bekommen diese eine leuchtende Markierung.
 */
UCLASS()
class SHADOWCAT_API AInkCanvas : public AActor
{
	GENERATED_BODY()

public:
	AInkCanvas();

	/** Ab so wenigen fehlenden Abschnitten einer Fahrbahn werden die Luecken markiert (nur Rundkurs). */
	UPROPERTY(EditAnywhere, Category = "Tinte")
	int32 BeaconThreshold = 8;

	/** Optische Unterteilung je Abschnitt im Rundkurs. */
	UPROPERTY(EditAnywhere, Category = "Tinte")
	int32 SubTiles = 3;

	/** Gerade Strecke: Abschnittsreihen im Fenster und davon hinter der Katze. */
	UPROPERTY(EditAnywhere, Category = "Tinte")
	int32 WindowRows = 64;

	UPROPERTY(EditAnywhere, Category = "Tinte")
	int32 WindowBehind = 16;

	/** Gefaerbte Abschnitte schwarz zeigen (aus: Spur = Pfotenabdruecke, Items = Kleckse). */
	UPROPERTY(EditAnywhere, Category = "Tinte")
	bool bShowCoverage = false;

	void Build(const FCircuitLayout& InLayout);
	void ResetInk(float CatA = 0.f);
	/** Gerade Strecke: Abschnitt Index der Fahrbahn G ist ein Loch (Kachel ausgeblendet). */
	void SetHole(int32 G, int32 Index);
	void ClearHole(int32 G, int32 Index);
	/** Gerade Strecke: Fenster an die Position der Katze anpassen. */
	void UpdateWindow(float CatA);

	/** Faerbt einen Abschnitt; true, wenn er vorher weiss war. */
	bool PaintSection(int32 G, int32 Index);
	/** Faerbt alle Abschnitte einer Fahrbahn zwischen zwei A-Werten (vorwaerts, mit Umlauf). */
	int32 PaintSpan(int32 G, float FromA, float ToA);
	/** Faerbt auf allen Fahrbahnen eines Kurses den Bereich CenterA +- HalfLength (Weltstrecke). */
	int32 PaintArea(int32 Circuit, float CenterA, float HalfLength);

	bool IsPainted(int32 G, int32 Index) const;
	int32 CountPainted(int32 G) const;
	int32 CountSections(int32 G) const;
	float LaneProgress(int32 G) const;
	bool IsComplete() const { return !Layout.bStraight && TotalPainted >= TotalSections && TotalSections > 0; }
	int32 GetTotalPainted() const { return TotalPainted; }
	int32 GetTotalSections() const { return TotalSections; }
	/** Test: alles einfaerben (optional ohne eine Fahrbahn). */
	void DebugPaintAll(int32 ExceptLane = INDEX_NONE);

	/** Optik der Spur: Abschnitt ist bis Fraction (0..1 in Laufrichtung) bedeckt; folgt der Katze. */
	void SetTrailCoverage(int32 G, int32 Index, float Fraction);
	/** Abschnitt, den die Katze verlassen hat, zu Ende einfaerben. */
	void FinishCoverage(int32 G, int32 Index);

	void StepCanvas(float DeltaTime);
	const FCircuitLayout& GetLayout() const { return Layout; }

private:
	/** Speicherplatz eines Abschnitts (Painted/Visual), -1 = nicht vorhanden (ausserhalb des Fensters). */
	int32 SectionKey(int32 G, int32 Index) const;
	void WriteVisual(int32 Key, bool bDirty);
	void PlaceWindowSlot(int32 Slot);
	void UpdateBeacons();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Tiles;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Beacons;

	FCircuitLayout Layout;
	int32 Sub = 1;
	TArray<int32> LaneStart;
	TArray<uint8> Painted;
	TArray<float> Visual;
	TMap<int32, float> Filling;
	TArray<int32> LaneCount;
	/** Gerade Strecke: absoluter Abschnitt je Fenster-Reihe. */
	TArray<int32> SlotAbs;
	/** Loecher (Fahrbahn, Abschnitt) auf der Geraden */
	TSet<FIntPoint> Holes;
	int32 TotalSections = 0;
	int32 TotalPainted = 0;
	float Time = 0.f;
};
