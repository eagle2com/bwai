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
	BWAPI::TilePosition target_position = { -1,1 };
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
	std::list<MinPatch*> min_patches;
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
	void onUnitDestroyed(BWAPI::Unit u);
	void onUnitShow(BWAPI::Unit u);
	void onUnitHide(BWAPI::Unit u);
	void tick();
	void draw();


	void assignWorkerToBase(Worker* w);
	Worker* getClosestWorker(BWAPI::TilePosition p);
	Worker* getLastWorker();

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

	std::list<BWAPI::Unit> barracks;
	int future_barracks = 0;
	std::list<BWAPI::Unit> marines;
	std::map<int, BWAPI::Unit> targets;
	BWAPI::Position current_attack_target;

	// We need to fill a build grid when constructing otherwise
	// two jobs could receive the same empty space as target positions
	std::vector<int> build_grid;
	int map_w, map_h;
	void setGridUsed(BWAPI::UnitType type, BWAPI::TilePosition tile, int status);
	bool isGridFree(BWAPI::UnitType type, BWAPI::TilePosition tile);
};

