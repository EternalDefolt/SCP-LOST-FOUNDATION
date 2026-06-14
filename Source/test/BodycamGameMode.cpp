// Game mode that spawns the bodycam first-person character.

#include "BodycamGameMode.h"
#include "BodycamCharacter.h"

ABodycamGameMode::ABodycamGameMode()
{
	DefaultPawnClass = ABodycamCharacter::StaticClass();
}
