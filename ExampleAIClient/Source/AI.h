#pragma once

struct Worker;
struct Base;
struct MinPatch;
struct Geyser;

struct MinPatch {
	std::list<Worker> workers;
	BWAPI::Unit u;
	bool isEmpty() {
		return u->getResources() <= 0;
	}
};

struct Worker {
	BWAPI::Unit u;
	Base* base = nullptr;
};

struct Base {
	std::list<Worker> min_workers;
	std::list<Worker> gas_workers;
};

class AI
{
public:
	AI();
	~AI();
	void onUnitCreated(BWAPI::Unit u);
};

