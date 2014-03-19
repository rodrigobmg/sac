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



#include "SteeringBehavior.h"
#include "systems/TransformationSystem.h"
#include "systems/PhysicsSystem.h"
#include "systems/RenderingSystem.h"

#include <util/IntersectionUtil.h>

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vector_angle.hpp>

#if SAC_DEBUG
#include "util/Draw.h"
#endif

glm::vec2 SteeringBehavior::seek(Entity e, const glm::vec2& targetPos, float maxSpeed) {
    return seek(TRANSFORM(e)->position, PHYSICS(e)->linearVelocity, targetPos, maxSpeed);
}

glm::vec2 SteeringBehavior::seek(const glm::vec2& pos, const glm::vec2& linearVel, const glm::vec2& targetPos, float maxSpeed) {
    glm::vec2 toTarget (targetPos - pos);
    float d = glm::length(toTarget);

    if (d > 0) {
        return toTarget * (maxSpeed / d);
    } else {
        return glm::vec2(.0f);
    }
}

glm::vec2 SteeringBehavior::flee(Entity e, const glm::vec2& targetPos, float maxSpeed) {
    glm::vec2 d (glm::normalize(TRANSFORM(e)->position - targetPos) * maxSpeed);
    return d;
}

glm::vec2 SteeringBehavior::arrive(Entity e, const glm::vec2& targetPos, float maxSpeed, float deceleration) {
    return arrive(TRANSFORM(e)->position, PHYSICS(e)->linearVelocity, targetPos, maxSpeed, deceleration);
}

glm::vec2 SteeringBehavior::arrive(const glm::vec2& pos, const glm::vec2& linearVel,const glm::vec2& targetPos, float maxSpeed, float deceleration) {
    glm::vec2 toTarget (targetPos - pos);
    float d = glm::length(toTarget);

    if (d > 0) {
        toTarget = glm::normalize(toTarget);
        float speed = glm::min(d / deceleration, maxSpeed);
        glm::vec2 desiredVelocity(toTarget * speed);
        return desiredVelocity;
    }
    return glm::vec2(0.0f, 0.0f);
}

glm::vec2 SteeringBehavior::wander(Entity e, WanderParams& params, float maxSpeed) {
    params.target += glm::vec2(
        glm::linearRand(-1.0f, 1.0f) * params.jitter,
        glm::linearRand(-1.0f, 1.0f) * params.jitter);
    params.target = glm::normalize(params.target);
    params.target *= params.radius;
    params.debugTarget = TRANSFORM(e)->position + glm::rotate(glm::vec2(params.distance, 0.0f) + params.target, TRANSFORM(e)->rotation);

    return seek(e, params.debugTarget, maxSpeed);
}

#define BASIC_STEERING_GRAPHICAL_DEBUG (1 & SAC_DEBUG)
static std::tuple<glm::vec2, glm::vec2> computeOverlappingObstaclesPosSize(Entity refObstacle, const std::list<Entity>& obs) {
    const auto* tc = TRANSFORM(refObstacle);
    glm::vec2 position = tc->position, size = tc->size;
    int count = 1;
    for (Entity o: obs) {
        if (o == refObstacle) continue;
        const auto* tc2 = TRANSFORM(o);
        if (IntersectionUtil::rectangleRectangle(tc, tc2)) {
            #if BASIC_STEERING_GRAPHICAL_DEBUG
            Draw::Rectangle(__FILE__, tc2->position, tc2->size, tc2->rotation, Color(1, 0, 0, 0.5));
            #endif
            position += tc2->position;
            size += tc2->size;
            ++count;
        }
    }
    return std::make_tuple(position / (float)count, size / (float)count);
}

// TODO: pour calculer la vitesse désirée on peut faire aussi:
// - tourner la vitesse actuelle (pour qu'elle soit tangente à l'obstacle considéré)
// - puis réduire la vitesse tant qu'une collision est détectée (avec n'importe quel obstacle)
glm::vec2 SteeringBehavior::obstacleAvoidance(Entity e, const glm::vec2& velocity, std::list<Entity>& obstacles, float maxSpeed) {
    float size = TRANSFORM(e)->size.x * (1 + 0.5 * glm::length(velocity) / maxSpeed);

    const auto* tc = TRANSFORM(e);
    const glm::vec2 & rectSize = glm::vec2(size, TRANSFORM(e)->size.y);
    const glm::vec2 & rectPos = tc->position + glm::rotate(glm::vec2(rectSize.x * 0.5, 0), tc->rotation);
    float rectRot = glm::orientedAngle(glm::vec2(1.f, 0.f), glm::normalize(velocity));

    #if BASIC_STEERING_GRAPHICAL_DEBUG
        Draw::Clear(__FILE__);

        // display box-view of the object (where it wants to go)
        Draw::Rectangle(__FILE__, rectPos, rectSize, rectRot, Color(1, 0, 0, .5));
    #endif

    glm::vec2 force;
    glm::vec2 intersectionPoints[4], normals[4];
    const float halfWidth = tc->size.y * 0.5;
    float minDist = 1000;
    Entity obs = 0;
    glm::vec2 nearest, normal;

    for (auto obstacle : obstacles) {
        const auto* tcObstacle = TRANSFORM(obstacle);

        if (IntersectionUtil::rectangleRectangle(tcObstacle->position, tcObstacle->size,
            tcObstacle->rotation, rectPos, rectSize, rectRot)) {

            #if BASIC_STEERING_GRAPHICAL_DEBUG
            // display a box containing the obstacle
            Draw::Rectangle(__FILE__, tcObstacle->position,
                tcObstacle->size + glm::vec2(halfWidth), tcObstacle->rotation,
                Color(0, 1, 0, .5));
            #endif

            float angles[] = {0, glm::radians(25.0f), glm::radians(-25.0f)};

            for (int i=0; i<3; i++) {
                // we need to get the point of intersection of them to know if its the
                // closer rectangle from entity e
                int intersectCount = IntersectionUtil::linePolygon(
                    // open-ended line starting at e's position
                    tc->position, tc->position + glm::rotate(glm::vec2(1000, 0), tc->rotation + angles[i]),
                    // rectangle
                    theTransformationSystem.shapes[tcObstacle->shape],
                    tcObstacle->position, tcObstacle->size /*+ glm::vec2(halfWidth)*/, tcObstacle->rotation,
                    // result
                    intersectionPoints, normals);

                for (int i = 0; i < intersectCount; ++i) {
                    #if BASIC_STEERING_GRAPHICAL_DEBUG
                        // display the intersection points with the obstacle
                        Draw::Point(__FILE__, intersectionPoints[i], Color(0, 1, 1));
                    #endif
                    float dist = glm::distance(intersectionPoints[i], tc->position);

                    // Find the 2 nearest obstacles
                    if (dist < minDist) {
                        minDist= dist;
                        nearest = intersectionPoints[i];
                        normal = normals[i];
                        obs = obstacle;
                    }
                }
            }
        }
    }

    if (obs) {
        #if BASIC_STEERING_GRAPHICAL_DEBUG
        // display the real nearest intersection point with any obstacle
        Draw::Vec2(__FILE__, nearest, normal, Color(0, 0, 0));
        #endif

        // deduce collision normal
        glm::vec2 p (nearest - tc->position);

        #if 0
        auto groupPosSize = computeOverlappingObstaclesPosSize(obs, obstacles);
        #else
        auto groupPosSize = std::make_tuple(TRANSFORM(obs)->position, TRANSFORM(obs)->size);
        #endif

        // bounding radius
        float bRadius = glm::max(std::get<1>(groupPosSize).x, std::get<1>(groupPosSize).y);

        // local coords of obstacle
        glm::vec2 local =
            glm::vec2(
                glm::dot(std::get<0>(groupPosSize) - tc->position, glm::rotate(glm::vec2(1, 0), tc->rotation)),
                glm::dot(std::get<0>(groupPosSize) - tc->position, glm::rotate(glm::vec2(0, 1), tc->rotation))
            );

        glm::vec2 shift = glm::vec2(-normal.y, normal.x) * glm::sign(local.y) * bRadius;
        Draw::Vec2(__FILE__, tc->position, shift, Color(1, 0, 1), "shift");

        float l = glm::length(velocity);
        glm::vec2 dv = velocity + shift;
        // brake if necessary
        float brake = glm::max(0.0f, tc->size.x / local.x - 1);

        dv += velocity * (-brake);

        if ((l = glm::length(dv)) > maxSpeed) {
            dv *= maxSpeed / l;
        }

        return dv;
    } else {
        return velocity;
    }

    return force;
}

glm::vec2 SteeringBehavior::groupCohesion(Entity e, std::list<Entity>& group, float maxSpeed) {
    if (group.size() == 0) {
        return glm::vec2(0.);
    }

    glm::vec2 averagePosition;

    for (Entity neighbor : group) {
        averagePosition += TRANSFORM(neighbor)->position;
    }

    //normalize
    averagePosition /= group.size();

    return seek(e, averagePosition, maxSpeed);
}

glm::vec2 SteeringBehavior::groupAlign(Entity e, std::list<Entity>& group, float maxSpeed) {
    if (group.size() == 0) {
        return glm::vec2(0.);
    }

    glm::vec2 averageDirection;

    for (Entity neighbor : group) {
        averageDirection += glm::rotate(glm::vec2(1, 0), TRANSFORM(neighbor)->rotation);
    }

    //normalize
    averageDirection /= group.size();

    if (averageDirection != glm::vec2(0.)) {
        averageDirection = glm::normalize(averageDirection) * maxSpeed;
    }

    auto currentSpeed = PHYSICS(e)->linearVelocity;
    if (currentSpeed != glm::vec2(0.)) {
        currentSpeed = glm::normalize(currentSpeed);
    }
    return averageDirection;
}

glm::vec2 SteeringBehavior::groupSeparate(Entity e, std::list<Entity>& group, float maxSpeed) {
    glm::vec2 force;

    auto & myPos = TRANSFORM(e)->position;
    for (Entity neighbor : group) {
        auto & neighborPos = TRANSFORM(neighbor)->position;

        auto direction = (myPos - neighborPos);

        // the more neighbor is far away, the less we are attracted by it (norm2)
        force += direction / glm::length2(direction);
    }

    if (force != glm::vec2(0.)) {
        force = glm::normalize(force) * maxSpeed;
    }

    return force;
}

glm::vec2 SteeringBehavior::wallAvoidance(Entity e, const glm::vec2& velocity,
    const std::list<Entity>& walls, float maxSpeed) {

    const auto& myPos = TRANSFORM(e)->position;

    // Use 3 probes (0°, 45°, -45°) and in case of hits add a force along
    // the normal of the wall proportionnal to the hit
    float feelers[3*2] = {
        glm::radians(0.f), 1,
        glm::radians(40.f), 0.85,
        glm::radians(-40.f), 0.85
    };

    float closestIP = 0;
    Entity closestWall = 0;
    float overShoot = 0;
    glm::vec2 wallNormal;

    const glm::vec2 feelerStart = myPos + glm::rotate(glm::vec2(TRANSFORM(e)->size.x * 0.25f, 0.0f), TRANSFORM(e)->rotation);

    for (int i=0; i<3; i++) {
        const glm::vec2 feelerEnd = feelerStart + feelers[2*i + 1] * glm::rotate(glm::vec2(TRANSFORM(e)->size.x * feelers[2*i+1], 0.0f), TRANSFORM(e)->rotation + feelers[2*i]);
        // glm::rotate(PHYSICS(e)->linearVelocity, feelers[2*i]);

        #if SAC_DEBUG
            Draw::Vec2(__FILE__,
                feelerStart,
                feelerEnd - feelerStart,
                Color(0, 1, 0, 0.75));
        #endif

        for (auto & wall : walls) {
            LOGT_EVERY_N(6000, "Assuming min dimension of wall = 0");
            const auto* tc2 = TRANSFORM(wall);
            glm::vec2 dir, wallA, wallB;
            if (tc2->size.x < tc2->size.y) {
                dir = glm::rotate(glm::vec2(0.0f, tc2->size.y * 0.5f), tc2->rotation);
                wallA = tc2->position - dir;
                wallB = tc2->position + dir;
            } else {
                dir = glm::rotate(glm::vec2(tc2->size.x * 0.5f, 0.0f), tc2->rotation);
                wallA = tc2->position + dir;
                wallB = tc2->position - dir;
            }

            glm::vec2 intersectionPoint;

            if (IntersectionUtil::lineLine(
                    feelerStart,
                    feelerEnd,
                    wallA,
                    wallB,
                    &intersectionPoint)) {
                #if SAC_DEBUG
                Draw::Vec2(__FILE__,
                    feelerStart,
                    feelerEnd - feelerStart,
                    Color(1, 0, 0, 1));
                Draw::Vec2(__FILE__,
                    wallA,
                    wallB,
                    Color(0, 0, 1));
                #endif

                auto dist = glm::distance2(feelerStart, intersectionPoint);

                if (closestWall == 0 || dist < closestIP) {
                    closestIP = dist;
                    closestWall = wall;
                    overShoot = glm::distance(feelerEnd, intersectionPoint) / glm::distance(feelerEnd, feelerStart);
                    auto w = wallA - wallB;
                    wallNormal = glm::normalize(glm::vec2(-w.y, w.x));
                    // orient wall normal toward vehicle
                    wallNormal *= glm::sign(glm::dot(myPos - intersectionPoint, wallNormal));
                }
            }
        }
    }

    if (closestWall) {
        float reactionLength = overShoot * maxSpeed;
        return glm::normalize(velocity) * (maxSpeed - reactionLength) + wallNormal * reactionLength;
    } else {
        return velocity;
    }
}
