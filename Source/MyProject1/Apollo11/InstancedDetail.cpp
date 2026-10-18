#include "Apollo11/InstancedDetail.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AInstancedDetail::AInstancedDetail()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);
}

int32 AInstancedDetail::AddSet(UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials,
                               const TArray<FTransform>& Transforms, const TArray<float>& CustomData, int32 NumCustomData,
                               bool bCastShadow, float CullDistance)
{
	if (!Mesh || Transforms.Num() == 0)
	{
		return 0;
	}
	Modify();
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(Mesh);
	for (int32 i = 0; i < Mesh->GetStaticMaterials().Num() && Materials.Num() > 0; ++i)
	{
		C->SetMaterial(i, Materials[FMath::Min(i, Materials.Num() - 1)]);
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	C->SetCanEverAffectNavigation(false);
	C->CastShadow = bCastShadow;
	if (CullDistance > 0.0f)
	{
		C->SetCullDistances(0, FMath::RoundToInt(CullDistance));
	}
	C->CreationMethod = EComponentCreationMethod::Instance;
	C->SetupAttachment(GetRootComponent());
	AddInstanceComponent(C);
	C->RegisterComponent();
	C->SetNumCustomDataFloats(FMath::Max(0, NumCustomData));
	C->AddInstances(Transforms, false, true);
	if (NumCustomData > 0 && CustomData.Num() >= Transforms.Num() * NumCustomData)
	{
		for (int32 i = 0; i < Transforms.Num(); ++i)
		{
			C->SetCustomData(i, TArrayView<const float>(CustomData.GetData() + i * NumCustomData, NumCustomData), false);
		}
		C->MarkRenderStateDirty();
	}
	return Transforms.Num();
}

void AInstancedDetail::ClearSets()
{
	Modify();
	TArray<UInstancedStaticMeshComponent*> Comps;
	GetComponents(Comps);
	for (UInstancedStaticMeshComponent* C : Comps)
	{
		RemoveInstanceComponent(C);
		C->DestroyComponent();
	}
}

int32 AInstancedDetail::GetInstanceTotal() const
{
	TArray<UInstancedStaticMeshComponent*> Comps;
	GetComponents(Comps);
	int32 N = 0;
	for (const UInstancedStaticMeshComponent* C : Comps)
	{
		N += C->GetInstanceCount();
	}
	return N;
}

int32 AInstancedDetail::GetSetCount() const
{
	TArray<UInstancedStaticMeshComponent*> Comps;
	GetComponents(Comps);
	return Comps.Num();
}
