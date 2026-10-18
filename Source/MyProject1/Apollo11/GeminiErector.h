#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeminiErector.generated.h"
class UStaticMeshComponent;
UCLASS()
class AGeminiAccessDoor : public AActor
{
    GENERATED_BODY()
public:
    AGeminiAccessDoor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintPure) bool IsClosed() const { return !bOpen && Angle<.1f; }
    UFUNCTION(BlueprintPure) FString GetPrompt() const;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) bool bErectorDoor=true;
private:
    void Toggle();
    UPROPERTY() TObjectPtr<USceneComponent> Hinge;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Leaf;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Handle;
    bool bOpen=false;
    float Angle=0;
};
UCLASS()
class AGeminiErector : public AActor
{
    GENERATED_BODY()
public:
    AGeminiErector();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintPure) float GetAngle() const { return Angle; }
    UFUNCTION(BlueprintPure) float GetPlatformAngle() const { return FoldAngle; }
    UFUNCTION(BlueprintPure) bool IsMoving() const { return bMoving; }
    UFUNCTION(BlueprintPure) FString GetPrompt() const;
    UFUNCTION(BlueprintPure) int32 GetMovingActorCount() const { return Parts.Num(); }
    UFUNCTION(BlueprintPure) int32 GetAimedControl() const;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector Pivot=FVector(-1000,0,160);
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector ConsoleLocation=FVector(-1810,-1080,98);
    // Prototype traversal duration, not a sourced historical hydraulic speed.
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float TravelSeconds=60.f;
private:
    void Toggle();
    void Press();
    void Stop();
    bool AtConsole() const;
    bool ClearToMove() const;
    void ApplyAngle();
    struct FPart { TWeakObjectPtr<AActor> Actor; FTransform Raised; int32 FoldSide=0; float FoldZ=0; };
    TArray<FPart> Parts;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Rams;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Rods;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PlatformRams;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PlatformRods;
    float Angle=0,StartAngle=0,TargetAngle=0,Elapsed=0;
    float Progress=0,TargetProgress=0,FoldAngle=0;
    bool bMoving=false;
    bool bStopped=false;
};
