#include "PhysicsSystem.h"
#include "TransformationSystem.h"
#include "RenderingSystem.h"
#include <glm/gtx/perpendicular.hpp>
#include <glm/gtx/norm.hpp>

#include "util/drawVector.h"

INSTANCE_IMPL(PhysicsSystem);

PhysicsSystem::PhysicsSystem() : ComponentSystemImpl<PhysicsComponent>("Physics") {
    PhysicsComponent tc;
    componentSerializer.add(new Property<glm::vec2>("linear_velocity", OFFSET(linearVelocity, tc), glm::vec2(0.001, 0)));
    componentSerializer.add(new Property<float>("angular_velocity", OFFSET(angularVelocity, tc), 0.001));
    componentSerializer.add(new Property<float>("mass", OFFSET(mass, tc), 0.001));
    componentSerializer.add(new Property<float>("frottement", OFFSET(frottement, tc), 0.001));
    componentSerializer.add(new Property<glm::vec2>("gravity", OFFSET(gravity, tc), glm::vec2(0.001, 0)));
}

#if SAC_DEBUG
void PhysicsSystem::addDebugOnlyDrawForce(const glm::vec2 & pos, const glm::vec2 & size) {
    float norm2 = glm::length2(size);
    if (norm2 < 0.00001f)
        return;

    norm2Max = glm::max(norm2Max, norm2);


    if (currentDraw == drawForceVectors.size()) {
        std::pair<Entity, glm::vec2[2]> pair;

        pair.first = drawVector(pos, size);
        pair.second[0] = pos;
        pair.second[1] = size;
        drawForceVectors.push_back(pair);
    } else {
        drawForceVectors[currentDraw].second[0] = pos;
        drawForceVectors[currentDraw].second[1] = size;
    }

    ++currentDraw;
}
#else
#define addDebugOnlyDrawForce(a,b,c,d) {}
#endif

void PhysicsSystem::DoUpdate(float dt) {
#if SAC_DEBUG
    currentDraw = 0;
    norm2Max = 0.f;
#endif

    //update orphans first
    FOR_EACH_ENTITY_COMPONENT(Physics, a, pc)
		TransformationComponent* tc = TRANSFORM(a);
		if (!tc || tc->parent == 0) {
			if (pc->mass <= 0)
				continue;

			pc->momentOfInertia = pc->mass * tc->size.x * tc->size.y / 6;

			// linear accel
			glm::vec2 linearAccel(pc->gravity * pc->mass);

            addDebugOnlyDrawForce(tc->position, pc->gravity * pc->mass);

			// angular accel
			float angAccel = 0;

            if (pc->frottement != 0.f) {
                pc->addForce(glm::vec2(0.f), - pc->frottement * pc->linearVelocity, dt);
            }

			for (unsigned int i=0; i<pc->forces.size(); i++) {
				Force force(pc->forces[i].first);

                addDebugOnlyDrawForce(tc->position + force.point, force.vector);

				float& durationLeft = pc->forces[i].second;

				if (durationLeft < dt) {
					force.vector *= durationLeft / dt;
				}

				linearAccel += force.vector;

		        if (force.point != glm::vec2(0.0f, 0.0f)) {
			        angAccel += glm::dot(glm::vec2(- force.point.y, force.point.x), force.vector);
		        }

				durationLeft -= dt;
				if (durationLeft < 0) {
					pc->forces.erase(pc->forces.begin() + i);
					i--;
				}
			}

			linearAccel /= pc->mass;
			angAccel /= pc->momentOfInertia;

			// acceleration is constant over dt: use basic Euler integration for velocity
            const glm::vec2 nextVelocity(pc->linearVelocity + linearAccel * dt);
            tc->position += (pc->linearVelocity + nextVelocity) * dt * 0.5f;
            // velocity varies over dt: use Verlet integration for position
            pc->linearVelocity = nextVelocity;
			const float nextAngularVelocity = pc->angularVelocity + angAccel * dt;
			tc->rotation += (pc->angularVelocity + nextAngularVelocity) * dt * 0.5;
            pc->angularVelocity = nextAngularVelocity;
	    }
	}

    //copy parent property to its sons
    FOR_EACH_ENTITY_COMPONENT(Physics, a, pc)
		if (!TRANSFORM(a))
			continue;

		Entity parent = TRANSFORM(a)->parent;
		if (parent) {
			while (TRANSFORM(parent)->parent) {
				parent = TRANSFORM(parent)->parent;
			}

            PhysicsComponent* ppc = thePhysicsSystem.Get(parent, false);
            if (ppc) {
    			pc->linearVelocity = ppc->linearVelocity;
    			pc->angularVelocity = ppc->angularVelocity;
            } else {
                pc->linearVelocity = glm::vec2(0.0f, 0.0f);
                pc->angularVelocity = 0;
            }
		}
    }

#if SAC_DEBUG
    const float sizeForMaxForce = 2.f;
    float normMax = glm::sqrt(norm2Max) / sizeForMaxForce;

    for (unsigned i = 0; i < currentDraw; ++i) {
        glm::vec2 pos = drawForceVectors[i].second[0];
        glm::vec2 size = drawForceVectors[i].second[1];

        size /= normMax;
        float currentNorm = glm::length(size);

        //force vectors size must be in [0;sizeForMaxForce] (sizeForMaxForce = max vector)
        //but if the final force size is too small, change color/size
        if (currentNorm < 0.1f * sizeForMaxForce) {
            size = 0.1f * sizeForMaxForce * glm::normalize(size);
            //RENDERING(drawForceVectors[i].first)->color = Color(0.5,1.,0.,1.);
        } else {
            //RENDERING(drawForceVectors[i].first)->color = Color(1.,1.,1.,1.);
        }


        drawVector(pos, size, drawForceVectors[i].first);
    }

    for (unsigned i = currentDraw; i < drawForceVectors.size(); ++i) {
        RENDERING(drawForceVectors[i].first)->show = false;
    }
#endif
}


void PhysicsSystem::addMoment(PhysicsComponent* pc, float m) {
	// add 2 opposed forces
    //WARNING: shouldn't be +size,0 and -size,0 instead of 1,0 / -1, 0?
	pc->addForce(glm::vec2(1, 0), glm::vec2(0, m * 0.5), 0.016);
	pc->addForce(glm::vec2(-1, 0), glm::vec2(0, -m * 0.5), 0.016);
}

#if SAC_INGAME_EDITORS
void PhysicsSystem::addEntityPropertiesToBar(Entity entity, TwBar* bar) {
    PhysicsComponent* tc = Get(entity, false);
    if (!tc) return;
    TwAddVarRW(bar, "velocity.X", TW_TYPE_FLOAT, &tc->linearVelocity.x, "group=Physics precision=2 step=0,01");
    TwAddVarRW(bar, "velocity.Y", TW_TYPE_FLOAT, &tc->linearVelocity.y, "group=Physics precision=2 step=0,01");
    TwAddVarRW(bar, "angularVelocity", TW_TYPE_FLOAT, &tc->angularVelocity, "group=Physics step=0,01 precision=2");
    TwAddVarRW(bar, "mass", TW_TYPE_FLOAT, &tc->mass, "group=Physics precision=1");
    TwAddVarRW(bar, "gravity.X", TW_TYPE_FLOAT, &tc->gravity.x, "group=Physics precision=2 step=0,01");
    TwAddVarRW(bar, "gravity.Y", TW_TYPE_FLOAT, &tc->gravity.y, "group=Physics precision=2 step=0,01");
}
#endif
