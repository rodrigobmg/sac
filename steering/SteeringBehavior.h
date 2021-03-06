/*
    This file is part of Soupe Au Caillou.

    @author Soupe au Caillou - Jordane Pelloux-Prayer
    @author Soupe au Caillou - Gautier Pelloux-Prayer
    @author Soupe au Caillou - Pierre-Eric Pelloux-Prayer

    Soupe Au Caillou is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3.

    Soupe Au Caillou is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Soupe Au Caillou.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <glm/glm.hpp>
#include "../base/EntityManager.h"

namespace Steering {
    struct Context {
        /* clockwise
            0 -> 0°
            1 -> 45°
            ...
            7 -> 315°
        */
        float directions[9];
#if SAC_DEBUG
        Entity entities[9];
#endif
    };

    const glm::vec2 direction(float rotation, int index);
    float angle(float rotation, int index);

    template <class T>
    void behavior(Entity e,
                  float dt,
                  T& param,
                  Context* interest,
                  Context* priority,
                  Context* danger);
}

#if 0
        static glm::vec2 seek(Entity e, const glm::vec2& targetPos, float maxSpeed);

        static glm::vec2 seek(const glm::vec2& pos, const glm::vec2& linearVel, const glm::vec2& targetPos, float maxSpeed);

        static glm::vec2 flee(Entity e, const glm::vec2& targetPos, float maxSpeed);

        static glm::vec2 arrive(Entity e, const glm::vec2& targetPos, float maxSpeed, float deceleration);

        static glm::vec2 arrive(const glm::vec2& pos, const glm::vec2& linearVel,const glm::vec2& targetPos, float maxSpeed, float deceleration);

        static glm::vec2 wander(Entity e, WanderParams& params, float maxSpeed);

        static glm::vec2 obstacleAvoidance(Entity e, const glm::vec2& velocity, std::list<Entity>& obstacles, float maxSpeed);

        static glm::vec2 groupCohesion(Entity e, std::list<Entity>& group, float maxSpeed);

        static glm::vec2 groupAlign(Entity e, std::list<Entity>& group, float maxSpeed);

        static glm::vec2 groupSeparate(Entity e, std::list<Entity>& group, float maxSpeed);

        static glm::vec2 wallAvoidance(Entity e, const glm::vec2& velocity, const std::list<Entity>& walls, float maxSpeed);

        static glm::vec2 boxContainer(Entity e, const glm::vec2& velocity, const glm::vec2& position, const glm::vec2& size, float maxSpeed);
};
#endif
