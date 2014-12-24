#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <cstdlib>
#include <SimpleAI.h>
#include "actions/ExampleTask.h"
#include "GameEntity.h"
#include "GameMap.h"
#include <chrono>
#include <algorithm>

namespace {
int id = 1;
ai::GroupMgr groupManager;
}

static std::string getOptParam(char** begin, char** end, const std::string& option, const std::string& defaultVal = "") {
	char** itr = std::find(begin, end, option);
	if (itr != end && ++itr != end) {
		return *itr;
	}
	return defaultVal;
}

static bool optExists(char** begin, char** end, const std::string& option) {
	return std::find(begin, end, option) != end;
}

static ai::example::GameMap *createMap(ai::GroupMgr& groupManager, int amount, ai::Server& server, const ai::TreeNodePtr& root, const std::string& name) {
	ai::example::GameMap* map = new ai::example::GameMap(300, name, server);

	for (int i = 0; i < amount; ++i) {
		ai::example::GameEntity* e = map->addEntity(new ai::example::GameEntity(id++, root, groupManager));
		e->setPosition(map->getStartPosition());
	}

	map->initializeAggro();

	return map;
}

#ifndef AI_NO_THREADING
static void runMap(ai::example::GameMap* map) {
	const std::chrono::milliseconds delay(100);
	auto timeLast = std::chrono::steady_clock::now();
	for (;;) {
		const auto timeNow = std::chrono::steady_clock::now();
		const auto dt = std::chrono::duration_cast < std::chrono::milliseconds > (timeNow - timeLast).count();
		timeLast = timeNow;
		map->update(static_cast<uint32_t>(dt));
		std::this_thread::sleep_for(delay);
	}
}

static void runServer(ai::Server* server) {
	const std::chrono::milliseconds delay(500);
	auto timeLast = std::chrono::steady_clock::now();
	for (;;) {
		const auto timeNow = std::chrono::steady_clock::now();
		const auto dt = std::chrono::duration_cast < std::chrono::milliseconds > (timeNow - timeLast).count();
		timeLast = timeNow;
		server->update(dt);
		std::this_thread::sleep_for(delay);
	}
}

static void runDespawnSpawn(ai::example::GameMap* map, const ai::TreeNodePtr* root) {
	const std::chrono::milliseconds delay(5000);
	for (;;) {
		ai::example::GameEntity *rnd = map->getRandomEntity();
		if (rnd != nullptr) {
			map->remove(rnd);
		}

		ai::example::GameEntity* e = map->addEntity(new ai::example::GameEntity(id++, *root, groupManager));
		e->setPosition(map->getStartPosition());

		std::this_thread::sleep_for(delay);
	}
}

#endif

int main(int argc, char **argv) {
	char **b = argv;
	char **e = argv + argc;
	if (argc <= 1 || optExists(b, e, "-h") || optExists(b, e, "-help") || !optExists(b, e, "-file")) {
		std::cerr << "usage: simpleai-run -file behaviourtree.lua [options]" << std::endl;
		std::cerr << "Valid options are (default values are given here):" << std::endl;
		std::cerr << "-amount 10    - how many entities are spawned on each map" << std::endl;
		std::cerr << "-maps 4       - how many maps should get spawned" << std::endl;
		std::cerr << "-name example - the name of the behaviour tree in the given script" << std::endl;
		std::cerr << "-port 12345   - the port of the server to listen on" << std::endl;
		std::cerr << "-seed 1       - use a fixed seed for all the random actions" << std::endl;
		std::cerr << "-help -h      - show this help screen" << std::endl;
		return EXIT_FAILURE;
	}

	const int seed = std::stoi(getOptParam(b, e, "-seed", "1"));
	const int mapAmount = std::stoi(getOptParam(b, e, "-maps", "4"));
	const int amount = std::stoi(getOptParam(b, e, "-amount", "10"));
	const short port = static_cast<short>(std::stoi(getOptParam(b, e, "-port", "12345")));
	const std::string name = getOptParam(b, e, "-name", "example");
	const std::string filename = getOptParam(b, e, "-file");

	srand(seed);

	ai::AIRegistry registry;
	// add your own tasks and conditions here
	ai::LUATreeLoader loader(registry);

	std::ifstream t(filename);
	if (!t) {
		std::cerr << "could not load " << filename << std::endl;
		return EXIT_FAILURE;
	}

	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	if (!loader.init(str)) {
		std::cerr << "could not load the tree" << std::endl;
		std::cerr << loader.getError() << std::endl;
		return EXIT_FAILURE;
	}
	std::cerr << "loaded the tree: " << filename << std::endl;
	const ai::TreeNodePtr& root = loader.load(name);
	if (!root) {
		std::cerr << "could not find behaviour with the name " << name << std::endl;
		std::vector<std::string> trees;
		loader.getTrees(trees);
		std::cerr << "found " << trees.size() << " trees" << std::endl;
		for (std::vector<std::string>::const_iterator i = trees.begin(); i != trees.end(); ++i) {
			std::cerr << "found " << *i << std::endl;
		}
		return EXIT_FAILURE;
	}

	std::cout << *root.get() << std::endl;
	std::cout << "successfully loaded the behaviour tree " << name << std::endl;
	std::cout << "now run this behaviour tree with " << amount << " entities on each map" << std::endl;
	std::cout << "spawn " << mapAmount << " maps with seed " << seed << std::endl;
#ifdef AI_NO_THREADING
	std::cout << "compiled without threading support" << std::endl;
#else
	std::cout << "compiled with threading support" << std::endl;
#endif

	ai::Server server(port, "0.0.0.0");
	if (!server.start()) {
		std::cerr << "Could not start the server on port " << port << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << "Started server on port " << port << std::endl;

	std::vector<ai::example::GameMap*> maps;
	for (int i = 0; i < mapAmount; ++i) {
		maps.push_back(createMap(groupManager, amount, server, root, "Map" + std::to_string(i)));
	}

#ifdef AI_NO_THREADING
	const std::chrono::milliseconds delay(10);
	auto timeLast = std::chrono::steady_clock::now();
	for (;;) {
		const auto timeNow = std::chrono::steady_clock::now();
		const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - timeLast).count();
		timeLast = timeNow;
		for (std::vector<ai::example::GameMap*>::const_iterator i = maps.begin(); i != maps.end(); ++i) {
			(*i)->update(static_cast<uint32_t>(dt));
		}
		server.update(dt);
		std::this_thread::sleep_for(delay);
	}
#else
	typedef std::vector<std::thread> Threads;
	Threads threads;
	for (std::vector<ai::example::GameMap*>::const_iterator i = maps.begin(); i != maps.end(); ++i) {
		threads.push_back(std::thread(runMap, *i));
	}

	for (std::vector<ai::example::GameMap*>::const_iterator i = maps.begin(); i != maps.end(); ++i) {
		threads.push_back(std::thread(runDespawnSpawn, *i, &root));
	}

	threads.push_back(std::thread(runServer, &server));

	std::cout << "hit enter to quit" << std::flush;
	std::cin.get();
	std::cout << "quitting" << std::endl;

	for (Threads::iterator i = threads.begin(); i != threads.end(); ++i) {
		i->detach();
	}
#endif

	return EXIT_SUCCESS;
}
