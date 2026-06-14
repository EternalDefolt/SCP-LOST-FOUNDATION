// A simple test-room light switch: walk into its trigger zone and the linked
// lights toggle on/off. Lights are assigned per-instance (set from the level/Python).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LightLever.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class TEST_API ALightLever : public AActor
{
	GENERATED_BODY()

public:
	ALightLever();

	// Lights this lever switches. Filled in when the level is built.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lever")
	TArray<AActor*> Lights;

	// Seconds to ignore re-triggers so a single walk-through is one toggle.
	UPROPERTY(EditAnywhere, Category = "Lever")
	float Debounce = 0.6f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Lever")
	UStaticMeshComponent* LeverMesh;

	UPROPERTY(VisibleAnywhere, Category = "Lever")
	UBoxComponent* Trigger;

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

private:
	bool bOn = true;
	float LastToggle = -1000.f;
};
