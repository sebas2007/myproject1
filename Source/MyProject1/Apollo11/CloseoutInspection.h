#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CloseoutInspection.generated.h"

/** Local visual preparation rehearsal, not a suit-flow, leak or launch certification. */
UCLASS()
class ACloseoutInspection : public AActor
{
    GENERATED_BODY()
public:
    ACloseoutInspection();
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintPure, Category="Closeout") FString GetUsePrompt() const;
    UFUNCTION(BlueprintPure, Category="Closeout") int32 GetCompletedCount() const;
    UFUNCTION(BlueprintPure, Category="Closeout") bool IsCheckComplete(int32 Index) const;
    UFUNCTION(BlueprintPure, Category="Closeout") float GetInspectionProgress() const { return HoldTime / 2.f; }
    /** World locations on existing equipment; the generator does not move that equipment. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Closeout") TArray<FVector> InspectionPoints;
private:
    int32 AimedCheck() const;
    bool HatchAccessible() const;
    uint8 CompleteMask=0;
    int32 HoldingIndex=INDEX_NONE;
    float HoldTime=0.f;
};
