// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AeroFightersGameMode.h"
#include "AeroFightersPawn.h"

AAeroFightersGameMode::AAeroFightersGameMode()
{
	// set default pawn class to our flying pawn
	DefaultPawnClass = AAeroFightersPawn::StaticClass();
}
