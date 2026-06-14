#include "LightLever.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

ALightLever::ALightLever()
{
	PrimaryActorTick.bCanEverTick = false;

	LeverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeverMesh"));
	SetRootComponent(LeverMesh);
	// A small upright handle so the switch is visible in the room.
	LeverMesh->SetRelativeScale3D(FVector(0.15f, 0.15f, 0.6f));
	LeverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeObj(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeObj.Succeeded())
	{
		LeverMesh->SetStaticMesh(CubeObj.Object);
	}

	// Generous trigger zone so the player reliably toggles by walking up to it.
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(LeverMesh);
	Trigger->SetBoxExtent(FVector(120.f, 120.f, 120.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ALightLever::OnTriggerBegin);
}

void ALightLever::OnTriggerBegin(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastToggle < Debounce)
	{
		return;
	}
	LastToggle = Now;

	bOn = !bOn;
	for (AActor* L : Lights)
	{
		if (L)
		{
			L->SetActorHiddenInGame(!bOn);   // hides the light source's visuals
			L->SetActorEnableCollision(false);
			// actually drive the light: toggle every light component on the actor
			TArray<ULightComponent*> Comps;
			L->GetComponents<ULightComponent>(Comps);
			for (ULightComponent* C : Comps)
			{
				C->SetVisibility(bOn);
			}
		}
	}
}
