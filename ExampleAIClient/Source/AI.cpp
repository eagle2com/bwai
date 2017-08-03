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
	map_w = Broodwar->mapWidth();
	map_h = Broodwar->mapHeight();
	current_attack_target = BWAPI::Position(BWAPI::TilePosition(map_w/2, map_h/2));

	build_grid.resize(map_w*map_h);
	std::fill(build_grid.begin(), build_grid.end(), 0);

	auto start = BWTA::getStartLocation(Broodwar->self())->getTilePosition();
	auto starts = BWTA::getBaseLocations();

	bool start_added = false;

	cout << "BWTA start location: " << start << endl;
	cout << "BWAPI start location: " << Broodwar->self()->getStartLocation() << endl;

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
		//const auto d1 = BWTA::getGroundDistance(lh.tile_pos, start);
		//const auto d2 = BWTA::getGroundDistance(rh.tile_pos, start);
		const auto d1 = lh.tile_pos.getDistance(start);
		const auto d2 = rh.tile_pos.getDistance(start);
		return d1 <= d2;
	});

	int i = 0;
	for (auto& b : bases) {
		b.id = i++;
		cout << "Base at: " << b.tile_pos << ", id: " << b.id << endl;
	}
}

void AI::assignWorkerToBase(Worker * w) {
	w->u->stop();
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

	w->base = min_base;
	min_base->min_workers.push_back(w);
}

void AI::onUnitDestroyed(BWAPI::Unit u) {
	if (u->getPlayer() == self()) {
		if (u->getType().isBuilding() || u->getType().isAddon()) {
			setGridUsed(u->getType(), u->getTilePosition(), 0);
		}
		
		if (u->getType() == BWAPI::UnitTypes::Terran_Marine) {
			marines.remove(u);
		}
		if (u->getType() == BWAPI::UnitTypes::Terran_Barracks) {
			barracks.remove(u);
		}
	}
}

void AI::onUnitCompleted(BWAPI::Unit u) {
	if (u->getType() == BWAPI::UnitTypes::Terran_Supply_Depot) {
		future_supply -= u->getType().supplyProvided();
	}
	else if (u->getType() == BWAPI::UnitTypes::Terran_Barracks) {
		barracks.push_back(u);
		future_barracks--;
	}
	else if (u->getType() == BWAPI::UnitTypes::Terran_Marine) {
		u->attack(current_attack_target);
		marines.push_back(u);
	}
}

void AI::onUnitCreated(BWAPI::Unit u)
{
	if (u->getPlayer() == self()) {

		if (u->getType().isBuilding()) {
			setGridUsed(u->getType(), u->getTilePosition(), 1);
		}

		incomplete_units.push_back(u);

		if (Broodwar->getFrameCount() > 0 && u->getType().isBuilding()) {
			reserved_min -= u->getType().mineralPrice();
			reserved_gas -= u->getType().gasPrice();
		}

		//cout << "unit created: " << u->getType().getName().c_str() << endl;
		if(u->getType().isWorker()) {
			Worker* w = new Worker();
			w->u = u;
			workers.push_back(w);
			assignWorkerToBase(w);
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

		MinPatch *patch = new MinPatch();
		patch->u = u;
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

	// Military
	if (!targets.empty()) {
		BWAPI::Unit u = targets.begin()->second;
		int i = 0;
		int frame = Broodwar->getFrameCount();
		for (auto& m : marines) {
			// limit attacks to 1 per 2 seconds
			if ((i++ + frame) % 50 == 0) {
				m->attack(u->getPosition());
			}
		}
		
		current_attack_target = u->getPosition();
	}

	// Check the worker list
	for (auto it = workers.begin(); it != workers.end();) {
		Worker* w = *it;
		if (!w->u->exists()) {
			if (w->min_patch != nullptr) {
				w->base->min_workers.remove(w);
				w->min_patch->workers.remove(w);
			}
			else if (w->gas_patch != nullptr) {
				w->base->gas_workers.remove(w);
				w->gas_patch->workers.remove(w);
			}
			it = workers.erase(it);
		}
		else {
			it++;
		}
	}

	// Check mineral patches
	for (auto &b : bases) {
		for (auto min_it = b.min_patches.begin(); min_it != b.min_patches.end(); ) {
			MinPatch *p = *min_it;
			if (p->u->getResources() <= 4) {
				min_it = b.min_patches.erase(min_it);
				for(auto& w: p->workers) {
					w->min_patch = nullptr;
					w->base->min_workers.remove(w);
					assignWorkerToBase(w);
				}
			}
			else {
				min_it++;
			}
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
	int production_facilities = barracks.size()*2;
	int used_min_patches = 0;

	for (auto& b : bases) {
		if (b.cc != nullptr && b.cc->isCompleted()) {
			active_bases++; //TODO: Change this into a better production evaluation
			production_facilities++;
			used_min_patches += b.min_patches.size();
		}
	}

	for (auto& b : bases) {
		if (b.cc != nullptr) {
			// if the CC on a base was destroyed, remove it
			if (!b.cc->exists()) {
				b.cc = nullptr;
				b.expanding = false;
				for (auto& p : b.min_patches) {
					p->workers.clear();
				}
				for (auto& w : b.min_workers) {
					w->min_patch = nullptr;
					w->base = nullptr;
					assignWorkerToBase(w);
				}

				b.min_workers.clear();
			}
		}
		if (b.cc != nullptr && workers.size() < used_min_patches*2) {
			if (b.cc->getTrainingQueue().empty() && mins() > 50) {
				b.cc->train(BWAPI::UnitTypes::Terran_SCV);
			}
			
		}
		for (auto& w : b.min_workers) {
			if (w->min_patch == nullptr && w->u->isCompleted()) {
				if (!b.min_patches.empty()) {
					MinPatch* patch = b.min_patches.front();
					int min_workers = 2;
					int min_dist = std::numeric_limits<int>::max();
					for (auto& p : b.min_patches) {
						if (p->workers.size() < min_workers) {
							min_workers = p->workers.size();
							patch = p;
						}

						else if (p->workers.size() == min_workers && p->u->getDistance(w->u) < min_dist) {
							min_dist = p->u->getDistance(w->u);
							patch = p;
						}
					}
					w->min_patch = patch;
					w->min_patch->workers.push_back(w);
				}
				else {
					b.min_workers.remove(w);
					assignWorkerToBase(w);
				}
			}

			if (w->u->isIdle() && w->u->isCompleted()) {
				if(w->min_patch != nullptr) w->u->rightClick(w->min_patch->u);
			}
		}
	}

	if (supply() <= 2*2*production_facilities && self()->supplyTotal() < 400) {
		if (mins() > 100) {
			reserved_min += 100;
			//auto tile_pos = Broodwar->getBuildLocation(BWAPI::UnitTypes::Terran_Supply_Depot, self()->getStartLocation());
			//Worker* w = getClosestWorker(tile_pos);
			Worker* w = getLastWorker();
			//freeWorker(w);
			BuildJob job;
			job.type = BWAPI::UnitTypes::Terran_Supply_Depot;
			job.w = w;
			job.w->has_job = true;
			build_jobs.push_back(job);

			future_supply += job.type.supplyProvided();
		}
	}

	for (auto& barrack : barracks) {
		if (barrack->getTrainingQueue().size() == 0 && mins() > 50) {
			barrack->train(BWAPI::UnitTypes::Terran_Marine);
		}
	}

	if (mins() > 150 && barracks.size() + future_barracks < active_bases * 3) {
		future_barracks++;
		Worker* w = getLastWorker();
		BuildJob job;
		job.w = w;
		job.w->has_job = true;
		job.type = BWAPI::UnitTypes::Terran_Barracks;
		build_jobs.push_back(job);
		reserved_min += 150;
	}

	if (mins() > 400) {
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


		if (job->target_position.x == -1) {
			bool stop_job = false;
			job->target_position = Broodwar->getBuildLocation(job->type, job->w->u->getTilePosition());
			int i = 5;
			while (!isGridFree(job->type, job->target_position)) {
				job->target_position = Broodwar->getBuildLocation(job->type, job->target_position + 
				BWAPI::TilePosition{1, 1});
				if (i-- == 0) {
					if (job->type == BWAPI::UnitTypes::Terran_Supply_Depot) {
						future_supply -= 10;
					}
					else if (job->type == BWAPI::UnitTypes::Terran_Barracks) {
						future_barracks--;
					}
					stop_job = true;
					break;
				}
			}

			if (stop_job) {
				cout << job->type << " job canceled" << endl;
				job->w->has_job = false;
				job = build_jobs.erase(job);
				continue;
			}
		}

		setGridUsed(job->type, job->target_position, 1);

		// check if worker is alive
		if (!job->w->u->exists()) {
			job->w = getClosestWorker(job->target_position);
			job->w->has_job = true;
		}

		if (job->target == nullptr) {
			if (job->w->u->getBuildUnit() != nullptr) {
				job->target = job->w->u->getBuildUnit();
				//cout << "Build target: " << job->target << endl;
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
			// if the building was destroyed
			if (!job->target->exists()) {
				job->target = nullptr;
			}
			else if (job->target->isCompleted()) {
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

Worker* AI::getLastWorker() {
	for (auto it = workers.rbegin(); it != workers.rend(); it++) {
		Worker* w = (*it);
		if (!w->u->isCompleted()) continue;
		if (!w->has_job) {
			return w;
		}
	}

	return nullptr;
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
		"supply: %d\n",
		mins(), reserved_min, gas(), reserved_gas, supply());

	Broodwar->drawTextScreen(0, 13*3, "targets: %d", targets.size());
	Broodwar->drawTextScreen(0, 13 * 4, "APM: %d", Broodwar->getAPM());

	for (auto& b : bases) {
		auto cc = BWAPI::UnitTypes::Terran_Command_Center;
		auto p = BWAPI::Position(b.tile_pos);
		Broodwar->drawBoxMap(p.x, p.y,
			p.x + cc.dimensionRight() + cc.dimensionLeft(), p.y + cc.dimensionDown() + cc.dimensionUp(), BWAPI::Colors::Green);
		for (auto& patch : b.min_patches) {
			auto pos = patch->u->getPosition();
			int r = patch->u->getType().dimensionRight();
			Broodwar->drawCircleMap(pos, r, BWAPI::Colors::Green);
			Broodwar->drawTextMap(pos.x-5, pos.y-10, "%d: %d", patch->workers.size(), patch->u->getResources());
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
		
	for (auto& pair : targets) {
		BWAPI::Unit u = pair.second;
		Broodwar->drawCircleMap(u->getPosition(), u->getType().dimensionRight(), BWAPI::Colors::Red);
	}

	auto mouse_pos = Broodwar->getMousePosition();
	auto world_pos = mouse_pos + Broodwar->getScreenPosition();
	auto tile_pos = BWAPI::TilePosition(world_pos);

	Broodwar->drawTextMouse(0, 20, "%d %d\n%d %d", world_pos.x, world_pos.y, tile_pos.x, tile_pos.y);
}

void AI::setGridUsed(BWAPI::UnitType type, BWAPI::TilePosition tile, int status) {
	for (int x = tile.x; x < tile.x + type.tileWidth(); x++) {
		for (int y = tile.y; y < tile.y + type.tileHeight(); y++) {
			build_grid[x + map_w*y] = status;
		}
	}
}

bool AI::isGridFree(BWAPI::UnitType type, BWAPI::TilePosition tile) {
	// Broodwar returns an out of bounds results if it could not find a tile
	if (tile.x >= map_w || tile.y >= map_h) return false;
	for (int x = tile.x; x < tile.x + type.tileWidth(); x++) {
		for (int y = tile.y; y < tile.y + type.tileHeight(); y++) {
			if (build_grid[x + map_w*y] == 1) {
				return false;
			}
		}
	}

	return true;
}


void AI::onUnitShow(BWAPI::Unit u) {
	if (!u->getPlayer()->isEnemy(self())) return;

	//this is the first target we see again
	/*if (targets.empty()) {
		current_attack_target = u->getPosition();
		cout << "Attacking  " << current_attack_target << endl;
		// attack-move all the marines to it
		for (auto& m : marines) {
			m->attack(u->getPosition());
		}
	}*/
	targets[u->getID()] = u;
}

void AI::onUnitHide(BWAPI::Unit u) {
	if (!u->getPlayer()->isEnemy(self())) return;
	targets.erase(u->getID());

	/*if (targets.size() == 1) {
		current_attack_target = u->getPosition();
		cout << "Attacking  " << current_attack_target << endl;
		// attack-move all the marines to it
		for (auto& m : marines) {
			m->attack(u->getPosition());
		}
	}*/
}