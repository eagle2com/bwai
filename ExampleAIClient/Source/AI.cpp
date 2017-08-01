#include "stdafx.h"
#include "AI.h"


AI::AI()
{
}


AI::~AI()
{
}

void AI::onGameStarted()
{
	auto starts = Broodwar->getStartLocations();
	auto start  = Broodwar->self()->getStartLocation();

	for (auto& s : starts) {
		Base b;
		b.tile_pos = s;
		bases.push_back(b);
		cout << "Added base at: " << s << endl;
	}
}

void AI::onUnitCreated(BWAPI::Unit u)
{
	if (u->getPlayer() == Broodwar->self()) {
		Broodwar->printf("unit created: %s", u->getType().getName().c_str());

		if (u->getType().isResourceDepot()) {
			// If it is a command center, assign it to the correct base
			for (auto& b : bases) {
				if (u->getTilePosition() == b.tile_pos) {
					b.cc = u;
					break;
				}
			}
			
		}
	}

	else if (u->getType().isMineralField()) {
		// If it is a mineral patch, add it to the correct base
		int min_dist = std::numeric_limits<int>::max();
		Base& min_b = bases.front();
		for (auto& b : bases) {
			int dist = b.tile_pos.getApproxDistance(u->getTilePosition());
			if (dist < min_dist) {
				min_dist = dist;
				min_b = b;
			}
		}

		MinPatch patch;
		patch.u = u;
		min_b.min_patches.push_back(patch);
		//cout << "Added mineral patch at " << u->getTilePosition() << " to base at " << min_b.tile_pos << endl;
	}
	else if (u->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
		// If it is a gas geyser, add it to the correct base
		int min_dist = std::numeric_limits<int>::max();
		Base& min_b = bases.front();
		for (auto& b : bases) {
			int dist = b.tile_pos.getApproxDistance(u->getTilePosition());
			if (dist < min_dist) {
				min_dist = dist;
				min_b = b;
			}
		}

		GasPatch patch;
		patch.u = u;
		min_b.gas_patches.push_back(patch);
	}
}

void AI::draw() {
	for (auto& b : bases) {
		auto p = BWAPI::Position(b.tile_pos);
		Broodwar->drawBoxMap(p.x, p.y, p.x + 32*4, p.y + 32*3, BWAPI::Colors::Green);
		for (auto& patch : b.min_patches) {
			auto pos = patch.u->getPosition();
			int r = patch.u->getType().dimensionRight();
			Broodwar->drawCircleMap(pos, r, BWAPI::Colors::Green);
		}
	}
}