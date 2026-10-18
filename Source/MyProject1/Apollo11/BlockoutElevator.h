// Blockout elevator for the Launch Umbilical Tower (Apollo 11 pad, Milestone 1).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlockoutElevator.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EBlockoutElevatorState : uint8
{
	Closed,
	Opening,
	Open,
	Closing,
	Moving
};

/**
 *  Two-stop elevator built from cube meshes: a cab with bi-parting doors, landing doors and a call
 *  button at each stop. Press E (Interact) next to a call button to call the cab, or inside the cab
 *  to ride to the other stop. The shaft walls themselves are static blockout geometry in the level.
 *
 *  Actor origin = shaft center at the lower stop floor. The doors face +X.
 *  Speed: LUT elevators ran at 600 ft/min (Moonport, NASA SP-4204; see Docs/dimensions.md).
 */
UCLASS()
class ABlockoutElevator : public AActor
{
	GENERATED_BODY()

public:
	ABlockoutElevator();

	/** Height of the upper stop above the lower stop (cm). Level 320 = 320 ft = 9753.6 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Elevator")
	float UpperStopHeight = 9753.6f;

	/** Cruise speed (cm/s). 600 ft/min = 304.8 cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Elevator")
	float MaxSpeed = 304.8f;

	/** Acceleration used to ease in and out of each stop (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Elevator")
	float Acceleration = 100.f;

	/** Seconds for the doors to fully open or close. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Elevator")
	float DoorTime = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Elevator|Look")
	UMaterialInterface* CabMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Elevator|Look")
	UMaterialInterface* DoorMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Elevator|Look")
	UMaterialInterface* ButtonMaterial = nullptr;

	/** Call the cab to a stop (0 = lower, 1 = upper). Opens the doors if it is already there. */
	UFUNCTION(BlueprintCallable, Category="Elevator")
	void CallToStop(int32 Stop);

	/** Cab panel: ride to the other stop. */
	UFUNCTION(BlueprintCallable, Category="Elevator")
	void PressCabButton();
	/** Floor selection in the cab; original indices 0=deck and 1=320 retained. */
	UFUNCTION(BlueprintCallable, Category="Elevator") void SelectStop(int32 Stop);
	UFUNCTION(BlueprintPure, Category="Elevator") FString GetStopLabel(int32 Stop) const;
	UFUNCTION(BlueprintPure, Category="Elevator") int32 GetStopCount() const { return StopLevels.Num(); }
	UFUNCTION(BlueprintPure, Category="Elevator") int32 GetSelectedStop() const { return SelectedStop; }
	UFUNCTION(BlueprintPure, Category="Elevator") FString GetUsePrompt() const;
	UFUNCTION(BlueprintPure, Category="Elevator") bool IsPlayerInCab() const;
	UFUNCTION(BlueprintPure, Category="Elevator") int32 GetAimedStop() const;
	UFUNCTION(BlueprintPure, Category="Elevator") FVector GetButtonCenterWorld(int32 Stop) const;
	UFUNCTION(BlueprintPure, Category="Elevator") FString GetFloorCounterText() const;
	/** Tower feet relative to deck. A=-12.5 is an inferred base-floor height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Elevator") TArray<float> StopLevels;

	/** What pressing E does for this actor: cab button if inside the cab, else the call button it stands at. */
	UFUNCTION(BlueprintCallable, Category="Elevator")
	void InteractFrom(AActor* Interactor);

	UFUNCTION(BlueprintPure, Category="Elevator")
	int32 GetCurrentStop() const { return CurrentStop; }

	UFUNCTION(BlueprintPure, Category="Elevator")
	EBlockoutElevatorState GetElevatorState() const { return State; }

	/** Cab floor height above the lower stop (cm). */
	UFUNCTION(BlueprintPure, Category="Elevator")
	float GetCabHeight() const { return CabZ; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void OnInteractPressed();
	float StopZ(int32 Stop) const { return StopLevels.IsValidIndex(Stop) ? StopLevels[Stop]*30.48f : 0.f; }
	void CycleSelection(int32 Direction);
	void UpdateIndicators();
	void OnFloorButtonPressed();
	void ApplyDoors();
	void ApplyCab();

	UStaticMeshComponent* MakeBox(FName Name, USceneComponent* Parent);
	static void PlaceBox(UStaticMeshComponent* Box, const FVector& Min, const FVector& Max);

	UPROPERTY(VisibleAnywhere, Category="Elevator")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category="Elevator")
	USceneComponent* Cab;

	UPROPERTY(VisibleAnywhere, Category="Elevator")
	UBoxComponent* CabTrigger;

	UPROPERTY(VisibleAnywhere, Category="Elevator")
	TArray<UStaticMeshComponent*> CabShell;

	UPROPERTY(VisibleAnywhere, Category="Elevator")
	UStaticMeshComponent* CabDoorL;

	UPROPERTY(VisibleAnywhere, Category="Elevator")
	UStaticMeshComponent* CabDoorR;

	UPROPERTY(VisibleAnywhere, Category="Elevator")
	UStaticMeshComponent* CabPanel;

	// Index 0 = lower stop, 1 = upper stop
	UPROPERTY(VisibleAnywhere, Transient, Category="Elevator")
	TArray<UStaticMeshComponent*> LandingDoorL;

	UPROPERTY(VisibleAnywhere, Transient, Category="Elevator")
	TArray<UStaticMeshComponent*> LandingDoorR;

	UPROPERTY(VisibleAnywhere, Transient, Category="Elevator")
	TArray<UStaticMeshComponent*> CallButton;

	UPROPERTY(VisibleAnywhere, Transient, Category="Elevator")
	TArray<UBoxComponent*> CallTrigger;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> FloorButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextRenderComponent>> FloorButtonLabels;
	UPROPERTY() TObjectPtr<UTextRenderComponent> CabIndicator;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> LandingIndicators;
	int32 SelectedStop = 1;
	FString LastIndicatorText;

	EBlockoutElevatorState State = EBlockoutElevatorState::Closed;
	int32 CurrentStop = 0;
	int32 TargetStop = 0;
	float CabZ = 0.f;
	float Velocity = 0.f;
	float DoorAlpha = 0.f;  // 0 = closed, 1 = open
};
