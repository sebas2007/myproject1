// Instanced set dressing (tower piping parts, boxes, lights, placards, ...): one instanced static mesh component per
// mesh, built from editor Python (Tools/Realism/build_tower_detail.py) and saved with the level as instance components.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InstancedDetail.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Holds instanced detail meshes. AddSet adds one instanced component (world-space transforms, optional per-instance
 * custom data read by the material with PerInstanceCustomData). No collision: the walkable blockout keeps it.
 */
UCLASS()
class AInstancedDetail : public AActor
{
	GENERATED_BODY()

public:
	AInstancedDetail();

	/** Adds one instanced component. Materials apply to slots in order; the last one fills the remaining slots.
	 *  CustomData holds NumCustomData floats per instance (may be empty). Returns the number of instances added. */
	UFUNCTION(BlueprintCallable, Category = "Detail")
	int32 AddSet(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials, const TArray<FTransform>& Transforms,
	             const TArray<float>& CustomData, int32 NumCustomData = 0, bool bCastShadow = true, float CullDistance = 0.0f);

	/** Removes every instanced component added with AddSet */
	UFUNCTION(BlueprintCallable, Category = "Detail")
	void ClearSets();

	UFUNCTION(BlueprintCallable, Category = "Detail")
	int32 GetInstanceTotal() const;

	UFUNCTION(BlueprintCallable, Category = "Detail")
	int32 GetSetCount() const;
};
