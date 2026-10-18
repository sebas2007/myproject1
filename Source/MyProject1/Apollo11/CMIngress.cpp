// Command Module ingress/egress at the White Room hatch, the three crew couches and the seated suit arms.

#include "Apollo11/CMIngress.h"
#include "Apollo11/BodycamComponent.h"
#include "Apollo11/CMPanel.h"
#include "MyProject1Character.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMIngress, Log, All);

namespace
{
	// how the crews got in on the pad (Apollo 11 closeout photos and crew accounts; PHOTO-DERIVED timing):
	// hold the bar over the hatch, swing the legs in, slide in on the back over the centre couch, move across to
	// the left couch. Chest-camera poses in the actor frame (origin = White Room floor at the hatch plane).
	FCMIngressKey Key(float Time, const FVector& Loc, const FRotator& Rot)
	{
		FCMIngressKey K;
		K.Time = Time;
		K.Location = Loc;
		K.Rotation = Rot;
		return K;
	}

	FCMSeat Seat(const TCHAR* Code, const TCHAR* Role, const TCHAR* Name, const FVector& Eye, const FVector& LookAt,
	             const FVector4& Limits, const FVector2D& Hide)
	{
		FCMSeat S;
		S.Code = Code;
		S.Role = Role;
		S.Astronaut = Name;
		S.Eye = Eye;
		S.LookAt = LookAt;
		S.Limits = Limits;
		S.BodyHideYaw = Hide;
		return S;
	}

	float SmoothStep(float X)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		return X * X * (3.f - 2.f * X);
	}

}

ACMIngress::ACMIngress()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// fitted to the Columbia scan couches (Tools/Realism/build_cockpit_ingress.py writes the same keys); the last key is
	// replaced at BeginPlay by the Commander's seat pose
	Keys = {
		Key(0.0f, FVector(-70.f, 0.f, 136.f), FRotator(-5.f, 0.f, 0.f)),          // standing at the hatch
		Key(1.1f, FVector(-48.f, 0.f, 142.f), FRotator(40.f, 0.f, 0.f)),          // hands on the bar above the hatch
		Key(2.3f, FVector(-30.f, 0.f, 160.f), FRotator(50.f, 0.f, -3.f)),         // pull up, legs swing into the hatch
		Key(3.6f, FVector(25.f, 0.f, 165.f), FRotator(52.f, 0.f, 0.f)),           // on the back, feet first through the hatch
		Key(5.0f, FVector(128.f, 0.f, 192.f), FRotator(76.f, 0.f, 0.f)),          // over the centre couch, clear of its headrest
		Key(7.0f, FVector(130.f, -62.f, 190.f), FRotator(57.5f, 96.f, 95.1f)),    // left couch (replaced by Seats[0])
	};
	// Eye points: the Columbia scan's headrests are at x 118..136, top z 171 (actor frame). A helmeted head rests on them:
	// helmet shell ~2 cm + back of the head to the eyes ~17 cm -> eyes ~19 cm above the headrest, over its centre
	// (Apollo 11 pre-launch photos of the crew strapped in: visor ~ 40-50 cm below the main display console).
	// Couch centre lines y = -62 / 0 / +62. The relaxed view rests on the console in front of each couch.
	Seats = {
		Seat(TEXT("CDR"), TEXT("Commander"), TEXT("Neil A. Armstrong"), FVector(129.f, -62.f, 190.f), FVector(170.f, -42.f, 236.f),
		     FVector4(120.f, 70.f, 35.f, 40.f), FVector2D(-55.f, 180.f)),
		Seat(TEXT("CMP"), TEXT("Command Module Pilot"), TEXT("Michael Collins"), FVector(129.f, 0.f, 190.f), FVector(170.f, 0.f, 238.f),
		     FVector4(95.f, 95.f, 35.f, 40.f), FVector2D(-180.f, 180.f)),
		Seat(TEXT("LMP"), TEXT("Lunar Module Pilot"), TEXT("Edwin E. Aldrin Jr."), FVector(129.f, 62.f, 190.f), FVector(170.f, 42.f, 236.f),
		     FVector4(70.f, 120.f, 35.f, 40.f), FVector2D(-180.f, 55.f)),
	};
	SndHatchSwing = FSoftObjectPath(TEXT("/Game/Apollo11/Audio/Cockpit/S_CM_HatchSwing.S_CM_HatchSwing"));
	SndHatchLatch = FSoftObjectPath(TEXT("/Game/Apollo11/Audio/Cockpit/S_CM_HatchLatch.S_CM_HatchLatch"));
	SndHatchRatchet = FSoftObjectPath(TEXT("/Game/Apollo11/Audio/Cockpit/S_CM_HatchRatchet.S_CM_HatchRatchet"));
	ArmSegmentMesh = FSoftObjectPath(TEXT("/Game/Apollo11/Meshes/Night/Detail/SM_D_Cyl.SM_D_Cyl"));
	ArmJointMesh = FSoftObjectPath(TEXT("/Game/Apollo11/Meshes/Night/Detail/SM_D_Sphere.SM_D_Sphere"));
	GloveMeshL = FSoftObjectPath(TEXT("/Game/Apollo11/Meshes/Night/Cockpit/SM_SuitGlove_L.SM_SuitGlove_L"));
	GloveMeshR = FSoftObjectPath(TEXT("/Game/Apollo11/Meshes/Night/Cockpit/SM_SuitGlove_R.SM_SuitGlove_R"));
	SuitMaterial = FSoftObjectPath(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_SuitWhite.MI_W_SuitWhite"));
	GloveMaterial = FSoftObjectPath(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_GloveWhite.MI_W_GloveWhite"));
	RingMaterial = FSoftObjectPath(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_HandrailSteel.MI_W_HandrailSteel"));
}

void ACMIngress::BeginPlay()
{
	Super::BeginPlay();
	// the ingress path ends in the Commander's couch
	if (Keys.Num() > 0 && Seats.Num() > 0)
	{
		Keys.Last().Location = Seats[0].Eye;
		Keys.Last().Rotation = SeatBaseLocal(0).Rotator();
	}
	BodySeatY = Seats.Num() > 0 ? Seats[0].Eye.Y : -62.f;
	for (AActor* A : SeatedBody)
	{
		BodyBaseLocations.Add(A ? A->GetActorLocation() : FVector::ZeroVector);
	}
	for (TActorIterator<ACMPanel> It(GetWorld()); It; ++It)
	{
		Panel = *It;
		break;
	}
	BuildArms();
	SetSeatedBodyVisible(false);
	SetHatchAmount(0.f);
	// legacy key bindings, like the elevator; not consumed, so the elevator's E keeps working too
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		EnableInput(PC);
		if (InputComponent)
		{
			auto Bind = [this](const FKey& K, EInputEvent Ev, void (ACMIngress::*Fn)())
			{
				FInputKeyBinding& B = InputComponent->BindKey(K, Ev, this, Fn);
				B.bConsumeInput = false;
			};
			Bind(EKeys::E, IE_Pressed, &ACMIngress::OnUsePressed);
			Bind(EKeys::One, IE_Pressed, &ACMIngress::OnSeat1);
			Bind(EKeys::Two, IE_Pressed, &ACMIngress::OnSeat2);
			Bind(EKeys::Three, IE_Pressed, &ACMIngress::OnSeat3);
			Bind(EKeys::Tab, IE_Pressed, &ACMIngress::OnTab);
			Bind(EKeys::LeftShift, IE_Pressed, &ACMIngress::OnShiftDown);
			Bind(EKeys::LeftShift, IE_Released, &ACMIngress::OnShiftUp);
		}
	}
}

void ACMIngress::EndPlay(const EEndPlayReason::Type Reason)
{
	SetBlockersEnabled(true);
	Super::EndPlay(Reason);
}

AMyProject1Character* ACMIngress::GetPlayerCharacter() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? Cast<AMyProject1Character>(PC->GetPawn()) : nullptr;
}

bool ACMIngress::IsPawnAtHatch() const
{
	const AMyProject1Character* Char = GetPlayerCharacter();
	if (!Char || State != ECMIngressState::Idle)
	{
		return false;
	}
	// capsule bottom in the actor frame (floor-relative), so the zone does not depend on the capsule size
	const FVector Feet = Char->GetActorLocation() - FVector(0.f, 0.f, Char->GetSimpleCollisionHalfHeight());
	if (!PromptZone.IsInsideOrOn(GetActorTransform().InverseTransformPosition(Feet)))
	{
		return false;
	}
	const FVector View = Char->GetControlRotation().Vector().GetSafeNormal2D();
	return FVector::DotProduct(View, GetActorForwardVector().GetSafeNormal2D()) >= PromptMinFacing;
}

FString ACMIngress::GetPrompt() const
{
	if (State == ECMIngressState::Seated)
	{
		return bExitPending ? TEXT("Opening hatch...") : TEXT("Press E to exit");
	}
	return IsPawnAtHatch() ? TEXT("Press E to enter") : FString();
}

void ACMIngress::OnUsePressed()
{
	Use();
}

void ACMIngress::Use()
{
	if (State == ECMIngressState::Idle && IsPawnAtHatch())
	{
		StartPath(true);
	}
	else if (State == ECMIngressState::Seated && !bSwitching && !bExitPending)
	{
		// the closeout crew opens the hatch first; the path starts once it is open (UpdateHatch)
		bExitPending = true;
		if (HatchState == ECMHatchState::Closed || HatchState == ECMHatchState::Closing)
		{
			HatchState = ECMHatchState::Opening;
			PlayHatchSound(SndHatchLatch, 0.8f);
			PlayHatchSound(SndHatchSwing);
		}
	}
}

void ACMIngress::SetSeatedBodyVisible(bool bVisible)
{
	bBodyShown = bVisible;
	for (AActor* A : SeatedBody)
	{
		if (A)
		{
			A->SetActorHiddenInGame(!bVisible);
		}
	}
	for (UStaticMeshComponent* C : ArmParts)
	{
		if (C)
		{
			C->SetVisibility(bVisible);
		}
	}
}

void ACMIngress::SetBlockersEnabled(bool bEnable)
{
	for (AActor* A : Blockers)
	{
		if (A)
		{
			A->SetActorEnableCollision(bEnable);
		}
	}
}

// ------------------------------------------------------------------------------------------------ seats
FQuat ACMIngress::SeatBaseLocal(int32 Index) const
{
	// lying on the back with the head toward the hatch (-X): the top of the view points toward -X
	const FCMSeat& S = GetSeat(Index);
	const FVector Fwd = (S.LookAt - S.Eye).GetSafeNormal();
	return FRotationMatrix::MakeFromXZ(Fwd, FVector(-1.f, 0.f, 0.f)).ToQuat();
}

FVector ACMIngress::GetSeatViewLocation() const
{
	return GetActorTransform().TransformPosition(GetSeat(SeatIndex).Eye);
}

FQuat ACMIngress::GetSeatBaseQuat() const
{
	return GetActorQuat() * SeatBaseLocal(SeatIndex);
}

FQuat ACMIngress::SeatRotation() const
{
	return GetSeatBaseQuat() * FRotator(SeatLook.Y, SeatLook.X, 0.f).Quaternion();
}

bool ACMIngress::SelectSeat(int32 Index)
{
	if (State != ECMIngressState::Seated || bSwitching || bExitPending || Index == SeatIndex || Index < 0 || Index >= Seats.Num())
	{
		return false;
	}
	SwitchStartRot = SeatRotation();
	SwitchFrom = SeatIndex;
	SeatIndex = Index;
	SwitchTime = 0.f;
	bSwitching = true;
	SeatLook = FVector2D::ZeroVector;
	if (AController* C = GetPlayerCharacter() ? GetPlayerCharacter()->GetController() : nullptr)
	{
		C->SetControlRotation(FRotator::ZeroRotator);
	}
	UE_LOG(LogCMIngress, Log, TEXT("CM seat: %s -> %s (%s)"), *Seats[SwitchFrom].Code, *Seats[SeatIndex].Code, *Seats[SeatIndex].Astronaut);
	return true;
}

void ACMIngress::MoveSeatedBody(float SeatY)
{
	BodySeatY = SeatY;
	const FVector Off = GetActorTransform().TransformVector(FVector(0.f, SeatY - Seats[0].Eye.Y, 0.f));
	for (int32 i = 0; i < SeatedBody.Num() && i < BodyBaseLocations.Num(); ++i)
	{
		if (SeatedBody[i])
		{
			SeatedBody[i]->SetActorLocation(BodyBaseLocations[i] + Off);
		}
	}
}

// ------------------------------------------------------------------------------------------------ path
void ACMIngress::StartPath(bool bEnter)
{
	AMyProject1Character* Char = GetPlayerCharacter();
	UBodycamComponent* Cam = Char ? Char->FindComponentByClass<UBodycamComponent>() : nullptr;
	if (!Char || !Cam || Keys.Num() < 2)
	{
		return;
	}
	const FTransform Frame = GetActorTransform();
	PathT.Reset();
	PathLoc.Reset();
	PathRot.Reset();
	const float Total = Keys.Last().Time;
	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		const FCMIngressKey& K = Keys[bEnter ? i : Keys.Num() - 1 - i];
		PathT.Add(bEnter ? K.Time : Total - K.Time);
		PathLoc.Add(Frame.TransformPosition(K.Location));
		PathRot.Add(Frame.GetRotation() * K.Rotation.Quaternion());
	}
	// start from where the camera actually is (standing anywhere in the prompt zone / looking around in any couch)
	PathLoc[0] = Cam->GetViewLocation();
	PathRot[0] = Cam->GetViewRotation();
	for (int32 i = 1; i < PathRot.Num(); ++i)
	{
		PathRot[i].EnforceShortestArcWith(PathRot[i - 1]);
	}
	PathTan.SetNum(PathRot.Num());
	for (int32 i = 0; i < PathRot.Num(); ++i)
	{
		FQuat::CalcTangents(PathRot[FMath::Max(i - 1, 0)], PathRot[i], PathRot[FMath::Min(i + 1, PathRot.Num() - 1)], 0.f, PathTan[i]);
	}
	PathDuration = FMath::Max(Total, 0.1f);
	PathTime = 0.f;

	Char->SetCockpitLocked(true);
	Char->SetActorEnableCollision(false);
	SetSeatedBodyVisible(false);
	if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	SetBlockersEnabled(false);
	State = bEnter ? ECMIngressState::Entering : ECMIngressState::Exiting;
	UE_LOG(LogCMIngress, Log, TEXT("CM ingress: %s (%.1f s)"), bEnter ? TEXT("entering") : TEXT("exiting"), PathDuration);
}

void ACMIngress::SamplePath(float T, FVector& OutLoc, FQuat& OutRot) const
{
	const int32 N = PathT.Num();
	int32 i = 0;
	while (i < N - 2 && T > PathT[i + 1])
	{
		++i;
	}
	const float Span = FMath::Max(PathT[i + 1] - PathT[i], KINDA_SMALL_NUMBER);
	const float S = FMath::Clamp((T - PathT[i]) / Span, 0.f, 1.f);
	// uniform Catmull-Rom through the key positions (end points repeated)
	const FVector& P0 = PathLoc[FMath::Max(i - 1, 0)];
	const FVector& P1 = PathLoc[i];
	const FVector& P2 = PathLoc[i + 1];
	const FVector& P3 = PathLoc[FMath::Min(i + 2, N - 1)];
	const float S2 = S * S, S3 = S2 * S;
	OutLoc = 0.5f * ((2.f * P1) + (-P0 + P2) * S + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * S2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * S3);
	OutRot = FQuat::Squad(PathRot[i], PathTan[i], PathRot[i + 1], PathTan[i + 1], S).GetNormalized();
}

void ACMIngress::ApplyView(const FVector& WorldLoc, const FQuat& WorldRot)
{
	AMyProject1Character* Char = GetPlayerCharacter();
	UBodycamComponent* Cam = Char ? Char->FindComponentByClass<UBodycamComponent>() : nullptr;
	if (!Cam)
	{
		return;
	}
	Cam->SetViewOverride(WorldLoc, WorldRot);
	// keep the (collision-free) pawn with the camera so streaming, audio and lighting follow it
	Char->SetActorLocation(WorldLoc - FVector(0.f, 0.f, 40.f), false, nullptr, ETeleportType::TeleportPhysics);
}

void ACMIngress::FinishPath()
{
	AMyProject1Character* Char = GetPlayerCharacter();
	SetBlockersEnabled(true);
	if (State == ECMIngressState::Entering)
	{
		State = ECMIngressState::Seated;
		SeatIndex = 0;
		MoveSeatedBody(Seats[0].Eye.Y);
		SetSeatedBodyVisible(true);
		bWristInit = false;
		SeatLook = FVector2D::ZeroVector;
		HatchTimer = 0.f;
		bExitPending = false;
		if (AController* C = Char ? Char->GetController() : nullptr)
		{
			C->SetControlRotation(FRotator::ZeroRotator);   // seated look = control rotation offsets from zero
		}
		UE_LOG(LogCMIngress, Log, TEXT("CM ingress: seated (left couch)"));
		return;
	}
	State = ECMIngressState::Idle;
	bExitPending = false;
	SeatIndex = 0;
	MoveSeatedBody(Seats[0].Eye.Y);
	if (!Char)
	{
		return;
	}
	// stand up where the path started: capsule placed so the chest camera is at the first key
	UBodycamComponent* Cam = Char->FindComponentByClass<UBodycamComponent>();
	const FTransform Frame = GetActorTransform();
	const FRotator Stand = (Frame.GetRotation() * Keys[0].Rotation.Quaternion()).Rotator();
	const FRotator Yaw(0.f, Stand.Yaw, 0.f);
	const FVector Chest = Cam ? Cam->ChestOffset : FVector(14.f, 5.f, 40.f);
	Char->SetActorLocationAndRotation(Frame.TransformPosition(Keys[0].Location) - Yaw.RotateVector(Chest), Yaw, false, nullptr,
	                                  ETeleportType::TeleportPhysics);
	if (AController* C = Char->GetController())
	{
		C->SetControlRotation(FRotator(Stand.Pitch, Stand.Yaw, 0.f));
	}
	Char->SetActorEnableCollision(true);
	if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
	if (Cam)
	{
		Cam->ClearViewOverride();
	}
	Char->SetCockpitLocked(false);
	UE_LOG(LogCMIngress, Log, TEXT("CM ingress: back in the White Room"));
}

bool ACMIngress::AddSeatLook(float Yaw, float Pitch)
{
	if (State != ECMIngressState::Seated)
	{
		return false;
	}
	const FVector4& L = GetSeat(SeatIndex).Limits;
	// clamp here too: an offset past +-180 would otherwise wrap before the next tick clamps it
	SeatLook.X = FMath::Clamp(SeatLook.X + Yaw, -L.X, L.Y);
	SeatLook.Y = FMath::Clamp(SeatLook.Y + Pitch, -L.W, L.Z);
	if (AController* C = GetPlayerCharacter() ? GetPlayerCharacter()->GetController() : nullptr)
	{
		C->SetControlRotation(FRotator(SeatLook.Y, SeatLook.X, 0.f));
	}
	return true;
}

// ------------------------------------------------------------------------------------------------ hatch
void ACMIngress::PlayHatchSound(const TSoftObjectPtr<USoundBase>& Sound, float Volume) const
{
	if (USoundBase* S = Sound.LoadSynchronous())
	{
		const FVector At = HatchParts.Num() > 0 && HatchParts[0] ? HatchParts[0]->GetActorLocation() : GetActorLocation();
		UGameplayStatics::PlaySoundAtLocation(this, S, At, Volume, FMath::FRandRange(0.97f, 1.03f));
	}
}

void ACMIngress::SetHatchAmount(float T)
{
	HatchT = FMath::Clamp(T, 0.f, 1.f);
	// the closeout crew swings it in steadily and seats it gently: smoothstep with a slower finish
	const float E = SmoothStep(HatchT);
	const float Yaw = FMath::Lerp(HatchOpenYaw, HatchClosedYaw, E);
	// parts are modelled in the closed pose with their origin on the hinge line: rotate them about the hinge axis
	const FVector Axis = HatchHingeAxis.IsNearlyZero() ? FVector::UpVector : HatchHingeAxis.GetSafeNormal();
	const FQuat Q(Axis, FMath::DegreesToRadians(Yaw));
	for (AActor* A : HatchParts)
	{
		if (A)
		{
			A->SetActorRotation(Q);
		}
	}
}

void ACMIngress::UpdateHatch(float Dt)
{
	switch (HatchState)
	{
	case ECMHatchState::Open:
		if (State == ECMIngressState::Seated && !bExitPending)
		{
			HatchTimer += Dt;
			if (HatchTimer >= HatchCloseDelay)
			{
				HatchState = ECMHatchState::Closing;
				bRatchetPlayed = false;
				PlayHatchSound(SndHatchSwing);
				UE_LOG(LogCMIngress, Log, TEXT("CM hatch: closing"));
			}
		}
		else if (State == ECMIngressState::Seated && bExitPending && !bSwitching)
		{
			StartPath(false);
		}
		break;
	case ECMHatchState::Closing:
		SetHatchAmount(HatchT + Dt / FMath::Max(HatchSwingTime, 0.1f));
		if (HatchT >= 0.93f && !bRatchetPlayed)
		{
			bRatchetPlayed = true;
			PlayHatchSound(SndHatchLatch);
		}
		if (HatchT >= 1.f)
		{
			HatchState = ECMHatchState::Closed;
			PlayHatchSound(SndHatchRatchet, 0.9f);              // the latch handle is ratcheted down
			UE_LOG(LogCMIngress, Log, TEXT("CM hatch: closed and latched"));
		}
		break;
	case ECMHatchState::Opening:
		SetHatchAmount(HatchT - Dt / FMath::Max(HatchSwingTime * 0.7f, 0.1f));
		if (HatchT <= 0.f)
		{
			HatchState = ECMHatchState::Open;
			UE_LOG(LogCMIngress, Log, TEXT("CM hatch: open"));
		}
		break;
	default:
		break;
	}
}

// ------------------------------------------------------------------------------------------------ suit arms
void ACMIngress::BuildArms()
{
	UStaticMesh* Seg = ArmSegmentMesh.LoadSynchronous();
	UStaticMesh* Joint = ArmJointMesh.LoadSynchronous();
	UStaticMesh* Glove[2] = {GloveMeshL.LoadSynchronous(), GloveMeshR.LoadSynchronous()};
	UMaterialInterface* Suit = SuitMaterial.LoadSynchronous();
	UMaterialInterface* GloveMat = GloveMaterial.LoadSynchronous();
	UMaterialInterface* Ring = RingMaterial.LoadSynchronous();
	if (!Seg || !Joint || !Glove[0] || !Glove[1])
	{
		UE_LOG(LogCMIngress, Warning, TEXT("CM suit arms: meshes missing (run build_cockpit_launch.build('body'))"));
		return;
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		UStaticMesh* Meshes[5] = {Seg, Joint, Seg, Seg, Glove[Side]};
		UMaterialInterface* Mats[5] = {Suit, Suit, Suit, Ring, GloveMat};
		for (int32 k = 0; k < 5; ++k)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(Meshes[k]);
			if (Mats[k])
			{
				C->SetMaterial(0, Mats[k]);
			}
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetUsingAbsoluteLocation(true);
			C->SetUsingAbsoluteRotation(true);
			C->SetUsingAbsoluteScale(true);
			C->SetupAttachment(RootComponent);
			C->RegisterComponent();
			C->SetVisibility(false);
			ArmParts.Add(C);
		}
	}
	bArmsBuilt = true;
}

void ACMIngress::PoseArm(int32 Side, const FVector& S, const FVector& W, const FVector& Up, const FVector& Pole)
{
	const float L1 = UpperArmLength, L2 = ForearmLength;
	FVector D = W - S;
	const float Dist = FMath::Clamp(D.Size(), FMath::Abs(L1 - L2) + 2.f, L1 + L2 - 0.5f);
	const FVector Dir = D.GetSafeNormal();
	const FVector Wc = S + Dir * Dist;
	// two-bone IK: elbow on the circle around the shoulder-wrist line, toward the pole (outward and down: suit elbows
	// sag onto the couch side)
	const float A = (L1 * L1 - L2 * L2 + Dist * Dist) / (2.f * Dist);
	const float H = FMath::Sqrt(FMath::Max(L1 * L1 - A * A, 0.f));
	const FVector Perp = (Pole - Dir * FVector::DotProduct(Pole, Dir)).GetSafeNormal();
	const FVector E = S + Dir * A + Perp * H;
	UStaticMeshComponent* P[5];
	for (int32 k = 0; k < 5; ++k)
	{
		P[k] = ArmParts[Side * 5 + k];
	}
	auto Segment = [](UStaticMeshComponent* C, const FVector& From, const FVector& To, float R)
	{
		const FVector V = To - From;
		C->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZ(V.GetSafeNormal()).ToQuat(), From, FVector(R, R, V.Size())));
	};
	Segment(P[0], S, E, 5.7f);                                                     // upper arm (bulky pressurized sleeve)
	P[1]->SetWorldTransform(FTransform(FQuat::Identity, E, FVector(6.0f)));        // elbow convolute
	Segment(P[2], E, Wc, 4.9f);                                                    // forearm
	const FVector FDir = (Wc - E).GetSafeNormal();
	Segment(P[3], Wc - FDir * 1.2f, Wc + FDir * 1.6f, 5.5f);                       // wrist disconnect ring
	const FVector GUp = (Up - FDir * FVector::DotProduct(Up, FDir)).GetSafeNormal();
	P[4]->SetWorldTransform(FTransform(FRotationMatrix::MakeFromXZ(FDir, GUp.IsNearlyZero() ? FVector::UpVector : GUp).ToQuat(),
	                                   Wc + FDir * 1.6f, FVector(1.f)));
}

void ACMIngress::UpdateArms(float Dt)
{
	if (!bArmsBuilt || State != ECMIngressState::Seated || !bBodyShown)
	{
		ReachHand = -1;
		ReachError = -1.f;
		return;
	}
	const FTransform F = GetActorTransform();
	const float Ys = BodySeatY;
	FVector Target[2], Up[2], Shoulder[2];
	for (int32 i = 0; i < 2; ++i)
	{
		const float Sg = i == 0 ? 1.f : -1.f;                        // left arm uses the offsets as given (-Y), right mirrored
		Shoulder[i] = F.TransformPosition(FVector(ShoulderOffset.X, Ys + Sg * ShoulderOffset.Y, ShoulderOffset.Z));
		const FVector& RW = bArmsLowered ? LoweredWrist : RestWrist;
		Target[i] = F.TransformPosition(FVector(RW.X, Ys + Sg * RW.Y, RW.Z));
		Up[i] = F.TransformVector(bArmsLowered ? FVector(0.f, -Sg, 0.f) : FVector(0.f, 0.f, 1.f));
	}
	// reach: the view rests on an interactive control -> the nearer hand goes to it
	int32 Hand = -1;
	FVector Tip = FVector::ZeroVector;
	FVector AimLoc, AimN;
	FName AimId;
	const bool bAim = !bArmsLowered && !bSwitching && !bExitPending && Panel && Panel->GetAimedPose(AimLoc, AimN, AimId);
	if (bAim)
	{
		AimHold = AimId == LastAimed ? AimHold + Dt : 0.f;
		LastAimed = AimId;
		if (AimHold >= ReachDelay)
		{
			Hand = FVector::DistSquared(Shoulder[0], AimLoc) <= FVector::DistSquared(Shoulder[1], AimLoc) ? 0 : 1;
			Tip = AimLoc + AimN * 3.f;                                // fingertip stops just in front of the switch
			const FVector Dir = (Tip - Shoulder[Hand]).GetSafeNormal();
			FVector W = Tip - Dir * HandLength;
			const float Reach = UpperArmLength + ForearmLength - 1.f;
			if (FVector::Dist(W, Shoulder[Hand]) > Reach)
			{
				W = Shoulder[Hand] + (W - Shoulder[Hand]).GetSafeNormal() * Reach;    // the suit stops it short
			}
			Target[Hand] = W;
			Up[Hand] = AimN;                                          // back of the glove away from the panel
		}
	}
	else
	{
		AimHold = 0.f;
		LastAimed = NAME_None;
	}
	ReachHand = Hand;
	if (!bWristInit)
	{
		for (int32 i = 0; i < 2; ++i)
		{
			Wrist[i] = Target[i];
			WristVel[i] = FVector::ZeroVector;
			GloveUp[i] = Up[i];
		}
		bWristInit = true;
	}
	const float W0 = ArmStiffness;
	for (int32 i = 0; i < 2; ++i)
	{
		// critically damped spring with a speed cap: stiff, slightly lagging pressurized-suit motion
		const FVector Acc = (Target[i] - Wrist[i]) * (W0 * W0) - WristVel[i] * (2.f * W0);
		WristVel[i] = (WristVel[i] + Acc * Dt).GetClampedToMaxSize(ArmMaxSpeed);
		Wrist[i] += WristVel[i] * Dt;
		GloveUp[i] = FMath::Lerp(GloveUp[i], Up[i], FMath::Min(Dt * 4.f, 1.f)).GetSafeNormal();
		const float Sg = i == 0 ? -1.f : 1.f;
		const FVector Pole = F.TransformVector(FVector(0.15f, Sg, -0.8f)).GetSafeNormal();
		PoseArm(i, Shoulder[i], Wrist[i], GloveUp[i], Pole);
	}
	if (Hand >= 0)
	{
		const FVector Dir = (Tip - Shoulder[Hand]).GetSafeNormal();
		ReachError = FVector::Dist(Wrist[Hand] + Dir * HandLength, Tip);
	}
	else
	{
		ReachError = -1.f;
	}
}

// ------------------------------------------------------------------------------------------------ tick
void ACMIngress::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	UpdateHatch(Dt);
	if (State == ECMIngressState::Entering || State == ECMIngressState::Exiting)
	{
		PathTime += Dt;
		const float U = FMath::Clamp(PathTime / PathDuration, 0.f, 1.f);
		FVector Loc;
		FQuat Rot;
		SamplePath(SmoothStep(U) * PathDuration, Loc, Rot);
		ApplyView(Loc, Rot);
		if (U >= 1.f)
		{
			FinishPath();
		}
		return;
	}
	if (State != ECMIngressState::Seated)
	{
		return;
	}
	const FTransform Frame = GetActorTransform();
	if (bSwitching)
	{
		// move across to the other couch: a short slide with a small lift over the couch edges
		SwitchTime += Dt;
		const float U = FMath::Clamp(SwitchTime / FMath::Max(SeatSwitchTime, 0.1f), 0.f, 1.f);
		const float S = SmoothStep(U);
		const FCMSeat& A = GetSeat(SwitchFrom);
		const FCMSeat& B = GetSeat(SeatIndex);
		FVector P = FMath::Lerp(A.Eye, B.Eye, S);
		P.Z += 7.f * FMath::Sin(PI * S);
		const FQuat R = FQuat::Slerp(SwitchStartRot, GetSeatBaseQuat(), S);
		MoveSeatedBody(FMath::Lerp(A.Eye.Y, B.Eye.Y, S));
		ApplyView(Frame.TransformPosition(P), R);
		if (AController* C = GetPlayerCharacter() ? GetPlayerCharacter()->GetController() : nullptr)
		{
			C->SetControlRotation(FRotator::ZeroRotator);
		}
		if (U >= 1.f)
		{
			bSwitching = false;
			MoveSeatedBody(B.Eye.Y);
		}
		UpdateArms(Dt);
		return;
	}
	// look-around: the control rotation is the offset from the couch view, clamped (and written back clamped,
	// so it never winds up beyond the limits)
	const FVector4& L = GetSeat(SeatIndex).Limits;
	if (AController* C = GetPlayerCharacter() ? GetPlayerCharacter()->GetController() : nullptr)
	{
		const FRotator R = C->GetControlRotation();
		SeatLook.X = FMath::Clamp(FRotator::NormalizeAxis(R.Yaw), -L.X, L.Y);
		SeatLook.Y = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch), -L.W, L.Z);
		C->SetControlRotation(FRotator(SeatLook.Y, SeatLook.X, 0.f));
	}
	const FVector2D Hide = GetSeat(SeatIndex).BodyHideYaw;
	const bool bShowBody = SeatLook.X > Hide.X && SeatLook.X < Hide.Y;
	if (bShowBody != bBodyShown)
	{
		SetSeatedBodyVisible(bShowBody);
	}
	ApplyView(GetSeatViewLocation(), SeatRotation());
	UpdateArms(Dt);
}
