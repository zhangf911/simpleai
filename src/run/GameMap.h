#pragma once

#include "GameEntity.h"
#include <server/Zone.h>
#include <common/Thread.h>
#include <set>
#include <iterator>
#include <iostream>

namespace ai {
namespace example {

class GameMap {
private:
	int _size;
	ai::Zone _zone;
	ai::Server& _server;
	typedef std::set<ai::example::GameEntity*> Entities;
	Entities _entities;
	MUTEX(_mutex);

public:
	GameMap(int size, const std::string& name, ai::Server& server) :
			_size(size), _zone(name), _server(server) {
		_server.addZone(&_zone);
	}

	~GameMap() {
		for (Entities::iterator i = _entities.begin(); i != _entities.end(); ++i) {
			delete *i;
		}
		_server.removeZone(&_zone);
	}

	inline const ai::Zone& getZone() const {
		return _zone;
	}

	inline GameEntity* getRandomEntity() const {
		SCOPEDLOCK(_mutex);
		if (_entities.empty())
			return nullptr;
		const int size = static_cast<int>(_entities.size());
		const int randomIndex = ai::random(0, size - 1);
		Entities::iterator i = _entities.begin();
		std::advance(i, randomIndex);
		return *i;
	}

	inline const std::string& getName() const {
		return _zone.getName();
	}

	inline GameEntity* addEntity(GameEntity* entity) {
		SCOPEDLOCK(_mutex);
		_entities.insert(entity);
		ai::AI& ai = *entity;
		_zone.addAI(&ai);
		// pick some random start position
		entity->setPosition(getStartPosition());
		return entity;
	}

	inline bool remove(const ai::CharacterId& id) {
		SCOPEDLOCK(_mutex);
		// TODO: improve
		for (Entities::iterator i = _entities.begin(); i != _entities.end(); ++i) {
			GameEntity* entity = *i;
			if (entity->getId() == id) {
				return remove(entity);
			}
		}
		return false;
	}

	inline bool remove(GameEntity* entity) {
		SCOPEDLOCK(_mutex);
		if (entity == nullptr)
			return false;
		if (_entities.erase(entity) != 1)
			return false;
		ai::AI& ai = *entity;
		_zone.removeAI(&ai);
		return true;
	}

	inline void update(long dt) {
		SCOPEDLOCK(_mutex);
		for (Entities::iterator i = _entities.begin(); i != _entities.end(); ++i) {
			(*i)->update(dt, _size);
		}
	}

	inline bool isBlocked(const ai::Vector3f& pos) const {
		if (pos.x < -_size || pos.x >= _size)
			return true;
		if (pos.z < -_size || pos.z >= _size)
			return true;
		return false;
	}

	void initializeAggro() {
		SCOPEDLOCK(_mutex);
		// TODO: remove me once we have an attack
		Entities::iterator i = _entities.begin();
		if (i == _entities.end())
			return;
		GameEntity *firstEntity = *i++;
		for (; i != _entities.end(); ++i) {
			ai::Entry* e = firstEntity->addAggro(**i, 1000.0f + static_cast<float>(rand() % 1000));
			e->setReduceByValue(1.0f + static_cast<float>(rand() % 3));
		}
	}

	// returns a random start position within the boundaries
	ai::Vector3f getStartPosition() const {
		const int x = (rand() % (2 * _size)) - _size;
		const float y = 0.0f;
		const int z = (rand() % (2 * _size)) - _size;
		return ai::Vector3f(static_cast<float>(x), y, static_cast<float>(z));
	}
};

}
}