#include "stdafx.h"
#include "AI.h"


AI::AI()
{
}


AI::~AI()
{
}

void AI::onUnitCreated(BWAPI::Unit u)
{
	if (u->getPlayer() == Broodwar->self()) {
		Broodwar->printf("unit created: %s", u->getType().getName().c_str());
	}
}
