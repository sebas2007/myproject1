#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeminiCabinPreview.generated.h"
class ACameraActor;
/** Observer camera for the uncrewed GT-1 instrumentation cabin; no flight controls. */
UCLASS()
class AGeminiCabinPreview : public AActor
{
    GENERATED_BODY()
public:
    AGeminiCabinPreview();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintPure) bool IsViewing() const { return bViewing; }
    UFUNCTION(BlueprintPure) FString GetPrompt() const;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector Entrance=FVector(-300,0,3480);
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector ViewLocation=FVector(-65,0,3535);
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FRotator ViewRotation=FRotator(-12,0,0);
private:
    void Toggle();
    bool CanEnter() const;
    bool bViewing=false;
    FRotator EntryControl;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> Observer;
    UPROPERTY(Transient) TObjectPtr<AActor> PreviousTarget;
};
