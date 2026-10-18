// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "MyProject1Character.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A basic first person character
 */
UCLASS(abstract)
class AMyProject1Character : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: first person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Chest-mounted body-camera look (F8 / "Bodycam" console command toggles it) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	class UBodycamComponent* Bodycam;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	class UInputAction* MouseLookAction;
	
public:
	AMyProject1Character();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Walking stride phase in radians: half a cycle per step; a foot strikes at (k + 1/2) * PI (the bob's low point).
	 *  The bodycam bob reads this so the camera and the footsteps stay in step. */
	float GetStepPhase() const { return StepPhase; }

	/** Ground speed normalised to the normal walk (1 = NormalWalkSpeed), for the bodycam bob amplitude. */
	float GetWalkIntensity() const;

	// ---- walking feel: a pad technician in work boots (2026-10-17; values in Docs/dimensions.md "Walking")

	/** Normal walk, cm/s (the template default was 600) */
	UPROPERTY(EditAnywhere, Category = "Walking")
	float NormalWalkSpeed = 140.0f;

	/** Brisk walk / jog while Shift is held (outside the cockpit), cm/s */
	UPROPERTY(EditAnywhere, Category = "Walking")
	float BriskSpeed = 300.0f;

	/** On stairs and open grating, cm/s (Shift scales it by BriskSpeed / NormalWalkSpeed) */
	UPROPERTY(EditAnywhere, Category = "Walking")
	float CarefulSpeed = 110.0f;

	/** Speed-up rate, cm/s^2: reaches the walk in ~0.4 s, the jog in ~0.9 s */
	UPROPERTY(EditAnywhere, Category = "Walking")
	float WalkAcceleration = 380.0f;

	/** Slow-down rate when the keys are released, cm/s^2 (plus BrakingFriction x speed): 140 cm/s stops in ~0.3 s */
	UPROPERTY(EditAnywhere, Category = "Walking")
	float WalkBrakingDeceleration = 420.0f;

	/** How fast the speed limit follows a change of surface or Shift, cm/s per second (no jolt stepping onto grating) */
	UPROPERTY(EditAnywhere, Category = "Walking")
	float SpeedBlendRate = 260.0f;

	/** Step length at speed v: StepBase + StepPerSpeed * v (140 cm/s -> 71 cm, 110 -> 64 cm, 300 -> 106 cm) */
	UPROPERTY(EditAnywhere, Category = "Walking")
	float StepBase = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Walking")
	float StepPerSpeed = 0.22f;

	/** Footstep sounds per surface (a random one of each set plays on every foot strike) */
	UPROPERTY(EditAnywhere, Category = "Walking|Sound")
	TArray<TSoftObjectPtr<class USoundBase>> GratingSteps;

	UPROPERTY(EditAnywhere, Category = "Walking|Sound")
	TArray<TSoftObjectPtr<class USoundBase>> PlateSteps;

	UPROPERTY(EditAnywhere, Category = "Walking|Sound")
	TArray<TSoftObjectPtr<class USoundBase>> ConcreteSteps;

	UPROPERTY(EditAnywhere, Category = "Walking|Sound")
	float FootstepVolume = 0.7f;

	/** Current surface: 0 concrete / ground, 1 steel plate, 2 open grating. */
	UFUNCTION(BlueprintPure, Category = "Walking")
	int32 GetSurface() const { return Surface; }

	UFUNCTION(BlueprintPure, Category = "Walking")
	bool IsOnStairs() const { return bOnStairs; }

	UFUNCTION(BlueprintPure, Category = "Walking")
	float GetTargetSpeed() const { return TargetSpeed; }

	/** Foot strikes since BeginPlay (play-test check of the stride timing). */
	UFUNCTION(BlueprintPure, Category = "Walking")
	int32 GetStepCount() const { return StepCount; }

	/** Force the brisk walk (as if Shift were held); used by the play-tests. */
	UFUNCTION(BlueprintCallable, Category = "Walking")
	void SetBrisk(bool bOn) { bBriskHeld = bOn; }

protected:

	/** Called from Input Actions for movement input */
	void MoveInput(const FInputActionValue& Value);

	/** Called from Input Actions for looking input */
	void LookInput(const FInputActionValue& Value);

	/** Handles aim inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoAim(float Yaw, float Pitch);

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles jump start inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump end inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

protected:

	/** Set up input action bindings */
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	

public:

	/** Returns the first person mesh **/
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** While locked (CM ingress path / seated in the couch) move and jump inputs are ignored; look input still
	 *  moves the control rotation, which the ingress actor reads as the clamped seated look. */
	void SetCockpitLocked(bool bLocked) { bCockpitLocked = bLocked; }

	bool IsCockpitLocked() const { return bCockpitLocked; }

private:
	bool bCockpitLocked = false;

	void UpdateSurface(float DeltaSeconds);
	void PlayFootstep();

	bool bBriskHeld = false;
	bool bOnStairs = false;
	int32 Surface = 0;
	float TargetSpeed = 140.0f;
	float StepPhase = 0.0f;
	float ClimbRate = 0.0f;
	float StairHold = 0.0f;
	float LastZ = 0.0f;
	bool bHaveLastZ = false;
	int32 StepCount = 0;
	int32 LastStepIndex = -1;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class USoundBase>> LoadedConcrete;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class USoundBase>> LoadedPlate;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class USoundBase>> LoadedGrating;

public:
	/** Returns first person camera component **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

