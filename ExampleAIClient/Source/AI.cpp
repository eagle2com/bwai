#include "stdafx.h"
#include "AI.h"


bool operator==(const Worker& w1, const Worker& w2) {
	return w1.u == w2.u;
}

AI::AI()
{
}


AI::~AI()
{
}

void AI::onGameStarted()
{
	auto start = BWTA::getStartLocation(Broodwar->self())->getTilePosition();
	auto starts = BWTA::getBaseLocations();

	bool start_added = false;

	for (auto& s : starts) {
		Base b;
		b.tile_pos = s->getTilePosition();
		bases.push_back(b);
		//cout << "Added base at: " << b.tile_pos << endl;
		if (b.tile_pos == start) start_added = true;
	}

	if (!start_added) {
		Base b;
		b.tile_pos = start;
		bases.push_back(b);
		//cout << "Added base at: " << b.tile_pos << endl;
	}

	bases.sort([&start](const Base& lh, const Base& rh) {
		const auto d1 = BWTA::getGroundDistance(lh.tile_pos, start);
		const auto d2 = BWTA::getGroundDistance(rh.tile_pos, start);
		return d1 <= d2;
	});

	int i = 0;
	for (auto& b : bases) {
		b.id = i++;
		//cout << "Base at: " << b.tile_pos << ", id: " << b.id << endl;
	}
}

void AI::onUnitCompleted(BWAPI::Unit u) {
	if (u->getType() == BWAPI::UnitTypes::Terran_Supply_Depot) {
		future_supply -= u->getType().supplyProvided();
	}
}

void AI::onUnitCreated(BWAPI::Unit u)
{
	if (u->getPlayer() == Broodwar->self()) {

		incomplete_units.push_back(u);

		if (Broodwar->getFrameCount() > 0 && u->getType().isBuilding()) {
			reserved_min -= u->getType().mineralPrice();
			reserved_gas -= u->getType().gasPrice();
		}

		//cout << "unit created: " << u->getType().getName().c_str() << endl;
		if(u->getType().isWorker()) {

			// assign to base with lowest count of workers
			int min_workers = std::numeric_limits<int>::max();
			Base* min_base = &bases.front();
			for (auto& b : bases) {
				// don't bother with bases without a cc (or an unfinished one)
				if (b.cc == nullptr || !b.cc->isCompleted()) continue; 
				if (b.min_workers.size() < min_workers) {
					min_workers = b.min_workers.size();
					min_base = &b;
				}
			}

			Worker* w = new Worker();
			w->base = min_base;
			w->u = u;
			min_base->min_workers.push_back(w);
			workers.push_back(w);
		}
		else if (u->getType().isResourceDepot()) {
			// If it is a command center, assign it to the correct base
			for (auto& b : bases) {
				if (u->getTilePosition() == b.tile_pos) {
					b.cc = u;
					cout << "Added cc to base " << b.id << endl;
					break;
				}
			}
		}
	}

	else if (u->getType().isMineralField()) {
		// If it is a mineral patch, add it to the correct base
		int min_dist = std::numeric_limits<int>::max();
		Base* min_b = &bases.front();
		for (auto& b : bases) {
			int dist = b.tile_pos.getApproxDistance(u->getTilePosition());
			if (dist < min_dist) {
				min_dist = dist;
				min_b = &b;
			}
		}

		MinPatch patch;
		patch.u = u;
		min_b->min_patches.push_back(patch);
		//cout << "Added mineral patch at " << u->getTilePosition() << " to base " << min_b.id << endl;
	}
	else if (u->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
		// If it is a gas geyser, add it to the correct base
		int min_dist = std::numeric_limits<int>::max();
		Base* min_b = &bases.front();
		for (auto& b : bases) {
			int dist = b.tile_pos.getApproxDistance(u->getTilePosition());
			if (dist < min_dist) {
				min_dist = dist;
				min_b = &b;
			}
		}

		GasPatch patch;
		patch.u = u;
		min_b->gas_patches.push_back(patch);
	}
}

void AI::tick() {

	for (auto it = workers.begin(); it != workers.end();) {
		Worker* w = *it;
		if (!w->u->exists()) {
			it = workers.erase(it);
		}
		else {
			it++;
		}
	}

	for (auto it = incomplete_units.begin(); it != incomplete_units.end(); ) {
		BWAPI::Unit u = *it;
		if (u->isCompleted()) {
			onUnitCompleted(u);
			it = incomplete_units.erase(it);
		}
		else if (!u->exists()) {
			it = incomplete_units.erase(it);
		}
		else {
			it++;
		}
	}

	int active_bases = 0;
	for (auto& b : bases) {
		if (b.cc != nullptr) {
			// if the CC on a base was destroyed, remove it
			if (!b.cc->exists()) {
				b.cc = nullptr;
				b.expanding = false;
			}
		}
		if (b.cc != nullptr) {
			if (b.cc->getTrainingQueue().empty()) {
				b.cc->train(BWAPI::UnitTypes::Terran_SCV);
			}
			active_bases++; //TODO: Change this into a better production evaluation
		}
		for (auto& w : b.min_workers) {
			if (w->min_patch == nullptr && w->u->isCompleted()) {
				MinPatch* patch = &b.min_patches.front();
				int min_workers = 3;
				int min_dist = std::numeric_limits<int>::max();
				for (auto& p : b.min_patches) {
					if (p.workers.size() < min_workers) {
						min_workers = p.workers.size();
						patch = &p;
					}

					else if (p.workers.size() == min_workers && p.u->getDistance(w->u) < min_dist) {
						min_dist = p.u->getDistance(w->u);
						patch = &p;
					}
				}
				w->min_patch = patch;
				w->min_patch->workers.push_back(w);
			}

			if (w->u->isIdle() && w->u->isCompleted()) {
				if(w->min_patch != nullptr) w->u->rightClick(w->min_patch->u);
			}
		}
	}

	if (supply() <= 2*2*active_bases) {
		if (mins() > 100) {
			reserved_min += 100;
			auto tile_pos = Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Supply_Depot, self()->getStartLocation());
			Worker* w = getClosestWorker(tile_pos);
			//freeWorker(w);
			BuildJob job;
			job.target_position = tile_pos;
			job.type = BWAPI::UnitTypes::Terran_Supply_Depot;
			job.w = w;
			job.w->has_job = true;
			build_jobs.push_back(job);

			future_supply += job.type.supplyProvided();
		}
	}

	else if (mins() > 400) {
		// Find the next base without a CC
		Base* base = nullptr;
		for (auto& b : bases) {
			if (b.cc == nullptr && !b.expanding) {
				reserved_min += 400;
				Worker* w = getClosestWorker(b.tile_pos);
				//freeWorker(w);
				BuildJob job;
				job.target_position = b.tile_pos;
				job.type = BWAPI::UnitTypes::Terran_Command_Center;
				job.w = w;
				job.w->has_job = true;
				build_jobs.push_back(job);

				b.expanding = true;
				break;
			}
		}
	}
	
	for (auto job = build_jobs.begin(); job != build_jobs.end();) {

		// check if worker is alive
		if (!job->w->u->exists()) {
			job->w = getClosestWorker(job->target_position);
		}

		if (job->target == nullptr) {
			if (job->w->u->getBuildUnit() != nullptr) {
				job->target = job->w->u->getBuildUnit();
				cout << "Build target: " << job->target << endl;
			}
		}

		if (job->target == nullptr) {
			if (!job->w->u->isConstructing() && job->target_position.getApproxDistance(job->w->u->getTilePosition()) > 5) {
				if (job->w->u->getOrderTargetPosition() != BWAPI::Position(job->target_position)) {
					job->w->u->move(BWAPI::Position(job->target_position));
				}
			}
			else {
				job->w->u->build(job->type, job->target_position);
			}

			job++;
		}
		else {
			if (job->target->isCompleted()) {
				job->w->has_job = false;
				job = build_jobs.erase(job);
			}
			else {
				job++;
			}
		}
	}
}

BWAPI::Player AI::self() {
	return Broodwar->self();
}

int AI::mins() {
	return self()->minerals() - reserved_min;
}

int AI::gas() {
	return self()->gas() - reserved_gas;
}

int AI::supply() {
	return self()->supplyTotal() + future_supply - self()->supplyUsed();
}

Worker* AI::getClosestWorker(BWAPI::TilePosition p) {
	int min_dist = std::numeric_limits<int>::max();
	Worker* min_w = workers.front();
	for (auto& w : workers) {
		if (!w->u->isCompleted() || w->has_job) continue;
		int dist = w->u->getTilePosition().getApproxDistance(p);
		if (dist < min_dist) {
			min_dist = dist;
			min_w = w;
		}
	}
	return min_w;
}

void AI::freeWorker(Worker* w) {
	if (w->min_patch != nullptr) {
		w->min_patch->workers.remove(w);
		w->base->min_workers.remove(w);
	}
	else if (w->gas_patch != nullptr) {
		w->gas_patch->workers.remove(w);
		w->base->gas_workers.remove(w);
	}
}

BWAPI::Position AI::buildingSize(BWAPI::UnitType type) {
	return BWAPI::Position(type.dimensionLeft() + type.dimensionRight(), type.dimensionUp() + type.dimensionDown());
}


void AI::draw() {

	Broodwar->drawTextScreen(0, 0, 
		"min: %d (%d)\n"
		"gas: %d (%d)\n"
		"supply: %d\n", mins(), reserved_min, gas(), reserved_gas, supply());

	for (auto& b : bases) {
		auto cc = BWAPI::UnitTypes::Terran_Command_Center;
		auto p = BWAPI::Position(b.tile_pos);
		Broodwar->drawBoxMap(p.x, p.y,
			p.x + cc.dimensionRight() + cc.dimensionLeft(), p.y + cc.dimensionDown() + cc.dimensionUp(), BWAPI::Colors::Green);
		for (auto& patch : b.min_patches) {
			auto pos = patch.u->getPosition();
			int r = patch.u->getType().dimensionRight();
			Broodwar->drawCircleMap(pos, r, BWAPI::Colors::Green);
			Broodwar->drawTextMap(pos, "%d", patch.workers.size());
		}

		for (auto& gas : b.gas_patches) {
			auto pos = gas.u->getPosition();
			auto g = BWAPI::UnitTypes::Resource_Vespene_Geyser;
			Broodwar->drawBoxMap(pos - BWAPI::Position{ g.dimensionLeft(), g.dimensionUp() }, pos + BWAPI::Position{ g.dimensionRight(), g.dimensionDown() }, BWAPI::Colors::Green);
		}
	}

	for (auto& job : build_jobs) {
		// Highlight builders and target positions
		Broodwar->drawCircleMap(job.w->u->getPosition(), BWAPI::UnitTypes::Terran_SCV.dimensionRight(), BWAPI::Colors::Blue);
		BWAPI::Position p = BWAPI::Position(job.target_position);
		Broodwar->drawBoxMap(p, p + buildingSize(job.type), BWAPI::Colors::Blue);
		Broodwar->drawLineMap(job.w->u->getPosition(), p, BWAPI::Colors::Blue);
	}
}