// The interactive Command Module main display console.

#include "Apollo11/CMPanel.h"
#include "Apollo11/CMControls.h"
#include "Apollo11/CMIngress.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/LightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogCMPanel, Log, All);

namespace
{
	const TCHAR* CtlDir = TEXT("/Game/Apollo11/Meshes/Cockpit/Controls/");
	const TCHAR* MatDir = TEXT("/Game/Apollo11/Materials/Cockpit/");
	const TCHAR* SndDir = TEXT("/Game/Apollo11/Audio/Cockpit/");

	FSoftObjectPath Asset(const TCHAR* Dir, const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), Dir, Name, Name));
	}

	template <typename T>
	T* LoadSoft(const TSoftObjectPtr<T>& P, const TCHAR* What)
	{
		T* O = P.LoadSynchronous();
		if (!O)
		{
			UE_LOG(LogCMPanel, Warning, TEXT("CM panel: asset %s (%s) not found"), *P.ToString(), What);
		}
		return O;
	}

	// SM-related annunciators: inhibited when the C&W CSM/CM switch is at CM (AOH 2.10, C&W system)
	const TCHAR* SMLamps[] = {TEXT("CW_L51"), TEXT("CW_L52"), TEXT("CW_L53"), TEXT("CW_L54"), TEXT("CW_L33"), TEXT("CW_R11"),
	                          TEXT("CW_R12"), TEXT("CW_R13"), TEXT("CW_R14"), TEXT("CW_R31"), TEXT("CW_R42")};
}

ACMPanel::ACMPanel()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;     // after the bodycam has placed the camera for this frame
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	MeshToggleBase = Asset(CtlDir, TEXT("SM_CMC_ToggleBase"));
	MeshToggleLever = Asset(CtlDir, TEXT("SM_CMC_ToggleLever"));
	MeshGuard = Asset(CtlDir, TEXT("SM_CMC_Guard"));
	MeshKnob = Asset(CtlDir, TEXT("SM_CMC_Knob"));
	MeshThumbwheel = Asset(CtlDir, TEXT("SM_CMC_Thumbwheel"));
	MeshValve = Asset(CtlDir, TEXT("SM_CMC_Valve"));
	MeshBreakerCollar = Asset(CtlDir, TEXT("SM_CMC_BreakerCollar"));
	MeshBreakerKnob = Asset(CtlDir, TEXT("SM_CMC_BreakerKnob"));
	MeshCube = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
	MeshCylinder = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	MeshPlane = FSoftObjectPath(TEXT("/Engine/BasicShapes/Plane.Plane"));
	MatPaint = Asset(MatDir, TEXT("M_CMPaint"));
	MatLabel = Asset(MatDir, TEXT("M_CMLabel"));
	MatLens = Asset(MatDir, TEXT("M_CMLens"));
	MatStripes = Asset(MatDir, TEXT("M_CMStripes"));
	MatDial = Asset(MatDir, TEXT("M_CMDial"));
	TexDialGMeter = FSoftObjectPath(TEXT("/Game/Apollo11/Textures/Cockpit/T_CMDial_GMeter.T_CMDial_GMeter"));
	TexDialAlphaPc = FSoftObjectPath(TEXT("/Game/Apollo11/Textures/Cockpit/T_CMDial_AlphaPc.T_CMDial_AlphaPc"));
	SndToggle1 = Asset(SndDir, TEXT("S_CM_Toggle1"));
	SndToggle2 = Asset(SndDir, TEXT("S_CM_Toggle2"));
	SndGuardOpen = Asset(SndDir, TEXT("S_CM_GuardOpen"));
	SndGuardClose = Asset(SndDir, TEXT("S_CM_GuardClose"));
	SndBreakerPop = Asset(SndDir, TEXT("S_CM_BreakerPop"));
	SndBreakerPush = Asset(SndDir, TEXT("S_CM_BreakerPush"));
	SndDetent = Asset(SndDir, TEXT("S_CM_Detent"));
	SndButtonPress = Asset(SndDir, TEXT("S_CM_ButtonPress"));
	SndButtonRelease = Asset(SndDir, TEXT("S_CM_ButtonRelease"));
	SndThumbwheel = Asset(SndDir, TEXT("S_CM_Thumbwheel"));
	SndMasterAlarm = Asset(SndDir, TEXT("S_CM_MasterAlarm"));
}

void ACMPanel::BeginPlay()
{
	Super::BeginPlay();
	if (!Ingress)
	{
		for (TActorIterator<ACMIngress> It(GetWorld()); It; ++It)
		{
			Ingress = *It;
			break;
		}
	}
	LoadAssets();
	for (AActor* A : FloodLights)
	{
		const ULightComponent* L = A ? A->FindComponentByClass<ULightComponent>() : nullptr;
		FloodBase.Add(L ? L->Intensity : 0.f);
	}
	for (AActor* A : PanelGlowLights)
	{
		const ULightComponent* L = A ? A->FindComponentByClass<ULightComponent>() : nullptr;
		GlowBase.Add(L ? L->Intensity : 0.f);
	}
	BuildControls();
	if (USoundBase* Alarm = SndMasterAlarm.LoadSynchronous())
	{
		AlarmAudio = NewObject<UAudioComponent>(this);
		AlarmAudio->SetSound(Alarm);
		AlarmAudio->bAutoActivate = false;
		AlarmAudio->SetVolumeMultiplier(0.35f);
		AlarmAudio->SetupAttachment(RootComponent);
		AlarmAudio->RegisterComponent();
	}
	// legacy key bindings like the elevator / ingress; not consumed so the other actors keep their input
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		EnableInput(PC);
		if (InputComponent)
		{
			auto Bind = [this](const FKey& Key, EInputEvent Ev, void (ACMPanel::*Fn)())
			{
				FInputKeyBinding& B = InputComponent->BindKey(Key, Ev, this, Fn);
				B.bConsumeInput = false;
			};
			Bind(EKeys::LeftMouseButton, IE_Pressed, &ACMPanel::OnLeftPressed);
			Bind(EKeys::LeftMouseButton, IE_Released, &ACMPanel::OnLeftReleased);
			Bind(EKeys::RightMouseButton, IE_Pressed, &ACMPanel::OnRightPressed);
			Bind(EKeys::MouseScrollUp, IE_Pressed, &ACMPanel::OnWheelUp);
			Bind(EKeys::MouseScrollDown, IE_Pressed, &ACMPanel::OnWheelDown);
		}
	}
}

void ACMPanel::LoadAssets()
{
	ToggleBase = LoadSoft(MeshToggleBase, TEXT("toggle base"));
	ToggleLever = LoadSoft(MeshToggleLever, TEXT("toggle lever"));
	GuardMesh = LoadSoft(MeshGuard, TEXT("guard"));
	Knob = LoadSoft(MeshKnob, TEXT("knob"));
	Thumbwheel = LoadSoft(MeshThumbwheel, TEXT("thumbwheel"));
	Valve = LoadSoft(MeshValve, TEXT("valve"));
	BreakerCollar = LoadSoft(MeshBreakerCollar, TEXT("breaker collar"));
	BreakerKnob = LoadSoft(MeshBreakerKnob, TEXT("breaker knob"));
	Cube = LoadSoft(MeshCube, TEXT("cube"));
	Cylinder = LoadSoft(MeshCylinder, TEXT("cylinder"));
	Plane = LoadSoft(MeshPlane, TEXT("plane"));
	Paint = LoadSoft(MatPaint, TEXT("paint"));
	LabelMat = LoadSoft(MatLabel, TEXT("label"));
	LensMat = LoadSoft(MatLens, TEXT("lens"));
	StripesMat = LoadSoft(MatStripes, TEXT("stripes"));
	DialMat = LoadSoft(MatDial, TEXT("dial"));
	DialGMeter = LoadSoft(TexDialGMeter, TEXT("G dial"));
	DialAlphaPc = LoadSoft(TexDialAlphaPc, TEXT("alpha/Pc dial"));
	Toggle1 = SndToggle1.LoadSynchronous();
	Toggle2 = SndToggle2.LoadSynchronous();
	GuardOpen = SndGuardOpen.LoadSynchronous();
	GuardClose = SndGuardClose.LoadSynchronous();
	BreakerPop = SndBreakerPop.LoadSynchronous();
	BreakerPush = SndBreakerPush.LoadSynchronous();
	Detent = SndDetent.LoadSynchronous();
	ButtonPress = SndButtonPress.LoadSynchronous();
	ButtonRelease = SndButtonRelease.LoadSynchronous();
	ThumbwheelTick = SndThumbwheel.LoadSynchronous();
}

FTransform ACMPanel::GetFrame() const
{
	return Ingress ? Ingress->GetActorTransform() : GetActorTransform();
}

void ACMPanel::BuildControls()
{
	if (bBuilt)
	{
		return;
	}
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / DataFile;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogCMPanel, Error, TEXT("CM panel: cannot read %s"), *Path);
		return;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogCMPanel, Error, TEXT("CM panel: %s is not valid JSON"), *Path);
		return;
	}
	if (!Paint || !Cube || !Plane || !LensMat || !LabelMat || !StripesMat)
	{
		UE_LOG(LogCMPanel, Error, TEXT("CM panel: core assets missing - run Tools/Realism/build_cm_controls.py"));
		return;
	}
	TMap<FString, int32> Counts;
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("controls")))
	{
		const TSharedPtr<FJsonObject> J = V->AsObject();
		const FString Type = J->GetStringField(TEXT("type"));
		UClass* Cls = Type == TEXT("toggle") ? UCMToggle::StaticClass()
			: Type == TEXT("rotary") ? UCMRotary::StaticClass()
			: Type == TEXT("pushbutton") ? UCMPushButton::StaticClass()
			: Type == TEXT("breaker") ? UCMBreaker::StaticClass()
			: Type == TEXT("light") ? UCMLight::StaticClass()
			: Type == TEXT("talkback") ? UCMTalkback::StaticClass()
			: Type == TEXT("gauge") ? UCMGauge::StaticClass() : nullptr;
		if (!Cls)
		{
			continue;
		}
		UCMControl* C = NewObject<UCMControl>(this, Cls, FName(*J->GetStringField(TEXT("id"))));
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		C->Init(this, J);
		Controls.Add(C);
		ById.Add(C->ControlId, C);
		Counts.FindOrAdd(Type)++;
	}
	bBuilt = true;
	FString Summary;
	for (const TPair<FString, int32>& P : Counts)
	{
		Summary += FString::Printf(TEXT(" %s %d"), *P.Key, P.Value);
	}
	UE_LOG(LogCMPanel, Log, TEXT("CM panel: %d controls built from %s (%s )"), Controls.Num(), *DataFile, *Summary);
	EvaluateLogic(0.f);
	ApplyLights();
}

UCMControl* ACMPanel::FindControl(FName Id) const
{
	const TObjectPtr<UCMControl>* C = ById.Find(Id);
	return C ? C->Get() : nullptr;
}

bool ACMPanel::IsSeated() const
{
	return Ingress && Ingress->GetIngressState() == ECMIngressState::Seated;
}

bool ACMPanel::ViewRay(FVector& Origin, FVector& Dir) const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	Origin = PC->PlayerCameraManager->GetCameraLocation();
	Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();
	return true;
}

UCMControl* ACMPanel::Pick(const FVector& O, const FVector& D, float& OutUp, bool bExactOnly) const
{
	// what you see is what you click: each control's clickable area is its own visible parts (UCMControl::RayHit), and the
	// first one the view ray meets wins. Exact drawn geometry first; only if the ray meets no control exactly do the parts
	// count grown by a few millimetres (so the aim dot just beside a thin lever still takes it, never a neighbour's part
	// that is actually under the dot). (Until 2026-10-17 every control had a fixed-size box and the box whose centre was
	// nearest the ray, relative to its size, won: at the seats' slant view a neighbour's box often won.)
	for (int32 Pass = 0; Pass < (bExactOnly ? 1 : 2); ++Pass)
	{
		UCMControl* Best = nullptr;
		float BestT = MaxPickDistance;
		for (UCMControl* C : Controls)
		{
			if (!C) continue;
			float T, Up;
			const bool bHit = Pass == 0 ? C->RayHit(O, D, T, Up) : C->RayHit(O, D, T, Up, UCMControl::PickMargin, UCMControl::PickMinHalf);
			if (bHit && T < BestT)
			{
				BestT = T;
				OutUp = Up;
				Best = C;
			}
		}
		if (Best)
		{
			return Best;
		}
	}
	return nullptr;
}

void ACMPanel::UpdateAim()
{
	UCMControl* Best = nullptr;
	float BestUp = 0.f;
	FVector O, D;
	if (IsSeated() && ViewRay(O, D))
	{
		Best = Pick(O, D, BestUp);
	}
	if (Best != Aimed)
	{
		if (Aimed)
		{
			Aimed->SetHighlighted(false);
		}
		if (Best)
		{
			Best->SetHighlighted(true);
		}
		Aimed = Best;
	}
	AimUp = BestUp;
}

FName ACMPanel::GetAimedControl() const
{
	return Aimed ? Aimed->ControlId : NAME_None;
}

bool ACMPanel::GetAimedPose(FVector& OutLocation, FVector& OutNormal, FName& OutId) const
{
	if (!Aimed || !Aimed->IsOperable() || !IsSeated())
	{
		return false;
	}
	OutNormal = Aimed->GetForwardVector();
	OutLocation = Aimed->GetVisibleCenter();
	OutId = Aimed->ControlId;
	return true;
}

FVector ACMPanel::GetControlVisibleCenter(FName Id) const
{
	const UCMControl* C = FindControl(Id);
	return C ? C->GetVisibleCenter() : FVector::ZeroVector;
}

bool ACMPanel::AimAtControl(FName Id, float UpOffset)
{
	const UCMControl* C = FindControl(Id);
	if (!C || !IsSeated() || Ingress->IsSwitchingSeat())
	{
		return false;
	}
	// relative to the relaxed view of whichever couch the player is in; aim at the control's visible parts
	const FVector Cam = Ingress->GetSeatViewLocation();
	const FQuat Base = Ingress->GetSeatBaseQuat();
	const FVector4& L = Ingress->GetSeat(Ingress->GetSeatIndex()).Limits;
	// A large guard/base can be occluded from a slanted couch. Aim at a visible part
	// that the real picker actually resolves to this control, just as TestControl does.
	for (const FVector& Center : C->GetVisiblePartCenters())
	{
		const FVector Target = Center + C->GetUpVector() * UpOffset;
		const FVector Dir = (Target - Cam).GetSafeNormal();
		const FRotator Want = Base.UnrotateVector(Dir).Rotation();
		if (Want.Yaw < -L.X || Want.Yaw > L.Y || Want.Pitch < -L.W || Want.Pitch > L.Z) continue;
		float Up = 0.f;
		if (Pick(Cam, Dir, Up) != C) continue;
		const FVector2D Cur = Ingress->GetSeatLook();
		Ingress->AddSeatLook(Want.Yaw - Cur.X, Want.Pitch - Cur.Y);
		const FVector2D Got = Ingress->GetSeatLook();
		return FMath::Abs(Got.X - Want.Yaw) < 0.2f && FMath::Abs(Got.Y - Want.Pitch) < 0.2f;
	}
	return false;
}

bool ACMPanel::InputAction(int32 Action)
{
	if (!IsSeated())
	{
		return false;
	}
	switch (Action)
	{
	case 0:
		if (!Aimed)
		{
			return false;
		}
		if (!Aimed->IsOperable())
		{
			SetHint(TEXT("Indicator - nothing to operate"), 1.f);
			return false;
		}
		Pressed = Aimed;
		return Aimed->Primary(AimUp);
	case 1:
		if (Pressed)
		{
			Pressed->PrimaryRelease();
			Pressed = nullptr;
			return true;
		}
		return false;
	case 2:
		return Aimed && Aimed->IsOperable() && Aimed->Secondary();
	case 3:
	case 4:
		return Aimed && Aimed->IsOperable() && Aimed->Wheel(Action == 3 ? 1 : -1);
	default:
		return false;
	}
}

FString ACMPanel::TestControl(FName Id)
{
	UCMControl* C = FindControl(Id);
	if (!C)
	{
		return TEXT("MISSING");
	}
	if (!IsSeated() || Ingress->IsSwitchingSeat())
	{
		return TEXT("NOT_SEATED");
	}
	// the ray a player sends with the aim dot on the middle of one of this control's visible parts (largest part first; a part
	// hidden behind a neighbour from this couch is skipped), from this couch's eye
	const FVector Eye = Ingress->GetSeatViewLocation();
	const FVector4& L = Ingress->GetSeat(Ingress->GetSeatIndex()).Limits;
	FString First;
	UCMControl* Hit = nullptr;
	float Up = 0.f;
	FVector D = FVector::ForwardVector;
	for (const FVector& Target : C->GetVisiblePartCenters())
	{
		const FRotator Want = Ingress->GetSeatBaseQuat().UnrotateVector(Target - Eye).Rotation();
		FString R;
		if (Want.Yaw < -L.X || Want.Yaw > L.Y || Want.Pitch < -L.W || Want.Pitch > L.Z)
		{
			R = TEXT("OUT_OF_VIEW");
		}
		else
		{
			D = (Target - Eye).GetSafeNormal();
			Hit = Pick(Eye, D, Up);
			if (!Hit)
			{
				R = TEXT("NO_HIT");
			}
			else if (Hit != C)
			{
				// a real occlusion (the other control's drawn geometry is in front of this one's) or a mis-pick?
				float Tt = 0.f, Th = 0.f, U2 = 0.f;
				const bool bT = C->RayHit(Eye, D, Tt, U2);
				const bool bH = Hit->RayHit(Eye, D, Th, U2);
				R = FString::Printf(TEXT("%s %s"), (bH && (!bT || Th <= Tt)) ? TEXT("OCCLUDED") : TEXT("MISPICK"), *Hit->ControlId.ToString());
			}
		}
		if (R.IsEmpty())
		{
			break;                                                 // this part is under the dot and picks the control
		}
		Hit = nullptr;
		if (First.IsEmpty() || R.StartsWith(TEXT("MISPICK")))
		{
			First = R;                                             // report a mis-pick in preference to anything else
		}
		if (R.StartsWith(TEXT("MISPICK")))
		{
			return R;
		}
	}
	if (!Hit)
	{
		return First.IsEmpty() ? TEXT("NO_HIT") : First;
	}
	if (!C->IsOperable())
	{
		return TEXT("PASS");                                       // an indicator: the aim shows its label and state
	}
	// operate it through the mouse path and check that exactly this control responded; then put everything back
	TArray<uint32> Before;
	for (const UCMControl* X : Controls)
	{
		Before.Add(X->Snapshot());
	}
	UCMControl* const OldAimed = Aimed;
	const float OldUp = AimUp;
	Aimed = C;
	AimUp = Up;
	const UCMToggle* T = Cast<UCMToggle>(C);
	const UCMPushButton* B = Cast<UCMPushButton>(C);
	if ((T && T->IsGuarded() && !T->IsGuardOpen()) || (B && B->Guard != ECMGuard::None && !B->bGuardOpen))
	{
		InputAction(2);                                            // right click lifts the guard
	}
	else if (C->IsA<UCMRotary>())
	{
		if (!InputAction(3))
		{
			InputAction(4);
		}
	}
	else
	{
		InputAction(0);
		InputAction(1);
	}
	TArray<FString> Changed;
	for (int32 i = 0; i < Controls.Num(); ++i)
	{
		if (Controls[i]->Snapshot() != Before[i])
		{
			Changed.Add(Controls[i]->ControlId.ToString());
		}
		Controls[i]->Restore(Before[i]);
	}
	Pressed = nullptr;
	Aimed = OldAimed;
	AimUp = OldUp;
	EvaluateLogic(0.f);
	ApplyLights();
	if (Changed.Num() == 1 && Changed[0] == Id.ToString())
	{
		return TEXT("PASS");
	}
	return Changed.Num() == 0 ? TEXT("NO_RESPONSE") : TEXT("WRONG_RESPONSE ") + FString::Join(Changed, TEXT(","));
}

void ACMPanel::ResetControls()
{
	for (UCMControl* C : Controls)
	{
		C->Restore(uint32(C->LaunchState));
	}
	Pressed = nullptr;
	bMasterAlarm = false;
	CautionsOn.Reset();
	CauseTimers.Reset();
	EvaluateLogic(0.f);
	ApplyLights();
	UE_LOG(LogCMPanel, Log, TEXT("CM panel: all controls back to the launch configuration"));
}

int32 ACMPanel::GetControlState(FName Id) const
{
	const UCMControl* C = FindControl(Id);
	return C ? C->State : -1;
}

FString ACMPanel::GetControlPosition(FName Id) const
{
	const UCMControl* C = FindControl(Id);
	return C ? C->GetPositionName() : FString();
}

bool ACMPanel::GetControlFlag(FName Id) const
{
	const UCMControl* C = FindControl(Id);
	if (const UCMLight* L = Cast<UCMLight>(C))
	{
		return L->IsLit();
	}
	if (const UCMTalkback* T = Cast<UCMTalkback>(C))
	{
		return T->IsBarberPole();
	}
	if (const UCMToggle* G = Cast<UCMToggle>(C))
	{
		return G->IsGuardOpen();
	}
	if (const UCMPushButton* B = Cast<UCMPushButton>(C))
	{
		return B->IsLit();
	}
	return false;
}

TArray<FName> ACMPanel::GetControlIds(const FString& Type) const
{
	TArray<FName> Out;
	for (const UCMControl* C : Controls)
	{
		const bool bMatch = (Type == TEXT("toggle") && C->IsA<UCMToggle>() && !Cast<UCMToggle>(C)->IsGuarded())
			|| (Type == TEXT("guarded") && C->IsA<UCMToggle>() && Cast<UCMToggle>(C)->IsGuarded())
			|| (Type == TEXT("rotary") && C->IsA<UCMRotary>()) || (Type == TEXT("pushbutton") && C->IsA<UCMPushButton>())
			|| (Type == TEXT("breaker") && C->IsA<UCMBreaker>()) || (Type == TEXT("light") && C->IsA<UCMLight>())
			|| (Type == TEXT("talkback") && C->IsA<UCMTalkback>()) || (Type == TEXT("gauge") && C->IsA<UCMGauge>());
		if (bMatch)
		{
			Out.Add(C->ControlId);
		}
	}
	return Out;
}

void ACMPanel::GetAimText(FString& OutLabel, FString& OutHint) const
{
	OutLabel = Aimed ? Aimed->GetStatusText() : FString();
	OutHint = HintTime > 0.f ? Hint : (Aimed ? Aimed->GetHintText() : FString());
}

void ACMPanel::SetHint(const FString& InHint, float Seconds)
{
	Hint = InHint;
	HintTime = Seconds;
}

void ACMPanel::PlayControlSound(USoundBase* Sound, const FVector& Location, float Volume) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, SoundVolume * Volume, FMath::FRandRange(0.96f, 1.04f));
	}
}

void ACMPanel::OnControlChanged(UCMControl* Control, int32 OldState)
{
	UE_LOG(LogCMPanel, Log, TEXT("CM control %s: %s"), *Control->ControlId.ToString(), *Control->GetStatusText());
	if (const UCMToggle* T = Cast<UCMToggle>(Control))
	{
		// SM RCS helium valve switches: momentary OPEN / CLOSE drives the valve talkback (gray = open, barber pole = closed)
		if (!T->Talkback.IsNone())
		{
			if (UCMTalkback* TB = Cast<UCMTalkback>(FindControl(T->Talkback)))
			{
				if (T->State == 0)
				{
					TB->SetBarberPole(false);
				}
				else if (T->State == T->Positions.Num() - 1)
				{
					TB->SetBarberPole(true);
				}
			}
		}
	}
	if (Control->ControlId == FName(TEXT("P1_MASTER_ALARM")) && OldState == -1 && bMasterAlarm)
	{
		bMasterAlarm = false;                       // pressing the lit MASTER ALARM button silences and extinguishes it
		UE_LOG(LogCMPanel, Log, TEXT("CM panel: master alarm reset"));
	}
	if (const UCMRotary* R = Cast<UCMRotary>(Control))
	{
		if (!R->LightChannel.IsEmpty())
		{
			ApplyLights();
		}
	}
	if (Control->ControlId == FName(TEXT("P8_FLOOD_FIXED")))
	{
		ApplyLights();
	}
}

void ACMPanel::ApplyLights()
{
	// FLOOD knob: cabin floodlights (launch setting 4 = the tuned 55 lm), FIXED = full bright; INTEGRAL knob: panel glow
	const int32 Flood = GetControlState(TEXT("P8_LT_FLOOD"));
	const bool bFixed = GetControlState(TEXT("P8_FLOOD_FIXED")) == 0;
	const int32 Integral = GetControlState(TEXT("P8_LT_INTEGRAL"));
	const float FloodScale = bFixed ? 2.2f : (Flood < 0 ? 1.f : float(Flood) / 4.f);
	const float GlowScale = Integral < 0 ? 1.f : float(Integral) / 5.f;
	for (int32 i = 0; i < FloodLights.Num(); ++i)
	{
		if (ULightComponent* L = FloodLights[i] ? FloodLights[i]->FindComponentByClass<ULightComponent>() : nullptr)
		{
			L->SetIntensity(FloodBase[i] * FloodScale);
		}
	}
	for (int32 i = 0; i < PanelGlowLights.Num(); ++i)
	{
		if (ULightComponent* L = PanelGlowLights[i] ? PanelGlowLights[i]->FindComponentByClass<ULightComponent>() : nullptr)
		{
			L->SetIntensity(GlowBase[i] * GlowScale);
		}
	}
}

void ACMPanel::EvaluateLogic(float Dt)
{
	auto St = [this](const TCHAR* Id) { return GetControlState(FName(Id)); };
	const bool bPower = St(TEXT("P2_CW_POWER")) != 1;               // C/W POWER 1 / 2 (OFF = centre)
	const int32 Test = St(TEXT("P2_CW_LAMP_TEST"));                  // 0 = test 1 (left half), 2 = test 2 (right half)
	const bool bCM = St(TEXT("P2_CW_CSM_CM")) == 1;
	const bool bAck = St(TEXT("P2_CW_NORMAL")) == 2;

	// caution causes (simple models): BMAG unpowered -> its temperature goes out of limits; both helium isolation valves of a
	// quad closed -> the quad's manifold pressure decays
	TMap<FName, bool> Cause;
	Cause.Add(TEXT("CW_L11"), St(TEXT("P7_BMAG_PWR1")) == 0);
	Cause.Add(TEXT("CW_L12"), St(TEXT("P7_BMAG_PWR2")) == 0);
	const TCHAR* Quads[] = {TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D")};
	for (int32 q = 0; q < 4; ++q)
	{
		const bool bHe1 = GetControlFlag(FName(*FString::Printf(TEXT("TB_SM_RCS_HE1_%s"), Quads[q])));
		const bool bHe2 = GetControlFlag(FName(*FString::Printf(TEXT("TB_SM_RCS_HE2_%s"), Quads[q])));
		Cause.Add(FName(*FString::Printf(TEXT("CW_L5%d"), q + 1)), bHe1 && bHe2);
	}
	bool bNewCaution = false;
	for (const TPair<FName, bool>& P : Cause)
	{
		float& Timer = CauseTimers.FindOrAdd(P.Key);
		Timer = P.Value ? FMath::Min(Timer + Dt, CautionDelay + 1.f) : FMath::Max(Timer - Dt * 3.f, 0.f);
		bool bInhibited = false;
		for (const TCHAR* S : SMLamps)
		{
			bInhibited |= bCM && P.Key == FName(S);
		}
		const bool bWasOn = CautionsOn.Contains(P.Key);
		const bool bOn = !bInhibited && (bWasOn ? Timer > 0.f : Timer >= CautionDelay);
		if (bOn && !bWasOn)
		{
			CautionsOn.Add(P.Key);
			bNewCaution = true;
			UE_LOG(LogCMPanel, Log, TEXT("CM panel: caution %s"), *P.Key.ToString());
		}
		else if (!bOn && bWasOn)
		{
			CautionsOn.Remove(P.Key);
		}
	}
	if (bNewCaution && bPower && !bAck)
	{
		bMasterAlarm = true;
	}
	if (!bPower)
	{
		bMasterAlarm = false;
	}
	for (UCMControl* C : Controls)
	{
		if (UCMLight* L = Cast<UCMLight>(C))
		{
			if (!L->CWGroup.IsEmpty())
			{
				const bool bTest = (Test == 0 && L->CWGroup == TEXT("L")) || (Test == 2 && L->CWGroup == TEXT("R"));
				L->SetLit(bPower && (bTest || CautionsOn.Contains(L->ControlId)));
			}
		}
	}
	if (UCMPushButton* MA = Cast<UCMPushButton>(FindControl(TEXT("P1_MASTER_ALARM"))))
	{
		MA->SetLit(bMasterAlarm || (bPower && Test != 1));
	}
	if (AlarmAudio)
	{
		if (bMasterAlarm && !AlarmAudio->IsPlaying())
		{
			AlarmAudio->Play();
		}
		else if (!bMasterAlarm && AlarmAudio->IsPlaying())
		{
			AlarmAudio->Stop();
		}
	}
	// LV engine lights: lit before ignition while the EDS is powered (EDS POWER on and an EDS breaker closed)
	const bool bEDS = St(TEXT("P7_EDS_PWR")) == 0 && (St(TEXT("CB8_EDS_1_BAT_A")) == 0 || St(TEXT("CB8_EDS_2_BAT_C")) == 0 ||
	                                                 St(TEXT("CB8_EDS_3_BAT_B")) == 0);
	for (const TCHAR* Id : {TEXT("P1_LT_LV_ENG1"), TEXT("P1_LT_LV_ENG2"), TEXT("P1_LT_LV_ENG3"), TEXT("P1_LT_LV_ENG4"), TEXT("P1_LT_LV_ENG5")})
	{
		if (UCMLight* L = Cast<UCMLight>(FindControl(Id)))
		{
			L->SetLit(bEDS);
		}
	}
}

void ACMPanel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	HintTime = FMath::Max(HintTime - Dt, 0.f);
	UpdateAim();
	if (Pressed && !IsSeated())
	{
		Pressed->PrimaryRelease();
		Pressed = nullptr;
	}
	EvaluateLogic(Dt);
	for (UCMControl* C : Controls)
	{
		C->Animate(Dt);
	}
}
