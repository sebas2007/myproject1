// Chest-mounted body-camera look for the first-person character.

#include "Apollo11/BodycamComponent.h"
#include "Apollo11/CMIngress.h"
#include "Apollo11/CMPanel.h"
#include "Apollo11/TowerServiceCabinet.h"
#include "Apollo11/CloseoutInspection.h"
#include "Apollo11/BlockoutElevator.h"
#include "Apollo11/MissionMenu.h"
#include "Apollo11/GeminiCabinPreview.h"
#include "Apollo11/GeminiErector.h"
#include "Apollo11/GeminiTechnician.h"
#include "EngineUtils.h"
#include "MyProject1Character.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogBodycam, Log, All);

// "Bodycam" toggles, "Bodycam 0" / "Bodycam 1" switch off / on (every bodycam in the calling world)
static FAutoConsoleCommandWithWorldAndArgs GBodycamCommand(
	TEXT("Bodycam"),
	TEXT("Bodycam [0|1]: switch the chest body-camera look off/on (no argument toggles)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		for (TObjectIterator<UBodycamComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && !It->IsTemplate())
			{
				if (Args.Num() > 0)
				{
					It->SetBodycamEnabled(FCString::Atoi(*Args[0]) != 0);
				}
				else
				{
					It->ToggleBodycam();
				}
			}
		}
	}));

// the seated breathing motion moves the aim dot by a few millimetres; the automated pick checks switch it off
static TAutoConsoleVariable<int32> CVarSeatBreathing(TEXT("a11.SeatBreathing"), 1,
	TEXT("1: subtle breathing motion of the seated view (default); 0: still (automated play-tests)"));

UBodycamComponent::UBodycamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;     // after input has moved the control rotation
}

void UBodycamComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AMyProject1Character* Char = Cast<AMyProject1Character>(GetOwner()))
	{
		Camera = Char->GetFirstPersonCameraComponent();
	}
	if (!Camera)
	{
		Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
	}
	if (!Camera)
	{
		UE_LOG(LogBodycam, Warning, TEXT("Bodycam: no camera on %s"), *GetNameSafe(GetOwner()));
		return;
	}
	OrigParent = Camera->GetAttachParent();
	OrigSocket = Camera->GetAttachSocketName();
	OrigRelative = Camera->GetRelativeTransform();
	bOrigPawnControlRotation = Camera->bUsePawnControlRotation;
	bOrigFirstPersonFOV = Camera->bEnableFirstPersonFieldOfView;
	OrigFOV = Camera->FieldOfView;
	if (UMaterialInterface* Mat = Cast<UMaterialInterface>(PostMaterial.TryLoad()))
	{
		PostMID = UMaterialInstanceDynamic::Create(Mat, this);
	}
	else
	{
		UE_LOG(LogBodycam, Warning, TEXT("Bodycam: post material %s not found; lens/sensor effects off"), *PostMaterial.ToString());
	}
	SetBodycamEnabled(bStartEnabled);
}

void UBodycamComponent::ToggleBodycam()
{
	SetBodycamEnabled(!bEnabled);
}

void UBodycamComponent::SetBodycamEnabled(bool bEnable)
{
	if (!Camera)
	{
		return;
	}
	bEnabled = bEnable;
	Apply(bEnable);
	UE_LOG(LogBodycam, Log, TEXT("Bodycam %s"), bEnable ? TEXT("ON") : TEXT("OFF"));
}

void UBodycamComponent::Apply(bool bEnable)
{
	AMyProject1Character* Char = Cast<AMyProject1Character>(GetOwner());
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	if (bEnable)
	{
		if (Char)
		{
			Camera->AttachToComponent(Char->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			Char->GetFirstPersonMesh()->SetVisibility(false);    // template arms don't fit a chest camera
		}
		Camera->SetRelativeLocation(ChestOffset);
		Camera->bUsePawnControlRotation = false;               // rotation is driven in UpdateCamera
		Camera->bEnableFirstPersonFieldOfView = false;
		Camera->SetFieldOfView(FieldOfView);
		bHaveSmoothed = false;

		PP = FPostProcessSettings();
		// slow camcorder auto exposure: metering for the midtones, highlights roll off hard, shadows crush.
		// Tuned 2026-10-17 from rendered captures: +0.4 EV and local exposure off blew the searchlit rocket and the
		// White Room to white, and the hard toe crushed the twilight sky to black.
		PP.bOverride_AutoExposureSpeedUp = true;            PP.AutoExposureSpeedUp = 0.3f;   // dark to bright environment: slow, camcorder-like
		PP.bOverride_AutoExposureSpeedDown = true;          PP.AutoExposureSpeedDown = 3.0f; // bright to dark environment: fast, so the view after the White Room recovers in ~1 s
		PP.bOverride_AutoExposureLowPercent = true;         PP.AutoExposureLowPercent = 60.f;
		PP.bOverride_AutoExposureHighPercent = true;        PP.AutoExposureHighPercent = 97.f;
		PP.bOverride_AutoExposureBias = true;               PP.AutoExposureBias = ExposureBias;
		PP.bOverride_LocalExposureHighlightContrastScale = true; PP.LocalExposureHighlightContrastScale = 0.6f;
		PP.bOverride_LocalExposureShadowContrastScale = true;    PP.LocalExposureShadowContrastScale = 0.9f;
		// Per-world daylight override. Never mutate component/class defaults or
		// a shared volume/material; fresh Apollo worlds retain the profile above.
		if (GetWorld() && GetWorld()->GetMapName().EndsWith(TEXT("LVL_Gemini1_Prototype")))
		{
			PP.bOverride_AutoExposureMinBrightness = true; PP.AutoExposureMinBrightness = 6.f;
			PP.bOverride_AutoExposureMaxBrightness = true; PP.AutoExposureMaxBrightness = 16.f;
			PP.AutoExposureBias = -.65f;
			PP.AutoExposureSpeedUp = 3.f; PP.AutoExposureSpeedDown = 2.f;
			PP.AutoExposureLowPercent = 70.f; PP.AutoExposureHighPercent = 95.f;
		}
		PP.bOverride_FilmToe = true;                        PP.FilmToe = 0.56f;
		PP.bOverride_FilmShoulder = true;                   PP.FilmShoulder = 0.18f;
		PP.bOverride_ColorSaturation = true;                PP.ColorSaturation = FVector4(0.86, 0.86, 0.86, 1.0);
		// lens: dirt and flares on the searchlights, vignette
		PP.bOverride_BloomIntensity = true;                 PP.BloomIntensity = 0.6f;
		PP.bOverride_BloomThreshold = true;                 PP.BloomThreshold = 1.5f;
		if (UTexture* Dirt = Cast<UTexture>(LensDirt.TryLoad()))
		{
			PP.bOverride_BloomDirtMask = true;              PP.BloomDirtMask = Dirt;
			PP.bOverride_BloomDirtMaskIntensity = true;     PP.BloomDirtMaskIntensity = 2.5f;
		}
		PP.bOverride_LensFlareIntensity = true;             PP.LensFlareIntensity = 0.3f;
		PP.bOverride_VignetteIntensity = true;              PP.VignetteIntensity = 0.25f;
		// 30 fps body camera shutter: visible blur on fast turns
		PP.bOverride_MotionBlurAmount = true;               PP.MotionBlurAmount = 0.75f;
		PP.bOverride_MotionBlurMax = true;                  PP.MotionBlurMax = 4.0f;
		PP.bOverride_MotionBlurTargetFPS = true;            PP.MotionBlurTargetFPS = 30;
		if (PostMID)
		{
			PP.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, PostMID));
		}
		Camera->PostProcessBlendWeight = 1.f;
	}
	else
	{
		if (OrigParent.IsValid())
		{
			Camera->AttachToComponent(OrigParent.Get(), FAttachmentTransformRules::KeepRelativeTransform, OrigSocket);
		}
		Camera->SetRelativeTransform(OrigRelative);
		Camera->bUsePawnControlRotation = bOrigPawnControlRotation;
		Camera->bEnableFirstPersonFieldOfView = bOrigFirstPersonFOV;
		Camera->SetFieldOfView(OrigFOV);
		Camera->PostProcessBlendWeight = 0.f;
		PP = FPostProcessSettings();
		if (Char)
		{
			Char->GetFirstPersonMesh()->SetVisibility(true);
		}
	}
	ApplyViewProfile();
}

void UBodycamComponent::ApplyViewProfile()
{
	if (!Camera) return;
	// The capsule is viewed through a helmet, with no body-camera image warp.
	// Rebuild the outdoor profile when leaving so F8 and ingress never lose the user's choice.
	if (!bViewOverride) return;
	Camera->bEnableFirstPersonFieldOfView = false;
	Camera->SetFieldOfView(85.f);
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP = FPostProcessSettings();
	PP.bOverride_SceneFringeIntensity = true; PP.SceneFringeIntensity = 0.f;
	PP.bOverride_FilmGrainIntensity = true; PP.FilmGrainIntensity = 0.008f;
	PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.12f;
	PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 0.15f;
	PP.bOverride_LensFlareIntensity = true; PP.LensFlareIntensity = 0.f;
	PP.bOverride_MotionBlurAmount = true; PP.MotionBlurAmount = 0.12f;
	PP.bOverride_AutoExposureSpeedUp = true; PP.AutoExposureSpeedUp = 2.f;
	PP.bOverride_AutoExposureSpeedDown = true; PP.AutoExposureSpeedDown = 3.f;
	Camera->PostProcessBlendWeight = 1.f;
}

void UBodycamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->IsLocalController())
	{
		if (!bHudSet && !Cast<ABodycamHUD>(PC->GetHUD()))
		{
			PC->ClientSetHUD(ABodycamHUD::StaticClass());
			bHudSet = true;
		}
		if (PC->PlayerCameraManager)                            // vertical look limit (also with the bodycam off)
		{
			PC->PlayerCameraManager->ViewPitchMin = -MaxLookPitch;
			PC->PlayerCameraManager->ViewPitchMax = MaxLookPitch;
		}
		if (!bInputBound && PC->InputComponent)
		{
			PC->InputComponent->BindKey(EKeys::F8, IE_Pressed, this, &UBodycamComponent::ToggleBodycam);
			bInputBound = true;
		}
	}
	if (bEnabled && Camera)
	{
		UpdateCamera(FMath::Min(DeltaTime, 0.1f));
	}
	else if (bViewOverride && Camera)
	{
		// plain first-person camera following the scripted view; the template arms would float, so hide them
		Camera->bUsePawnControlRotation = false;
		Camera->SetWorldLocationAndRotation(OverrideLocation, OverrideRotation);
		if (AMyProject1Character* Char = Cast<AMyProject1Character>(GetOwner()))
		{
			Char->GetFirstPersonMesh()->SetVisibility(false);
		}
	}
}

void UBodycamComponent::SetViewOverride(const FVector& WorldLocation, const FQuat& WorldRotation)
{
	const bool bEntering = !bViewOverride;
	bViewOverride = true;
	OverrideLocation = WorldLocation;
	OverrideRotation = WorldRotation;
	if (bEntering) ApplyViewProfile();
}

void UBodycamComponent::ClearViewOverride()
{
	if (!bViewOverride)
	{
		return;
	}
	bViewOverride = false;
	if (Camera)
	{
		Apply(bEnabled);
	}
}

FVector UBodycamComponent::GetViewLocation() const
{
	return Camera ? Camera->GetComponentLocation() : GetOwner()->GetActorLocation();
}

FQuat UBodycamComponent::GetViewRotation() const
{
	return Camera ? Camera->GetComponentQuat() : GetOwner()->GetActorQuat();
}

void UBodycamComponent::UpdateCamera(float Dt)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || Dt <= 0.f)
	{
		return;
	}
	Clock += Dt;
	const FRotator Control = Pawn->GetControlRotation();
	// control pitch is stored as 0..360: normalize it before use (scaling the raw value turned 10 deg down into 62),
	// and keep it 1:1 with the input so vertical look has the same sensitivity as yaw
	const float LookPitch = FMath::Clamp(FRotator::NormalizeAxis(Control.Pitch), -MaxLookPitch, MaxLookPitch);

	// inertia: the chest-mounted camera lags behind fast turns and overshoots slightly
	const FQuat Target = bViewOverride ? OverrideRotation
		: FRotator(FMath::Clamp(LookPitch + MountPitch, -MaxLookPitch, MaxLookPitch), Control.Yaw, 0.f).Quaternion();
	if (!bHaveSmoothed)
	{
		SmoothedRot = Target;
		LastYaw = Control.Yaw;
		bHaveSmoothed = true;
	}
	SmoothedRot = FQuat::Slerp(SmoothedRot, Target, 1.f - FMath::Exp(-RotationLagSpeed * Dt));
	const FRotator Smoothed = SmoothedRot.Rotator();
	const float RawRate = FMath::FindDeltaAngleDegrees(LastYaw, Smoothed.Yaw) / Dt;
	YawRate = FMath::Lerp(YawRate, RawRate, 1.f - FMath::Exp(-20.f * Dt));
	LastYaw = Smoothed.Yaw;

	// walking bob and sway from the ground speed, a dip on landing
	float Speed = 0.f;
	bool bFalling = false;
	if (const ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		Speed = Char->GetVelocity().Size2D();
		bFalling = Char->GetCharacterMovement() && Char->GetCharacterMovement()->IsFalling();
	}
	if (bWasFalling && !bFalling)
	{
		LandDip = 4.f;
	}
	bWasFalling = bFalling;
	LandDip = FMath::FInterpTo(LandDip, 0.f, Dt, 6.f);
	// stride from the character (shared with its footsteps); amplitude 1 at the normal 140 cm/s walk
	float Walk = bFalling ? 0.f : FMath::Clamp(Speed / 140.f, 0.f, 2.4f);
	if (const AMyProject1Character* Me = Cast<AMyProject1Character>(Pawn))
	{
		StepPhase = Me->GetStepPhase();
		Walk = Me->GetWalkIntensity();
	}
	else
	{
		StepPhase += Dt * Speed / FMath::Max(StepLength, 1.f) * PI;      // one half-cycle per step
	}
	// lowest at the foot strikes ((k + 1/2) * PI), highest at mid-stance
	const float BobZ = -BobVertical * Walk * FMath::Abs(FMath::Sin(StepPhase)) - LandDip;
	const float BobY = BobLateral * Walk * FMath::Sin(StepPhase * 0.5f);
	const float Roll = BobRollDeg * Walk * FMath::Sin(StepPhase * 0.5f);

	// handheld jitter: low-frequency noise, stronger while walking
	const float J = JitterDeg * (0.6f + 0.6f * Walk);
	const float JP = J * FMath::PerlinNoise1D(Clock * 1.7f + 11.3f) + 0.3f * J * FMath::PerlinNoise1D(Clock * 7.9f);
	const float JY = J * FMath::PerlinNoise1D(Clock * 1.3f + 47.1f) + 0.3f * J * FMath::PerlinNoise1D(Clock * 9.1f + 5.f);
	const float JR = 1.5f * J * FMath::PerlinNoise1D(Clock * 1.1f + 91.7f);
	// the camera body tilts into turns a little (chest twist)
	const float TurnLean = FMath::Clamp(YawRate * 0.012f, -3.f, 3.f);

	if (bViewOverride)
	{
		// scripted view (CM ingress / seated): the pose comes from the path, the camera keeps its lag and jitter
		// no bob, sway or jitter while seated: only a subtle breathing motion
		const float Breath = CVarSeatBreathing.GetValueOnGameThread() != 0 ? FMath::Sin(Clock * 2.0f * PI / 4.2f) : 0.f;
		const FRotator BreathRot(0.12f * Breath, 0.f, 0.05f * Breath);
		Camera->SetWorldLocationAndRotation(OverrideLocation + FVector(0.f, 0.f, 0.25f * Breath), SmoothedRot * BreathRot.Quaternion());
	}
	else
	{
		Camera->SetRelativeLocation(ChestOffset + FVector(0.f, BobY, BobZ));
		Camera->SetWorldRotation(FRotator(Smoothed.Pitch + JP - LandDip * 0.4f, Smoothed.Yaw + JY, Roll + JR + TurnLean));
	}

	if (PostMID)
	{
		PostMID->SetScalarParameterValue(TEXT("Barrel"), Barrel);
		PostMID->SetScalarParameterValue(TEXT("CA"), ChromaticAberration);
		PostMID->SetScalarParameterValue(TEXT("Grain"), Grain);
		PostMID->SetScalarParameterValue(TEXT("Sharpen"), Sharpen);
		PostMID->SetScalarParameterValue(TEXT("Compression"), Compression);
		PostMID->SetScalarParameterValue(TEXT("ShutterSkew"), FMath::Clamp(YawRate * RollingShutter, -0.03f, 0.03f));
		PostMID->SetScalarParameterValue(TEXT("Seed"), Clock);
		PostMID->SetScalarParameterValue(TEXT("Enable"), 1.f);
	}
}

// ------------------------------------------------------------------------------------------------ overlay
void ABodycamHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	// Elevator-local aiming and floor feedback. The capsule HUD and picker are unchanged.
	for (TActorIterator<ABlockoutElevator> It(GetWorld()); It; ++It)
	{
		if (!It->IsPlayerInCab()) continue;
		const float S=Canvas->ClipY/1080.f;
		const FVector2D Center(Canvas->ClipX*.5f,Canvas->ClipY*.5f);
		FCanvasTileItem Rim(Center-FVector2D(3*S,3*S),FVector2D(6*S,6*S),FLinearColor(0,0,0,.9f));
		Rim.BlendMode=SE_BLEND_Translucent; Canvas->DrawItem(Rim);
		FCanvasTileItem Dot(Center-FVector2D(1.5f*S,1.5f*S),FVector2D(3*S,3*S),It->GetAimedStop()!=INDEX_NONE ? FLinearColor(1,.8f,.35f,1) : FLinearColor::White);
		Dot.BlendMode=SE_BLEND_Translucent; Canvas->DrawItem(Dot);
		UFont* Font=GEngine->GetLargeFont();
		const FString Counter=It->GetFloorCounterText();
		float W=0,H=0; Canvas->StrLen(Font,Counter,W,H);
		const float Scale=.95f*S;
		const FVector2D P(Canvas->ClipX-W*Scale-35*S,95*S);
		FCanvasTileItem Back(P-FVector2D(12*S,8*S),FVector2D(W*Scale+24*S,H*Scale+16*S),FLinearColor(0,0,0,.7f));
		Back.BlendMode=SE_BLEND_Translucent; Canvas->DrawItem(Back);
		FCanvasTextItem Text(P,FText::FromString(Counter),Font,FLinearColor(.95f,.95f,.85f,1));
		Text.Scale=FVector2D(Scale,Scale); Text.bOutlined=true; Canvas->DrawItem(Text);
		break;
	}
	DrawPrompt();
	bool bGeminiMission=false;
	FString GeminiPrompt;
	for (TActorIterator<AGeminiAccessDoor> It(GetWorld()); It; ++It) if (!It->GetPrompt().IsEmpty()) GeminiPrompt=It->GetPrompt();
	for (TActorIterator<AGeminiErector> It(GetWorld()); It; ++It)
	{
		if (!It->GetPrompt().IsEmpty())
		{
			GeminiPrompt=It->GetPrompt();
			if (GetOwningPawn() && FVector::Dist(GetOwningPawn()->GetActorLocation(),It->ConsoleLocation)<210)
			{
				const FVector2D C(Canvas->ClipX*.5f,Canvas->ClipY*.5f);
				FCanvasTileItem Dot(C-FVector2D(2,2),FVector2D(4,4),It->GetAimedControl()>0 ? FLinearColor(1,.75f,.2f,1) : FLinearColor::White);
				Canvas->DrawItem(Dot);
			}
		}
	}
	for (TActorIterator<AGeminiTechnician> It(GetWorld()); It; ++It)
	{
		const FString P=It->GetPrompt();
		if (!P.IsEmpty()) {
			GeminiPrompt=P;
			if (It->GetAimedTask()>=0) {
				const FVector2D C(Canvas->ClipX*.5f,Canvas->ClipY*.5f);
				FCanvasTileItem Dot(C-FVector2D(2,2),FVector2D(4,4),FLinearColor(1,.75f,.2f,1)); Canvas->DrawItem(Dot);
			}
		}
	}
	if (!GeminiPrompt.IsEmpty())
	{
		const float S=Canvas->ClipY/1080.f;
		FCanvasTextItem T(FVector2D(28*S,Canvas->ClipY-125*S),FText::FromString(GeminiPrompt),GEngine->GetLargeFont(),FLinearColor::White);
		T.Scale=FVector2D(.9f*S,.9f*S); T.bOutlined=true; Canvas->DrawItem(T);
	}
	for (TActorIterator<AGeminiCabinPreview> It(GetWorld()); It; ++It)
	{
		const FString P=It->GetPrompt();
		if (!P.IsEmpty())
		{
			const float S=Canvas->ClipY/1080.f;
			FCanvasTextItem Hint(FVector2D(28*S,Canvas->ClipY-90*S),FText::FromString(P),GEngine->GetLargeFont(),FLinearColor::White);
			Hint.Scale=FVector2D(.9f*S,.9f*S); Hint.bOutlined=true; Canvas->DrawItem(Hint);
		}
	}
	for (TActorIterator<AMissionNavigation> It(GetWorld()); It; ++It)
	{
		const float S=Canvas->ClipY/1080.f;
		FCanvasTextItem Hint(FVector2D(28*S,Canvas->ClipY-45*S),FText::FromString(TEXT("M - Mission selection")),GEngine->GetLargeFont(),FLinearColor(.92f,.92f,.86f,.9f));
		Hint.Scale=FVector2D(.8f*S,.8f*S); Hint.bOutlined=true; Canvas->DrawItem(Hint);
		if (It->bGeminiPrototype)
		{
			bGeminiMission=true;
			FCanvasTextItem Label(FVector2D(28*S,92*S),FText::FromString(TEXT("GEMINI 1 / LC-19 PAD PROTOTYPE - SHARED SURROUNDINGS")),GEngine->GetLargeFont(),FLinearColor(.95f,.85f,.6f,1));
			Label.Scale=FVector2D(.85f*S,.85f*S); Label.bOutlined=true; Canvas->DrawItem(Label);
		}
		break;
	}
	DrawPanelAim();
	DrawSeatMenu();
	const APawn* Pawn = GetOwningPawn();
	const UBodycamComponent* Cam = Pawn ? Pawn->FindComponentByClass<UBodycamComponent>() : nullptr;
	if (!Cam || !Cam->IsBodycamEnabled() || Cam->IsHelmetView())
	{
		return;
	}
	const int32 Secs = (bGeminiMission ? 11*3600 : StartSeconds) + FMath::FloorToInt(GetWorld()->GetTimeSeconds());
	const FString Stamp = FString::Printf(TEXT("%s  %02d:%02d:%02d"),bGeminiMission ? TEXT("1964-04-08") : TEXT("1969-07-16"), (Secs / 3600) % 24, (Secs / 60) % 60, Secs % 60);
	if (!MonoFont)
	{
		MonoFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
	}
	UFont* Mono = MonoFont;
	UFont* Font = Mono ? Mono : GEngine->GetLargeFont();
	const float Scale = Canvas->ClipY / 1080.f * (Mono ? 1.15f : 1.5f);

	auto Line = [&](const FString& Text, float Y)
	{
		float W = 0.f, H = 0.f;
		Canvas->StrLen(Font, Text, W, H);
		FCanvasTextItem Item(FVector2D(Canvas->ClipX - W * Scale - Canvas->ClipX * 0.035f, Y), FText::FromString(Text), Font,
		                     FLinearColor(0.92f, 0.92f, 0.88f, 0.92f));
		Item.Scale = FVector2D(Scale, Scale);
		Item.bOutlined = true;
		Item.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		Canvas->DrawItem(Item);
		return H * Scale;
	};
	float Y = Canvas->ClipY * 0.04f;
	Y += Line(Stamp, Y) * 1.1f;
	Line(bGeminiMission ? TEXT("LC19  PROTOTYPE") : UnitId, Y);
	// blinking record dot, top left
	if (FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.f) < 0.6f)
	{
		FCanvasTextItem Rec(FVector2D(Canvas->ClipX * 0.035f, Canvas->ClipY * 0.04f), FText::FromString(TEXT("● REC")), Font,
		                    FLinearColor(0.9f, 0.08f, 0.06f, 0.9f));
		Rec.Scale = FVector2D(Scale, Scale);
		Rec.bOutlined = true;
		Rec.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		Canvas->DrawItem(Rec);
	}
}

void ABodycamHUD::DrawPanelAim()
{
	const ACMPanel* Panel = nullptr;
	for (TActorIterator<ACMPanel> It(GetWorld()); It; ++It)
	{
		Panel = *It;
		break;
	}
	if (!Panel || !Panel->IsSeated())
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	const FVector2D C(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	// small aim dot: 4 px white with a dark rim, readable on the light panels and in the dark lower bays
	const float R = FMath::Max(2.f * S, 1.5f);
	FCanvasTileItem Rim(C - FVector2D(R + 1.f, R + 1.f), FVector2D(2.f * R + 2.f, 2.f * R + 2.f), FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Rim.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Rim);
	FCanvasTileItem Dot(C - FVector2D(R, R), FVector2D(2.f * R, 2.f * R), FLinearColor(0.95f, 0.95f, 0.92f, 0.9f));
	Dot.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Dot);

	FString LabelText, HintText;
	Panel->GetAimText(LabelText, HintText);
	if (LabelText.IsEmpty() && HintText.IsEmpty())
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	auto Text = [&](const FString& T, float Y, float Scale, const FLinearColor& Col)
	{
		FCanvasTextItem Item(FVector2D(C.X + 16.f * S, Y), FText::FromString(T), Font, Col);
		Item.Scale = FVector2D(Scale, Scale);
		Item.bOutlined = true;
		Item.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		Canvas->DrawItem(Item);
	};
	Text(LabelText, C.Y + 10.f * S, 1.15f * S, FLinearColor(0.96f, 0.96f, 0.92f, 0.95f));
	Text(HintText, C.Y + 34.f * S, 0.9f * S, FLinearColor(0.75f, 0.78f, 0.80f, 0.85f));
}

void ABodycamHUD::DrawSeatMenu()
{
	const ACMIngress* Ing = nullptr;
	for (TActorIterator<ACMIngress> It(GetWorld()); It; ++It)
	{
		Ing = *It;
		break;
	}
	if (!Ing || Ing->GetIngressState() != ECMIngressState::Seated)
	{
		return;
	}
	const float S = Canvas->ClipY / 1080.f;
	UFont* Font = GEngine->GetMediumFont();
	auto Text = [&](const FString& T, float X, float Y, float Scale, const FLinearColor& Col, bool bCentre)
	{
		float W = 0.f, H = 0.f;
		Canvas->StrLen(Font, T, W, H);
		FCanvasTextItem Item(FVector2D(bCentre ? X - W * Scale * 0.5f : X, Y), FText::FromString(T), Font, Col);
		Item.Scale = FVector2D(Scale, Scale);
		Item.bOutlined = true;
		Item.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		Canvas->DrawItem(Item);
	};
	const int32 Cur = Ing->GetSeatIndex();
	const FCMSeat& Seat = Ing->GetSeat(Cur);
	const float Y = Canvas->ClipY * 0.93f;
	if (!Ing->IsSeatMenuOpen())
	{
		// unobtrusive: current couch + the key hint, small and dim
		Text(FString::Printf(TEXT("%s  %s     [Tab] seats"), *Seat.Code, *Seat.Astronaut.ToUpper()), Canvas->ClipX * 0.5f, Y,
		     0.8f * S, FLinearColor(0.82f, 0.84f, 0.84f, 0.55f), true);
		return;
	}
	const int32 N = Ing->GetSeatCount();
	const float Slot = 300.f * S;
	const float X0 = Canvas->ClipX * 0.5f - Slot * (N - 1) * 0.5f;
	FCanvasTileItem Back(FVector2D(X0 - Slot * 0.55f, Y - 8.f * S), FVector2D(Slot * N + Slot * 0.1f, 58.f * S), FLinearColor(0.f, 0.f, 0.f, 0.38f));
	Back.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Back);
	for (int32 i = 0; i < N; ++i)
	{
		const FCMSeat& St = Ing->GetSeat(i);
		const bool bCur = i == Cur;
		const FLinearColor Col = bCur ? FLinearColor(1.f, 0.86f, 0.45f, 0.95f) : FLinearColor(0.85f, 0.87f, 0.87f, 0.75f);
		Text(FString::Printf(TEXT("%d  %s  %s"), i + 1, *St.Code, *St.Role.ToUpper()), X0 + Slot * i, Y, 0.85f * S, Col, true);
		Text(St.Astronaut, X0 + Slot * i, Y + 24.f * S, 0.8f * S, Col, true);
	}
}

void ABodycamHUD::DrawPrompt()
{
	// interaction prompt ("Press E to enter" at the CM hatch), lower centre, with or without the bodycam look
	FString Prompt;
	for (TActorIterator<ACloseoutInspection> It(GetWorld()); It && Prompt.IsEmpty(); ++It)
	{
		Prompt = It->GetUsePrompt();
	}
	for (TActorIterator<ABlockoutElevator> It(GetWorld()); It && Prompt.IsEmpty(); ++It)
	{
		Prompt = It->GetUsePrompt();
	}
	for (TActorIterator<ACMIngress> It(GetWorld()); It && Prompt.IsEmpty(); ++It)
	{
		Prompt = It->GetPrompt();
	}
	for (TActorIterator<ATowerServiceCabinet> It(GetWorld()); It && Prompt.IsEmpty(); ++It)
	{
		Prompt = It->GetUsePrompt();
	}
	if (Prompt.IsEmpty())
	{
		return;
	}
	UFont* Font = GEngine->GetLargeFont();
	const float Scale = Canvas->ClipY / 1080.f * 1.6f;
	float W = 0.f, H = 0.f;
	Canvas->StrLen(Font, Prompt, W, H);
	FCanvasTextItem Item(FVector2D((Canvas->ClipX - W * Scale) * 0.5f, Canvas->ClipY * 0.74f), FText::FromString(Prompt), Font,
	                     FLinearColor(0.95f, 0.95f, 0.92f, 0.95f));
	Item.Scale = FVector2D(Scale, Scale);
	Item.bOutlined = true;
	Item.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	Canvas->DrawItem(Item);
}
