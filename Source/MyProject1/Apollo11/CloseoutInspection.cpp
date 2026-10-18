#include "Apollo11/CloseoutInspection.h"
#include "Apollo11/CMIngress.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"

namespace
{
    const TCHAR* const CheckNames[]={TEXT("Tool tray / loose items"),TEXT("Hatch seal / sill"),TEXT("Hose coupling / cable routing")};
}
ACloseoutInspection::ACloseoutInspection()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
bool ACloseoutInspection::HatchAccessible() const
{
    for (TActorIterator<ACMIngress> It(GetWorld());It;++It)
        return It->GetIngressState()==ECMIngressState::Idle && It->GetHatchState()==ECMHatchState::Open;
    return false;
}
int32 ACloseoutInspection::AimedCheck() const
{
    const APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);
    if (!PC || !PC->PlayerCameraManager || !HatchAccessible()) return INDEX_NONE;
    const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();
    // Only from inside the White Room, on this floor. Prevent checks through exterior walls.
    const FVector P=GetActorTransform().InverseTransformPosition(Eye);
    if (P.X<0 || P.X>305 || FMath::Abs(P.Y)>169 || P.Z<65 || P.Z>220) return INDEX_NONE;
    float Best=.975f;
    int32 Index=INDEX_NONE;
    for (int32 I=0;I<FMath::Min(InspectionPoints.Num(),3);++I)
    {
        const FVector D=InspectionPoints[I]-Eye;
        const float Dot=FVector::DotProduct(D.GetSafeNormal(),PC->PlayerCameraManager->GetCameraRotation().Vector());
        if (D.SizeSquared()<FMath::Square(210.f) && Dot>Best) { Best=Dot; Index=I; }
    }
    return Index;
}
bool ACloseoutInspection::IsCheckComplete(int32 Index) const
{
    return Index>=0 && Index<3 && (CompleteMask & (1<<Index))!=0;
}
int32 ACloseoutInspection::GetCompletedCount() const
{
    return int32(IsCheckComplete(0))+int32(IsCheckComplete(1))+int32(IsCheckComplete(2));
}
FString ACloseoutInspection::GetUsePrompt() const
{
    const int32 I=AimedCheck();
    if (I==INDEX_NONE) return FString();
    if (IsCheckComplete(I)) return FString::Printf(TEXT("PREPARATION REHEARSAL | %s checked | %d/3"),CheckNames[I],GetCompletedCount());
    return FString::Printf(TEXT("Hold F - Inspect %s | %d/3 | %.0f%%"),CheckNames[I],GetCompletedCount(),HoldTime*50.f);
}
void ACloseoutInspection::Tick(float Dt)
{
    Super::Tick(Dt);
    const int32 I=AimedCheck();
    const APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);
    if (I==INDEX_NONE || IsCheckComplete(I) || !PC || !PC->IsInputKeyDown(EKeys::F))
    { HoldingIndex=INDEX_NONE; HoldTime=0; return; }
    if (HoldingIndex!=I) { HoldingIndex=I; HoldTime=0; }
    HoldTime+=Dt;
    if (HoldTime>=2.f)
    {
        CompleteMask |= (1<<I);
        HoldTime=0;
        UE_LOG(LogTemp,Log,TEXT("Closeout visual rehearsal: %s recorded (%d/3); no pressure/flow certification"),CheckNames[I],GetCompletedCount());
    }
}
