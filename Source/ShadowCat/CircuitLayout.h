#pragma once

#include "CoreMinimal.h"
#include "CircuitLayout.generated.h"

/**
 * Geometrie des Levels: mehrere ineinanderliegende Rundkurse (Stadion-Ovale mit gemeinsamer Mitte),
 * jeder mit mehreren Fahrbahnen. Alle Positionen werden ueber EINEN Parameter A beschrieben
 * (Bogenlaenge entlang der Mittellinie von Kurs 0) plus eine seitliche Verschiebung Lat (positiv = nach aussen = rechts
 * in Laufrichtung). Parallelkurven eines Stadions sind wieder Stadien, dadurch ist das exakt.
 *
 * Globale Fahrbahn G = Kurs * LanesPerCircuit + Fahrbahn (0 = links/innen).
 */
USTRUCT(BlueprintType)
struct SHADOWCAT_API FCircuitLayout
{
	GENERATED_BODY()

	/** true = endlose gerade Strecke (Parameter A = Weltstrecke entlang +X), keine Kurven/Runden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	bool bStraight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	int32 NumCircuits = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	int32 LanesPerCircuit = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	float LaneWidth = 140.f;

	/** Abstand der Mittellinien benachbarter Kurse (Fahrbahnbreite + Luecke). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	float CircuitSpacing = 700.f;

	/** Laenge jeder der beiden Geraden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	float StraightLength = 3000.f;

	/** Kurvenradius der Mittellinie von Kurs 0 (innerster Kurs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	float BaseRadius = 1200.f;

	/** Ziel-Laenge eines Einfaerbe-Abschnitts in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tinte")
	float SectionLength = 300.f;

	/** Verbindungsstellen: Anteil jeder Geraden (von..bis), auf dem man den Kurs wechseln kann. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	float ConnectionFrom = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rundkurse")
	float ConnectionTo = 0.8f;

	int32 NumLanesTotal() const { return NumCircuits * LanesPerCircuit; }
	int32 CircuitOf(int32 G) const { return G / LanesPerCircuit; }
	int32 LaneOf(int32 G) const { return G % LanesPerCircuit; }
	float LaneLat(int32 G) const { return CircuitOf(G) * CircuitSpacing + (LaneOf(G) - (LanesPerCircuit - 1) * 0.5f) * LaneWidth; }
	/** Naechste globale Fahrbahn zu einer seitlichen Position. */
	int32 NearestLane(float Lat) const;

	/** Umfang (Parameter A) einer Runde. */
	float LoopLength() const { return bStraight ? 1.0e9f : 2.f * StraightLength + 2.f * PI * BaseRadius; }
	float WrapA(float A) const;
	/** Kuerzeste Differenz B - A im Kreis (-Laenge/2 .. Laenge/2). */
	float DeltaA(float A, float B) const;
	bool IsOnCurve(float A) const;
	/** Weltstrecke pro Einheit A bei seitlicher Position Lat (1 auf Geraden). */
	float Stretch(float A, float Lat) const;
	/** Weltposition (Z = 0) und Vorwaertsrichtung. */
	void Sample(float A, float Lat, FVector& OutPos, FVector& OutForward) const;
	FVector Position(float A, float Lat) const { FVector P, F; Sample(A, Lat, P, F); return P; }
	/** Liegt A auf einer Verbindungsstelle (Mitte einer Geraden)? */
	bool IsConnection(float A) const;

	/** Bogenlaenge einer Fahrbahn bis A. */
	float LaneArc(int32 G, float A) const;
	float LaneLength(int32 G) const { return bStraight ? 1.0e9f : 2.f * StraightLength + 2.f * PI * FMath::Max(10.f, BaseRadius + LaneLat(G)); }
	int32 NumSections(int32 G) const { return bStraight ? 0x3FFFFFFF : FMath::Max(4, FMath::RoundToInt(LaneLength(G) / SectionLength)); }
	int32 SectionAt(int32 G, float A) const;
	/** Parameter A des Anfangs eines Abschnitts. */
	float SectionStartA(int32 G, int32 Index) const;
	/** Transform der Abschnitts-Kachel (Mitte, Ausrichtung) und ihre Weltlaenge. */
	void SectionTransform(int32 G, int32 Index, FVector& OutCenter, FRotator& OutRot, float& OutLength) const;
	/** Inverse von LaneArc. */
	float AFromLaneArc(int32 G, float Arc) const;
};
