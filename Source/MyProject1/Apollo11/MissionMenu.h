#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Actor.h"
#include "MissionMenu.generated.h"

UCLASS()
class AMissionMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AMissionMenuGameMode();
    virtual void InitGame(const FString& MapName,const FString& Options,FString& Error) override;
    bool bStartInSelection=false;
};

UCLASS()
class AMissionMenuController : public APlayerController
{
    GENERATED_BODY()
public:
    AMissionMenuController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
private:
    void Click(); void Enter(); void Back(); void Apollo(); void Gemini();
};

UCLASS()
class AMissionMenuHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void DrawHUD() override;
    UFUNCTION(BlueprintCallable,Category="Missions") void Activate(FName Action);
    UFUNCTION(BlueprintPure,Category="Missions") bool IsSelectingMission() const { return bSelecting; }
    UFUNCTION(BlueprintPure,Category="Missions") FVector4 GetButtonRect(FName Action) const;
    void ClickAt(FVector2D Point);
private:
    struct FButton { FName Action; FBox2D Rect; };
    TArray<FButton> Buttons;
    bool bSelecting=false;
    void Text(const FString& Value,float X,float Y,float Scale,FLinearColor Color);
    void Button(FName Action,const FString& Title,const FString& Subtitle,float X,float Y,float W,float H);
};

/** One per playable mission; a fresh world travel prevents cross-mission state leaks. */
UCLASS()
class AMissionNavigation : public AActor
{
    GENERATED_BODY()
public:
    AMissionNavigation();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable,Category="Missions") void ReturnToSelection();
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Missions") bool bGeminiPrototype=false;
private:
    void OnMenuPressed();
    int32 PreviousParallelTranslate=1;
    bool bScopedGeminiRHI=false;
};
