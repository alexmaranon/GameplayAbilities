// Copyright Epic Games, Inc. All Rights Reserved.

#include "GAS_Tarea3GameMode.h"
#include "GAS_Tarea3Character.h"
#include "UObject/ConstructorHelpers.h"

AGAS_Tarea3GameMode::AGAS_Tarea3GameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
