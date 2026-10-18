// Interactive Command Module panel controls.

#include "Apollo11/CMControls.h"
#include "Apollo11/CMPanel.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace
{
	FVector JVec(const TSharedPtr<FJsonObject>& J, const TCHAR* Field, const FVector& Default = FVector::ZeroVector)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (J->TryGetArrayField(Field, A) && A->Num() >= 3)
		{
			return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
		return Default;
	}

	FVector2D JVec2(const TSharedPtr<FJsonObject>& J, const TCHAR* Field, const FVector2D& Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (J->TryGetArrayField(Field, A) && A->Num() >= 2)
		{
			return FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber());
		}
		return Default;
	}

	FLinearColor JRect(const TSharedPtr<FJsonObject>& J, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (J->TryGetArrayField(Field, A) && A->Num() >= 4)
		{
			return FLinearColor((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber(), (*A)[3]->AsNumber());
		}
		return FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	float JNum(const TSharedPtr<FJsonObject>& J, const TCHAR* Field, float Default)
	{
		double V = Default;
		return J->TryGetNumberField(Field, V) ? float(V) : Default;
	}

	FString JStr(const TSharedPtr<FJsonObject>& J, const TCHAR* Field)
	{
		FString S;
		J->TryGetStringField(Field, S);
		return S;
	}

	ECMGuard GuardFrom(const FString& S)
	{
		return S == TEXT("gray") ? ECMGuard::Gray : S == TEXT("stripe") ? ECMGuard::Stripe : S == TEXT("red") ? ECMGuard::Red : ECMGuard::None;
	}

	/** Exponential approach (frame-rate independent). */
	float Approach(float Cur, float Target, float Rate, float Dt)
	{
		return FMath::Abs(Target - Cur) < 1e-3f ? Target : Cur + (Target - Cur) * (1.f - FMath::Exp(-Rate * Dt));
	}

	// colours of the 1969 hardware (PHOTO-DERIVED from the Columbia scan textures and Apollo cabin photos)
	const FLinearColor Chrome(0.72f, 0.72f, 0.70f);
	const FLinearColor Steel(0.50f, 0.50f, 0.48f);
	const FLinearColor GuardGray(0.40f, 0.41f, 0.40f);
	const FLinearColor GuardRed(0.48f, 0.05f, 0.035f);
	const FLinearColor StripeYellow(0.75f, 0.55f, 0.04f);
	const FLinearColor KnobColor(0.50f, 0.48f, 0.43f);
	const FLinearColor BreakerBlack(0.025f, 0.025f, 0.025f);
	const FLinearColor BezelDark(0.06f, 0.06f, 0.06f);
	const FLinearColor Ink(0.90f, 0.90f, 0.86f);
	const FLinearColor PanelGray(0.42f, 0.43f, 0.41f);           // MDC gray (scan panel texture ~ sRGB 125, linear)
	const FLinearColor RailFloor(0.22f, 0.22f, 0.21f);
	const FLinearColor NutSteel(0.24f, 0.24f, 0.23f);
	const float LampBrightness = 100.f;                              // lit lamps read in the bodycam's cabin exposure

	// a plane (100 x 100, normal +Z) turned so its +X = panel up and +Y = viewer's right, normal out of the panel;
	// the face materials map the texture from that local position, so mesh UVs don't matter
	const FRotator FaceRot = FRotationMatrix::MakeFromXY(FVector(0, 0, 1), FVector(0, -1, 0)).Rotator();
}

// ======================================================================================================== base
void UCMControl::Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J)
{
	Panel = InPanel;
	Json = J;
	ControlId = FName(*JStr(J, TEXT("id")));
	Label = JStr(J, TEXT("label"));
	Short = JStr(J, TEXT("short"));
	PanelName = JStr(J, TEXT("panel"));
	const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
	if (J->TryGetArrayField(TEXT("positions"), P))
	{
		for (const TSharedPtr<FJsonValue>& V : *P)
		{
			Positions.Add(V->AsString());
		}
	}
	LaunchState = int32(JNum(J, TEXT("launch"), 0.f));
	State = LaunchState;
	Recess = JNum(J, TEXT("recess"), 0.f);

	// pose on the panel: position on the fitted panel face, +X out of the face, +Z panel-up
	const FTransform F = Panel->GetFrame();
	const FVector WN = F.TransformVectorNoScale(JVec(J, TEXT("normal"), FVector::UpVector)).GetSafeNormal();
	FVector WU = F.TransformVectorNoScale(JVec(J, TEXT("up"), FVector::ForwardVector));
	WU = (WU - WN * FVector::DotProduct(WU, WN)).GetSafeNormal();
	SetWorldLocationAndRotation(F.TransformPosition(JVec(J, TEXT("pos"))), FRotationMatrix::MakeFromXZ(WN, WU).Rotator());
}

void UCMControl::SetStateDirect(int32 NewState)
{
	State = FMath::Clamp(NewState, 0, FMath::Max(Positions.Num() - 1, 0));
}

FString UCMControl::GetPositionName() const
{
	return Positions.IsValidIndex(State) ? Positions[State] : FString();
}

FString UCMControl::GetStatusText() const
{
	const FString Pos = GetPositionName();
	return Pos.IsEmpty() ? Label : FString::Printf(TEXT("%s  -  %s"), *Label, *Pos);
}

void UCMControl::SetHighlighted(bool bOn)
{
	if (bOn == bHighlighted)
	{
		return;
	}
	bHighlighted = bOn;
	for (UMaterialInstanceDynamic* M : HighlightMIDs)
	{
		if (M)
		{
			M->SetScalarParameterValue(TEXT("Highlight"), bOn ? 1.f : 0.f);
		}
	}
}

bool UCMControl::RayHit(const FVector& Origin, const FVector& Dir, float& OutT, float& OutAimUp, float Margin, float MinHalf) const
{
	// the clickable area is the control's own visible geometry: each part's mesh bounds in the part's current pose (a thrown
	// lever or an opened guard is clicked where it is drawn), grown by Margin. The nearest part along the ray counts.
	float Best = TNumericLimits<float>::Max();
	for (const UStaticMeshComponent* P : PickParts)
	{
		const UStaticMesh* M = P ? P->GetStaticMesh() : nullptr;
		if (!M || !P->IsVisible())
		{
			continue;
		}
		const FBox B = M->GetBoundingBox();
		const FTransform PT = P->GetComponentTransform();
		const FVector S = PT.GetScale3D().GetAbs();
		// in the part's scaled local space the ray parameter stays in world cm
		const FVector O = PT.InverseTransformPosition(Origin);
		const FVector D = PT.InverseTransformVector(Dir);
		const FVector C = B.GetCenter();
		FVector H = B.GetExtent();
		for (int32 A = 0; A < 3; ++A)
		{
			const float Sa = FMath::Max(S[A], 1e-4f);
			H[A] = FMath::Max(H[A] + Margin / Sa, MinHalf / Sa);
		}
		float T0 = 0.f, T1 = 1e6f;
		bool bMiss = false;
		for (int32 A = 0; A < 3 && !bMiss; ++A)
		{
			const float Lo = C[A] - H[A], Hi = C[A] + H[A];
			if (FMath::Abs(D[A]) < 1e-9f)
			{
				bMiss = O[A] < Lo || O[A] > Hi;
				continue;
			}
			float Ta = (Lo - O[A]) / D[A], Tb = (Hi - O[A]) / D[A];
			if (Ta > Tb)
			{
				Swap(Ta, Tb);
			}
			T0 = FMath::Max(T0, Ta);
			T1 = FMath::Min(T1, Tb);
			bMiss = T0 > T1;
		}
		if (!bMiss && T0 < Best)
		{
			Best = T0;
		}
	}
	if (Best == TNumericLimits<float>::Max())
	{
		return false;
	}
	OutT = Best;
	// hit point along panel-up relative to the control centre (above / below a toggle's pivot)
	OutAimUp = GetComponentTransform().InverseTransformPositionNoScale(Origin + Dir * Best).Z;
	return true;
}

TArray<FVector> UCMControl::GetVisiblePartCenters() const
{
	TArray<TPair<float, FVector>> Parts;
	for (const UStaticMeshComponent* P : PickParts)
	{
		if (P && P->GetStaticMesh() && P->IsVisible())
		{
			const FBox B = P->GetStaticMesh()->GetBoundingBox();
			const FVector E = B.GetExtent() * P->GetComponentTransform().GetScale3D().GetAbs();
			Parts.Add({FMath::Max(E.X, 0.05f) * FMath::Max(E.Y, 0.05f) * FMath::Max(E.Z, 0.05f),
			           P->GetComponentTransform().TransformPosition(B.GetCenter())});
		}
	}
	Parts.Sort([](const TPair<float, FVector>& A, const TPair<float, FVector>& B) { return A.Key > B.Key; });
	TArray<FVector> Out;
	for (const TPair<float, FVector>& P : Parts)
	{
		Out.Add(P.Value);
	}
	return Out;
}

FVector UCMControl::GetVisibleCenter() const
{
	// the middle of the control's largest visible part (toggle nut or closed guard, knob, button cap, breaker collar, lens):
	// the union of all parts can have its centre in empty air beside a thrown lever
	FVector Best = GetComponentLocation();
	float BestSize = -1.f;
	for (const UStaticMeshComponent* P : PickParts)
	{
		if (P && P->GetStaticMesh() && P->IsVisible())
		{
			const FBox B = P->GetStaticMesh()->GetBoundingBox();
			const FVector E = B.GetExtent() * P->GetComponentTransform().GetScale3D().GetAbs();
			const float Size = FMath::Max(E.X, 0.05f) * FMath::Max(E.Y, 0.05f) * FMath::Max(E.Z, 0.05f);
			if (Size > BestSize)
			{
				BestSize = Size;
				Best = P->GetComponentTransform().TransformPosition(B.GetCenter());
			}
		}
	}
	return Best;
}

UMaterialInstanceDynamic* UCMControl::PaintMID(const FLinearColor& Color, float Roughness, float Metallic)
{
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Panel->Paint, this);
	M->SetVectorParameterValue(TEXT("Color"), Color);
	M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	return M;
}

UStaticMeshComponent* UCMControl::AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& LocalNRU, const FRotator& Rot,
                                          const FVector& Scale, USceneComponent* Parent, bool bHighlight, bool bShadow, bool bPick)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(GetOwner());
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(bShadow);
	C->bCastDynamicShadow = bShadow;
	C->SetupAttachment(Parent ? Parent : this);
	C->SetRelativeLocationAndRotation(LocalNRU, Rot);
	C->SetRelativeScale3D(Scale);
	if (Material)
	{
		for (int32 i = 0; i < FMath::Max(C->GetNumMaterials(), 1); ++i)
		{
			C->SetMaterial(i, Material);
		}
		if (bHighlight)
		{
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material))
			{
				HighlightMIDs.AddUnique(MID);
			}
		}
	}
	C->RegisterComponent();
	if (bPick)
	{
		PickParts.Add(C);
	}
	return C;
}

UStaticMeshComponent* UCMControl::AddBox(const FVector& CenterNRU, const FVector& SizeNRU, const FLinearColor& Color, float Roughness,
                                         float Metallic, USceneComponent* Parent, bool bShadow, bool bPick)
{
	// engine cube: 100 cm, pivot in the centre
	return AddPart(Panel->Cube, PaintMID(Color, Roughness, Metallic), CenterNRU, FRotator::ZeroRotator,
	               FVector(SizeNRU.X, SizeNRU.Y, SizeNRU.Z) / 100.f, Parent, true, bShadow, bPick);
}

UStaticMeshComponent* UCMControl::AddFace(const FVector& CenterNRU, float Width, float Height, UMaterialInterface* Material,
                                          USceneComponent* Parent, bool bPick)
{
	// plane local X = panel up (height), local Y = viewer's right (width)
	return AddPart(Panel->Plane, Material, CenterNRU, FaceRot, FVector(Height / 100.f, Width / 100.f, 1.f), Parent, true, false, bPick);
}

void UCMControl::AddLabelPlate()
{
	const FLinearColor Rect = JRect(Json, TEXT("label_rect"));
	if (Rect.B <= 0.f || !Panel->LabelMat)
	{
		return;
	}
	// lettering 0.3 cm high (a 0.5 cm atlas cell); the plate is larger, in the panel colour, so it also covers the scan's
	// own (blurred, often two-line) engraving of the same label
	const float TextH = 0.5f;
	const float TextW = TextH * JNum(Json, TEXT("label_aspect"), 3.f);
	const float H = 1.0f;
	const float W = FMath::Max(TextW, Recess > 0.f ? JNum(Json, TEXT("cell_w"), 2.6f) - 0.2f : 1.6f);
	const FVector Bg = JVec(Json, TEXT("label_bg"), FVector(0.45f, 0.46f, 0.44f));
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Panel->LabelMat, this);
	M->SetVectorParameterValue(TEXT("Rect"), Rect);
	M->SetVectorParameterValue(TEXT("Fit"), FLinearColor(W / TextW, H / TextH, 0.f, 0.f));
	// the sampled scan colour is the lit, tone-mapped texture: back to an albedo near the panel's (roughly linear)
	M->SetVectorParameterValue(TEXT("Bg"), FLinearColor(FMath::Pow(Bg.X, 2.2f), FMath::Pow(Bg.Y, 2.2f), FMath::Pow(Bg.Z, 2.2f)));
	M->SetVectorParameterValue(TEXT("Ink"), Ink);
	// not clickable: the plate lies between controls (it made clicks on the next row land on this control)
	AddFace(NRU(0.03f, 0.f, JNum(Json, TEXT("label_offset"), 1.6f) - (Recess > 0.f ? 0.2f : 0.f)), W, H, M, nullptr, false);
}

void UCMControl::PlaySound(USoundBase* Sound, float Volume) const
{
	if (Panel)
	{
		Panel->PlayControlSound(Sound, GetComponentLocation(), Volume);
	}
}

void UCMControl::Changed(int32 OldState)
{
	if (Panel)
	{
		Panel->OnControlChanged(this, OldState);
	}
}

// ======================================================================================================== toggle
void UCMToggle::Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J)
{
	Super::Init(InPanel, J);
	Guard = GuardFrom(JStr(J, TEXT("guard")));
	bLeverLock = J->HasField(TEXT("lock")) && J->GetBoolField(TEXT("lock"));
	const TArray<TSharedPtr<FJsonValue>>* S = nullptr;
	if (J->TryGetArrayField(TEXT("spring"), S))
	{
		for (const TSharedPtr<FJsonValue>& V : *S)
		{
			SpringPositions.Add(int32(V->AsNumber()));
		}
	}
	Talkback = FName(*JStr(J, TEXT("talkback")));

	const float Floor = -Recess;
	if (Recess > 0.f)
	{
		// clean switch-rail cell over the museum's hole covers: floor plate and the guard posts either side
		const float W = JNum(J, TEXT("cell_w"), 2.6f);
		// (rail decoration, shared with the neighbours: not part of this switch's clickable area)
		AddBox(NRU(Floor + 0.05f, 0.f, 0.f), FVector(0.1f, W + 0.05f, 3.1f), RailFloor, 0.65f, 0.f, nullptr, true, false);
		for (float S2 : {-0.5f, 0.5f})
		{
			AddBox(NRU(Floor + (Recess + 1.95f) * 0.5f, S2 * W, 0.f), FVector(Recess + 1.95f, 0.24f, 3.0f), PanelGray, 0.55f, 0.f, nullptr,
			       true, false);
		}
	}
	// bushing + hex nut, lever on its pivot inside the bushing
	AddPart(Panel->ToggleBase, PaintMID(NutSteel, 0.5f, 1.f), NRU(Floor + 0.1f, 0.f, 0.f), FRotator::ZeroRotator, FVector::OneVector);
	LeverPivot = NewObject<USceneComponent>(GetOwner());
	LeverPivot->SetupAttachment(this);
	LeverPivot->SetRelativeLocation(NRU(Floor + 0.62f, 0.f, 0.f));
	LeverPivot->RegisterComponent();
	AddPart(Panel->ToggleLever, PaintMID(Chrome, 0.25f, 1.f), FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, LeverPivot);
	if (bLeverLock)
	{
		// lever-lock collar: the lever must be pulled out of its detent before it moves
		AddPart(Panel->Cylinder, PaintMID(Steel, 0.3f, 1.f), FVector(0.42f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f), FVector(0.0048f, 0.0048f, 0.003f),
		        LeverPivot);
	}
	if (IsGuarded())
	{
		// hinged cover over the switch, hinge on the up edge (closed it holds the switch in its safe position)
		GuardPivot = NewObject<USceneComponent>(GetOwner());
		GuardPivot->SetupAttachment(this);
		GuardPivot->SetRelativeLocation(NRU(Floor + 0.05f, 0.f, 1.05f));
		GuardPivot->RegisterComponent();
		UMaterialInstanceDynamic* GM = UMaterialInstanceDynamic::Create(Panel->StripesMat, this);
		GM->SetVectorParameterValue(TEXT("ColorA"), Guard == ECMGuard::Red ? GuardRed : GuardGray);
		GM->SetVectorParameterValue(TEXT("ColorB"), StripeYellow);
		GM->SetVectorParameterValue(TEXT("ColorC"), FLinearColor(0.02f, 0.02f, 0.02f));
		GM->SetScalarParameterValue(TEXT("Stripes"), Guard == ECMGuard::Stripe ? 1.f : 0.f);
		GM->SetScalarParameterValue(TEXT("Period"), 0.9f);
		AddPart(Panel->GuardMesh, GM, FVector(Recess, 0.f, 0.f), FRotator::ZeroRotator, FVector::OneVector, GuardPivot);
		PickExtent = FVector(1.0f, 1.6f, 2.8f);
	}
	else
	{
		PickExtent = FVector(0.95f, 1.45f, 2.3f);
	}
	AddLabelPlate();
	SetStateDirect(State);
}

float UCMToggle::AngleFor(int32 S) const
{
	const int32 N = Positions.Num();
	return N <= 1 ? 0.f : Throw * (1.f - 2.f * float(S) / float(N - 1));
}

int32 UCMToggle::RestFor(int32 S) const
{
	// a spring-loaded position returns to its neighbour toward the centre
	const int32 N = Positions.Num();
	return S == 0 ? FMath::Min(1, N - 1) : (S == N - 1 ? FMath::Max(N - 2, 0) : S);
}

void UCMToggle::SetStateDirect(int32 NewState)
{
	Super::SetStateDirect(NewState);
	LeverAngle = AngleFor(State);
	if (LeverPivot)
	{
		LeverPivot->SetRelativeRotation(FRotator(LeverAngle, 0.f, 0.f));
	}
}

void UCMToggle::Restore(uint32 Packed)
{
	bGuardOpen = IsGuarded() && (Packed & 0x10000u) != 0;
	GuardAngle = bGuardOpen ? 105.f : 0.f;
	if (GuardPivot)
	{
		GuardPivot->SetRelativeRotation(FRotator(GuardAngle, 0.f, 0.f));
	}
	bHeld = false;
	LeverPull = 0.f;
	SetStateDirect(int32(Packed & 0xFFFF));
	if (LeverPivot)
	{
		LeverPivot->SetRelativeLocation(NRU(-Recess + 0.62f, 0.f, 0.f));
	}
}

bool UCMToggle::MoveTo(int32 NewState)
{
	NewState = FMath::Clamp(NewState, 0, Positions.Num() - 1);
	if (NewState == State)
	{
		return false;
	}
	const int32 Old = State;
	State = NewState;
	PlaySound(bLeverLock ? Panel->Toggle2 : (FMath::RandBool() ? Panel->Toggle1 : Panel->Toggle2), bLeverLock ? 1.f : 0.85f);
	Changed(Old);
	return true;
}

bool UCMToggle::Primary(float AimUp)
{
	if (IsGuarded() && !bGuardOpen)
	{
		Panel->SetHint(TEXT("Guard closed - right click to lift it"));
		return false;
	}
	const int32 N = Positions.Num();
	int32 Target = State;
	if (N == 2)
	{
		Target = 1 - State;
	}
	else if (N > 2)
	{
		if (AimUp > 0.3f)
		{
			Target = State - 1;                                      // aimed above the pivot: push the lever up
		}
		else if (AimUp < -0.3f)
		{
			Target = State + 1;
		}
		else
		{
			Target = State == 0 ? 1 : (State == N - 1 ? N - 2 : 0);
		}
		if (Target < 0 || Target >= N)
		{
			Target = State == 0 ? 1 : N - 2;                          // already at that stop: go back the other way
		}
	}
	bHeld = true;
	return MoveTo(Target);
}

void UCMToggle::PrimaryRelease()
{
	bHeld = false;
}

bool UCMToggle::Wheel(int32 Dir)
{
	if (IsGuarded() && !bGuardOpen)
	{
		Panel->SetHint(TEXT("Guard closed - right click to lift it"));
		return false;
	}
	bHeld = false;
	return MoveTo(State - Dir);
}

void UCMToggle::SetGuardOpen(bool bOpen)
{
	if (!IsGuarded() || bOpen == bGuardOpen)
	{
		return;
	}
	bGuardOpen = bOpen;
	PlaySound(bOpen ? Panel->GuardOpen : Panel->GuardClose, 0.9f);
	const int32 Old = State;
	if (!bOpen && State != LaunchState)
	{
		// closing the cover pushes the lever back to the safe (guarded) position
		State = LaunchState;
		PlaySound(Panel->Toggle1, 0.7f);
	}
	Changed(Old);
}

bool UCMToggle::Secondary()
{
	if (!IsGuarded())
	{
		return false;
	}
	SetGuardOpen(!bGuardOpen);
	return true;
}

bool UCMToggle::Animate(float Dt)
{
	if (!bHeld && SpringPositions.Contains(State) && FMath::Abs(LeverAngle - AngleFor(State)) < 2.f)
	{
		MoveTo(RestFor(State));                                      // spring-loaded: back to rest once released
	}
	const float Target = AngleFor(State);
	const float NewAngle = Approach(LeverAngle, Target, 38.f, Dt);
	const float PullTarget = bLeverLock && FMath::Abs(Target - LeverAngle) > 1.5f ? 0.22f : 0.f;
	const float NewPull = Approach(LeverPull, PullTarget, 30.f, Dt);
	const float NewGuard = Approach(GuardAngle, bGuardOpen ? 105.f : 0.f, 14.f, Dt);
	const bool bMoving = NewAngle != LeverAngle || NewPull != LeverPull || NewGuard != GuardAngle;
	if (bMoving)
	{
		LeverAngle = NewAngle;
		LeverPull = NewPull;
		GuardAngle = NewGuard;
		LeverPivot->SetRelativeLocationAndRotation(NRU(-Recess + 0.62f + LeverPull, 0.f, 0.f), FRotator(LeverAngle, 0.f, 0.f));
		if (GuardPivot)
		{
			GuardPivot->SetRelativeRotation(FRotator(GuardAngle, 0.f, 0.f));
		}
	}
	return bMoving || (SpringPositions.Contains(State) && !bHeld);
}

FString UCMToggle::GetHintText() const
{
	if (IsGuarded() && !bGuardOpen)
	{
		return TEXT("GUARDED  -  right click: lift guard");
	}
	FString H = Positions.Num() > 2 ? TEXT("click above / below the lever: up / down") : TEXT("click: flip");
	if (IsGuarded())
	{
		H += TEXT("   right click: close guard");
	}
	if (bLeverLock)
	{
		H += TEXT("   (lever lock)");
	}
	if (SpringPositions.Num())
	{
		H += TEXT("   (spring-loaded: hold)");
	}
	return H;
}

// ======================================================================================================== rotary
void UCMRotary::Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J)
{
	Super::Init(InPanel, J);
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("angles"), A))
	{
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			Angles.Add(float(V->AsNumber()));
		}
	}
	while (Angles.Num() < Positions.Num())
	{
		Angles.Add(Angles.Num() ? Angles.Last() + 30.f : 0.f);
	}
	const FString Knob = JStr(J, TEXT("knob"));
	bThumbwheel = Knob == TEXT("thumbwheel");
	LightChannel = JStr(J, TEXT("light"));
	const float D = JNum(J, TEXT("knob_d"), 4.f);

	KnobPivot = NewObject<USceneComponent>(GetOwner());
	KnobPivot->SetupAttachment(this);
	KnobPivot->RegisterComponent();
	if (bThumbwheel)
	{
		// thumbwheel: knurled wheel on a left-right axis, its rim 0.35 cm proud of the face, in a dark window
		KnobPivot->SetRelativeLocation(NRU(-0.45f, 0.f, 0.f));
		AddPart(Panel->Thumbwheel, PaintMID(FLinearColor(0.05f, 0.05f, 0.05f), 0.5f, 0.f), FVector::ZeroVector, FRotator::ZeroRotator,
		        FVector::OneVector, KnobPivot);
		AddBox(NRU(-0.9f, 0.f, 0.f), FVector(0.1f, 1.0f, 2.1f), FLinearColor(0.015f, 0.015f, 0.015f), 0.8f, 0.f, nullptr, false);
		PickExtent = FVector(0.65f, 1.1f, 0.9f);
	}
	else if (Knob == TEXT("valve"))
	{
		AddPart(Panel->Valve, PaintMID(Steel, 0.35f, 1.f), FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, KnobPivot);
		PickExtent = FVector(1.4f, 1.4f, 1.5f);
	}
	else
	{
		// pointer knob (normalised mesh, diameter 1) with a white index line along the pointer
		AddPart(Panel->Knob, PaintMID(KnobColor, 0.45f, 0.f), FVector::ZeroVector, FRotator::ZeroRotator, FVector(D), KnobPivot);
		AddBox(NRU(0.365f * D, 0.f, 0.24f * D), FVector(0.02f, 0.045f * D, 0.36f * D), Ink, 0.5f, 0.f, KnobPivot, false);
		PickExtent = FVector(D * 0.5f, D * 0.5f, 0.4f * D);
	}
	AddLabelPlate();
	SetStateDirect(State);
}

void UCMRotary::SetStateDirect(int32 NewState)
{
	Super::SetStateDirect(NewState);
	Angle = Angles.IsValidIndex(State) ? Angles[State] : 0.f;
	if (KnobPivot)
	{
		KnobPivot->SetRelativeRotation(bThumbwheel ? FRotator(Angle, 0.f, 0.f) : FRotator(0.f, 0.f, Angle));
	}
}

bool UCMRotary::Step(int32 Dir)
{
	const int32 New = State + Dir;
	if (!Positions.IsValidIndex(New))
	{
		return false;
	}
	const int32 Old = State;
	State = New;
	PlaySound(bThumbwheel ? Panel->ThumbwheelTick : Panel->Detent, 0.9f);
	Changed(Old);
	return true;
}

bool UCMRotary::Primary(float AimUp)
{
	// click: one detent clockwise; from the last detent back to the first
	if (Step(1))
	{
		return true;
	}
	const int32 Old = State;
	State = 0;
	PlaySound(Panel->Detent, 0.9f);
	Changed(Old);
	return true;
}

bool UCMRotary::Secondary()
{
	return Step(-1);
}

bool UCMRotary::Wheel(int32 Dir)
{
	return Step(Dir);
}

bool UCMRotary::Animate(float Dt)
{
	const float Target = Angles.IsValidIndex(State) ? Angles[State] : 0.f;
	const float New = Approach(Angle, Target, 22.f, Dt);
	if (New == Angle)
	{
		return false;
	}
	Angle = New;
	// pointer: rotation about the panel normal (clockwise as the crew sees it); thumbwheel: about its left-right axle
	KnobPivot->SetRelativeRotation(bThumbwheel ? FRotator(Angle, 0.f, 0.f) : FRotator(0.f, 0.f, Angle));
	return true;
}

FString UCMRotary::GetHintText() const
{
	return bThumbwheel ? TEXT("wheel: turn") : TEXT("wheel: turn   click: next position   right click: back");
}

// ======================================================================================================== pushbutton
void UCMPushButton::Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J)
{
	Super::Init(InPanel, J);
	Guard = GuardFrom(JStr(J, TEXT("guard")));
	const FVector2D S = JVec2(J, TEXT("size"), FVector2D(2.3f, 1.9f));
	const FString Lit = JStr(J, TEXT("lit_color"));
	LitColor = Lit == TEXT("red") ? FLinearColor(1.f, 0.07f, 0.03f) : FLinearColor(1.f, 0.62f, 0.2f);

	AddBox(NRU(0.17f, 0.f, 0.f), FVector(0.34f, S.X + 0.3f, S.Y + 0.3f), BezelDark, 0.5f, 0.f);       // bezel
	CapPivot = NewObject<USceneComponent>(GetOwner());
	CapPivot->SetupAttachment(this);
	CapPivot->SetRelativeLocation(NRU(0.34f, 0.f, 0.f));
	CapPivot->RegisterComponent();
	AddBox(NRU(0.15f, 0.f, 0.f), FVector(0.3f, S.X - 0.1f, S.Y - 0.1f), FLinearColor(0.03f, 0.03f, 0.03f), 0.35f, 0.f, CapPivot);
	LensMID = UMaterialInstanceDynamic::Create(Panel->LensMat, this);
	LensMID->SetVectorParameterValue(TEXT("Rect"), JRect(J, TEXT("legend_rect")));
	LensMID->SetVectorParameterValue(TEXT("OffColor"), FLinearColor(0.018f, 0.018f, 0.018f));
	LensMID->SetScalarParameterValue(TEXT("Brightness"), LampBrightness);
	LensMID->SetVectorParameterValue(TEXT("Ink"), FLinearColor(0.75f, 0.75f, 0.72f));
	LensMID->SetVectorParameterValue(TEXT("LitColor"), LitColor);
	LensMID->SetScalarParameterValue(TEXT("Round"), 0.f);
	AddFace(NRU(0.305f, 0.f, 0.f), S.X - 0.2f, S.Y - 0.2f, LensMID, CapPivot);
	if (Guard != ECMGuard::None)
	{
		GuardPivot = NewObject<USceneComponent>(GetOwner());
		GuardPivot->SetupAttachment(this);
		GuardPivot->SetRelativeLocation(NRU(0.1f, 0.f, S.Y * 0.5f + 0.25f));
		GuardPivot->RegisterComponent();
		UMaterialInstanceDynamic* GM = UMaterialInstanceDynamic::Create(Panel->StripesMat, this);
		GM->SetVectorParameterValue(TEXT("ColorA"), GuardGray);
		GM->SetScalarParameterValue(TEXT("Stripes"), 0.f);
		// flip cover: plate 0.75 cm above the face hanging down from the hinge, short side walls
		AddPart(Panel->Cube, GM, NRU(0.8f, 0.f, -(S.Y * 0.5f + 0.25f)), FRotator::ZeroRotator,
		        FVector(0.12f, S.X + 0.45f, S.Y + 0.5f) / 100.f, GuardPivot);
		for (float Side : {-1.f, 1.f})
		{
			AddPart(Panel->Cube, GM, NRU(0.42f, Side * (S.X * 0.5f + 0.2f), -(S.Y * 0.5f + 0.25f)), FRotator::ZeroRotator,
			        FVector(0.75f, 0.1f, S.Y + 0.5f) / 100.f, GuardPivot);
		}
	}
	HighlightMIDs.AddUnique(LensMID);
	PickExtent = FVector(S.X * 0.5f + 0.15f, S.Y * 0.5f + 0.15f, Guard != ECMGuard::None ? 1.0f : 0.8f);
}

bool UCMPushButton::Primary(float AimUp)
{
	if (Guard != ECMGuard::None && !bGuardOpen)
	{
		Panel->SetHint(TEXT("Guard closed - right click to lift it"));
		return false;
	}
	bPressed = true;
	State = ++PressCount;                                            // a pushbutton's "state" is its press count
	PlaySound(Panel->ButtonPress, 0.9f);
	Changed(-1);
	return true;
}

void UCMPushButton::PrimaryRelease()
{
	if (bPressed)
	{
		bPressed = false;
		PlaySound(Panel->ButtonRelease, 0.7f);
	}
}

void UCMPushButton::Restore(uint32 Packed)
{
	PressCount = int32(Packed & 0xFFFF);
	State = PressCount;
	bPressed = false;
	Travel = 0.f;
	bGuardOpen = Guard != ECMGuard::None && (Packed & 0x10000u) != 0;
	GuardAngle = bGuardOpen ? 100.f : 0.f;
	if (CapPivot)
	{
		CapPivot->SetRelativeLocation(NRU(0.34f, 0.f, 0.f));
	}
	if (GuardPivot)
	{
		GuardPivot->SetRelativeRotation(FRotator(GuardAngle, 0.f, 0.f));
	}
}

bool UCMPushButton::Secondary()
{
	if (Guard == ECMGuard::None)
	{
		return false;
	}
	bGuardOpen = !bGuardOpen;
	PlaySound(bGuardOpen ? Panel->GuardOpen : Panel->GuardClose, 0.9f);
	Changed(-2);
	return true;
}

bool UCMPushButton::Animate(float Dt)
{
	const float NewTravel = Approach(Travel, bPressed ? 0.22f : 0.f, 40.f, Dt);
	const float NewLit = Approach(LitLevel, bLit ? 1.f : 0.f, 14.f, Dt);    // incandescent bulb, ~0.1 s
	const float NewGuard = Approach(GuardAngle, bGuardOpen ? 100.f : 0.f, 14.f, Dt);
	const bool bMoving = NewTravel != Travel || NewLit != LitLevel || NewGuard != GuardAngle;
	if (bMoving)
	{
		Travel = NewTravel;
		LitLevel = NewLit;
		GuardAngle = NewGuard;
		CapPivot->SetRelativeLocation(NRU(0.34f - Travel, 0.f, 0.f));
		LensMID->SetScalarParameterValue(TEXT("Lit"), LitLevel);
		if (GuardPivot)
		{
			GuardPivot->SetRelativeRotation(FRotator(GuardAngle, 0.f, 0.f));
		}
	}
	return bMoving;
}

FString UCMPushButton::GetStatusText() const
{
	return FString::Printf(TEXT("%s  -  %s%s"), *Label, bLit ? TEXT("LIT") : TEXT("OFF"), bPressed ? TEXT(" (pressed)") : TEXT(""));
}

FString UCMPushButton::GetHintText() const
{
	if (Guard != ECMGuard::None && !bGuardOpen)
	{
		return TEXT("GUARDED  -  right click: lift guard");
	}
	return Guard != ECMGuard::None ? TEXT("click: press   right click: close guard") : TEXT("click: press");
}

// ======================================================================================================== breaker
void UCMBreaker::Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J)
{
	Super::Init(InPanel, J);
	AddPart(Panel->BreakerCollar, PaintMID(NutSteel, 0.5f, 1.f), FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector);
	KnobPivot = NewObject<USceneComponent>(GetOwner());
	KnobPivot->SetupAttachment(this);
	KnobPivot->SetRelativeLocation(NRU(0.2f, 0.f, 0.f));
	KnobPivot->RegisterComponent();
	AddPart(Panel->BreakerKnob, PaintMID(BreakerBlack, 0.45f, 0.f), FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, KnobPivot);
	// white band on the shaft: inside the collar when closed, shows when the breaker has popped / been pulled
	AddPart(Panel->Cylinder, PaintMID(FLinearColor(0.85f, 0.85f, 0.82f), 0.5f, 0.f), NRU(0.2f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f),
	        FVector(0.0053f, 0.0053f, 0.003f), KnobPivot, true, false);
	PickExtent = FVector(0.72f, 0.72f, 1.5f);
	AddLabelPlate();
	SetStateDirect(State);
}

void UCMBreaker::SetStateDirect(int32 NewState)
{
	Super::SetStateDirect(NewState);
	Out = State == 0 ? 0.f : 0.38f;
	if (KnobPivot)
	{
		KnobPivot->SetRelativeLocation(NRU(0.2f + Out, 0.f, 0.f));
	}
}

bool UCMBreaker::Primary(float AimUp)
{
	const int32 Old = State;
	State = State == 0 ? 1 : 0;
	PlaySound(State == 1 ? Panel->BreakerPop : Panel->BreakerPush, 1.f);
	Changed(Old);
	return true;
}

bool UCMBreaker::Wheel(int32 Dir)
{
	return false;
}

bool UCMBreaker::Animate(float Dt)
{
	const float New = Approach(Out, State == 0 ? 0.f : 0.38f, State == 0 ? 30.f : 45.f, Dt);
	if (New == Out)
	{
		return false;
	}
	Out = New;
	KnobPivot->SetRelativeLocation(NRU(0.2f + Out, 0.f, 0.f));
	return true;
}

// ======================================================================================================== light
void UCMLight::Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J)
{
	Super::Init(InPanel, J);
	CWGroup = JStr(J, TEXT("cw"));
	bBlank = Short.IsEmpty();
	bLit = J->HasField(TEXT("on")) && J->GetBoolField(TEXT("on"));
	Level = bLit ? 1.f : 0.f;
	const FVector2D S = JVec2(J, TEXT("size"), FVector2D(2.f, 1.f));
	const bool bRound = J->HasField(TEXT("round")) && J->GetBoolField(TEXT("round"));
	const FString Col = JStr(J, TEXT("color"));
	LensMID = UMaterialInstanceDynamic::Create(Panel->LensMat, this);
	LensMID->SetVectorParameterValue(TEXT("Rect"), JRect(J, TEXT("legend_rect")));
	// unlit: the C&W lenses read as pale glass with dark lettering; round status lights are darker frosted glass
	LensMID->SetVectorParameterValue(TEXT("OffColor"), CWGroup.IsEmpty() ? FLinearColor(0.32f, 0.30f, 0.26f) : FLinearColor(0.55f, 0.54f, 0.50f));
	LensMID->SetVectorParameterValue(TEXT("Ink"), FLinearColor(0.03f, 0.03f, 0.03f));
	LensMID->SetVectorParameterValue(TEXT("LitColor"), Col == TEXT("red") ? FLinearColor(1.f, 0.08f, 0.04f) : FLinearColor(1.f, 0.6f, 0.16f));
	LensMID->SetScalarParameterValue(TEXT("Round"), bRound ? 1.f : 0.f);
	LensMID->SetScalarParameterValue(TEXT("Lit"), Level);
	LensMID->SetScalarParameterValue(TEXT("Brightness"), LampBrightness);
	AddFace(NRU(0.15f, 0.f, 0.f), S.X, S.Y, LensMID);              // the scan's own lens is cut away below it
	HighlightMIDs.AddUnique(LensMID);
	PickExtent = FVector(S.X * 0.5f, S.Y * 0.5f, 0.4f);
}

bool UCMLight::Animate(float Dt)
{
	const float New = Approach(Level, bLit ? 1.f : 0.f, 14.f, Dt);
	if (New == Level)
	{
		return false;
	}
	Level = New;
	LensMID->SetScalarParameterValue(TEXT("Lit"), Level);
	return true;
}

FString UCMLight::GetStatusText() const
{
	return FString::Printf(TEXT("%s  -  %s"), *Label, bLit ? TEXT("ON") : TEXT("OFF"));
}

// ======================================================================================================== talkback
void UCMTalkback::Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J)
{
	Super::Init(InPanel, J);
	const FVector2D S = JVec2(J, TEXT("size"), FVector2D(1.3f, 0.9f));
	bWantBarber = bShownBarber = JStr(J, TEXT("flag")) == TEXT("barber");
	FlagPivot = NewObject<USceneComponent>(GetOwner());
	FlagPivot->SetupAttachment(this);
	FlagPivot->SetRelativeLocation(NRU(0.04f, 0.f, 0.f));
	FlagPivot->RegisterComponent();
	FlagMID = UMaterialInstanceDynamic::Create(Panel->StripesMat, this);
	FlagMID->SetVectorParameterValue(TEXT("ColorA"), FLinearColor(0.30f, 0.30f, 0.29f));
	FlagMID->SetVectorParameterValue(TEXT("ColorB"), FLinearColor(0.80f, 0.80f, 0.77f));
	FlagMID->SetVectorParameterValue(TEXT("ColorC"), FLinearColor(0.02f, 0.02f, 0.02f));
	FlagMID->SetScalarParameterValue(TEXT("Period"), 0.32f);
	FlagMID->SetScalarParameterValue(TEXT("Stripes"), bShownBarber ? 1.f : 0.f);
	AddFace(FVector::ZeroVector, S.X, S.Y, FlagMID, FlagPivot);
	HighlightMIDs.AddUnique(FlagMID);
	PickExtent = FVector(S.X * 0.5f, S.Y * 0.5f, 0.4f);
	AddLabelPlate();
}

bool UCMTalkback::Animate(float Dt)
{
	if (Flip <= 0.f && bWantBarber == bShownBarber)
	{
		return false;
	}
	// the flag turns edge-on (~70 ms), the other face comes round
	const float Prev = Flip;
	Flip += Dt * 14.f;
	if (Prev < 1.f && Flip >= 1.f)
	{
		bShownBarber = bWantBarber;
		FlagMID->SetScalarParameterValue(TEXT("Stripes"), bShownBarber ? 1.f : 0.f);
	}
	if (Flip >= 2.f)
	{
		Flip = 0.f;
	}
	const float A = Flip < 1.f ? Flip * 90.f : (2.f - Flip) * 90.f;
	FlagPivot->SetRelativeRotation(FRotator(0.f, A, 0.f));          // about the panel-up axis
	return true;
}

FString UCMTalkback::GetStatusText() const
{
	return FString::Printf(TEXT("%s  -  %s"), *Label, bShownBarber ? TEXT("BARBER POLE (closed)") : TEXT("GRAY (open)"));
}

// ======================================================================================================== gauge
void UCMGauge::Init(ACMPanel* InPanel, const TSharedPtr<FJsonObject>& J)
{
	Super::Init(InPanel, J);
	Value = Target = JNum(J, TEXT("value"), 0.f);
	VMin = JNum(J, TEXT("vmin"), 0.f);
	VMax = JNum(J, TEXT("vmax"), 1.f);
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (J->TryGetArrayField(TEXT("angles"), A) && A->Num() >= 2)
	{
		A0 = float((*A)[0]->AsNumber());
		A1 = float((*A)[1]->AsNumber());
	}
	const float D = JVec2(J, TEXT("size"), FVector2D(5.f, 5.f)).X;
	UMaterialInstanceDynamic* Dial = UMaterialInstanceDynamic::Create(Panel->DialMat, this);
	Dial->SetTextureParameterValue(TEXT("Dial"), ControlId == FName(TEXT("P1_G_METER")) ? Panel->DialGMeter.Get() : Panel->DialAlphaPc.Get());
	AddFace(NRU(0.06f, 0.f, 0.f), D * 0.84f, D * 0.84f, Dial);
	HighlightMIDs.AddUnique(Dial);
	NeedlePivot = NewObject<USceneComponent>(GetOwner());
	NeedlePivot->SetupAttachment(this);
	NeedlePivot->SetRelativeLocation(NRU(0.12f, 0.f, 0.f));
	NeedlePivot->RegisterComponent();
	AddBox(NRU(0.02f, 0.f, D * 0.17f), FVector(0.04f, 0.11f, D * 0.36f), FLinearColor(0.9f, 0.9f, 0.86f), 0.5f, 0.f, NeedlePivot, false);
	AddPart(Panel->Cylinder, PaintMID(FLinearColor(0.02f, 0.02f, 0.02f), 0.4f, 0.f), NRU(0.06f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f),
	        FVector(0.005f, 0.005f, 0.0012f), NeedlePivot, false, false);
	PickExtent = FVector(D * 0.45f, D * 0.45f, 0.5f);
	const float Ang = A0 + (A1 - A0) * FMath::Clamp((Value - VMin) / FMath::Max(VMax - VMin, 1e-3f), 0.f, 1.f);
	NeedlePivot->SetRelativeRotation(FRotator(0.f, 0.f, Ang));
}

bool UCMGauge::Animate(float Dt)
{
	const float New = Approach(Value, Target, 4.f, Dt);
	if (New == Value)
	{
		return false;
	}
	Value = New;
	const float Ang = A0 + (A1 - A0) * FMath::Clamp((Value - VMin) / FMath::Max(VMax - VMin, 1e-3f), 0.f, 1.f);
	NeedlePivot->SetRelativeRotation(FRotator(0.f, 0.f, Ang));
	return true;
}

FString UCMGauge::GetStatusText() const
{
	return FString::Printf(TEXT("%s  -  %.1f"), *Label, Value);
}
