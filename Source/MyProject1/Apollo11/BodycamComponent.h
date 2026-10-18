// Chest-mounted body-camera look for the first-person character (the "Bodycam" game style).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/HUD.h"
#include "BodycamComponent.generated.h"

class UCameraComponent;
class UFont;
class UMaterialInstanceDynamic;

/**
 * Moves the character's camera to the chest and gives it body-camera behaviour:
 *  - very wide lens with barrel distortion, chromatic aberration, rolling-shutter skew on turns, over-sharpening,
 *    compression blocking, sensor noise stronger in the shadows (post-process material PP_Bodycam)
 *  - lens dirt + flares on bright lights, slow auto exposure with blown highlights and crushed shadows, motion blur
 *  - camera inertia on turns, walking bob and sway, handheld jitter
 *  - timestamp + unit ID overlay (ABodycamHUD)
 * Toggle: F8 or the console command "Bodycam [0|1]". Switching off restores the original first-person camera.
 */
UCLASS(ClassGroup = (Apollo11), meta = (BlueprintSpawnableComponent))
class UBodycamComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBodycamComponent();

	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void SetBodycamEnabled(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Bodycam")
	void ToggleBodycam();

	UFUNCTION(BlueprintPure, Category = "Bodycam")
	bool IsBodycamEnabled() const { return bEnabled; }

	/** CM ingress/seated world pose: uses the clean 85 degree helmet profile until ClearViewOverride restores outdoors. */
	void SetViewOverride(const FVector& WorldLocation, const FQuat& WorldRotation);
	void ClearViewOverride();
	bool HasViewOverride() const { return bViewOverride; }
	bool IsHelmetView() const { return bViewOverride; }

	/** Current camera pose (world). */
	FVector GetViewLocation() const;
	FQuat GetViewRotation() const;

	UPROPERTY(EditAnywhere, Category = "Bodycam")
	bool bStartEnabled = true;

	/** Camera position relative to the capsule centre (capsule half height 96 cm -> lens ~1.36 m above the floor) */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Mount")
	FVector ChestOffset = FVector(14.0, 5.0, 40.0);

	/** The camera is clipped to the chest and looks slightly down */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Mount")
	float MountPitch = -4.0f;

	/** Vertical look limit (deg up/down); also applied to the player camera manager while the bodycam is on. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Mount")
	float MaxLookPitch = 85.0f;

	/** Horizontal field of view before distortion */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Lens")
	float FieldOfView = 118.0f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Lens")
	float Barrel = 0.24f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Lens")
	float ChromaticAberration = 0.004f;

	/** Auto-exposure bias (EV). 0: night views at the plain camera's brightness; highlights are held by local exposure. */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Sensor")
	float ExposureBias = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Sensor")
	float Grain = 0.085f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Sensor")
	float Sharpen = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Sensor")
	float Compression = 0.35f;

	/** Rolling-shutter skew in screen widths per degree/second of yaw */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Sensor")
	float RollingShutter = 0.00008f;

	/** How fast the camera catches up with the view direction (higher = stiffer) */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Motion")
	float RotationLagSpeed = 10.0f;

	/** Vertical bob at the normal walk (140 cm/s), cm; grows with speed (x2.1 at the 300 cm/s jog) */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Motion")
	float BobVertical = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Motion")
	float BobLateral = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Motion")
	float BobRollDeg = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Motion")
	float JitterDeg = 0.18f;

	/** Distance covered per step, cm, for pawns other than AMyProject1Character (which supplies its own stride) */
	UPROPERTY(EditAnywhere, Category = "Bodycam|Motion")
	float StepLength = 75.0f;

	UPROPERTY(EditAnywhere, Category = "Bodycam|Assets")
	FSoftObjectPath PostMaterial = FSoftObjectPath(TEXT("/Game/Apollo11/Materials/Night/PP_Bodycam.PP_Bodycam"));

	UPROPERTY(EditAnywhere, Category = "Bodycam|Assets")
	FSoftObjectPath LensDirt = FSoftObjectPath(TEXT("/Game/Apollo11/Textures/Night/T_LensDirt.T_LensDirt"));

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Apply(bool bEnable);
	void ApplyViewProfile();
	void UpdateCamera(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PostMID;

	bool bEnabled = false;
	bool bInputBound = false;
	bool bViewOverride = false;
	FVector OverrideLocation = FVector::ZeroVector;
	FQuat OverrideRotation = FQuat::Identity;
	bool bHudSet = false;

	// original first-person camera setup, restored when the bodycam is switched off
	TWeakObjectPtr<USceneComponent> OrigParent;
	FName OrigSocket;
	FTransform OrigRelative;
	bool bOrigPawnControlRotation = true;
	bool bOrigFirstPersonFOV = true;
	float OrigFOV = 90.0f;

	FQuat SmoothedRot = FQuat::Identity;
	bool bHaveSmoothed = false;
	float StepPhase = 0.0f;
	float Clock = 0.0f;
	float LastYaw = 0.0f;
	float YawRate = 0.0f;
	float LandDip = 0.0f;
	bool bWasFalling = false;
};

/** Body-camera overlay: date/time (running from 1969-07-16 05:40:00) and unit ID, top right. */
UCLASS()
class ABodycamHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere, Category = "Bodycam")
	FString UnitId = TEXT("KSC 39A  UNIT 07");

	/** Clock at the start of play: 05:40:00 EDT */
	UPROPERTY(EditAnywhere, Category = "Bodycam")
	int32 StartSeconds = 5 * 3600 + 40 * 60;

private:
	void DrawPrompt();
	/** Seated in the CM: aim dot in the screen centre, the aimed control's label + position and a hint line. */
	void DrawPanelAim();
	/** Seated in the CM: small seat strip at the bottom (Tab opens the three couches, 1/2/3 switch). */
	void DrawSeatMenu();

	/** Monospaced overlay font, held here so it stays referenced (a function-static raw pointer was garbage collected). */
	UPROPERTY(Transient)
	TObjectPtr<UFont> MonoFont;
};
