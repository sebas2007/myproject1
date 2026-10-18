#include "Apollo11/GeminiTechnician.h"
#include "Apollo11/GeminiErector.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
namespace {
const float Landings[]={500,850,1200,1653.16f,1982.445f,2250,2600,3080.64f,3340.555f};
const FVector Points[]={FVector(713,230,125),FVector(585,-115,600),FVector(595,105,3435.555f)};
}
AGeminiTechnician::AGeminiTechnician()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gray(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_EquipGray"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Red(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_TowerRed_Merged"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> White(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_WRWhite"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Black(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_CableBlack"));
    auto Box=[&](FString N,FVector P,FVector Size,UMaterialInterface* M,USceneComponent* Parent)->UStaticMeshComponent* {
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*N); C->SetupAttachment(Parent); C->SetStaticMesh(Cube.Object);
        C->SetRelativeLocation(P); C->SetRelativeScale3D(Size/100); C->SetMaterial(0,M);
        C->SetMobility(EComponentMobility::Movable); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); return C;
    };
    CabinetHinge=CreateDefaultSubobject<USceneComponent>(TEXT("CabinetHinge")); CabinetHinge->SetupAttachment(RootComponent);
    CabinetHinge->SetRelativeLocation(FVector(713,150,25));
    Box(TEXT("CabinetDoor"),FVector(0,80,75),FVector(3,160,150),Gray.Object,CabinetHinge);
    Box(TEXT("Latch"),FVector(4,145,75),FVector(6,4,18),Black.Object,CabinetHinge);
    Box(TEXT("IsolatedTestPanel"),FVector(703,230,118),FVector(3,105,85),Gray.Object,RootComponent);
    Box(TEXT("AnalogMeterFace"),FVector(706,230,135),FVector(2,44,32),White.Object,RootComponent);
    MeterNeedle=Box(TEXT("TestLoopNeedle"),FVector(708,230,132),FVector(1,19,1),Black.Object,RootComponent);
    MeterNeedle->SetRelativeScale3D(FVector(.006f,.009f,.20f));
    for(int32 I=0;I<9;++I) {
        const float A=FMath::DegreesToRadians(-65.f+I*16.25f);
        auto* Tick=Box(FString::Printf(TEXT("MeterGraduation%d"),I),FVector(708,230+15*FMath::Sin(A),125+16*FMath::Cos(A)),FVector(.7f,.7f,2.4f),Black.Object,RootComponent);
        Tick->SetRelativeRotation(FRotator(0,0,-FMath::RadiansToDegrees(A)));
    }
    for(int32 Side=-1;Side<=1;Side+=2) {
        Box(FString::Printf(TEXT("MeterVerticalBezel%d"),Side),FVector(707,230+Side*23,135),FVector(3,2,35),Black.Object,RootComponent);
        Box(FString::Printf(TEXT("MeterHorizontalBezel%d"),Side),FVector(707,230,135+Side*17),FVector(3,48,2),Black.Object,RootComponent);
    }
    Box(TEXT("LocalTestSwitch"),FVector(706,230,95),FVector(7,5,5),Black.Object,RootComponent);
    for(int32 I=0; I<UE_ARRAY_COUNT(Landings); ++I) {
        auto* G=Box(FString::Printf(TEXT("LadderGate%d"),I),FVector(670,0,Landings[I]+78),FVector(4,230,4),Red.Object,RootComponent);
        G->SetCollisionProfileName(TEXT("BlockAll")); Gates.Add(G);
        Box(FString::Printf(TEXT("GateHinge%d"),I),FVector(670,-115,Landings[I]+78),FVector(9,7,18),Gray.Object,RootComponent);
    }
}
void AGeminiTechnician::BeginPlay() {
    Super::BeginPlay(); EnableInput(GetWorld()->GetFirstPlayerController());
    InputComponent->BindKey(EKeys::E,IE_Pressed,this,&AGeminiTechnician::Use).bConsumeInput=false;
}
bool AGeminiTechnician::AccessAvailable() const {
    for(TActorIterator<AGeminiErector> It(GetWorld());It;++It) if(It->IsMoving() || It->GetAngle()>1) return false;
    return true;
}
bool AGeminiTechnician::IsAccessOccupied() const {
    if(bClimbing) return true;
    for(TActorIterator<APawn> It(GetWorld());It;++It) {
        const FVector P=It->GetActorLocation();
        if(P.X>300 && P.X<820 && FMath::Abs(P.Y)<210 && P.Z>250) return true;
    }
    return false;
}
bool AGeminiTechnician::AtLadder() const {
    auto* PC=GetWorld()->GetFirstPlayerController(); if(!PC || !PC->GetPawn()) return false;
    const FVector P=PC->GetPawn()->GetActorLocation();
    if(FVector2D::Distance(FVector2D(P.X,P.Y),FVector2D(800,0))>210 || P.Z>3500) return false;
    if(P.Z>250) { bool Landing=false; for(float Z:Landings) if(FMath::Abs(P.Z-Z-98)<30) Landing=true; if(!Landing) return false; }
    // At a landing the chest camera is almost in the rung plane. Test facing
    // through the guarded opening, avoiding an unstable near-camera aim point.
    const FVector D=(FVector(800,0,P.Z)-P).GetSafeNormal2D();
    const FVector Facing=PC->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal2D();
    return FVector::DotProduct(D,Facing)>.72f;
}
int32 AGeminiTechnician::GetAimedTask() const {
    auto* PC=GetWorld()->GetFirstPlayerController(); if(!PC || !PC->GetPawn() || bClimbing) return -1;
    const FVector Eye=PC->PlayerCameraManager->GetCameraLocation(),Ray=PC->PlayerCameraManager->GetCameraRotation().Vector();
    for(int32 I=0;I<3;++I) { const FVector D=Points[I]-Eye; const float T=FVector::DotProduct(D,Ray);
        if(T>0 && T<190 && (D-Ray*T).Size()<28) return I;
    }
    return -1;
}
void AGeminiTechnician::Use() {
    auto* PC=GetWorld()->GetFirstPlayerController(); auto* C=PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
    if(!C) return;
    if(bTransfer) return;
    if(bClimbing) {
        if(bReturning) { bReturning=false; return; }
        const FVector P=C->GetActorLocation(); float Z=-1;
        if(P.Z<130) Z=98;
        for(float L:Landings) if(FMath::Abs(P.Z-L-98)<27) Z=L+98;
        if(Z<0) {
            ReturnZ=98;
            for(float L:Landings) if(FMath::Abs(P.Z-L-98)<FMath::Abs(P.Z-ReturnZ)) ReturnZ=L+98;
            bReturning=true; return;
        }
        const FVector Exit(Z<150 ? 870 : 630,0,Z);
        TransferFrom=P; TransferTo=Exit; TransferTime=0; bTransfer=true; bLeaving=true; return;
    }
    if(GetAimedTask()==0) {
        if(!bCabinetOpen) { bCabinetOpen=true; if(Stage==0) Stage=1; }
        else { bCabinetOpen=false; if(Stage==4) Stage=5; }
        Hold=0; return;
    }
    if(AccessAvailable() && AtLadder()) {
        bClimbing=true; PC->SetIgnoreMoveInput(true); C->GetCharacterMovement()->StopMovementImmediately();
        C->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        TransferFrom=C->GetActorLocation(); TransferTo=FVector(800,0,TransferFrom.Z);
        TransferTime=0; bTransfer=true; bLeaving=false;
        PC->SetControlRotation(FRotator(0,180,0));
    }
}
FString AGeminiTechnician::GetPrompt() const {
    auto* PC=GetWorld()->GetFirstPlayerController(); if(!PC || !PC->GetPawn()) return FString();
    if(bClimbing) return bReturning ? TEXT("RETURNING TO NEAREST LANDING / E or W S cancels return") : TEXT("LADDER: W / S climb - E steps off at a landing or returns safely to the nearest one");
    const int32 Task=GetAimedTask();
    if(Task==0) {
        if(!bCabinetOpen) return Stage==5 ? TEXT("LOCAL CHECK RECORD READY / E - Reopen cabinet") : TEXT("E - Open local inspection cabinet");
        if(Stage==2) return FString::Printf(TEXT("Hold F - Isolated demonstration test loop (%.0f%%) / E close"),Hold/3*100);
        if(Stage==4) return TEXT("E - Close cabinet and complete local check record");
        return TEXT("E - Close inspection cabinet / Complete connector and duct inspections");
    }
    if(Task==1) return Stage>=2 ? TEXT("CONNECTOR / Visual check recorded") : Stage==1 && bCabinetOpen ? FString::Printf(TEXT("Hold F - Inspect connector lock and strain relief (%.0f%%)"),Hold/3*100) : TEXT("CONNECTOR INSPECTION / Open cabinet first; follow local check order");
    if(Task==2) return Stage>=4 ? TEXT("DUCT / Visual check recorded - return and close ground cabinet") : Stage==3 && bCabinetOpen ? FString::Printf(TEXT("Hold F - Inspect duct coupling and supports (%.0f%%)"),Hold/3*100) : TEXT("DUCT INSPECTION / Complete connector and isolated loop checks first");
    if(AtLadder()) return AccessAvailable() ? TEXT("E - Use maintenance ladder / W S climb / E at landings") : TEXT("MAST ACCESS CLOSED - Erector must be raised and stopped");
    const FVector P=PC->GetPawn()->GetActorLocation();
    if(FVector2D::Distance(FVector2D(P.X,P.Y),FVector2D(500,0))<700) {
        const TCHAR* Next=Stage==0 ? TEXT("Open ground cabinet") : Stage==1 ? TEXT("Inspect connector on first landing") : Stage==2 ? TEXT("Return to cabinet for isolated test loop") : Stage==3 ? TEXT("Inspect duct at top work landing") : Stage==4 ? TEXT("Return and close ground cabinet") : TEXT("Local check record READY");
        return FString::Printf(TEXT("MAST %d/5 - %s / Visual rehearsal; no live system certification"),Stage,Next);
    }
    return FString();
}
void AGeminiTechnician::Tick(float Dt) {
    Super::Tick(Dt); auto* PC=GetWorld()->GetFirstPlayerController(); auto* C=PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr; if(!C) return;
    DoorAngle=FMath::FInterpConstantTo(DoorAngle,bCabinetOpen ? -105.f : 0.f,Dt,90.f);
    CabinetHinge->SetRelativeRotation(FRotator(0,DoorAngle,0));
    const FQuat NeedleQ=FRotator(0,0,Stage>=3 ? -35.f : 35.f).Quaternion();
    MeterNeedle->SetRelativeRotation(NeedleQ);
    MeterNeedle->SetRelativeLocation(FVector(708,230,125)+NeedleQ.RotateVector(FVector(0,0,8)));
    if(bClimbing && !bTransfer) {
        float Direction=(PC->IsInputKeyDown(EKeys::W)?1.f:0.f)-(PC->IsInputKeyDown(EKeys::S)?1.f:0.f);
        if(Direction!=0) bReturning=false;
        if(bReturning) Direction=FMath::Sign(ReturnZ-C->GetActorLocation().Z);
        FVector P=C->GetActorLocation(); P.Z=FMath::Clamp(P.Z+Direction*125.f*Dt,98.f,3438.555f);
        if(bReturning) P.Z=FMath::FInterpConstantTo(C->GetActorLocation().Z,ReturnZ,Dt,125.f);
        C->SetActorLocation(P,true); C->GetCharacterMovement()->Velocity=FVector::ZeroVector;
        if(bReturning && FMath::Abs(C->GetActorLocation().Z-ReturnZ)<1) { bReturning=false; Use(); }
    }
    for(int32 I=0;I<Gates.Num();++I) {
        const bool Open=bClimbing && FMath::Abs(C->GetActorLocation().Z-Landings[I]-98)<145;
        const FQuat Q=FRotator(0,Open ? -90.f : 0.f,0).Quaternion();
        Gates[I]->SetRelativeLocation(FVector(670,-115,Landings[I]+78)+Q.RotateVector(FVector(0,115,0)));
        Gates[I]->SetRelativeRotation(Q);
    }
    if(bTransfer) {
        TransferTime=FMath::Min(TransferTime+Dt/2.5f,1.f);
        const float T=TransferTime*TransferTime*(3-2*TransferTime);
        // Lift clear of the deck edge before crossing it, then settle on the
        // landing. A straight diagonal clips the deck with the capsule's foot.
        FVector P=FMath::Lerp(TransferFrom,TransferTo,T);
        if(FMath::Max(TransferFrom.Z,TransferTo.Z)>250) {
            // Step around the ladder side rail, through the open gate; do not
            // pass the character/camera through the continuous rung plane.
            const FVector A(TransferFrom.X,80,TransferFrom.Z), B(TransferTo.X,80,TransferTo.Z);
            P=T<.3f ? FMath::Lerp(TransferFrom,A,T/.3f) : T<.7f ? FMath::Lerp(A,B,(T-.3f)/.4f) : FMath::Lerp(B,TransferTo,(T-.7f)/.3f);
        }
        P.Z=FMath::Max(TransferFrom.Z,TransferTo.Z)+12.f*FMath::Sin(PI*T);
        FHitResult Hit; C->SetActorLocation(P,true,&Hit);
        if(Hit.bBlockingHit) UE_LOG(LogTemp,Warning,TEXT("Gemini ladder transfer blocked by %s at %s"),*GetNameSafe(Hit.GetActor()),*C->GetActorLocation().ToString());
        if(TransferTime>=1) {
            bTransfer=false;
            if(bLeaving) {
                // A blocked transfer retains the ladder; never drop the player.
                if(FVector::Dist(C->GetActorLocation(),TransferTo)>8) { C->SetActorLocation(TransferFrom,false); }
                else { bClimbing=false; PC->SetIgnoreMoveInput(false); C->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
            }
        }
    }
    const int32 Task=GetAimedTask();
    const bool Correct=bCabinetOpen && ((Stage==1 && Task==1)||(Stage==2 && Task==0)||(Stage==3 && Task==2));
    if(Correct && PC->IsInputKeyDown(EKeys::F)) { Hold+=Dt; if(Hold>=3) { ++Stage; Hold=0; } } else Hold=0;
}
