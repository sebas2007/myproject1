// Cryogenic vapor around the Saturn V on the pad (Apollo 11, LC-39A).

#include "Apollo11/CryoVaporField.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCryoVapor, Log, All);

namespace VaporLayout
{
	// Docs/dimensions.md: world origin at the hardstand top on the vehicle axis, deck 47 ft up, 1 ft = 30.48 cm
	constexpr float Ft = 30.48f;
	constexpr float DeckZ = 47.f * Ft;
}

ACryoVaporField::ACryoVaporField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;      // after the camera has moved this frame
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		PuffMesh = Plane.Object;
	}
}

float ACryoVaporField::VehicleRadius(float HeightAboveDeck)
{
	// skin radius (cm): S-IC/S-II 33 ft, interstage cone, S-IVB 21.7 ft, SLA taper 281.16-309.16 ft, SM 154 in
	// (Boeing Saturn V Apollo flight configuration stations, Tools/Realism/spacecraft_geom.py)
	const float H = HeightAboveDeck / VaporLayout::Ft;
	if (H < 219.5f) return 503.f;
	if (H < 238.5f) return FMath::Lerp(503.f, 330.f, (H - 219.5f) / 19.f);
	if (H < 281.16f) return 330.f;
	if (H < 309.16f) return FMath::Lerp(330.f, 195.6f, (H - 281.16f) / 28.f);
	return 195.6f;
}

void ACryoVaporField::BuildDefaultVents()
{
	using namespace VaporLayout;
	Vents.Reset();
	auto AddRing = [this](float HeightFt, int32 Count, float FirstDeg, const FCryoVent& Proto)
	{
		const float Z = DeckZ + HeightFt * Ft;
		const float R = VehicleRadius(HeightFt * Ft) + 15.f;
		for (int32 i = 0; i < Count; ++i)
		{
			const float A = FMath::DegreesToRadians(FirstDeg + i * 360.f / Count);
			FCryoVent V = Proto;
			V.Location = FVector(R * FMath::Cos(A), R * FMath::Sin(A), Z);
			V.Direction = FVector(FMath::Cos(A), FMath::Sin(A), -0.25f);
			Vents.Add(V);
		}
	};
	// LOX boil-off rolling off the frosted tank skins: slow, sinking sheets (PHOTO-DERIVED placement). Emission is dense
	// enough that consecutive puffs overlap while the wind carries them (sparser settings read as thin ropes).
	FCryoVent Skin;
	Skin.Rate = 4.0f; Skin.Speed = 35.f; Skin.Life = 10.f; Skin.StartSize = 180.f; Skin.EndSize = 720.f; Skin.Opacity = 0.13f;
	AddRing(100.f, 6, 15.f, Skin);                 // S-IC LOX tank
	Skin.Rate = 3.0f; Skin.Opacity = 0.11f;
	AddRing(168.f, 4, 45.f, Skin);                 // S-II LOX tank (aft)
	Skin.Rate = 2.5f; Skin.Opacity = 0.10f; Skin.EndSize = 560.f;
	AddRing(251.f, 3, 60.f, Skin);                 // S-IVB LOX tank (aft end of the tank, 247-268 ft)
	// S-IC forward-skirt LOX vents: dense billowing jets, away from the tower side (world -X)
	FCryoVent Jet;
	Jet.Rate = 7.0f; Jet.Speed = 240.f; Jet.Spread = 0.35f; Jet.Life = 8.f; Jet.StartSize = 90.f; Jet.EndSize = 900.f; Jet.Opacity = 0.22f;
	AddRing(127.f, 1, 60.f, Jet);                  // the pair at +-60 deg from +X
	AddRing(127.f, 1, -60.f, Jet);
	// S-IC base: vapor from the tail service mast LOX lines pooling on the launcher deck and rolling off its edges
	FCryoVent Base;
	Base.Rate = 3.0f; Base.Speed = 50.f; Base.Spread = 0.8f; Base.Life = 11.f; Base.StartSize = 200.f; Base.EndSize = 900.f; Base.Opacity = 0.10f;
	AddRing(6.f, 4, 45.f, Base);
	// LOX fill/drain umbilical plates (swing arms 1 and 2): small leaks toward the tower
	FCryoVent Leak;
	Leak.Rate = 4.0f; Leak.Speed = 90.f; Leak.Spread = 0.5f; Leak.Life = 7.f; Leak.StartSize = 70.f; Leak.EndSize = 450.f; Leak.Opacity = 0.18f;
	Leak.Location = FVector(-530.f, 0.f, DeckZ + 64.f * Ft);       Leak.Direction = FVector(-1.f, 0.f, -0.3f); Vents.Add(Leak);
	Leak.Location = FVector(-530.f, -100.f, DeckZ + 124.f * Ft);   Vents.Add(Leak);
}

void ACryoVaporField::BeginPlay()
{
	Super::BeginPlay();
	if (Vents.Num() == 0)
	{
		BuildDefaultVents();
	}
	Rng.Initialize(19690716);
	Puffs.SetNum(Vents.Num());
	SpawnAccum.Init(0.f, Vents.Num());
	for (int32 v = 0; v < Vents.Num(); ++v)
	{
		UInstancedStaticMeshComponent* Comp = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Puffs_%02d"), v));
		Comp->SetupAttachment(RootComponent);
		Comp->SetStaticMesh(PuffMesh);
		if (VaporMaterial)
		{
			Comp->SetMaterial(0, VaporMaterial);
		}
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->NumCustomDataFloats = 1;
		Comp->RegisterComponent();
		Puffs[v].SetNum(MaxPerVent);
		TArray<FTransform> Init;
		Init.Init(FTransform(FQuat::Identity, Vents[v].Location, FVector(0.001)), MaxPerVent);
		Comp->AddInstances(Init, false, true);
		PuffComponents.Add(Comp);
	}
	for (float T = 0.f; T < PrewarmSeconds; T += 1.f / 15.f)
	{
		Simulate(1.f / 15.f);
	}
	UE_LOG(LogCryoVapor, Log, TEXT("CryoVaporField: %d vents, %d puffs live after %.0f s prewarm"), Vents.Num(), GetLiveCount(), PrewarmSeconds);
}

int32 ACryoVaporField::GetLiveCount() const
{
	int32 N = 0;
	for (const TArray<FPuff>& Pool : Puffs)
	{
		for (const FPuff& P : Pool)
		{
			N += P.bAlive ? 1 : 0;
		}
	}
	return N;
}

void ACryoVaporField::Simulate(float Dt)
{
	const FVector Origin = GetActorLocation();
	const FVector WindDir = WindToward.GetSafeNormal2D();
	for (int32 v = 0; v < Vents.Num(); ++v)
	{
		const FCryoVent& Vent = Vents[v];
		TArray<FPuff>& Pool = Puffs[v];
		SpawnAccum[v] += Vent.Rate * Dt;
		while (SpawnAccum[v] >= 1.f)
		{
			SpawnAccum[v] -= 1.f;
			for (FPuff& P : Pool)
			{
				if (!P.bAlive)
				{
					const FVector Dir = Rng.VRandCone(Vent.Direction.GetSafeNormal(), Vent.Spread);
					P.Pos = Origin + Vent.Location + Rng.VRand() * 10.f;
					P.Vel = Dir * Vent.Speed * Rng.FRandRange(0.7f, 1.2f);
					P.Age = 0.f;
					P.Life = Vent.Life * Rng.FRandRange(0.75f, 1.25f);
					P.Size0 = Vent.StartSize * Rng.FRandRange(0.8f, 1.2f);
					P.Size1 = Vent.EndSize * Rng.FRandRange(0.7f, 1.3f);
					P.Opacity = Vent.Opacity * Rng.FRandRange(0.7f, 1.1f);
					P.Roll = Rng.FRandRange(0.f, 2.f * PI);
					P.bAlive = true;
					break;
				}
			}
		}
		for (FPuff& P : Pool)
		{
			if (!P.bAlive)
			{
				continue;
			}
			P.Age += Dt;
			if (P.Age >= P.Life)
			{
				P.bAlive = false;
				continue;
			}
			// wind grows with height above the surrounding ground (1/7 power law), the puff is dragged toward it
			const float AboveGround = FMath::Max(200.f, P.Pos.Z - Origin.Z + GroundBelowOrigin);
			const FVector Wind = WindDir * WindSpeedRef * FMath::Pow(AboveGround / (60.f * VaporLayout::Ft), 0.14f);
			const float Drag = 1.f - FMath::Exp(-0.6f * Dt);
			P.Vel.X += (Wind.X - P.Vel.X) * Drag;
			P.Vel.Y += (Wind.Y - P.Vel.Y) * Drag;
			P.Vel.Z = FMath::Max(P.Vel.Z - SinkAccel * Dt, -MaxSinkSpeed);
			P.Vel += Rng.VRand() * Turbulence * Dt;
			P.Pos += P.Vel * Dt;
			// keep puffs outside the vehicle skin (they roll around it rather than through it)
			const FVector2D Radial(P.Pos.X - Origin.X, P.Pos.Y - Origin.Y);
			const float Height = P.Pos.Z - Origin.Z - VaporLayout::DeckZ;
			if (Height > 0.f && Height < 363.f * VaporLayout::Ft)
			{
				const float MinR = VehicleRadius(Height) + 0.25f * FMath::Lerp(P.Size0, P.Size1, P.Age / P.Life);
				const float R = Radial.Size();
				if (R < MinR && R > 1.f)
				{
					const FVector2D Out = Radial / R * MinR;
					P.Pos.X = Origin.X + Out.X;
					P.Pos.Y = Origin.Y + Out.Y;
				}
			}
			// stop at the deck / hardstand: spread out instead of sinking through
			const float Floor = Origin.Z + (FMath::Abs(Radial.X) < 2500.f && FMath::Abs(Radial.Y) < 2060.f ? VaporLayout::DeckZ : 0.f) + 30.f;
			if (P.Pos.Z < Floor)
			{
				P.Pos.Z = Floor;
				P.Vel.Z = 0.f;
			}
		}
	}
}

void ACryoVaporField::UpdateInstances(const FVector& CameraLocation)
{
	TArray<FTransform> Xf;
	for (int32 v = 0; v < PuffComponents.Num(); ++v)
	{
		UInstancedStaticMeshComponent* Comp = PuffComponents[v];
		const TArray<FPuff>& Pool = Puffs[v];
		Xf.Reset(Pool.Num());
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			const FPuff& P = Pool[i];
			if (!P.bAlive)
			{
				Xf.Add(FTransform(FQuat::Identity, P.Pos, FVector(0.001)));
				Comp->SetCustomDataValue(i, 0, 0.f, false);
				continue;
			}
			const float T = P.Age / P.Life;
			const float Size = FMath::Lerp(P.Size0, P.Size1, FMath::Sqrt(T));
			const float Fade = FMath::SmoothStep(0.f, 0.15f, T) * (1.f - FMath::SmoothStep(0.45f, 1.f, T));
			const FVector ToCam = (CameraLocation - P.Pos).GetSafeNormal();
			const FQuat Face = FRotationMatrix::MakeFromZ(ToCam).ToQuat();
			const FQuat Rot = FQuat(ToCam, P.Roll) * Face;
			Xf.Add(FTransform(Rot, P.Pos, FVector(Size / 100.f)));
			Comp->SetCustomDataValue(i, 0, P.Opacity * Fade, false);
		}
		Comp->BatchUpdateInstancesTransforms(0, Xf, true, true, true);
	}
}

void ACryoVaporField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Simulate(FMath::Min(DeltaSeconds, 0.1f));
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		UpdateInstances(PC->PlayerCameraManager->GetCameraLocation());
	}
}
