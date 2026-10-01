#pragma once

#include "CoreMinimal.h"
#include "RunTypes.generated.h"

class AActor;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Art einer Gefahr auf der Strecke. */
UENUM(BlueprintType)
enum class EHazardKind : uint8
{
	Obstacle,
	Enemy
};

/** Ablauf eines Laufs (Startbildschirm -> Lauf -> Sturz -> Game Over). */
UENUM(BlueprintType)
enum class ERunPhase : uint8
{
	Menu,
	Running,
	Dying,
	GameOver,
	/** Alle Fahrbahnen eingefaerbt. */
	Won
};

/** Spielmodus. */
UENUM(BlueprintType)
enum class ERunMode : uint8
{
	/** Endlose Gerade mit 5 Fahrbahnen, Riese am Horizont, Score. */
	Endless
};

/** Gemeinsame, gecachte Grundformen und Materialien. Fehlt ein Asset, wird auf Engine-Standards zurueckgefallen. */
namespace RunAssets
{
	/** Engine-Grundform: "Cube", "Sphere", "Cylinder", "Cone", "Plane" (alle 100 cm, zentriert). */
	UStaticMesh* Shape(const TCHAR* Name);
	/** Projektmaterial aus /Game/Materials, z. B. "M_Mono". */
	UMaterialInterface* Material(const TCHAR* Name);
	/** Geteilte, beleuchtete Graustufe (Gray 0..1, Emissive = zusaetzliches Eigenleuchten). */
	UMaterialInstanceDynamic* Mono(float Gray, float Emissive = 0.f);
	/** Geteilte, unbeleuchtete leuchtende Graustufe. */
	UMaterialInstanceDynamic* Glow(float Gray, float Intensity);
	/** Neue, eigene Instanz eines Projektmaterials (fuer animierte Parameter). */
	UMaterialInstanceDynamic* NewMID(const TCHAR* Material, UObject* Outer);
	/** Legt zur Laufzeit ein Mesh-Bauteil ohne Kollision und Schatten an. */
	UStaticMeshComponent* AddShape(AActor* Owner, USceneComponent* Parent, const TCHAR* ShapeName, const FVector& Loc, const FVector& Scale, const FRotator& Rot, UMaterialInterface* Mat);
	/** Schaltet Kollision/Schatten eines Bauteils ab (mobilgerecht). */
	void MakeCheap(UStaticMeshComponent* C);
}
