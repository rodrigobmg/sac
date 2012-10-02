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
#include "NetworkSystem.h"
#include "../api/NetworkAPI.h"
#include "../base/EntityManager.h"
#include <queue>

struct NetworkComponentPriv : NetworkComponent {
    NetworkComponentPriv() : NetworkComponent(), guid(0), entityExistsGlobally(false), ownedLocally(true) {}
    unsigned int guid; // global unique id
    bool entityExistsGlobally, ownedLocally;
    std::map<std::string, float> lastUpdateAccum;
    std::queue<NetworkPacket> packetToProcess;
};

struct NetworkMessageHeader {
    enum Type {
        HandShake,
        CreateEntity,
        DeleteEntity,
        UpdateEntity
    } type;
    unsigned int entityGuid;

    union {
        struct {
            unsigned int nonce;
        } HANDSHAKE;
        struct {

        } CREATE;
        struct {

        } DELETE;
        struct {

        } UPDATE;
    };
};

static void sendHandShakePacket(NetworkAPI* net, unsigned nonce);

INSTANCE_IMPL(NetworkSystem);
 
 unsigned myNonce;
 bool hsDone;
NetworkSystem::NetworkSystem() : ComponentSystemImpl<NetworkComponent>("network"), networkAPI(0) {
    /* nothing saved (?!) */
    nextGuid = 2;
    hsDone = false;
    myNonce = MathUtil::RandomInt(65000);
}


void NetworkSystem::DoUpdate(float dt) {
    if (!networkAPI)
        return;
    if (!networkAPI->isConnectedToAnotherPlayer())
        return;
    // Pull packets from networkAPI
    {
        NetworkPacket pkt;
        while ((pkt = networkAPI->pullReceivedPacket()).size) {
            NetworkMessageHeader* header = (NetworkMessageHeader*) pkt.data;
            switch (header->type) {
                case NetworkMessageHeader::HandShake: {
                    if (header->HANDSHAKE.nonce == myNonce) {
                        LOGW("Handshake done");
                        hsDone = true;
                    } else {
                        sendHandShakePacket(networkAPI, header->HANDSHAKE.nonce);
                    }
                    break;
                }
                case NetworkMessageHeader::CreateEntity: {
                    Entity e = theEntityManager.CreateEntity();
                    ADD_COMPONENT(e, Network);
                    NetworkComponentPriv* nc = static_cast<NetworkComponentPriv*>(NETWORK(e));
                    nc->guid = header->entityGuid;
                    nc->ownedLocally = false;
                    std::cout << "Create entity :" << e << "/" << nc->guid << std::endl;
                    break;
                }
                case NetworkMessageHeader::DeleteEntity: {
                    Entity e = guidToEntity(header->entityGuid);
                    if (e) {
                        theEntityManager.DeleteEntity(e);
                    }
                    break;
                }
                case NetworkMessageHeader::UpdateEntity: {
                    NetworkComponentPriv* nc = guidToComponent(header->entityGuid);
                    if (nc) {
                        nc->packetToProcess.push(pkt);
                    }
                    break;
                }
            }
        }
    }
    
    if (!hsDone) {
        sendHandShakePacket(networkAPI, myNonce);
        return;
    }

    // Process update type packets received
    {
        uint8_t temp[1024];
        for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
            Entity e = it->first;
            NetworkComponentPriv* nc = static_cast<NetworkComponentPriv*> (it->second);
    
            if (nc->ownedLocally)
                continue;
            while (!nc->packetToProcess.empty()) {
                NetworkPacket pkt = nc->packetToProcess.front();
                int index = sizeof(NetworkMessageHeader);
                while (index < pkt.size) {
                    uint8_t nameLength = pkt.data[index++];
                    memcpy(temp, &pkt.data[index], nameLength);
                    temp[nameLength] = '\0';
                    index += nameLength;
                    int size;
                    memcpy(&size, &pkt.data[index], 4);
                    index += 4;
                    ComponentSystem* system = ComponentSystem::Named((const char*)temp);
                    index += system->deserialize(e, &pkt.data[index], size);
                }
                 nc->packetToProcess.pop();
            }
        }
    }

    // Process local entities : send required update to others
    {
        uint8_t temp[1024];
        for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
            Entity e = it->first;
            NetworkComponentPriv* nc = static_cast<NetworkComponentPriv*> (it->second);

            if (!nc->ownedLocally || nc->systemUpdatePeriod.empty())
                continue;

            if (!nc->entityExistsGlobally) {
                std::cout << "NOTIFY create : " << e << "/" << nc->guid << std::endl;
                nc->entityExistsGlobally = true;
                nc->guid = nextGuid++;
                if (!networkAPI->amIGameMaster()) {
                    nc->guid |= 0x1;
                }
                NetworkPacket pkt;
                NetworkMessageHeader* header = (NetworkMessageHeader*)temp;
                header->type = NetworkMessageHeader::CreateEntity;
                header->entityGuid = nc->guid;
                pkt.size = sizeof(NetworkMessageHeader);
                pkt.data = temp;
                networkAPI->sendPacket(pkt);
            }

            NetworkPacket pkt;
            // build packet header
            NetworkMessageHeader* header = (NetworkMessageHeader*)temp;
            header->type = NetworkMessageHeader::UpdateEntity;
            header->entityGuid = nc->guid;
            pkt.size = sizeof(NetworkMessageHeader);

            // browse systems to share on network for this entity (of course, batching this would make a lot of sense)
            for (std::map<std::string, float>::iterator jt = nc->systemUpdatePeriod.begin(); jt!=nc->systemUpdatePeriod.end(); ++jt) {
                float& accum = nc->lastUpdateAccum[jt->first];
                
                if (accum >= 0)
                    accum += dt;
                // time to update
                if (accum >= jt->second) {
                    ComponentSystem* system = ComponentSystem::Named(jt->first);
                    uint8_t* out;
                    int size = system->serialize(e, &out);
                    uint8_t nameLength = strlen(jt->first.c_str());
                    temp[pkt.size++] = nameLength;
                    memcpy(&temp[pkt.size], jt->first.c_str(), nameLength);
                    pkt.size += nameLength;
                    memcpy(&temp[pkt.size], &size, 4);
                    pkt.size += 4;
                    memcpy(&temp[pkt.size], out, size);
                    pkt.size += size;
                    accum = 0;
                }
                // if peridocity <= 0 => update only once
                if (jt->second <= 0) {
                    accum = -1;
                }
            }
            // finish up packet
            pkt.data = temp;
            networkAPI->sendPacket(pkt);
        }
    }
}

NetworkComponent* NetworkSystem::CreateComponent() {
    return new NetworkComponentPriv(); // ahah!
}

NetworkComponentPriv* NetworkSystem::guidToComponent(unsigned int guid) {
    for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
        NetworkComponentPriv* nc = static_cast<NetworkComponentPriv*> (it->second);
        if (nc->guid == guid)
            return nc;
    }
    //LOGE("Did not find entity with guid: %u", guid);
    return 0;
}

Entity NetworkSystem::guidToEntity(unsigned int guid) {
    for(ComponentIt it=components.begin(); it!=components.end(); ++it) {
        Entity e = it->first;
        NetworkComponentPriv* nc = static_cast<NetworkComponentPriv*> (it->second);
        if (nc->guid == guid)
            return e;
    }
    LOGE("Did not find entity with guid: %u", guid);
    return 0;
}

void NetworkSystem::deleteAllNonLocalEntities() {
    int count = 0;
    for(ComponentIt it=components.begin(); it!=components.end(); ) {
        Entity e = it->first;
        NetworkComponentPriv* nc = static_cast<NetworkComponentPriv*> (it->second);
        ++it;
        if (!nc->ownedLocally) {
            theEntityManager.DeleteEntity(e);
            count++;
        }
    }
    LOGI("Removed %d non local entities", count);
}

unsigned int NetworkSystem::entityToGuid(Entity e) {
    ComponentIt it=components.find(e);
    if (it == components.end()) {
        LOGE("Entity %lu has no ntework component");
        assert (false);
        return 0;
    }
    NetworkComponentPriv* nc = static_cast<NetworkComponentPriv*> (it->second);
    return nc->guid;
}

static void sendHandShakePacket(NetworkAPI* net, unsigned nonce) {
    uint8_t temp[64];
    NetworkPacket pkt;
    NetworkMessageHeader* header = (NetworkMessageHeader*)temp;
    header->type = NetworkMessageHeader::HandShake;
    header->HANDSHAKE.nonce = nonce;
    pkt.size = sizeof(NetworkMessageHeader);
    pkt.data = temp;
    net->sendPacket(pkt);
}