#include "Apollo11/GeminiErector.h"
#include "Apollo11/GeminiCabinPreview.h"
#include "Apollo11/GeminiTechnician.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
AGeminiAccessDoor::AGeminiAccessDoor()
{
    PrimaryActorTick.bCanEverTick=true;
    Hinge=CreateDefaultSubobject<USceneComponent>(TEXT("Hinge")); RootComponent=Hinge;
    Leaf=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf")); Leaf->SetupAttachment(Hinge);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> White(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_WRWhite"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Metal(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_Galv"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
    Leaf->SetStaticMesh(Cube.Object); Leaf->SetMaterial(0,White.Object);
    Leaf->SetRelativeLocation(FVector(0,80,110)); Leaf->SetRelativeScale3D(FVector(.04f,1.6f,2.2f));
    Leaf->SetMobility(EComponentMobility::Movable); Leaf->SetCollisionProfileName(TEXT("BlockAll"));
    Handle=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LatchHandle")); Handle->SetupAttachment(Hinge);
    Handle->SetStaticMesh(Cube.Object); Handle->SetMaterial(0,Metal.Object); Handle->SetRelativeScale3D(FVector(.08f,.17f,.025f));
    Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision); Handle->SetMobility(EComponentMobility::Movable);
    for (int32 I=0;I<3;++I)
    {
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HingeKnuckle%d"),I));
        C->SetupAttachment(Hinge); C->SetStaticMesh(Cylinder.Object); C->SetMaterial(0,Metal.Object);
        C->SetRelativeLocation(FVector(0,0,30+I*80)); C->SetRelativeScale3D(FVector(.07f,.07f,.14f));
        C->SetMobility(EComponentMobility::Movable); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}
void AGeminiAccessDoor::BeginPlay()
{
    Super::BeginPlay();
    EnableInput(GetWorld()->GetFirstPlayerController());
    InputComponent->BindKey(EKeys::E,IE_Pressed,this,&AGeminiAccessDoor::Toggle).bConsumeInput=false;
}
FString AGeminiAccessDoor::GetPrompt() const
{
    auto* PC=GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return FString();
    if (bErectorDoor) for (TActorIterator<AGeminiErector> It(GetWorld()); It; ++It) if (It->IsMoving() || It->GetAngle()>1) return FString();
    if (FVector::Dist(PC->GetPawn()->GetActorLocation(),GetActorLocation()+GetActorQuat().RotateVector(FVector(0,80,100)))>165) return FString();
    return bOpen ? TEXT("E - Close servicing access door") : TEXT("E - Open servicing access door");
}
void AGeminiAccessDoor::Toggle() { if (!GetPrompt().IsEmpty()) bOpen=!bOpen; }
void AGeminiAccessDoor::Tick(float Dt)
{
    Super::Tick(Dt); Angle=FMath::FInterpConstantTo(Angle,bOpen ? 85.f : 0.f,Dt,65.f);
    // Move leaf around its hinge rather than rotating its center.
    const FQuat Q=FRotator(0,Angle,0).Quaternion();
    Leaf->SetRelativeLocation(Q.RotateVector(FVector(0,80,110)));
    Leaf->SetRelativeRotation(Q);
    Handle->SetRelativeLocation(Q.RotateVector(FVector(6,143,105))); Handle->SetRelativeRotation(Q);
}
AGeminiErector::AGeminiErector()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gray(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_EquipGray"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Metal(TEXT("/Game/Apollo11/Materials/Weathered/MI_W_Galv"));
    for (int32 I=0;I<8;++I)
    {
        auto* Ram=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Ram%d"),I));
        auto* Rod=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Rod%d"),I));
        for (auto* C:{Ram,Rod}) { C->SetupAttachment(RootComponent); C->SetStaticMesh(Cylinder.Object); C->SetMobility(EComponentMobility::Movable); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
        Ram->SetMaterial(0,Gray.Object); Rod->SetMaterial(0,Metal.Object);
        if (I<2) { Rams.Add(Ram); Rods.Add(Rod); } else { PlatformRams.Add(Ram); PlatformRods.Add(Rod); }
    }
}
void AGeminiErector::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AActor> It(GetWorld());It;++It)
        if (It->ActorHasTag(TEXT("GeminiErectorMoving")))
        {
            const int32 Side=It->ActorHasTag(TEXT("GeminiFoldNorth")) ? 1 : It->ActorHasTag(TEXT("GeminiFoldSouth")) ? -1 : 0;
            const float Z=It->ActorHasTag(TEXT("GeminiFoldLevel6")) ? 2778.76f : It->ActorHasTag(TEXT("GeminiFoldLevel7")) ? 3050.f : 3322.32f;
            Parts.Add({*It,It->GetActorTransform(),Side,Z});
        }
    EnableInput(GetWorld()->GetFirstPlayerController());
    InputComponent->BindKey(EKeys::E,IE_Pressed,this,&AGeminiErector::Press).bConsumeInput=false;
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AGeminiErector::Press).bConsumeInput=false;
    ApplyAngle();
}
bool AGeminiErector::AtConsole() const
{
    auto* PC=GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return false;
    const FVector P=PC->GetPawn()->GetActorLocation();
    return FVector::Dist(P,ConsoleLocation)<210 && P.Z<160 && P.Y<-700;
}
bool AGeminiErector::ClearToMove() const
{
    for (TActorIterator<AGeminiTechnician> It(GetWorld());It;++It) if (It->IsAccessOccupied() || It->IsCabinetOpen()) return false;
    for (TActorIterator<AGeminiCabinPreview> It(GetWorld());It;++It) if (It->IsViewing()) return false;
    for (TActorIterator<AGeminiAccessDoor> It(GetWorld());It;++It) if (It->bErectorDoor && !It->IsClosed()) return false;
    // All pawns must be outside the full swept corridor, on the ground.
    for (TActorIterator<APawn> It(GetWorld());It;++It)
    {
        const FVector P=It->GetActorLocation();
        if (P.X>-5100 && P.X<350 && FMath::Abs(P.Y)<520 && P.Z<4200) return false;
    }
    return true;
}
FString AGeminiErector::GetPrompt() const
{
    if (bMoving) return FString::Printf(TEXT("ERECTOR %s - %.0f deg / STOP button halts travel"),TargetAngle>45 ? TEXT("LOWERING") : TEXT("RAISING"),Angle);
    if (!AtConsole()) return FString();
    if (!ClearToMove()) return TEXT("ERECTOR INTERLOCK - Close access doors and clear structure");
    if (bStopped) return TEXT("ERECTOR STOPPED / Aim at OPERATE and press E to resume");
    return Angle<1 ? TEXT("E / Click OPERATE - Lower erector and White Room") : TEXT("E / Click OPERATE - Raise erector and White Room");
}
int32 AGeminiErector::GetAimedControl() const
{
    if (!AtConsole()) return -1;
    auto* PC=GetWorld()->GetFirstPlayerController();
    const FVector Eye=PC->PlayerCameraManager->GetCameraLocation(), Ray=PC->PlayerCameraManager->GetCameraRotation().Vector();
    if (FMath::Abs(Ray.Y)<.001f) return -1;
    const float T=(-911.f-Eye.Y)/Ray.Y;
    if (T<0 || T>250) return -1;
    const FVector Hit=Eye+Ray*T;
    for (int32 I=0;I<2;++I) if (FVector::DistSquared(Hit,FVector(-1810+I*70,-911,145))<4.5f*4.5f) return I+1;
    return -1;
}
void AGeminiErector::Press() { const int32 C=GetAimedControl(); if (C==1) Toggle(); else if (C==2) Stop(); }
void AGeminiErector::Stop() { if (bMoving && AtConsole()) { bMoving=false; bStopped=true; } }
void AGeminiErector::Toggle()
{
    if (bMoving || !AtConsole() || !ClearToMove()) return;
    if (!bStopped) TargetProgress=Progress<1 ? TravelSeconds+8.f : 0.f;
    TargetAngle=TargetProgress>1 ? 90.f : 0.f; bMoving=true; bStopped=false;
}
void AGeminiErector::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!bMoving) return;
    // A person entering the exclusion corridor stops travel until it is clear again.
    if (!ClearToMove()) return;
    Progress=FMath::FInterpConstantTo(Progress,TargetProgress,Dt,1.f);
    const float F=FMath::Clamp(Progress/8.f,0.f,1.f),T=FMath::Clamp((Progress-8.f)/FMath::Max(TravelSeconds,1.f),0.f,1.f);
    FoldAngle=90.f*F*F*(3-2*F); Angle=90.f*T*T*(3-2*T); ApplyAngle();
    if (FMath::IsNearlyEqual(Progress,TargetProgress,.001f)) bMoving=false;
}
void AGeminiErector::ApplyAngle()
{
    const FQuat Q=FRotator(Angle,0,0).Quaternion();
    for (const FPart& P:Parts)
        if (AActor* A=P.Actor.Get())
        {
            FVector L=P.Raised.GetLocation(); FQuat R=P.Raised.GetRotation();
            if (P.FoldSide)
            {
                const FVector FP(0,P.FoldSide*300.f,P.FoldZ);
                const FQuat FQ=FRotator(0,0,-P.FoldSide*FoldAngle).Quaternion();
                L=FP+FQ.RotateVector(L-FP); R=FQ*R;
            }
            A->SetActorLocationAndRotation(Pivot+Q.RotateVector(L-Pivot),Q*R,false,nullptr,ETeleportType::TeleportPhysics);
        }
    for (int32 I=0;I<2;++I)
    {
        const float Y=I ? 300.f : -300.f;
        const FVector Base(-1500,Y,80), End=Pivot+Q.RotateVector(FVector(-780,Y,520)-Pivot);
        const FVector D=End-Base, Unit=D.GetSafeNormal(); const float L=D.Size(), Body=FMath::Min(310.f,L*.7f);
        Rams[I]->SetWorldLocationAndRotation(Base+Unit*Body*.5f,FRotationMatrix::MakeFromZ(D).ToQuat());
        Rams[I]->SetWorldScale3D(FVector(.5f,.5f,Body/100.f));
        Rods[I]->SetWorldLocationAndRotation(Base+Unit*(Body+(L-Body)*.5f),FRotationMatrix::MakeFromZ(D).ToQuat());
        Rods[I]->SetWorldScale3D(FVector(.24f,.24f,(L-Body)/100.f));
    }
    for (int32 I=0;I<6;++I)
    {
        const float Side=I%2 ? 1.f : -1.f;
        const float Z=I/2==0 ? 2778.76f : I/2==1 ? 3050.f : 3322.32f;
        const FVector FP(0,Side*300,Z),LocalBase(200,Side*310,Z-110);
        const FVector LocalEnd=FP+FRotator(0,0,-Side*FoldAngle).Quaternion().RotateVector(FVector(200,Side*200,Z-8)-FP);
        const FVector Base=Pivot+Q.RotateVector(LocalBase-Pivot),End=Pivot+Q.RotateVector(LocalEnd-Pivot);
        const FVector D=End-Base,U=D.GetSafeNormal(); const float L=D.Size(),Body=80.f;
        PlatformRams[I]->SetWorldLocationAndRotation(Base+U*Body*.5f,FRotationMatrix::MakeFromZ(D).ToQuat());
        PlatformRams[I]->SetWorldScale3D(FVector(.06f,.06f,Body/100.f));
        PlatformRods[I]->SetWorldLocationAndRotation(Base+U*(Body+(L-Body)*.5f),FRotationMatrix::MakeFromZ(D).ToQuat());
        PlatformRods[I]->SetWorldScale3D(FVector(.03f,.03f,(L-Body)/100.f));
    }
}
