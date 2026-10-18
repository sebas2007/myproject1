// Interactive Command Module panel controls (toggle, rotary, pushbutton, circuit breaker, talkback, light, gauge).

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CMControls.generated.h"

class ACMPanel;
class FJsonObject;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECMGuard : uint8
{
	None,
	Gray,
	Stripe,
	Red
};

/**
 * Base of every panel control. Created by ACMPanel from Content/Apollo11/Data/CM_Controls.json at the control's scanned
 * position on its panel. Local frame: +X = panel normal (toward the crew), +Z = panel up (as drawn in the AOH),
 * +Y = the viewer's left. All sizes are real-world cm.
 */
UCLASS(Abstract, ClassGroup = (Apollo11))
class UCMControl : public USceneComponent
{
	GENERATED_BODY()

public:
	/** Reads the JSON entry, places the component on the panel and builds its meshes. */
	virtual void Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J);

	/** Left click. AimUp = where the aim ray hit, along panel-up relative to the control centre (cm); used to push a
	 *  three-position toggle up or down. Returns true if the control changed. */
	virtual bool Primary(float AimUp) { return false; }
	virtual void PrimaryRelease() {}
	/** Right click (switch guards; rotaries step back). */
	virtual bool Secondary() { return false; }
	/** Mouse wheel, +1 = up / clockwise. */
	virtual bool Wheel(int32 Dir) { return false; }
	/** Smooth movement toward the current state; returns true while still moving. */
	virtual bool Animate(float Dt) { return false; }
	/** Can the player operate it (lights, talkbacks and gauges only show their label). */
	virtual bool IsOperable() const { return true; }
	/** Sets the state without sound/feedback (launch configuration, restore). */
	virtual void SetStateDirect(int32 NewState);

	/** Everything the player can change (state, guard, press count), packed; Restore puts it back silently. Used by the
	 *  automated pick check and by the mission reset. */
	virtual uint32 Snapshot() const { return uint32(State); }
	virtual void Restore(uint32 Packed) { SetStateDirect(int32(Packed & 0xFFFF)); }

	/** "MAN ATT ROLL  -  RATE CMD" */
	virtual FString GetStatusText() const;
	virtual FString GetHintText() const { return FString(); }
	FString GetPositionName() const;

	void SetHighlighted(bool bOn);

	/** Ray (world) against the control's visible parts (PickParts: their mesh bounds in their current, animated pose, grown by
	 *  Margin cm); OutT = nearest hit distance, OutAimUp = hit offset along panel-up from the control centre (cm). */
	bool RayHit(const FVector& Origin, const FVector& Dir, float& OutT, float& OutAimUp, float Margin = 0.f, float MinHalf = ExactMinHalf) const;

	/** World centre of the largest visible part: where a player aims to operate this control. */
	FVector GetVisibleCenter() const;

	/** World centres of every visible part, largest first (the automated pick check tries each). */
	TArray<FVector> GetVisiblePartCenters() const;

	/** Exact pass: the drawn geometry (flat faces get this half thickness, cm). Near-miss pass (only when the ray meets no
	 *  control exactly): the parts grown by PickMargin, at least PickMinHalf thick. */
	static constexpr float ExactMinHalf = 0.04f;
	static constexpr float PickMargin = 0.3f;
	static constexpr float PickMinHalf = 0.3f;

	UPROPERTY(VisibleAnywhere, Category = "Control")
	FName ControlId;

	UPROPERTY(VisibleAnywhere, Category = "Control")
	FString Label;

	UPROPERTY(VisibleAnywhere, Category = "Control")
	FString Short;

	UPROPERTY(VisibleAnywhere, Category = "Control")
	FString PanelName;

	UPROPERTY(VisibleAnywhere, Category = "Control")
	TArray<FString> Positions;

	UPROPERTY(VisibleAnywhere, Category = "Control")
	int32 State = 0;

	UPROPERTY(VisibleAnywhere, Category = "Control")
	int32 LaunchState = 0;

	/** Approximate size of the control (half size along right, up; height above the face), cm: used for the reach pose and
	 *  the hand target only. Picking uses PickParts. */
	UPROPERTY(VisibleAnywhere, Category = "Control")
	FVector PickExtent = FVector(0.9f, 1.4f, 2.2f);

	/** The visible parts that make up the clickable area (lever + nut, guard, knob, cap + bezel, lens, flag, dial). Label plates
	 *  and the switch-rail cell around a control are decoration and not part of it. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PickParts;

protected:
	/** Adds a mesh part. LocalNRU = (out of panel, right, up) in cm relative to this component. bPick: part of the clickable area. */
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& LocalNRU, const FRotator& Rot,
	                              const FVector& Scale, USceneComponent* Parent = nullptr, bool bHighlight = true, bool bShadow = true,
	                              bool bPick = true);
	UStaticMeshComponent* AddBox(const FVector& CenterNRU, const FVector& SizeNRU, const FLinearColor& Color, float Roughness,
	                             float Metallic, USceneComponent* Parent = nullptr, bool bShadow = true, bool bPick = true);
	/** A flat decal quad on the panel face (label plate, lens, dial): centre (n, r, u), width along right, height along up. */
	UStaticMeshComponent* AddFace(const FVector& CenterNRU, float Width, float Height, UMaterialInterface* Material,
	                              USceneComponent* Parent = nullptr, bool bPick = true);
	UMaterialInstanceDynamic* PaintMID(const FLinearColor& Color, float Roughness, float Metallic);
	void AddLabelPlate();
	void PlaySound(USoundBase* Sound, float Volume = 1.f) const;
	void Changed(int32 OldState);
	static FVector NRU(float N, float R, float U) { return FVector(N, -R, U); }

	UPROPERTY(Transient)
	TObjectPtr<ACMPanel> Panel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> HighlightMIDs;

	TSharedPtr<FJsonObject> Json;
	float Recess = 0.f;
	bool bHighlighted = false;
};

/** Two- or three-position toggle; optional hinged guard (must be lifted first) or lever lock; spring-loaded positions. */
UCLASS(ClassGroup = (Apollo11))
class UCMToggle : public UCMControl
{
	GENERATED_BODY()

public:
	virtual void Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J) override;
	virtual bool Primary(float AimUp) override;
	virtual void PrimaryRelease() override;
	virtual bool Secondary() override;
	virtual bool Wheel(int32 Dir) override;
	virtual bool Animate(float Dt) override;
	virtual void SetStateDirect(int32 NewState) override;
	virtual FString GetHintText() const override;
	virtual uint32 Snapshot() const override { return uint32(State) | (bGuardOpen ? 0x10000u : 0u); }
	virtual void Restore(uint32 Packed) override;

	bool IsGuarded() const { return Guard != ECMGuard::None; }
	bool IsGuardOpen() const { return bGuardOpen; }
	bool MoveTo(int32 NewState);
	void SetGuardOpen(bool bOpen);

	UPROPERTY(VisibleAnywhere, Category = "Toggle")
	ECMGuard Guard = ECMGuard::None;

	UPROPERTY(VisibleAnywhere, Category = "Toggle")
	bool bGuardOpen = false;

	UPROPERTY(VisibleAnywhere, Category = "Toggle")
	bool bLeverLock = false;

	UPROPERTY(VisibleAnywhere, Category = "Toggle")
	TArray<int32> SpringPositions;

	/** Talkback driven by this switch (SM RCS helium valves). */
	UPROPERTY(VisibleAnywhere, Category = "Toggle")
	FName Talkback;

	/** Lever throw either side of centre, deg. */
	static constexpr float Throw = 26.f;

private:
	float AngleFor(int32 S) const;
	int32 RestFor(int32 S) const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> LeverPivot;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GuardPivot;

	float LeverAngle = 0.f;
	float LeverPull = 0.f;
	float GuardAngle = 0.f;
	bool bHeld = false;
};

/** Detented rotary switch (pointer knob, valve handle) or thumbwheel. */
UCLASS(ClassGroup = (Apollo11))
class UCMRotary : public UCMControl
{
	GENERATED_BODY()

public:
	virtual void Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J) override;
	virtual bool Primary(float AimUp) override;
	virtual bool Secondary() override;
	virtual bool Wheel(int32 Dir) override;
	virtual bool Animate(float Dt) override;
	virtual void SetStateDirect(int32 NewState) override;
	virtual FString GetHintText() const override;

	bool Step(int32 Dir);

	UPROPERTY(VisibleAnywhere, Category = "Rotary")
	TArray<float> Angles;

	UPROPERTY(VisibleAnywhere, Category = "Rotary")
	bool bThumbwheel = false;

	/** Interior-light channel driven by this knob: "flood", "integral", "numerics" or empty. */
	UPROPERTY(VisibleAnywhere, Category = "Rotary")
	FString LightChannel;

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> KnobPivot;

	float Angle = 0.f;
};

/** Momentary pushbutton with a lit legend lens; the sequencer buttons sit under a flip guard. */
UCLASS(ClassGroup = (Apollo11))
class UCMPushButton : public UCMControl
{
	GENERATED_BODY()

public:
	virtual void Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J) override;
	virtual bool Primary(float AimUp) override;
	virtual void PrimaryRelease() override;
	virtual bool Secondary() override;
	virtual bool Animate(float Dt) override;
	virtual FString GetStatusText() const override;
	virtual FString GetHintText() const override;
	virtual uint32 Snapshot() const override { return uint32(PressCount & 0xFFFF) | (bGuardOpen ? 0x10000u : 0u); }
	virtual void Restore(uint32 Packed) override;

	void SetLit(bool bOn) { bLit = bOn; }
	bool IsLit() const { return bLit; }
	bool IsPressed() const { return bPressed; }

	UPROPERTY(VisibleAnywhere, Category = "Pushbutton")
	ECMGuard Guard = ECMGuard::None;

	UPROPERTY(VisibleAnywhere, Category = "Pushbutton")
	bool bGuardOpen = false;

	/** Number of presses this session (sequencer pushbuttons only log; nothing fires on the pad). */
	UPROPERTY(VisibleAnywhere, Category = "Pushbutton")
	int32 PressCount = 0;

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CapPivot;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GuardPivot;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMID;

	FLinearColor LitColor = FLinearColor(1.f, 0.1f, 0.05f);
	bool bLit = false;
	bool bPressed = false;
	float Travel = 0.f;
	float LitLevel = 0.f;
	float GuardAngle = 0.f;
};

/** Push-pull circuit breaker: in = closed, pulled out = open (white band shows). */
UCLASS(ClassGroup = (Apollo11))
class UCMBreaker : public UCMControl
{
	GENERATED_BODY()

public:
	virtual void Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J) override;
	virtual bool Primary(float AimUp) override;
	virtual bool Wheel(int32 Dir) override;
	virtual bool Animate(float Dt) override;
	virtual void SetStateDirect(int32 NewState) override;
	virtual FString GetHintText() const override { return State == 0 ? TEXT("click: pull out (open)") : TEXT("click: push in (close)"); }

	bool IsClosed() const { return State == 0; }

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> KnobPivot;

	float Out = 0.f;
};

/** Annunciator / status light (lens with legend). */
UCLASS(ClassGroup = (Apollo11))
class UCMLight : public UCMControl
{
	GENERATED_BODY()

public:
	virtual void Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J) override;
	virtual bool IsOperable() const override { return false; }
	virtual bool Animate(float Dt) override;
	virtual FString GetStatusText() const override;

	void SetLit(bool bOn) { bLit = bOn; }
	bool IsLit() const { return bLit; }

	/** "L" / "R" = left / right half of the C&W matrix (empty for other lights). */
	UPROPERTY(VisibleAnywhere, Category = "Light")
	FString CWGroup;

	UPROPERTY(VisibleAnywhere, Category = "Light")
	bool bBlank = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMID;

	bool bLit = false;
	float Level = 0.f;
};

/** Talkback indicator: gray flag or barber pole (diagonal stripes). */
UCLASS(ClassGroup = (Apollo11))
class UCMTalkback : public UCMControl
{
	GENERATED_BODY()

public:
	virtual void Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J) override;
	virtual bool IsOperable() const override { return false; }
	virtual bool Animate(float Dt) override;
	virtual FString GetStatusText() const override;

	void SetBarberPole(bool bBarber) { bWantBarber = bBarber; }
	bool IsBarberPole() const { return bShownBarber; }

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> FlagPivot;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlagMID;

	bool bWantBarber = false;
	bool bShownBarber = false;
	float Flip = 0.f;            // 0 = flag face-on; the flag turns edge-on, swaps, turns back
};

/** Needle gauge driven by a value. */
UCLASS(ClassGroup = (Apollo11))
class UCMGauge : public UCMControl
{
	GENERATED_BODY()

public:
	virtual void Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J) override;
	virtual bool IsOperable() const override { return false; }
	virtual bool Animate(float Dt) override;
	virtual FString GetStatusText() const override;

	void SetValue(float V) { Target = V; }

	UPROPERTY(VisibleAnywhere, Category = "Gauge")
	float Value = 0.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> NeedlePivot;

	float Target = 0.f;
	float VMin = 0.f, VMax = 1.f, A0 = -135.f, A1 = 135.f;
};
