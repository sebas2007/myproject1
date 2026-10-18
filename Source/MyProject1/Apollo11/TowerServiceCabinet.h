#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TowerServiceCabinet.generated.h"

class UStaticMeshComponent;
class USceneComponent;

/** Inspectable tower cabinet. Door operation only; no invented propellant-system controls. */
UCLASS()
class ATowerServiceCabinet : public AActor
{
    GENERATED_BODY()
public:
    ATowerServiceCabinet();
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, Category="Tower") void ToggleDoors();
    UFUNCTION(BlueprintPure, Category="Tower") bool AreDoorsOpen() const { return bOpen; }
    UFUNCTION(BlueprintPure, Category="Tower") float GetDoorAmount() const { return DoorAmount; }
    FString GetUsePrompt() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Housing;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Internals;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftDoor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightDoor;
    UPROPERTY(EditAnywhere, Category="Tower") FString EquipmentName = TEXT("HYDRO-PNEUMATIC CABINET");
private:
    bool CanUse() const;
    UPROPERTY() TObjectPtr<USceneComponent> LeftHinge;
    UPROPERTY() TObjectPtr<USceneComponent> RightHinge;
    bool bOpen = false;
    float DoorAmount = 0.f;
};
