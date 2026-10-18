// Command Module ingress/egress at the White Room hatch, the three crew couches and the seated suit arms.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CMIngress.generated.h"

class ACMPanel;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ECMIngressState : uint8
{
	Idle,
	Entering,
	Seated,
	Exiting
};

UENUM(BlueprintType)
enum class ECMHatchState : uint8
{
	Open,
	Closing,
	Closed,
	Opening
};

/** One pose of the ingress path: chest-camera position and view rotation in the actor's frame, at Time seconds. */
USTRUCT(BlueprintType)
struct FCMIngressKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ingress")
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ingress")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ingress")
	FRotator Rotation = FRotator::ZeroRotator;
};

/** A crew couch: eye point and the point the relaxed view rests on (actor frame), look limits, crew member. */
USTRUCT(BlueprintType)
struct FCMSeat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FString Code;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FString Role;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FString Astronaut;

	/** Eye point of a suited, helmeted crewman lying in this couch (actor frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FVector Eye = FVector::ZeroVector;

	/** Where the relaxed view rests (the part of the main display console in front of this couch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FVector LookAt = FVector::ZeroVector;

	/** Look limits from the relaxed view, deg: left / right / up / down */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FVector4 Limits = FVector4(70.f, 70.f, 35.f, 40.f);

	/** Looking further left (negative) / right (positive) than this hides the seated body (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	FVector2D BodyHideYaw = FVector2D(-55.f, 180.f);
};

/**
 * "Press E to enter" at the CM hatch. Pressing E plays a first-person ingress the way the Apollo crews got in on the
 * pad: grab the handhold over the hatch, swing in feet first, slide down on the back over the centre couch and
 * across into the left (Commander's) couch, ending looking up at the main display console. Once seated:
 *  - the closeout crew swings the hatch shut and latches it (HatchParts rotate about their hinge origin, with sound);
 *    E opens it again and then plays the path in reverse back to the White Room
 *  - 1 / 2 / 3 move to the Commander's, Command Module Pilot's or Lunar Module Pilot's couch (short transition);
 *    Tab shows the seat menu at the bottom of the screen
 *  - the suit arms rest on the thighs; when the view rests on an interactive control the nearer gloved hand reaches
 *    for it (two-bone IK, stiff and lagged like a pressurized A7L suit); holding Shift lowers both arms out of view
 * The camera follows through UBodycamComponent::SetViewOverride, so the bodycam look stays on.
 * Blockers (the hatch blocker volume) lose their collision only while the path plays.
 * Actor origin = White Room floor at the hatch plane, below the hatch centre; +X points into the cabin.
 */
UCLASS()
class ACMIngress : public AActor
{
	GENERATED_BODY()

public:
	ACMIngress();

	/** E: enter when standing at the hatch, exit when seated (the hatch opens first). */
	UFUNCTION(BlueprintCallable, Category = "Ingress")
	void Use();

	UFUNCTION(BlueprintPure, Category = "Ingress")
	ECMIngressState GetIngressState() const { return State; }

	/** Prompt for the HUD ("" when none applies). */
	UFUNCTION(BlueprintPure, Category = "Ingress")
	FString GetPrompt() const;

	UFUNCTION(BlueprintPure, Category = "Ingress")
	bool IsPawnAtHatch() const;

	/** Seated look input (deg); clamped to the seat limits. Returns false when not seated. */
	UFUNCTION(BlueprintCallable, Category = "Ingress")
	bool AddSeatLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintPure, Category = "Ingress")
	FVector2D GetSeatLook() const { return SeatLook; }

	// ---- seats
	/** Move to couch Index (0 CDR, 1 CMP, 2 LMP). False while not seated, switching, or already there. */
	UFUNCTION(BlueprintCallable, Category = "Ingress|Seat")
	bool SelectSeat(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ingress|Seat")
	int32 GetSeatIndex() const { return SeatIndex; }

	UFUNCTION(BlueprintPure, Category = "Ingress|Seat")
	bool IsSwitchingSeat() const { return bSwitching; }

	UFUNCTION(BlueprintCallable, Category = "Ingress|Seat")
	void ToggleSeatMenu() { bSeatMenu = !bSeatMenu; }

	UFUNCTION(BlueprintPure, Category = "Ingress|Seat")
	bool IsSeatMenuOpen() const { return bSeatMenu; }

	/** Relaxed view of the current couch (world): eye location and base rotation (the seat look is applied on top). */
	FVector GetSeatViewLocation() const;
	FQuat GetSeatBaseQuat() const;
	const FCMSeat& GetSeat(int32 Index) const { return Seats[FMath::Clamp(Index, 0, Seats.Num() - 1)]; }
	int32 GetSeatCount() const { return Seats.Num(); }

	// ---- hatch
	UFUNCTION(BlueprintPure, Category = "Ingress|Hatch")
	ECMHatchState GetHatchState() const { return HatchState; }

	/** 0 = open, 1 = closed */
	UFUNCTION(BlueprintPure, Category = "Ingress|Hatch")
	float GetHatchClosedAmount() const { return HatchT; }

	// ---- suit arms
	UFUNCTION(BlueprintCallable, Category = "Ingress|Arms")
	void SetArmsLowered(bool bLowered) { bArmsLowered = bLowered; }

	/** -1 none, 0 left, 1 right: the hand reaching for the aimed control */
	UFUNCTION(BlueprintPure, Category = "Ingress|Arms")
	int32 GetReachingHand() const { return ReachHand; }

	/** Current wrist position of a hand (world) */
	UFUNCTION(BlueprintPure, Category = "Ingress|Arms")
	FVector GetHandLocation(int32 Hand) const { return Wrist[FMath::Clamp(Hand, 0, 1)]; }

	/** Distance (cm) from the reaching hand's fingertip to the aimed control, -1 when no hand reaches */
	UFUNCTION(BlueprintPure, Category = "Ingress|Arms")
	float GetReachError() const { return ReachError; }

	/** Path from standing at the hatch (first key) to seated in the left couch (last key, replaced at BeginPlay by the
	 *  Commander's seat pose), actor frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ingress")
	TArray<FCMIngressKey> Keys;

	/** Actors whose collision is off while the path plays (the hatch blocker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ingress")
	TArray<TObjectPtr<AActor>> Blockers;

	/** The prompt shows when the pawn is inside this box (actor frame, cm) and looks toward the hatch. */
	UPROPERTY(EditAnywhere, Category = "Ingress")
	FBox PromptZone = FBox(FVector(-190.f, -80.f, 0.f), FVector(10.f, 80.f, 220.f));

	UPROPERTY(EditAnywhere, Category = "Ingress")
	float PromptMinFacing = 0.35f;

	/** Commander, Command Module Pilot, Lunar Module Pilot (left, centre, right couch) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ingress|Seat")
	TArray<FCMSeat> Seats;

	UPROPERTY(EditAnywhere, Category = "Ingress|Seat")
	float SeatSwitchTime = 1.7f;

	/** The player's A7L suit torso, legs and hoses (placed by build_cockpit_launch.py): visible only while seated; they move
	 *  with the player between couches (their placed location is the Commander's couch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ingress|Seat")
	TArray<TObjectPtr<AActor>> SeatedBody;

	// ---- hatch (the unified hatch + boost cover hatch panel; origin of each part = the hinge line, rotation = yaw only)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ingress|Hatch")
	TArray<TObjectPtr<AActor>> HatchParts;

	UPROPERTY(EditAnywhere, Category = "Ingress|Hatch")
	float HatchOpenYaw = 150.f;

	UPROPERTY(EditAnywhere, Category = "Ingress|Hatch")
	float HatchClosedYaw = 0.f;

	/** Hinge axis the hatch parts turn about (world space, through each part's origin). The unified hatch is hinged along
	 *  a generator of the 33 deg cone, so this is tilted, not vertical; the open/closed "yaw" values are angles about it. */
	UPROPERTY(EditAnywhere, Category = "Ingress|Hatch")
	FVector HatchHingeAxis = FVector::UpVector;

	/** Seconds after the player is seated before the closeout crew swings the hatch shut */
	UPROPERTY(EditAnywhere, Category = "Ingress|Hatch")
	float HatchCloseDelay = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Ingress|Hatch")
	float HatchSwingTime = 4.5f;

	UPROPERTY(EditAnywhere, Category = "Ingress|Hatch")
	TSoftObjectPtr<USoundBase> SndHatchSwing;

	UPROPERTY(EditAnywhere, Category = "Ingress|Hatch")
	TSoftObjectPtr<USoundBase> SndHatchLatch;

	UPROPERTY(EditAnywhere, Category = "Ingress|Hatch")
	TSoftObjectPtr<USoundBase> SndHatchRatchet;

	// ---- suit arms (actor frame offsets relative to the couch centre line; y is mirrored for the right arm)
	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	FVector ShoulderOffset = FVector(140.f, -22.f, 160.5f);

	/** Rest: wrist on the thigh. Lowered (Shift): wrist down beside the hip on the couch edge. */
	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	FVector RestWrist = FVector(192.f, -19.f, 171.f);

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	FVector LoweredWrist = FVector(176.f, -27.f, 157.f);

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	float UpperArmLength = 31.f;

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	float ForearmLength = 30.f;

	/** Wrist to fingertip of the pressurized glove, cm */
	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	float HandLength = 16.f;

	/** Spring rate of the hand (rad/s; lower = more sluggish) and its top speed (cm/s): the pressurized suit resists */
	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	float ArmStiffness = 5.5f;

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	float ArmMaxSpeed = 75.f;

	/** Seconds the view must rest on a control before the hand starts toward it */
	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	float ReachDelay = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	TSoftObjectPtr<UStaticMesh> ArmSegmentMesh;      // unit cylinder, base at the origin along +Z

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	TSoftObjectPtr<UStaticMesh> ArmJointMesh;        // unit sphere

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	TSoftObjectPtr<UStaticMesh> GloveMeshL;

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	TSoftObjectPtr<UStaticMesh> GloveMeshR;

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	TSoftObjectPtr<UMaterialInterface> SuitMaterial;

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	TSoftObjectPtr<UMaterialInterface> GloveMaterial;

	UPROPERTY(EditAnywhere, Category = "Ingress|Arms")
	TSoftObjectPtr<UMaterialInterface> RingMaterial;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void OnUsePressed();
	void OnSeat1() { SelectSeat(0); }
	void OnSeat2() { SelectSeat(1); }
	void OnSeat3() { SelectSeat(2); }
	void OnTab() { ToggleSeatMenu(); }
	void OnShiftDown() { bArmsLowered = true; }
	void OnShiftUp() { bArmsLowered = false; }
	void StartPath(bool bEnter);
	void FinishPath();
	void SetBlockersEnabled(bool bEnable);
	void SetSeatedBodyVisible(bool bVisible);
	void SamplePath(float T, FVector& OutLoc, FQuat& OutRot) const;
	void ApplyView(const FVector& WorldLoc, const FQuat& WorldRot);
	FQuat SeatBaseLocal(int32 Index) const;
	FQuat SeatRotation() const;
	class AMyProject1Character* GetPlayerCharacter() const;

	void UpdateHatch(float Dt);
	void SetHatchAmount(float T);
	void PlayHatchSound(const TSoftObjectPtr<USoundBase>& Sound, float Volume = 1.f) const;

	void BuildArms();
	void UpdateArms(float Dt);
	void PoseArm(int32 Side, const FVector& ShoulderW, const FVector& WristW, const FVector& GloveUp, const FVector& Pole);
	void MoveSeatedBody(float SeatY);

	ECMIngressState State = ECMIngressState::Idle;
	bool bBodyShown = false;
	float PathTime = 0.f;
	float PathDuration = 0.f;
	FVector2D SeatLook = FVector2D::ZeroVector;

	// seats
	int32 SeatIndex = 0;
	bool bSwitching = false;
	bool bSeatMenu = false;
	int32 SwitchFrom = 0;
	float SwitchTime = 0.f;
	FQuat SwitchStartRot = FQuat::Identity;
	float BodySeatY = 0.f;           // current couch centre line y (actor frame) the body / arms follow
	TArray<FVector> BodyBaseLocations;

	// hatch
	ECMHatchState HatchState = ECMHatchState::Open;
	float HatchT = 0.f;
	float HatchTimer = 0.f;
	bool bExitPending = false;
	bool bRatchetPlayed = false;

	// arms
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ArmParts;     // per side: upper, elbow, forearm, ring, glove
	UPROPERTY(Transient)
	TObjectPtr<ACMPanel> Panel;
	bool bArmsLowered = false;
	bool bArmsBuilt = false;
	FVector Wrist[2] = {FVector::ZeroVector, FVector::ZeroVector};
	FVector WristVel[2] = {FVector::ZeroVector, FVector::ZeroVector};
	FVector GloveUp[2] = {FVector::UpVector, FVector::UpVector};
	bool bWristInit = false;
	int32 ReachHand = -1;
	float ReachError = -1.f;
	float AimHold = 0.f;
	FName LastAimed = NAME_None;

	// the path actually played (world space): Keys, with the first pose replaced by the camera's pose at the start
	TArray<float> PathT;
	TArray<FVector> PathLoc;
	TArray<FQuat> PathRot;
	TArray<FQuat> PathTan;
};
