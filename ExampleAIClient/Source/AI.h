#pragma once

struct Worker;
struct Base;
struct MinPatch;
struct GasPatch;
struct CC;

struct MinPatch {
	std::list<Worker*> workers;
	BWAPI::Unit u = nullptr;
};

struct GasPatch {
	std::list<Worker*> workers;
	BWAPI::Unit u = nullptr;
};

struct Worker {
	BWAPI::Unit u = nullptr;
	Base* base = nullptr;
	MinPatch* min_patch = nullptr;
	GasPatch* gas_patch = nullptr;
	bool has_job = false;
};

struct BuildJob {
	Worker* w;
	BWAPI::TilePosition target_position;
	BWAPI::UnitType type;
	BWAPI::Unit target = nullptr;
};

bool operator==(const Worker& w1, const Worker& w2);

struct Base {
	int id = 0;
	BWAPI::TilePosition tile_pos;
	BWAPI::Unit cc = nullptr;
	bool expanding = false;

	std::list<Worker*> min_workers;
	std::list<Worker*> gas_workers;
	std::list<MinPatch> min_patches;
	std::list<GasPatch> gas_patches;
};

class AI
{
public:
	AI();
	~AI();
	void onGameStarted();
	void onUnitCreated(BWAPI::Unit u);
	void onUnitCompleted(BWAPI::Unit u);
	void tick();
	void draw();

	Worker* getClosestWorker(BWAPI::TilePosition p);
	void freeWorker(Worker* w);
	BWAPI::Player self();

	BWAPI::Position buildingSize(BWAPI::UnitType type);

	std::list<Base> bases;

	int mins();
	int gas();
	int supply();

	int reserved_min = 0;
	int reserved_gas = 0;
	int future_supply = 0;

	std::list<Worker*> workers;
	std::list<BuildJob> build_jobs;

	std::list<BWAPI::Unit> incomplete_units;
};

