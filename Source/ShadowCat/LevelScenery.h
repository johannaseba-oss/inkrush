#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CircuitLayout.h"
#include "LevelScenery.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * Kulisse: Unterbau, Randsteine, Absperrungen und Verbindungsstege (Rundkurs), Deko aus gezeichneten Baeumen und
 * Haeusern (PNG-Bildtafeln, zur Kamera gedreht, instanziert) und Nebel aus weichen, zur Kamera gedrehten Wolken-Quads.
 * Gerade Endlos-Strecke: eine Periode Deko wird dreimal gebaut und springt mit der Katze mit.
 */
UCLASS()
class SHADOWCAT_API ALevelScenery : public AActor
{
	GENERATED_BODY()

public:
	ALevelScenery();

	/** Laenge einer Kulissen-Periode auf der Geraden (cm). */
	UPROPERTY(EditAnywhere, Category = "Kulisse")
	float Period = 10000.f;

	/** Density 0..1 = Anteil der Deko und des Nebels (Grafikstufe). */
	void Build(const FCircuitLayout& InLayout, float Density, int32 Seed);
	void StepScenery(float DeltaTime, const FVector& CatLoc);

private:
	UInstancedStaticMeshComponent* IsmFor(UStaticMesh* Mesh, UMaterialInterface* Mat);
	UInstancedStaticMeshComponent* Ism(const TCHAR* Shape, UMaterialInterface* Mat);
	void Add(const TCHAR* Shape, UMaterialInterface* Mat, const FVector& Loc, const FRotator& Rot, const FVector& Scale);
	/** Kette von Quadern entlang einer Kurslinie (Randsteine, Gelaender). */
	void AddStrip(const TCHAR* Shape, UMaterialInterface* Mat, float Lat, float Z, float Width, float Height, float StepLen, float FromA = 0.f, float ToA = -1.f);
	/** Nebelschwade; Color schwarz = dunkler Nebel (verdunkelt), hell = weisser Nebel. */
	void AddFog(const FVector& Loc, float SizeX, float SizeY, int32 Variant, float Opacity, const FLinearColor& Color = FLinearColor(0.82f, 0.82f, 0.84f));
	/** Bild-Deko (Baum/Haus aus PNG): senkrechte Tafel mit Fusspunkt P, dreht sich zur Kamera. */
	void AddSprite(FRandomStream& R, bool bHouse, const FVector& P, float Height);
	/** Tafel mit bestimmtem Bild; bFace = zur Kamera drehen, sonst fest mit Blickrichtung Yaw (z. B. Zaun entlang der Strasse). */
	void AddSpriteTex(UTexture2D* Tex, const FVector& P, float Height, float Flip, bool bFace, float Yaw = 0.f);
	/** Zaunstueck-Kette entlang der Strecke (seitliche Lage Lat) von A0 ueber Count Stuecke. */
	void AddFenceRun(float Lat, float A0, int32 Count);
	void LoadSpriteTextures();
	/** Platz frei? Placed = (X, Y, Radius); InPeriod > 0: periodisch in X vergleichen. */
	static bool IsFreeSpot(const TArray<FVector>& Placed, const FVector& P, float Radius, float InPeriod);
	void Clear();

	void BuildTrackEdges();
	void BuildGaps();
	void BuildDecor(FRandomStream& R, float Density);
	void BuildFog(FRandomStream& R, float Density);
	void BuildStraight(FRandomStream& R, float Density, bool bFog);

	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Isms;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Fog;

	TArray<FVector2D> FogSize;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FogMats;

	TArray<FVector> FogBase;
	TArray<float> FogPhase;

	/** Bildtafeln: /Game/Nature/T_Sprite_Tree*, T_Sprite_House* (Tools/Import/Nature/*.png, CAT_SETUP=nature). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> TreeTex;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> HouseTex;

	/** T_Sprite_StreetLamp, T_Sprite_Zaun */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LampTex;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FenceTex;

	/** Kleinkram am Strassenrand: Busch, Auto, Karton, Stoppschild, Kiste (je mit Hoehe in cm) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> PropTex;

	TArray<float> PropHeight;

	/** je Textur ein Material fuer die Tafeln */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UTexture2D>, TObjectPtr<UMaterialInstanceDynamic>> SpriteMats;

	struct FSprite
	{
		TObjectPtr<UInstancedStaticMeshComponent> Ism;
		int32 Instance = 0;
		FVector Base = FVector::ZeroVector;
		float Width = 100.f;
		float Height = 100.f;
		bool bFace = true;
	};
	TArray<FSprite> Sprites;
	FVector LastCam = FVector(1.0e9);
	FCircuitLayout Layout;
	float Time = 0.f;
};
