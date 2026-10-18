// Cryogenic vapor around the Saturn V on the pad: a small CPU particle simulation drawn as camera-facing quads.
// (Niagara authoring through MCP doesn't work in this setup, see CLAUDE.md.)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CryoVaporField.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** One vapor source on the vehicle (world space, relative to the actor). */
USTRUCT(BlueprintType)
struct FCryoVent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Vent")
	FVector Location = FVector::ZeroVector;

	/** Initial direction of the vapor (normalized on use) */
	UPROPERTY(EditAnywhere, Category = "Vent")
	FVector Direction = FVector(1.0, 0.0, 0.0);

	/** Puffs emitted per second */
	UPROPERTY(EditAnywhere, Category = "Vent")
	float Rate = 2.0f;

	/** Initial speed, cm/s */
	UPROPERTY(EditAnywhere, Category = "Vent")
	float Speed = 60.0f;

	/** Random cone half-angle of the initial direction, radians */
	UPROPERTY(EditAnywhere, Category = "Vent")
	float Spread = 0.45f;

	/** Puff lifetime, seconds */
	UPROPERTY(EditAnywhere, Category = "Vent")
	float Life = 8.0f;

	/** Puff diameter at birth and at the end of its life, cm */
	UPROPERTY(EditAnywhere, Category = "Vent")
	float StartSize = 70.0f;

	UPROPERTY(EditAnywhere, Category = "Vent")
	float EndSize = 420.0f;

	/** Peak opacity of one puff */
	UPROPERTY(EditAnywhere, Category = "Vent")
	float Opacity = 0.15f;
};

/**
 * Liquid-oxygen boil-off and vent plumes around the vehicle. Cold vapor is denser than air: it sinks, spreads and
 * drifts downwind (NWS: wind from 220 deg at 5 kt at launch, stronger with height). Drawn with one instanced
 * quad mesh per vent (lit translucent material, opacity in per-instance custom data 0), facing the player camera.
 * If Vents is empty, the AS-506 vent layout is used (BuildDefaultVents).
 */
UCLASS()
class ACryoVaporField : public AActor
{
	GENERATED_BODY()

public:
	ACryoVaporField();

	UPROPERTY(EditAnywhere, Category = "Vapor")
	TArray<FCryoVent> Vents;

	UPROPERTY(EditAnywhere, Category = "Vapor")
	TObjectPtr<UMaterialInterface> VaporMaterial;

	/** Quad mesh, facing +Z, 100 cm across (the engine plane) */
	UPROPERTY(EditAnywhere, Category = "Vapor")
	TObjectPtr<UStaticMesh> PuffMesh;

	/** Horizontal direction the wind blows toward (compass 40 deg = world (-0.766, -0.643)) */
	UPROPERTY(EditAnywhere, Category = "Vapor")
	FVector WindToward = FVector(-0.766, -0.643, 0.0);

	/** Wind speed at 60 ft above the ground, cm/s (5 kt) */
	UPROPERTY(EditAnywhere, Category = "Vapor")
	float WindSpeedRef = 260.0f;

	/** Height of the surrounding ground below the actor origin (hardstand top), cm */
	UPROPERTY(EditAnywhere, Category = "Vapor")
	float GroundBelowOrigin = 1280.0f;

	/** Downward acceleration of the cold vapor, cm/s^2, and its terminal sink speed, cm/s */
	UPROPERTY(EditAnywhere, Category = "Vapor")
	float SinkAccel = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Vapor")
	float MaxSinkSpeed = 80.0f;

	/** Random acceleration amplitude, cm/s^2 */
	UPROPERTY(EditAnywhere, Category = "Vapor")
	float Turbulence = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Vapor")
	int32 MaxPerVent = 80;

	/** Seconds simulated at BeginPlay so the plumes already exist when play starts */
	UPROPERTY(EditAnywhere, Category = "Vapor")
	float PrewarmSeconds = 8.0f;

	UFUNCTION(BlueprintCallable, Category = "Vapor")
	int32 GetLiveCount() const;

	/** Fills Vents with the AS-506 layout (LOX tank boil-off, S-IC forward vents, umbilical plate leaks) */
	UFUNCTION(CallInEditor, Category = "Vapor")
	void BuildDefaultVents();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FPuff
	{
		FVector Pos = FVector::ZeroVector;
		FVector Vel = FVector::ZeroVector;
		float Age = 0.0f;
		float Life = 1.0f;
		float Size0 = 1.0f;
		float Size1 = 1.0f;
		float Opacity = 0.0f;
		float Roll = 0.0f;
		bool bAlive = false;
	};

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PuffComponents;

	TArray<TArray<FPuff>> Puffs;
	TArray<float> SpawnAccum;
	FRandomStream Rng;

	void Simulate(float Dt);
	void UpdateInstances(const FVector& CameraLocation);
	static float VehicleRadius(float HeightAboveDeck);
};
