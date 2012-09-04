#include "Game.h"

#include <base/EntityManager.h>
#include "systems/TransformationSystem.h"
#include "systems/RenderingSystem.h"
#include "systems/ButtonSystem.h"
#include "systems/ADSRSystem.h"
#include "systems/TextRenderingSystem.h"
#include "systems/SoundSystem.h"
#include "systems/MusicSystem.h"
#include "systems/ContainerSystem.h"
#include "systems/PhysicsSystem.h"
#include "systems/ParticuleSystem.h"
#include "systems/ScrollingSystem.h"
#include "systems/MorphingSystem.h"
#include "api/AssetAPI.h"
#include "base/PlacementHelper.h"
#include <sstream>

Game::Game() {
	/* create EntityManager */
	EntityManager::CreateInstance();

	/* create systems singleton */
	TransformationSystem::CreateInstance();
	RenderingSystem::CreateInstance();
	SoundSystem::CreateInstance();
    MusicSystem::CreateInstance();
	ADSRSystem::CreateInstance();
	ButtonSystem::CreateInstance();
	TextRenderingSystem::CreateInstance();
	ContainerSystem::CreateInstance();
	PhysicsSystem::CreateInstance();
    ParticuleSystem::CreateInstance();
    ScrollingSystem::CreateInstance();
    MorphingSystem::CreateInstance();
}

Game::~Game() {
    LOGW("Delete game instance %p", this);
    theEntityManager.deleteAllEntities();
    EntityManager::DestroyInstance();

    TransformationSystem::DestroyInstance();
    RenderingSystem::DestroyInstance();
    SoundSystem::DestroyInstance();
    MusicSystem::DestroyInstance();
    ADSRSystem::DestroyInstance();
    ButtonSystem::DestroyInstance();
    TextRenderingSystem::DestroyInstance();
    ContainerSystem::DestroyInstance();
    PhysicsSystem::DestroyInstance();
    ParticuleSystem::DestroyInstance();
    ScrollingSystem::DestroyInstance();
    MorphingSystem::DestroyInstance();
}

void Game::loadFont(AssetAPI* asset, const std::string& name) {
	FileBuffer file = asset->loadAsset(name + ".desc");
	std::stringstream sfont;
	sfont << std::string((char*)file.data, file.size);
	std::string line;
	std::map<unsigned char, float> h2wratio;
	while (getline(sfont, line)) {
		if (line[0] == '#')
			continue;
		int c, w, h;
		sscanf(line.c_str(), "%d,%d,%d", &c, &w, &h);
		h2wratio[c] = (float)w / h;
	}
	delete[] file.data;
	h2wratio[' '] = h2wratio['a'];
	theTextRenderingSystem.registerFont(name, h2wratio);
}

void Game::sacInit(int windowW, int windowH) {
	PlacementHelper::ScreenHeight = 10;
    PlacementHelper::ScreenWidth = PlacementHelper::ScreenHeight * windowW / (float)windowH;
    PlacementHelper::WindowWidth = windowW;
    PlacementHelper::WindowHeight = windowH;
    PlacementHelper::GimpWidth = 800.0f;
    PlacementHelper::GimpHeight = 1280.0f;

	theRenderingSystem.setWindowSize(windowW, windowH, PlacementHelper::ScreenWidth, PlacementHelper::ScreenHeight);
	theTouchInputManager.init(Vector2(PlacementHelper::ScreenWidth, PlacementHelper::ScreenHeight), Vector2(windowW, windowH));

	theRenderingSystem.init();
}

void Game::backPressed() {}
		
void Game::togglePause(bool activate) {}
		
int Game::saveState(uint8_t** out) {
	return 0;
}