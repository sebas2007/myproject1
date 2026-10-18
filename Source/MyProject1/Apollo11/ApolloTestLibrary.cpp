// Test helpers for the automated play-tests.

#include "Apollo11/ApolloTestLibrary.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputKeyEventArgs.h"

UWorld* UApolloTestLibrary::GetActiveGameWorld()
{
	if (GEngine) for (const FWorldContext& Context:GEngine->GetWorldContexts())
		if (Context.World() && (Context.WorldType==EWorldType::Game || Context.WorldType==EWorldType::PIE)) return Context.World();
	return nullptr;
}

namespace
{
	APlayerController* FirstPC(UObject* WorldContext)
	{
		UWorld* W = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
		return W ? W->GetFirstPlayerController() : nullptr;
	}

	bool Send(APlayerController* PC, const FKey& Key, EInputEvent Event, float Amount)
	{
		if (!PC || !Key.IsValid())
		{
			return false;
		}
		FViewport* Viewport = nullptr;
		if (const ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			Viewport = LP->ViewportClient ? LP->ViewportClient->Viewport : nullptr;
		}
		// the same arguments the game viewport builds from a hardware event (default keyboard/mouse device)
		FInputKeyEventArgs Args(Viewport, IPlatformInputDeviceMapper::Get().GetDefaultInputDevice(), Key, Event, Amount, false,
		                        FPlatformTime::Cycles64());
		Args.ControllerId = 0;
		if (Event == IE_Axis)
		{
			Args.DeltaTime = PC->GetWorld() ? PC->GetWorld()->GetDeltaSeconds() : 1.f / 60.f;
			Args.NumSamples = 1;
		}
		return PC->InputKey(Args);
	}
}

bool UApolloTestLibrary::InjectKey(UObject* WorldContext, FName Key, bool bPressed)
{
	return Send(FirstPC(WorldContext), FKey(Key), bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f);
}

bool UApolloTestLibrary::InjectAxis(UObject* WorldContext, FName Key, float Delta)
{
	return Send(FirstPC(WorldContext), FKey(Key), IE_Axis, Delta);
}

bool UApolloTestLibrary::IsKeyDown(UObject* WorldContext, FName Key)
{
	const APlayerController* PC = FirstPC(WorldContext);
	return PC && PC->IsInputKeyDown(FKey(Key));
}
