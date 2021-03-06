#include "stdafx.h"

#include "AI.h"

void drawStats();
void drawBullets();
void drawVisibilityData();
void showPlayers();
void showForces();
bool show_bullets;
bool show_visibility_data;



void reconnect()
{
	while (!BWAPIClient.connect())
	{
		std::this_thread::sleep_for(1s);
	}
}

AI* ai = nullptr;

std::thread* map_thread = nullptr;
bool analyzis_finished = false;

int main(int argc, const char* argv[])
{
	std::cout << "Connecting..." << std::endl;;
	reconnect();
	while (true)
	{
		std::cout << "waiting to enter match" << std::endl;
		while (!Broodwar->isInGame())
		{
			BWAPI::BWAPIClient.update();
			if (!BWAPI::BWAPIClient.isConnected())
			{
				std::cout << "Reconnecting..." << std::endl;;
				reconnect();
			}
		}

		//map_thread = new std::thread([&]() {
		cout << "Analyzing map ... ";
		BWTA::analyze();
		analyzis_finished = true;
		//BWTA::cleanMemory();
		cout << " DONE!" << endl;
		//});

		

		std::cout << "starting match!" << std::endl;
		//Broodwar->sendText("power overwhelming");
		Broodwar << "The map is " << Broodwar->mapName() << ", a " << Broodwar->getStartLocations().size() << " player map" << std::endl;
		// Enable some cheat flags
		Broodwar->enableFlag(Flag::UserInput);
		// Uncomment to enable complete map information
		//Broodwar->enableFlag(Flag::CompleteMapInformation);
		
		ai = new AI();
		ai->onGameStarted();

		show_bullets = false;
		show_visibility_data = false;

		if (Broodwar->enemy())
			Broodwar << "The match up is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;

		while (Broodwar->isInGame())
		{
			for (auto &e : Broodwar->getEvents())
			{
				switch (e.getType())
				{
				case EventType::MatchEnd:
					if (e.isWinner())
						Broodwar << "I won the game" << std::endl;
					else
						Broodwar << "I lost the game" << std::endl;
					break;
				case EventType::SendText:
					if (e.getText() == "/show bullets")
					{
						show_bullets = !show_bullets;
					}
					else if (e.getText() == "/show players")
					{
						showPlayers();
					}
					else if (e.getText() == "/show forces")
					{
						showForces();
					}
					else if (e.getText() == "/show visibility")
					{
						show_visibility_data = !show_visibility_data;
					}
					else if (e.getText() == "/r") {
						Broodwar->restartGame();
					}
					else
					{
						//Broodwar << "You typed \"" << e.getText() << "\"!" << std::endl;
						Broodwar->sendText(e.getText().c_str());
					}
					break;
				case EventType::ReceiveText:
					Broodwar << e.getPlayer()->getName() << " said \"" << e.getText() << "\"" << std::endl;
					break;
				case EventType::PlayerLeft:
					Broodwar << e.getPlayer()->getName() << " left the game." << std::endl;
					break;
				case EventType::NukeDetect:
					if (e.getPosition() != Positions::Unknown)
					{
						Broodwar->drawCircleMap(e.getPosition(), 40, Colors::Red, true);
						Broodwar << "Nuclear Launch Detected at " << e.getPosition() << std::endl;
					}
					else
						Broodwar << "Nuclear Launch Detected" << std::endl;
					break;
				case EventType::UnitCreate:
					ai->onUnitCreated(e.getUnit());
					break;
				case EventType::UnitDestroy:
					ai->onUnitDestroyed(e.getUnit());
					break;
				case EventType::UnitMorph:
					
					break;
				case EventType::UnitShow:
					ai->onUnitShow(e.getUnit());
					break;
				case EventType::UnitHide:
					ai->onUnitHide(e.getUnit());
					break;
				case EventType::UnitRenegade:
					if (!Broodwar->isReplay())
						Broodwar->sendText("A %s [%p] is now owned by %s", e.getUnit()->getType().c_str(), e.getUnit(), e.getUnit()->getPlayer()->getName().c_str());
					break;
				case EventType::SaveGame:
					Broodwar->sendText("The game was saved to \"%s\".", e.getText().c_str());
					break;
				}
			}

			ai->tick();

			if (show_bullets)
				drawBullets();

			if (show_visibility_data)
				drawVisibilityData();

			//drawStats();
			Broodwar->drawTextScreen(300, 0, "FPS: %f", Broodwar->getAverageFPS());

			ai->draw();

			BWAPI::BWAPIClient.update();
			if (!BWAPI::BWAPIClient.isConnected())
			{
				std::cout << "Reconnecting..." << std::endl;
				reconnect();
			}
		}
		std::cout << "Game ended" << std::endl;
		BWTA::cleanMemory();
	}
	std::cout << "Press ENTER to continue..." << std::endl;
	std::cin.ignore();
	return 0;
}

void drawStats()
{
	int line = 0;
	Broodwar->drawTextScreen(5, 0, "I have %d units:", Broodwar->self()->allUnitCount());
	for (auto& unitType : UnitTypes::allUnitTypes())
	{
		int count = Broodwar->self()->allUnitCount(unitType);
		if (count)
		{
			Broodwar->drawTextScreen(5, 16 * line, "- %d %s%c", count, unitType.c_str(), count == 1 ? ' ' : 's');
			++line;
		}
	}
}

void drawBullets()
{
	for (auto &b : Broodwar->getBullets())
	{
		Position p = b->getPosition();
		double velocityX = b->getVelocityX();
		double velocityY = b->getVelocityY();
		Broodwar->drawLineMap(p, p + Position((int)velocityX, (int)velocityY), b->getPlayer() == Broodwar->self() ? Colors::Green : Colors::Red);
		Broodwar->drawTextMap(p, "%c%s", b->getPlayer() == Broodwar->self() ? Text::Green : Text::Red, b->getType().c_str());
	}
}

void drawVisibilityData()
{
	int wid = Broodwar->mapHeight(), hgt = Broodwar->mapWidth();
	for (int x = 0; x < wid; ++x)
		for (int y = 0; y < hgt; ++y)
		{
			if (Broodwar->isExplored(x, y))
				Broodwar->drawDotMap(x * 32 + 16, y * 32 + 16, Broodwar->isVisible(x, y) ? Colors::Green : Colors::Blue);
			else
				Broodwar->drawDotMap(x * 32 + 16, y * 32 + 16, Colors::Red);
		}
}

void showPlayers()
{
	Playerset players = Broodwar->getPlayers();
	for (auto p : players)
		Broodwar << "Player [" << p->getID() << "]: " << p->getName() << " is in force: " << p->getForce()->getName() << std::endl;
}

void showForces()
{
	Forceset forces = Broodwar->getForces();
	for (auto f : forces)
	{
		Playerset players = f->getPlayers();
		Broodwar << "Force " << f->getName() << " has the following players:" << std::endl;
		for (auto p : players)
			Broodwar << "  - Player [" << p->getID() << "]: " << p->getName() << std::endl;
	}
}
