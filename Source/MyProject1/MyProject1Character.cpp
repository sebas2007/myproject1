// Copyright Epic Games, Inc. All Rights Reserved.

#include "MyProject1Character.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MyProject1.h"
#include "Apollo11/BodycamComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "GameFramework/PlayerController.h"

AMyProject1Character::AMyProject1Character()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
	
	// Create the first person mesh that will be viewed only by this character's owner
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));

	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));

	// Create the Camera Component	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("First Person Camera"));
	FirstPersonCameraComponent->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	FirstPersonCameraComponent->bUsePawnControlRotation = true;
	FirstPersonCameraComponent->bEnableFirstPersonFieldOfView = true;
	FirstPersonCameraComponent->bEnableFirstPersonScale = true;
	FirstPersonCameraComponent->FirstPersonFieldOfView = 70.0f;
	FirstPersonCameraComponent->FirstPersonScale = 0.6f;

	Bodycam = CreateDefaultSubobject<UBodycamComponent>(TEXT("Bodycam"));

	// configure the character comps
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

	GetCapsuleComponent()->SetCapsuleSize(34.0f, 96.0f);

	// Configure character movement
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->AirControl = 0.5f;

	PrimaryActorTick.bCanEverTick = true;

	// footsteps (Tools/Audio/make_footsteps.py writes them, build_footsteps.py imports them)
	for (int32 i = 1; i <= 4; ++i)
	{
		auto Path = [i](const TCHAR* Kind)
		{
			return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Apollo11/Audio/Footsteps/S_Step_%s_%02d.S_Step_%s_%02d"), Kind, i, Kind, i)));
		};
		GratingSteps.Add(Path(TEXT("Grating")));
		PlateSteps.Add(Path(TEXT("Plate")));
		ConcreteSteps.Add(Path(TEXT("Concrete")));
	}
}

void AMyProject1Character::BeginPlay()
{
	Super::BeginPlay();
	// applied here (not only in the constructor) so values saved in the Blueprint can't bring back the 600 cm/s template walk
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = NormalWalkSpeed;
		Move->MaxAcceleration = WalkAcceleration;
		Move->BrakingDecelerationWalking = WalkBrakingDeceleration;
		Move->GroundFriction = 4.0f;               // turning still feels planted
		Move->bUseSeparateBrakingFriction = true;
		Move->BrakingFriction = 1.0f;
	}
	TargetSpeed = NormalWalkSpeed;
	auto Load = [](const TArray<TSoftObjectPtr<USoundBase>>& In, TArray<TObjectPtr<USoundBase>>& Out)
	{
		for (const TSoftObjectPtr<USoundBase>& S : In)
		{
			if (USoundBase* Snd = S.LoadSynchronous())
			{
				Out.Add(Snd);
			}
		}
	};
	Load(ConcreteSteps, LoadedConcrete);
	Load(PlateSteps, LoadedPlate);
	Load(GratingSteps, LoadedGrating);
}

float AMyProject1Character::GetWalkIntensity() const
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move || Move->IsFalling())
	{
		return 0.f;
	}
	return FMath::Clamp(GetVelocity().Size2D() / FMath::Max(NormalWalkSpeed, 1.f), 0.f, 2.4f);
}

void AMyProject1Character::UpdateSurface(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const FVector Loc = GetActorLocation();
	// vertical rate from the actual position (CharacterMovement steps up stairs without a vertical velocity)
	if (bHaveLastZ && DeltaSeconds > 0.f)
	{
		const float Rate = FMath::Abs(Loc.Z - LastZ) / DeltaSeconds;
		ClimbRate = FMath::Lerp(ClimbRate, Rate, 1.f - FMath::Exp(-6.f * DeltaSeconds));
	}
	LastZ = Loc.Z;
	bHaveLastZ = true;

	Surface = 0;
	bool bStairMesh = false;
	if (Move && Move->CurrentFloor.IsWalkableFloor())
	{
		if (const UPrimitiveComponent* Floor = Move->CurrentFloor.HitResult.GetComponent())
		{
			const AActor* FloorActor = Floor->GetOwner();
			const UMaterialInterface* M = Floor->GetNumMaterials() > 0 ? Floor->GetMaterial(0) : nullptr;
			const FString MatName = M ? M->GetName() : FString();
			if (MatName.Contains(TEXT("Grating")))
			{
				Surface = 2;
			}
			else if (MatName.Contains(TEXT("Concrete")) || MatName.Contains(TEXT("Sand")) || MatName.Contains(TEXT("Soil"))
				|| MatName.Contains(TEXT("Ground")) || MatName.Contains(TEXT("Asphalt")))
			{
				Surface = 0;
			}
			else
			{
				Surface = 1;
			}
			const FString Names = Floor->GetName() + TEXT("|") + (FloorActor ? FloorActor->GetName() : FString());
			bStairMesh = Names.Contains(TEXT("Stair")) || Names.Contains(TEXT("Tread"));
			// riding the elevator cab changes the height without climbing
			if (FloorActor && FloorActor->GetClass()->GetName().Contains(TEXT("Elevator")))
			{
				ClimbRate = 0.f;
			}
		}
	}
	// stairs: a stair mesh underfoot, or walking while the height changes (> 20 cm/s); held 0.5 s over landings
	const bool bClimbing = GetVelocity().Size2D() > 30.f && ClimbRate > 20.f;
	StairHold = (bStairMesh || bClimbing) ? 0.5f : FMath::Max(0.f, StairHold - DeltaSeconds);
	bOnStairs = StairHold > 0.f;
}

void AMyProject1Character::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}
	UpdateSurface(DeltaSeconds);

	// Shift: brisk walk (polled: Enhanced Input has no raw key bindings here). Seated in the CM, Shift lowers the suit
	// arms instead (ACMIngress) and the cockpit lock keeps this off.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const bool bShift = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
	const bool bBrisk = (bShift || bBriskHeld) && !bCockpitLocked;
	const float Base = (bOnStairs || Surface == 2) ? CarefulSpeed : NormalWalkSpeed;
	TargetSpeed = bBrisk ? Base * BriskSpeed / FMath::Max(NormalWalkSpeed, 1.f) : Base;
	Move->MaxWalkSpeed = FMath::FInterpConstantTo(Move->MaxWalkSpeed, TargetSpeed, DeltaSeconds, SpeedBlendRate);

	// stride: half a cycle per step, the step lengthens with speed; a foot strikes at the bob's low point
	const float Speed = Move->IsFalling() ? 0.f : GetVelocity().Size2D();
	if (Speed > 5.f)
	{
		StepPhase += DeltaSeconds * Speed / FMath::Max(StepBase + StepPerSpeed * Speed, 20.f) * PI;
	}
	else
	{
		// standing: settle into the nearest double-support pose so the next step starts cleanly
		const float Rest = (FMath::RoundToFloat(StepPhase / PI - 0.5f) + 0.5f) * PI;
		StepPhase = FMath::FInterpTo(StepPhase, Rest, DeltaSeconds, 4.f);
	}
	const int32 StepIndex = FMath::FloorToInt32(StepPhase / PI - 0.5f);
	if (StepIndex > LastStepIndex && Speed > 30.f && !bCockpitLocked)
	{
		PlayFootstep();
	}
	LastStepIndex = StepIndex;
}

void AMyProject1Character::PlayFootstep()
{
	++StepCount;
	const TArray<TObjectPtr<USoundBase>>& Set = Surface == 2 ? LoadedGrating : (Surface == 1 ? LoadedPlate : LoadedConcrete);
	if (Set.Num() == 0)
	{
		return;
	}
	USoundBase* Snd = Set[FMath::RandRange(0, Set.Num() - 1)];
	const float Speed = GetVelocity().Size2D();
	const float Vol = FootstepVolume * FMath::Clamp(0.55f + 0.45f * Speed / FMath::Max(NormalWalkSpeed, 1.f), 0.4f, 1.6f);
	const FVector Foot = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	UGameplayStatics::PlaySoundAtLocation(this, Snd, Foot, Vol, FMath::FRandRange(0.94f, 1.06f));
}

void AMyProject1Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AMyProject1Character::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMyProject1Character::DoJumpEnd);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyProject1Character::MoveInput);

		// Looking/Aiming
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMyProject1Character::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMyProject1Character::LookInput);

	}
	else
	{
		UE_LOG(LogMyProject1, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AMyProject1Character::MoveInput(const FInputActionValue& Value)
{
	// get the Vector2D move axis
	FVector2D MovementVector = Value.Get<FVector2D>();

	// pass the axis values to the move input
	DoMove(MovementVector.X, MovementVector.Y);

}

void AMyProject1Character::LookInput(const FInputActionValue& Value)
{
	// get the Vector2D look axis
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// pass the axis values to the aim input
	DoAim(LookAxisVector.X, LookAxisVector.Y);

}

void AMyProject1Character::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		// pass the rotation inputs
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMyProject1Character::DoMove(float Right, float Forward)
{
	if (GetController() && !bCockpitLocked)
	{
		// pass the move inputs
		AddMovementInput(GetActorRightVector(), Right);
		AddMovementInput(GetActorForwardVector(), Forward);
	}
}

void AMyProject1Character::DoJumpStart()
{
	// pass Jump to the character
	if (!bCockpitLocked)
	{
		Jump();
	}
}

void AMyProject1Character::DoJumpEnd()
{
	// pass StopJumping to the character
	StopJumping();
}
