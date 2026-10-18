#include "Apollo11/GeminiCabinPreview.h"
#include "Apollo11/GeminiErector.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/InputComponent.h"
AGeminiCabinPreview::AGeminiCabinPreview() { PrimaryActorTick.bCanEverTick=true; }
void AGeminiCabinPreview::BeginPlay()
{
    Super::BeginPlay();
    if (auto* PC=GetWorld()->GetFirstPlayerController())
    {
        EnableInput(PC);
        InputComponent->BindKey(EKeys::E,IE_Pressed,this,&AGeminiCabinPreview::Toggle).bConsumeInput=false;
    }
    Observer=GetWorld()->SpawnActor<ACameraActor>(ViewLocation,ViewRotation);
    Observer->GetCameraComponent()->SetFieldOfView(80.f);
}
bool AGeminiCabinPreview::CanEnter() const
{
    for (TActorIterator<AGeminiErector> It(GetWorld());It;++It) if (It->GetAngle()>1 || It->IsMoving()) return false;
    auto* PC=GetWorld()->GetFirstPlayerController();
    return PC && PC->GetPawn() && FVector::Dist(PC->GetPawn()->GetActorLocation(),Entrance)<230.f;
}
FString AGeminiCabinPreview::GetPrompt() const
{
    if (bViewing) return TEXT("GEMINI 1 - UNCREWED INSTRUMENTATION CABIN / Mouse: inspect / E: return");
    return CanEnter() ? TEXT("E - Inspect Gemini 1 instrumentation cabin (rough preview)") : FString();
}
void AGeminiCabinPreview::Toggle()
{
    auto* PC=GetWorld()->GetFirstPlayerController();
    if (!PC || !Observer) return;
    if (bViewing)
    {
        PC->SetViewTarget(PreviousTarget ? PreviousTarget.Get() : PC->GetPawn());
        PC->SetIgnoreMoveInput(false); PC->SetControlRotation(EntryControl); bViewing=false;
    }
    else if (CanEnter())
    {
        PreviousTarget=PC->GetViewTarget(); EntryControl=PC->GetControlRotation();
        PC->SetIgnoreMoveInput(true); PC->SetControlRotation(ViewRotation);
        Observer->SetActorRotation(ViewRotation); PC->SetViewTarget(Observer); bViewing=true;
    }
}
void AGeminiCabinPreview::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bViewing)
        if (auto* PC=GetWorld()->GetFirstPlayerController())
        {
            FRotator R=PC->GetControlRotation(); R.Normalize();
            R.Pitch=FMath::Clamp(R.Pitch,-55.f,55.f);
            R.Yaw=ViewRotation.Yaw+FMath::Clamp(FRotator::NormalizeAxis(R.Yaw-ViewRotation.Yaw),-100.f,100.f);
            R.Roll=0; PC->SetControlRotation(R); Observer->SetActorRotation(R);
        }
}
