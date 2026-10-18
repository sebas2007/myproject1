#include "Apollo11/TowerServiceCabinet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"

ATowerServiceCabinet::ATowerServiceCabinet()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Housing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Housing"));
    Housing->SetupAttachment(RootComponent);
    Internals = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Internals"));
    Internals->SetupAttachment(RootComponent);
    LeftHinge = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHinge"));
    LeftHinge->SetupAttachment(RootComponent);
    LeftHinge->SetRelativeLocation(FVector(55,-65,0));
    RightHinge = CreateDefaultSubobject<USceneComponent>(TEXT("RightHinge"));
    RightHinge->SetupAttachment(RootComponent);
    RightHinge->SetRelativeLocation(FVector(55,65,0));
    LeftDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoor"));
    LeftDoor->SetupAttachment(LeftHinge);
    RightDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoor"));
    RightDoor->SetupAttachment(RightHinge);
    // The generator places cabinets in equipment bays outside the crew route.
    for (UStaticMeshComponent* M : {Housing.Get(), Internals.Get(), LeftDoor.Get(), RightDoor.Get()})
    {
        M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        M->SetMobility(EComponentMobility::Movable);
    }
}

bool ATowerServiceCabinet::CanUse() const
{
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this,0);
    if (!PC || !PC->PlayerCameraManager) return false;
    const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
    const FVector LocalEye = GetActorTransform().InverseTransformPosition(Eye);
    // Operate from in front of this cabinet, never through the rear/adjacent storey.
    if (LocalEye.X < 60 || LocalEye.X > 245 || FMath::Abs(LocalEye.Y)>150 || LocalEye.Z<20 || LocalEye.Z>240) return false;
    const FVector Center = GetActorTransform().TransformPosition(FVector(55,0,125));
    const FVector D = (Center-Eye).GetSafeNormal();
    return FVector::DotProduct(PC->PlayerCameraManager->GetCameraRotation().Vector(),D)>0.86f;
}

FString ATowerServiceCabinet::GetUsePrompt() const
{
    return CanUse() ? FString::Printf(TEXT("E - %s inspection doors | %s"), bOpen ? TEXT("Close") : TEXT("Open"), *EquipmentName) : FString();
}

void ATowerServiceCabinet::ToggleDoors()
{
    bOpen = !bOpen;
    UE_LOG(LogTemp,Log,TEXT("Tower cabinet %s: inspection doors %s"), *EquipmentName, bOpen ? TEXT("open") : TEXT("closed"));
}

void ATowerServiceCabinet::Tick(float Dt)
{
    Super::Tick(Dt);
    APlayerController* PC = UGameplayStatics::GetPlayerController(this,0);
    if (PC && CanUse() && PC->WasInputKeyJustPressed(EKeys::E)) ToggleDoors();
    // Slow heavy sheet-metal doors, hinged to the housing rather than detached props.
    DoorAmount = FMath::FInterpConstantTo(DoorAmount, bOpen ? 1.f : 0.f, Dt, 0.65f);
    LeftHinge->SetRelativeRotation(FRotator(0,-105.f*DoorAmount,0));
    RightHinge->SetRelativeRotation(FRotator(0,105.f*DoorAmount,0));
}
