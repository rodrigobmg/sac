/*
	This file is part of Heriswap.

	@author Soupe au Caillou - Pierre-Eric Pelloux-Prayer
	@author Soupe au Caillou - Gautier Pelloux-Prayer

	Heriswap is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, version 3.

	Heriswap is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Heriswap.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include <string>
#include <vector>
#include <map>
#include <stdint.h>
#include <iostream>
#include <cstring>
#include <cassert>
#include "base/Log.h"
#include "base/TimeUtil.h"


#define Entity unsigned long

#define COMPONENT(x) struct x##Component
#define PERSISTENT_PROP 
#define RUNTIME_PROP 

#define INSTANCE_DECL(T) static T* _instance;
#define INSTANCE_IMPL(T) T* T::_instance = 0;

#define UPDATABLE_SYSTEM(type) \
	class type##System : public ComponentSystemImpl<type##Component> {	\
		public:	\
            static type##System* GetInstancePointer() { return _instance; } \
			static type##System& GetInstance() { return (*_instance); } \
			static void CreateInstance() { if (_instance != NULL) { LOGW("Creating another instance of type##System"); } _instance = new type##System(); LOGW(#type "System new instance created: %p", _instance);} \
			static void DestroyInstance() { if (_instance) delete _instance; LOGW(#type "System instance destroyed, was: %p", _instance); _instance = NULL; } \
			void DoUpdate(float dt); \
		\
		private:	\
			type##System();	\
			static type##System* _instance;

class ComponentSystem {
	public:
		ComponentSystem(const std::string& n) : name(n) { bool inserted = registry.insert(std::make_pair(name, this)).second; assert(inserted); }
        virtual ~ComponentSystem() { registry.erase(name); }

		virtual const std::string& getName() const { return name; }
		virtual void Add(Entity entity) = 0;
		virtual void Delete(Entity entity) = 0;
		virtual int serialize(Entity entity, uint8_t** out) = 0;
		virtual void deserialize(Entity entity, uint8_t* out, int size) = 0;

		void Update(float dt) {  float time = TimeUtil::getTime(); DoUpdate(dt); timeSpent = TimeUtil::getTime() - time; }
		float timeSpent;
			
		static ComponentSystem* Named(const std::string& n) {
			std::map<std::string, ComponentSystem*>::iterator it = registry.find(n);
			if (it == registry.end()) {
				LOGE("System with name: '%s' does not exist", n.c_str());
				return 0;
			}
			return (*it).second;
		}
		
		static std::vector<std::string> registeredSystemNames();

	protected:
		virtual void DoUpdate(float dt) = 0;
		static std::map<std::string, ComponentSystem*> registry;
		std::string name;
};

template <typename T>
class ComponentSystemImpl: public ComponentSystem {
	public:
		ComponentSystemImpl(const std::string& t) : ComponentSystem(t) {
			Activate();
            previous = 0;
		}

		void Add(Entity entity) {
			assert (components.find(entity) == components.end());
			components.insert(std::make_pair(entity, new T()));
		}

		void Delete(Entity entity) {
			typename std::map<Entity, T*>::iterator it = components.find(entity);
			if (it != components.end()) {
				delete it->second;
				components.erase(it);
			}
		}

		T* Get(Entity entity) {
            if (entity != previous) {
    			typename std::map<Entity, T*>::iterator it = components.find(entity);
    			if (it == components.end()) {
    				LOGE("Entity %lu has no component of type '%s'", entity, name.c_str());
                    #ifndef ANDROID
                    // crash here
                    assert (false);
                    #endif
    				return 0;
    			}
                previous = entity;
                previousComp = (*it).second;
            }
			return previousComp;
		}

		std::vector<Entity> RetrieveAllEntityWithComponent() {
			std::vector<Entity> result;
			for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
				result.push_back((*it).first);
			}
			return result;
		}
		
		long unsigned entityCount() const {
			return components.size();
		}

		int serialize(Entity entity, uint8_t** out) {
			T* component = Get(entity);
			*out = new uint8_t[sizeof(T)];
			memcpy(*out, component, sizeof(T));
			return sizeof(T);
		}

		void deserialize(Entity entity, uint8_t* in, int size) {
			T* component = new T();
			if (size != sizeof(T)) {
				LOGW("error in size: %d != %lu", size, sizeof(T));
			}
			memcpy(component, in, size);
			components[entity] = component;
		}

		void Clear() { components.clear(); }

		void Activate() { active = true; }
		void Deactivate() { active = false; }
		bool IsActive() const { return active; }

	protected:
		bool active;

		std::map<Entity, T*> components;
        Entity previous;
        T* previousComp;

		typedef typename std::map<Entity, T*> ComponentMap;
		typedef typename std::map<Entity, T*>::iterator ComponentIt;
		typedef typename std::map<Entity, T*>::const_iterator ComponentConstIt;

		float activationTime;
};
