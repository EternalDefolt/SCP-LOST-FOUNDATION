// Game mode that spawns the bodycam first-person character.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BodycamGameMode.generated.h"

UCLASS()
class TEST_API ABodycamGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABodycamGameMode();
};
