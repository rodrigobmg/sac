#pragma once

#include "System.h"

struct DebuggingComponent {
};

#define theDebuggingSystem DebuggingSystem::GetInstance()
#define DEBUGGING(e) theDebuggingSystem.Get(e)

UPDATABLE_SYSTEM(Debugging)

    private:
        std::map<std::string, Entity> debugEntities;

        Entity fps, entityCount, systems;
};