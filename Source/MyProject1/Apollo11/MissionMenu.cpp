#include "Apollo11/MissionMenu.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputCoreTypes.h"
#include "HAL/IConsoleManager.h"

namespace
{
    const FName MenuMap(TEXT("/Game/Missions/Maps/LVL_MissionMenu"));
    const FName ApolloMap(TEXT("/Game/Apollo11/Maps/LVL_Pad39A_Predawn"));
    const FName GeminiMap(TEXT("/Game/Missions/Maps/LVL_Gemini1_Prototype"));
    const FLinearColor Ink(.055f,.072f,.086f,1), Paper(.89f,.9f,.86f,1), Muted(.49f,.58f,.61f,1), Accent(.91f,.61f,.23f,1);
}
AMissionMenuGameMode::AMissionMenuGameMode()
{
    PlayerControllerClass=AMissionMenuController::StaticClass(); HUDClass=AMissionMenuHUD::StaticClass(); DefaultPawnClass=nullptr;
}
void AMissionMenuGameMode::InitGame(const FString& MapName,const FString& Options,FString& Error)
{
    Super::InitGame(MapName,Options,Error); bStartInSelection=UGameplayStatics::HasOption(Options,TEXT("SelectMission"));
}
AMissionMenuController::AMissionMenuController() { bShowMouseCursor=true; bEnableClickEvents=true; }
void AMissionMenuController::BeginPlay()
{
    Super::BeginPlay(); SetPause(false); bShowMouseCursor=true;
    FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
}
void AMissionMenuController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AMissionMenuController::Click);
    InputComponent->BindKey(EKeys::Enter,IE_Pressed,this,&AMissionMenuController::Enter);
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AMissionMenuController::Back);
    InputComponent->BindKey(EKeys::One,IE_Pressed,this,&AMissionMenuController::Apollo);
    InputComponent->BindKey(EKeys::Two,IE_Pressed,this,&AMissionMenuController::Gemini);
}
void AMissionMenuController::Click() { float X=0,Y=0; if (GetMousePosition(X,Y)) if (auto* H=Cast<AMissionMenuHUD>(GetHUD())) H->ClickAt(FVector2D(X,Y)); }
void AMissionMenuController::Enter() { if (auto* H=Cast<AMissionMenuHUD>(GetHUD())) H->Activate(H->IsSelectingMission() ? TEXT("Apollo") : TEXT("Play")); }
void AMissionMenuController::Back() { if (auto* H=Cast<AMissionMenuHUD>(GetHUD())) H->Activate(TEXT("Back")); }
void AMissionMenuController::Apollo() { if (auto* H=Cast<AMissionMenuHUD>(GetHUD())) if (H->IsSelectingMission()) H->Activate(TEXT("Apollo")); }
void AMissionMenuController::Gemini() { if (auto* H=Cast<AMissionMenuHUD>(GetHUD())) if (H->IsSelectingMission()) H->Activate(TEXT("Gemini")); }
void AMissionMenuHUD::BeginPlay()
{
    Super::BeginPlay(); if (const auto* M=GetWorld()->GetAuthGameMode<AMissionMenuGameMode>()) bSelecting=M->bStartInSelection;
}
void AMissionMenuHUD::Text(const FString& Value,float X,float Y,float Scale,FLinearColor Color)
{
    FCanvasTextItem Item(FVector2D(X,Y),FText::FromString(Value),GEngine->GetLargeFont(),Color);
    Item.Scale=FVector2D(Scale,Scale); Canvas->DrawItem(Item);
}
void AMissionMenuHUD::Button(FName Action,const FString& Title,const FString& Subtitle,float X,float Y,float W,float H)
{
    float MX=0,MY=0; PlayerOwner->GetMousePosition(MX,MY);
    const bool Hover=MX>=X && MX<=X+W && MY>=Y && MY<=Y+H;
    const float S=Canvas->ClipY/1080.f;
    FCanvasTileItem Tile(FVector2D(X,Y),FVector2D(W,H),Hover ? FLinearColor(.14f,.19f,.22f,1) : FLinearColor(.085f,.115f,.13f,1)); Canvas->DrawItem(Tile);
    FCanvasTileItem Edge(FVector2D(X,Y),FVector2D(4*S,H),Hover ? Accent : Muted); Canvas->DrawItem(Edge);
    Text(Title,X+28*S,Y+22*S,1.7f*S,Paper);
    if (!Subtitle.IsEmpty()) Text(Subtitle,X+28*S,Y+63*S,.85f*S,Muted);
    Buttons.Add({Action,FBox2D(FVector2D(X,Y),FVector2D(X+W,Y+H))});
}
void AMissionMenuHUD::DrawHUD()
{
    Super::DrawHUD(); if (!Canvas) return;
    Buttons.Reset(); const float S=Canvas->ClipY/1080.f; const float X=Canvas->ClipX*.12f, W=FMath::Min(1100*S,Canvas->ClipX*.76f);
    FCanvasTileItem Back(FVector2D(0,0),FVector2D(Canvas->ClipX,Canvas->ClipY),Ink); Canvas->DrawItem(Back);
    FCanvasTileItem Line(FVector2D(X,230*S),FVector2D(W,2*S),Accent); Canvas->DrawItem(Line);
    Text(TEXT("LAUNCH OPERATIONS"),X,108*S,3.2f*S,Paper);
    Text(bSelecting ? TEXT("MISSION SELECTION") : TEXT("GROUND CREW / EXPLORATION"),X,192*S,1.05f*S,Muted);
    if (bSelecting)
    {
        Button(TEXT("Apollo"),TEXT("01   APOLLO"),TEXT("Apollo 11 / existing LC-39A scene"),X,296*S,W,116*S);
        Button(TEXT("Gemini"),TEXT("02   GEMINI"),TEXT("Gemini 1 / LC-19-inspired pad in shared prototype surroundings"),X,434*S,W,116*S);
        Button(TEXT("Back"),TEXT("BACK"),TEXT("Return to main menu"),X,606*S,W*.5f,106*S);
        Text(TEXT("Select a mission to enter its playable scene.   1 Apollo   /   2 Gemini"),X,790*S,.9f*S,Muted);
    }
    else
    {
        Button(TEXT("Play"),TEXT("SELECT MISSION"),TEXT("Apollo or Gemini"),X,314*S,W,116*S);
        Button(TEXT("Quit"),TEXT("QUIT"),TEXT("Exit to desktop"),X,470*S,W*.5f,106*S);
        Text(TEXT("Enter to select a mission.   M returns here from either mission."),X,790*S,.9f*S,Muted);
    }
    Text(TEXT("APOLLO SCENE PRESERVED   /   GEMINI PAD PROTOTYPE"),X,965*S,.75f*S,Muted);
}
FVector4 AMissionMenuHUD::GetButtonRect(FName Action) const
{
    for (const FButton& B:Buttons) if (B.Action==Action) return FVector4(B.Rect.Min.X,B.Rect.Min.Y,B.Rect.Max.X,B.Rect.Max.Y);
    return FVector4(0,0,0,0);
}
void AMissionMenuHUD::ClickAt(FVector2D P) { for (const FButton& B:Buttons) if (B.Rect.IsInside(P)) { Activate(B.Action); return; } }
void AMissionMenuHUD::Activate(FName Action)
{
    if (Action==TEXT("Play")) { bSelecting=true; return; }
    if (Action==TEXT("Back")) { bSelecting=false; return; }
    if (Action==TEXT("Quit")) { UKismetSystemLibrary::QuitGame(this,PlayerOwner,EQuitPreference::Quit,false); return; }
    if (!bSelecting) return;
    if (Action==TEXT("Apollo") || Action==TEXT("Gemini"))
    {
        PlayerOwner->bShowMouseCursor=false; PlayerOwner->SetInputMode(FInputModeGameOnly());
        UGameplayStatics::OpenLevel(this,Action==TEXT("Apollo") ? ApolloMap : GeminiMap);
    }
}
AMissionNavigation::AMissionNavigation() { PrimaryActorTick.bCanEverTick=false; }
void AMissionNavigation::BeginPlay()
{
    Super::BeginPlay();
    // UE5.8 D3D12 residency command-list exhaustion reproduced in sustained
    // Gemini mast playtests. Serialize translation for this mission only;
    // preserve materials/lighting and restore the prior value on world travel.
    if (bGeminiPrototype) if (auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmd.ParallelTranslate.Enable")))
    {
        PreviousParallelTranslate=V->GetInt(); V->Set(0,ECVF_SetByCode); bScopedGeminiRHI=true;
    }
    if (auto* PC=GetWorld()->GetFirstPlayerController())
    {
        EnableInput(PC); if (InputComponent) InputComponent->BindKey(EKeys::M,IE_Pressed,this,&AMissionNavigation::OnMenuPressed).bExecuteWhenPaused=true;
    }
}
void AMissionNavigation::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bScopedGeminiRHI) if (auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmd.ParallelTranslate.Enable"))) V->Set(PreviousParallelTranslate,ECVF_SetByCode);
    Super::EndPlay(Reason);
}
void AMissionNavigation::OnMenuPressed() { ReturnToSelection(); }
void AMissionNavigation::ReturnToSelection()
{
    if (auto* PC=GetWorld()->GetFirstPlayerController()) PC->SetPause(false);
    UGameplayStatics::OpenLevel(this,MenuMap,true,TEXT("SelectMission"));
}
