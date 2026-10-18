// Test helpers for the automated play-tests (Tools/Blockout/playtest_game.py).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ApolloTestLibrary.generated.h"

/**
 * Feeds key and mouse events into the first local player controller exactly as the keyboard and mouse do
 * (APlayerController::InputKey -> PlayerInput -> Enhanced Input mappings and every legacy BindKey on the input stack), so a
 * play-test checks the real bindings: WASD / mouse look through IMC_Default + IMC_MouseLook, E on the elevator and the CM
 * hatch, Shift, Tab / 1 / 2 / 3 in the cockpit, the mouse buttons and wheel on the panel controls.
 */
UCLASS()
class UApolloTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Active standalone/PIE world for travel tests; no assumptions about the current map. */
	UFUNCTION(BlueprintPure, Category="Apollo11|Test") static UWorld* GetActiveGameWorld();
	/** Key press (bPressed) or release, e.g. "W", "E", "LeftShift", "Tab", "One", "LeftMouseButton", "MouseScrollUp". */
	UFUNCTION(BlueprintCallable, Category = "Apollo11|Test", meta = (WorldContext = "WorldContext"))
	static bool InjectKey(UObject* WorldContext, FName Key, bool bPressed);

	/** Axis movement, e.g. "MouseX" / "MouseY" (mouse counts, like one frame of mouse motion). */
	UFUNCTION(BlueprintCallable, Category = "Apollo11|Test", meta = (WorldContext = "WorldContext"))
	static bool InjectAxis(UObject* WorldContext, FName Key, float Delta);

	/** Is the key down as far as the player controller is concerned (what IsInputKeyDown polls). */
	UFUNCTION(BlueprintPure, Category = "Apollo11|Test", meta = (WorldContext = "WorldContext"))
	static bool IsKeyDown(UObject* WorldContext, FName Key);
};
