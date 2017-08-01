#pragma once

struct Worker;
struct Base;
struct MinPatch;
struct GasPatch;
struct CC;

struct MinPatch {
	std::list<Worker> workers;
	BWAPI::Unit u = nullptr;
};

struct GasPatch {
	std::list<Worker> workers;
	BWAPI::Unit u = nullptr;
};

struct Worker {
	BWAPI::Unit u = nullptr;
	Base* base = nullptr;
	MinPatch* min_patch = nullptr;
	GasPatch* gas_patch = nullptr;
};

struct Base {
	BWAPI::TilePosition tile_pos;
	BWAPI::Unit cc;

	std::list<Worker> min_workers;
	std::list<Worker> gas_workers;
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

	void draw();

	std::list<Base> bases;
};

