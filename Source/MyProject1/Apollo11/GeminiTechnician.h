#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeminiTechnician.generated.h"
class UStaticMeshComponent;
UCLASS()
class AGeminiTechnician : public AActor
{
    GENERATED_BODY()
public:
    AGeminiTechnician();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UFUNCTION(BlueprintPure) FString GetPrompt() const;
    UFUNCTION(BlueprintPure) bool IsClimbing() const { return bClimbing; }
    UFUNCTION(BlueprintPure) bool IsAccessOccupied() const;
    UFUNCTION(BlueprintPure) int32 GetWorkStage() const { return Stage; }
    UFUNCTION(BlueprintPure) bool IsCabinetOpen() const { return bCabinetOpen; }
    UFUNCTION(BlueprintPure) int32 GetAimedTask() const;
private:
    void Use();
    bool AccessAvailable() const;
    bool AtLadder() const;
    UPROPERTY() TObjectPtr<USceneComponent> CabinetHinge;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MeterNeedle;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Gates;
    bool bClimbing=false, bCabinetOpen=false;
    bool bTransfer=false,bLeaving=false;
    bool bReturning=false;
    float ReturnZ=98;
    FVector TransferFrom,TransferTo;
    float TransferTime=0;
    int32 Stage=0;
    float Hold=0, DoorAngle=0;
};
