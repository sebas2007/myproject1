// Blockout elevator for the Launch Umbilical Tower (Apollo 11 pad, Milestone 1).

#include "Apollo11/BlockoutElevator.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlockoutElevator, Log, All);

namespace ElevatorLayout
{
	// Cab outer half-size and height (cm). The shaft walls in the level leave 5 cm clearance around it.
	constexpr float CabHX = 135.f;
	constexpr float CabHY = 145.f;
	constexpr float CabH = 250.f;
	constexpr float Wall = 5.f;
	// Door opening (bi-parting, two 60 cm leaves) and its height
	constexpr float DoorHalfW = 60.f;
	constexpr float DoorH = 215.f;
	// Landing doors sit in the +X shaft wall, which spans x = 140 ... 152
	constexpr float LandingX0 = 143.f;
	constexpr float LandingX1 = 147.f;
	constexpr float ShaftFaceX = 152.f;
}

ABlockoutElevator::ABlockoutElevator()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	Cab = CreateDefaultSubobject<USceneComponent>(TEXT("Cab"));
	Cab->SetupAttachment(Root);

	static const TCHAR* ShellNames[] = {
		TEXT("CabFloor"), TEXT("CabBack"), TEXT("CabSideL"), TEXT("CabSideR"), TEXT("CabRoof"),
		TEXT("CabFrontL"), TEXT("CabFrontR"), TEXT("CabHeader")
	};
	for (const TCHAR* Name : ShellNames)
	{
		CabShell.Add(MakeBox(Name, Cab));
	}
	CabDoorL = MakeBox(TEXT("CabDoorL"), Cab);
	CabDoorR = MakeBox(TEXT("CabDoorR"), Cab);
	CabPanel = MakeBox(TEXT("CabPanel"), Cab);

	CabTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("CabTrigger"));
	CabTrigger->SetupAttachment(Cab);
	CabTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StopLevels={0.f,320.f,30.f,60.f,80.f,100.f,120.f,140.f,160.f,180.f,200.f,220.f,240.f,260.f,280.f,300.f,340.f,-12.5f};
	CabIndicator=CreateDefaultSubobject<UTextRenderComponent>(TEXT("CabFloorIndicator"));
	CabIndicator->SetupAttachment(Cab);
	CabIndicator->SetRelativeLocation(FVector(113,0,231));
	CabIndicator->SetRelativeRotation(FRotator(0,180,0));
	CabIndicator->SetHorizontalAlignment(EHTA_Center);
	CabIndicator->SetVerticalAlignment(EVRTA_TextCenter);
	CabIndicator->SetWorldSize(11);
	for (int32 Stop = 0; Stop < 18; ++Stop)
	{
		LandingDoorL.Add(MakeBox(*FString::Printf(TEXT("LandingDoorL_%d"), Stop), Root));
		LandingDoorR.Add(MakeBox(*FString::Printf(TEXT("LandingDoorR_%d"), Stop), Root));
		CallButton.Add(MakeBox(*FString::Printf(TEXT("CallButton_%d"), Stop), Root));
		UBoxComponent* Trigger = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("CallTrigger_%d"), Stop));
		Trigger->SetupAttachment(Root);
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CallTrigger.Add(Trigger);
		UTextRenderComponent* Indicator=CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("LandingIndicator_%d"),Stop));
		Indicator->SetupAttachment(Root);
		Indicator->SetHorizontalAlignment(EHTA_Center);
		Indicator->SetVerticalAlignment(EVRTA_TextCenter);
		Indicator->SetWorldSize(9);
		LandingIndicators.Add(Indicator);
		UStaticMeshComponent* Button=MakeBox(*FString::Printf(TEXT("FloorButton_%d"),Stop),Cab);
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		Button->SetStaticMesh(Cylinder.Object);
		Button->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FloorButtons.Add(Button);
		UTextRenderComponent* Label=CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("FloorButtonLabel_%d"),Stop));
		Label->SetupAttachment(Cab);
		Label->SetRelativeRotation(FRotator(0,-90,0));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(5);
		FloorButtonLabels.Add(Label);
	}
}

UStaticMeshComponent* ABlockoutElevator::MakeBox(FName Name, USceneComponent* Parent)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* Box = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Box->SetupAttachment(Parent);
	Box->SetStaticMesh(CubeMesh.Object);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	return Box;
}

void ABlockoutElevator::PlaceBox(UStaticMeshComponent* Box, const FVector& Min, const FVector& Max)
{
	// Engine cube is 100 cm with a centered pivot
	Box->SetRelativeLocation((Min + Max) * 0.5f);
	Box->SetRelativeScale3D((Max - Min) / 100.f);
}

void ABlockoutElevator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	using namespace ElevatorLayout;

	const float X0 = -CabHX, X1 = CabHX, Y0 = -CabHY, Y1 = CabHY;
	PlaceBox(CabShell[0], FVector(X0, Y0, -10.f), FVector(X1, Y1, 0.f));                          // floor
	PlaceBox(CabShell[1], FVector(X0, Y0, 0.f), FVector(X0 + Wall, Y1, CabH));                    // back
	PlaceBox(CabShell[2], FVector(X0 + Wall, Y0, 0.f), FVector(X1 - Wall, Y0 + Wall, CabH));      // side L
	PlaceBox(CabShell[3], FVector(X0 + Wall, Y1 - Wall, 0.f), FVector(X1 - Wall, Y1, CabH));      // side R
	PlaceBox(CabShell[4], FVector(X0, Y0, CabH), FVector(X1, Y1, CabH + 10.f));                   // roof
	PlaceBox(CabShell[5], FVector(X1 - Wall, Y0, 0.f), FVector(X1, -DoorHalfW, CabH));            // front L
	PlaceBox(CabShell[6], FVector(X1 - Wall, DoorHalfW, 0.f), FVector(X1, Y1, CabH));             // front R
	PlaceBox(CabShell[7], FVector(X1 - Wall, -DoorHalfW, DoorH), FVector(X1, DoorHalfW, CabH));   // header
	PlaceBox(CabPanel, FVector(-90.f,Y1-Wall-6.f,60.f),FVector(90.f,Y1-Wall,210.f));
	CabTrigger->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	CabTrigger->SetBoxExtent(FVector(X1 - Wall, Y1 - Wall, 100.f));

	for (int32 Stop = 0; Stop < FMath::Min(StopLevels.Num(),18); ++Stop)
	{
		const float Z = StopZ(Stop);
		PlaceBox(CallButton[Stop], FVector(ShaftFaceX, 75.f, Z + 100.f), FVector(ShaftFaceX + 4.f, 85.f, Z + 115.f));
		CallTrigger[Stop]->SetRelativeLocation(FVector(ShaftFaceX + 120.f, 0.f, Z + 100.f));
		CallTrigger[Stop]->SetBoxExtent(FVector(120.f, 150.f, 100.f));
		LandingIndicators[Stop]->SetRelativeLocation(FVector(ShaftFaceX+6,0,Z+245));
		// Six rows of round physical pushbuttons, ordered by floor rather than legacy index.
		TArray<int32> Order;
		for (int32 I=0;I<StopLevels.Num();++I) Order.Add(I);
		Order.Sort([this](int32 A,int32 B){return StopLevels[A]<StopLevels[B];});
		const int32 Slot=Order.Find(Stop);
		const FVector P(-60.f+(Slot%3)*60.f,Y1-Wall-7.f,190.f-(Slot/3)*23.f);
		FloorButtons[Stop]->SetRelativeLocation(P);
		FloorButtons[Stop]->SetRelativeRotation(FRotator(0,0,90));
		FloorButtons[Stop]->SetRelativeScale3D(FVector(.084f,.084f,.012f));
		FloorButtons[Stop]->SetMaterial(0,ButtonMaterial);
		FloorButtonLabels[Stop]->SetRelativeLocation(P+FVector(0,-1,-8));
		FloorButtonLabels[Stop]->SetText(FText::FromString(GetStopLabel(Stop)));
	}

	TArray<UStaticMeshComponent*> Doors = { CabDoorL, CabDoorR };
	Doors.Append(LandingDoorL);
	Doors.Append(LandingDoorR);
	for (UStaticMeshComponent* Door : Doors)
	{
		Door->SetMaterial(0, DoorMaterial);
	}
	for (UStaticMeshComponent* Shell : CabShell)
	{
		Shell->SetMaterial(0, CabMaterial);
	}
	CabPanel->SetMaterial(0, DoorMaterial);
	for (UStaticMeshComponent* Button : CallButton)
	{
		Button->SetMaterial(0, ButtonMaterial);
	}

	ApplyCab();
	ApplyDoors();
	UpdateIndicators();
}

void ABlockoutElevator::BeginPlay()
{
	Super::BeginPlay();

	// Legacy key binding: no Enhanced Input assets are needed for this blockout interaction.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		EnableInput(PC);
		if (InputComponent)
		{
			// not consumed: the CM ingress actor binds E as well
			InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ABlockoutElevator::OnInteractPressed).bConsumeInput = false;
			InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&ABlockoutElevator::OnFloorButtonPressed).bConsumeInput=false;
		}
	}
}

void ABlockoutElevator::OnInteractPressed()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		InteractFrom(PC->GetPawn());
	}
}

static bool IsInsideBox(const UBoxComponent* Box, const FVector& WorldPoint)
{
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldPoint);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void ABlockoutElevator::InteractFrom(AActor* Interactor)
{
	if (!Interactor)
	{
		return;
	}
	const FVector P = Interactor->GetActorLocation();
	if (IsInsideBox(CabTrigger, P))
	{
		const int32 Aimed=GetAimedStop();
		if (Aimed!=INDEX_NONE) SelectStop(Aimed);
		PressCabButton();
		return;
	}
	for (int32 Stop = 0; Stop < FMath::Min(StopLevels.Num(),18); ++Stop)
	{
		if (IsInsideBox(CallTrigger[Stop], P))
		{
			CallToStop(Stop);
			return;
		}
	}
}

void ABlockoutElevator::PressCabButton()
{
	CallToStop(SelectedStop);
}

void ABlockoutElevator::SelectStop(int32 Stop)
{
	if (StopLevels.IsValidIndex(Stop)) SelectedStop=Stop;
}
FString ABlockoutElevator::GetStopLabel(int32 Stop) const
{
	if (!StopLevels.IsValidIndex(Stop)) return TEXT("?");
	return StopLevels[Stop]<0 ? TEXT("A") : FString::Printf(TEXT("%.0f"),StopLevels[Stop]);
}
void ABlockoutElevator::CycleSelection(int32 Direction)
{
	TArray<int32> Order;
	for (int32 I=0;I<StopLevels.Num();++I) Order.Add(I);
	Order.Sort([this](int32 A,int32 B){return StopLevels[A]<StopLevels[B];});
	const int32 At=Order.Find(SelectedStop);
	SelectedStop=Order[FMath::Clamp(At+Direction,0,Order.Num()-1)];
}
FString ABlockoutElevator::GetUsePrompt() const
{
	if (!IsPlayerInCab()) return FString();
	if (State==EBlockoutElevatorState::Moving) return FString::Printf(TEXT("Travelling to level %s"),*GetStopLabel(TargetStop));
	const int32 Aim=GetAimedStop();
	return Aim!=INDEX_NONE ? FString::Printf(TEXT("Click / E - Level %s"),*GetStopLabel(Aim)) : TEXT("Aim at a floor button | UP / DOWN select | E travel");
}
bool ABlockoutElevator::IsPlayerInCab() const
{
	const APlayerController* PC=GetWorld()->GetFirstPlayerController();
	return PC && PC->GetPawn() && IsInsideBox(CabTrigger,PC->GetPawn()->GetActorLocation());
}
FVector ABlockoutElevator::GetButtonCenterWorld(int32 Stop) const
{
	return FloorButtons.IsValidIndex(Stop) ? FloorButtons[Stop]->GetComponentLocation() : FVector::ZeroVector;
}
int32 ABlockoutElevator::GetAimedStop() const
{
	if (!IsPlayerInCab()) return INDEX_NONE;
	const APlayerController* PC=GetWorld()->GetFirstPlayerController();
	if (!PC->PlayerCameraManager) return INDEX_NONE;
	const FTransform T=Cab->GetComponentTransform();
	const FVector O=T.InverseTransformPosition(PC->PlayerCameraManager->GetCameraLocation());
	const FVector D=T.InverseTransformVectorNoScale(PC->PlayerCameraManager->GetCameraRotation().Vector());
	const float Plane=ElevatorLayout::CabHY-ElevatorLayout::Wall-7.f;
	if (D.Y<=.001f) return INDEX_NONE;
	const float Dist=(Plane-O.Y)/D.Y;
	if (Dist<0 || Dist>220.f) return INDEX_NONE;
	const FVector Hit=O+D*Dist;
	for (int32 I=0;I<FloorButtons.Num();++I)
	{
		const FVector P=FloorButtons[I]->GetRelativeLocation();
		if (FMath::Square(Hit.X-P.X)+FMath::Square(Hit.Z-P.Z)<=FMath::Square(4.2f)) return I;
	}
	return INDEX_NONE;
}
void ABlockoutElevator::OnFloorButtonPressed()
{
	if (State==EBlockoutElevatorState::Moving || State==EBlockoutElevatorState::Closing) return;
	const int32 I=GetAimedStop();
	if (I!=INDEX_NONE) { SelectStop(I); CallToStop(I); }
}
FString ABlockoutElevator::GetFloorCounterText() const
{
	if (!IsPlayerInCab()) return FString();
	return LastIndicatorText+TEXT("  |  TO ")+GetStopLabel(State==EBlockoutElevatorState::Moving || State==EBlockoutElevatorState::Closing ? TargetStop : SelectedStop);
}
void ABlockoutElevator::UpdateIndicators()
{
	int32 Nearest=CurrentStop;
	for (int32 I=0;I<StopLevels.Num();++I)
		if (FMath::Abs(StopZ(I)-CabZ)<FMath::Abs(StopZ(Nearest)-CabZ)) Nearest=I;
	const FString Mark=(State==EBlockoutElevatorState::Moving ? (StopZ(TargetStop)>CabZ ? TEXT("UP ") : TEXT("DN ")) : TEXT("LEVEL "))+GetStopLabel(Nearest);
	if (LastIndicatorText==Mark) return;
	LastIndicatorText=Mark;
	CabIndicator->SetText(FText::FromString(Mark));
	for (int32 I=0;I<LandingIndicators.Num();++I) LandingIndicators[I]->SetText(FText::FromString(Mark));
}

void ABlockoutElevator::CallToStop(int32 Stop)
{
	Stop = FMath::Clamp(Stop, 0, StopLevels.Num()-1);
	if (State == EBlockoutElevatorState::Moving)
	{
		return;
	}
	UE_LOG(LogBlockoutElevator, Log, TEXT("%s: called to stop %d (cab at stop %d)"), *GetName(), Stop, CurrentStop);
	TargetStop = Stop;
	if (Stop == CurrentStop)
	{
		if (State != EBlockoutElevatorState::Open)
		{
			State = EBlockoutElevatorState::Opening;
		}
	}
	else
	{
		State = DoorAlpha > 0.f ? EBlockoutElevatorState::Closing : EBlockoutElevatorState::Moving;
	}
}

void ABlockoutElevator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (APlayerController* PC=GetWorld()->GetFirstPlayerController())
	{
		if (PC->GetPawn() && IsInsideBox(CabTrigger,PC->GetPawn()->GetActorLocation()))
		{
			if (PC->WasInputKeyJustPressed(EKeys::Up)) CycleSelection(1);
			if (PC->WasInputKeyJustPressed(EKeys::Down)) CycleSelection(-1);
		}
	}

	switch (State)
	{
	case EBlockoutElevatorState::Opening:
		DoorAlpha = FMath::Min(1.f, DoorAlpha + DeltaSeconds / DoorTime);
		if (DoorAlpha >= 1.f)
		{
			State = EBlockoutElevatorState::Open;
		}
		ApplyDoors();
		break;

	case EBlockoutElevatorState::Closing:
		DoorAlpha = FMath::Max(0.f, DoorAlpha - DeltaSeconds / DoorTime);
		if (DoorAlpha <= 0.f)
		{
			State = TargetStop != CurrentStop ? EBlockoutElevatorState::Moving : EBlockoutElevatorState::Closed;
		}
		ApplyDoors();
		break;

	case EBlockoutElevatorState::Moving:
	{
		const float Target = StopZ(TargetStop);
		const float Remaining = FMath::Abs(Target - CabZ);
		// Accelerate to cruise speed, then brake so the cab stops exactly at the landing
		const float BrakeLimit = FMath::Sqrt(2.f * Acceleration * Remaining);
		Velocity = FMath::Min3(MaxSpeed, BrakeLimit, Velocity + Acceleration * DeltaSeconds);
		const float Step = FMath::Max(Velocity, 5.f) * DeltaSeconds;
		if (Step >= Remaining)
		{
			CabZ = Target;
			Velocity = 0.f;
			CurrentStop = TargetStop;
			SelectedStop = CurrentStop==0 ? 1 : 0;
			State = EBlockoutElevatorState::Opening;
			UE_LOG(LogBlockoutElevator, Log, TEXT("%s: arrived at stop %d"), *GetName(), CurrentStop);
		}
		else
		{
			CabZ += FMath::Sign(Target - CabZ) * Step;
		}
		ApplyCab();
		break;
	}

	default:
		break;
	}
	UpdateIndicators();
	for (int32 I=0;I<FloorButtons.Num();++I)
	{
		const bool Active=I==TargetStop && (State==EBlockoutElevatorState::Moving || State==EBlockoutElevatorState::Closing || State==EBlockoutElevatorState::Opening);
		FVector P=FloorButtons[I]->GetRelativeLocation(); P.Y=ElevatorLayout::CabHY-ElevatorLayout::Wall-(Active ? 6.4f : 7.f);
		FloorButtons[I]->SetRelativeLocation(P);
		FloorButtonLabels[I]->SetTextRenderColor(Active ? FColor(255,192,90) : FColor(235,235,225));
	}
}

void ABlockoutElevator::ApplyCab()
{
	Cab->SetRelativeLocation(FVector(0.f, 0.f, CabZ));
}

void ABlockoutElevator::ApplyDoors()
{
	using namespace ElevatorLayout;
	const float Open = DoorAlpha * DoorHalfW;
	const float CX0 = CabHX - Wall - 4.f, CX1 = CabHX - Wall - 1.f;
	PlaceBox(CabDoorL, FVector(CX0, -DoorHalfW - Open, 0.f), FVector(CX1, -Open, DoorH));
	PlaceBox(CabDoorR, FVector(CX0, Open, 0.f), FVector(CX1, DoorHalfW + Open, DoorH));

	for (int32 Stop = 0; Stop < FMath::Min(StopLevels.Num(),18); ++Stop)
	{
		// Only the landing the cab is standing at opens
		const float LandingOpen = (Stop == CurrentStop && State != EBlockoutElevatorState::Moving) ? Open : 0.f;
		const float Z = StopZ(Stop);
		PlaceBox(LandingDoorL[Stop], FVector(LandingX0, -DoorHalfW - LandingOpen, Z), FVector(LandingX1, -LandingOpen, Z + DoorH));
		PlaceBox(LandingDoorR[Stop], FVector(LandingX0, LandingOpen, Z), FVector(LandingX1, DoorHalfW + LandingOpen, Z + DoorH));
	}
}
