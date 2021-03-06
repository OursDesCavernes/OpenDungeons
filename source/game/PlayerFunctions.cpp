/*
 *  Copyright (C) 2011-2015  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "game/Player.h"

#include "entities/GameEntity.h"
#include "entities/Tile.h"
#include "game/ResearchManager.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "gamemap/Pathfinding.h"
#include "network/ODClient.h"
#include "network/ODServer.h"
#include "network/ServerNotification.h"
#include "render/RenderManager.h"
#include "rooms/Room.h"
#include "rooms/RoomType.h"
#include "spells/SpellManager.h"
#include "spells/SpellType.h"
#include "traps/Trap.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"
#include "ODApplication.h"

//! \brief The number of seconds the local player must stay out of danger to trigger the calm music again.
const float BATTLE_TIME_COUNT = 10.0f;
//! \brief Minimum distance to consider that a new event of the same type is in another place
//! Note that the distance is squared (100 would mean 10 tiles)
const int MIN_DIST_EVENT_SQUARED = 100;

//! \brief The number of seconds the local player will not be notified again if the research queue is empty
const float NO_RESEARCH_TIME_COUNT = 60.0f;

//! \brief The number of seconds the local player will not be notified again if no treasury is available
const float NO_TREASURY_TIME_COUNT = 30.0f;

PlayerEvent* PlayerEvent::getPlayerEventFromPacket(GameMap* gameMap, ODPacket& is)
{
    PlayerEvent* event = new PlayerEvent;
    event->importFromPacket(gameMap, is);
    return event;
}

void PlayerEvent::exportPlayerEventToPacket(PlayerEvent* event, GameMap* gameMap, ODPacket& is)
{
    event->exportToPacket(gameMap, is);
}

void PlayerEvent::exportToPacket(GameMap* gameMap, ODPacket& os)
{
    os << mType;
    gameMap->tileToPacket(os, mTile);
}

void PlayerEvent::importFromPacket(GameMap* gameMap, ODPacket& is)
{
    OD_ASSERT_TRUE(is >> mType);
    mTile = gameMap->tileFromPacket(is);
}

void Player::pickUpEntity(GameEntity *entity)
{
    if(!entity->tryPickup(getSeat()))
    {
        OD_LOG_ERR("player seatId" + Helper::toString(getSeat()->getId()) + " couldn't pickup entity=" + entity->getName());
        return;
    }

    OD_LOG_INF("player seatId=" + Helper::toString(getSeat()->getId()) + " picked up " + entity->getName());
    entity->pickup();

    // Start tracking this creature as being in this player's hand
    addEntityToHand(entity);

    if (mGameMap->isServerGameMap())
    {
        entity->firePickupEntity(this);
        return;
    }

    if (this != mGameMap->getLocalPlayer())
    {
        OD_LOG_ERR("cannot pickup entity player seat=" + Helper::toString(getSeat()->getId()) + ", localPlayer seat id=" + Helper::toString(mGameMap->getLocalPlayer()->getSeat()->getId()) + ", entity=" + entity->getName());
        return;
    }
    RenderManager::getSingleton().rrPickUpEntity(entity, this);
}


bool Player::isDropHandPossible(Tile *t, unsigned int index)
{
    // if we have a creature to drop
    if (index >= mObjectsInHand.size())
    {
        OD_LOG_ERR("seatId=" + Helper::toString(getSeat()->getId()) + ", index=" + Helper::toString(index) + ", size=" + Helper::toString(mObjectsInHand.size()));
        return false;
    }

    GameEntity* entity = mObjectsInHand[index];
    return entity->tryDrop(getSeat(), t);
}

void Player::dropHand(Tile *t, unsigned int index)
{
    // Add the creature to the map
    if(index >= mObjectsInHand.size())
    {
        OD_LOG_ERR("seatId=" + Helper::toString(getSeat()->getId()) + ", index=" + Helper::toString(index) + ", size=" + Helper::toString(mObjectsInHand.size()));
        return;
    }

    GameEntity *entity = mObjectsInHand[index];
    mObjectsInHand.erase(mObjectsInHand.begin() + index);

    Ogre::Vector3 pos(static_cast<Ogre::Real>(t->getX()),
       static_cast<Ogre::Real>(t->getY()), entity->getPosition().z);
    if(mGameMap->isServerGameMap())
    {
        entity->drop(pos);
        entity->fireDropEntity(this, t);
        return;
    }

    entity->correctDropPosition(pos);
    OD_LOG_INF("player seatId=" + Helper::toString(getSeat()->getId()) + " drop " + entity->getName());
    entity->drop(pos);

    // If this is the result of another player dropping the creature it is currently not visible so we need to create a mesh for it
    //cout << "\nthis:  " << this << "\nme:  " << gameMap->getLocalPlayer() << endl;
    //cout.flush();
    if (this != mGameMap->getLocalPlayer())
    {
        OD_LOG_ERR("cannot pickup entity player seat=" + Helper::toString(getSeat()->getId()) + ", localPlayer seat id=" + Helper::toString(mGameMap->getLocalPlayer()->getSeat()->getId()) + ", entity=" + entity->getName());
        return;
    }
    // Send a render request to rearrange the creatures in the hand to move them all forward 1 place
    RenderManager::getSingleton().rrDropHand(entity, this);
}

void Player::rotateHand(Direction d)
{
    if(mObjectsInHand.size() > 1)
    {
        if(d == Direction::left)
        {
            std::rotate(mObjectsInHand.begin(), mObjectsInHand.begin() + 1, mObjectsInHand.end());
        }
        else
        {
            std::rotate(mObjectsInHand.begin(), mObjectsInHand.end() - 1, mObjectsInHand.end());
        }
        // Send a render request to move the entity into the "hand"
        RenderManager::getSingleton().rrRotateHand(this);
    }
}

void Player::notifyNoMoreDungeonTemple()
{
    if(mHasLost)
        return;

    mHasLost = true;
    // We check if there is still a player in the team with a dungeon temple. If yes, we notify the player he lost his dungeon
    // if no, we notify the team they lost
    std::vector<Room*> dungeonTemples = mGameMap->getRoomsByType(RoomType::dungeonTemple);
    bool hasTeamLost = true;
    for(Room* dungeonTemple : dungeonTemples)
    {
        if(getSeat()->isAlliedSeat(dungeonTemple->getSeat()))
        {
            hasTeamLost = false;
            break;
        }
    }

    if(hasTeamLost)
    {
        // This message will be sent in 1v1 or multiplayer so it should not talk about team. If we want to be
        // more precise, we shall handle the case
        for(Seat* seat : mGameMap->getSeats())
        {
            if(seat->getPlayer() == nullptr)
                continue;
            if(!seat->getPlayer()->getIsHuman())
                continue;
            if(!getSeat()->isAlliedSeat(seat))
                continue;

            ServerNotification *serverNotification = new ServerNotification(
                ServerNotificationType::chatServer, seat->getPlayer());
            serverNotification->mPacket << "You lost the game" << EventShortNoticeType::majorGameEvent;
            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
    }
    else
    {
        for(Seat* seat : mGameMap->getSeats())
        {
            if(seat->getPlayer() == nullptr)
                continue;
            if(!seat->getPlayer()->getIsHuman())
                continue;
            if(!getSeat()->isAlliedSeat(seat))
                continue;

            if(this == seat->getPlayer())
            {
                ServerNotification *serverNotification = new ServerNotification(
                    ServerNotificationType::chatServer, seat->getPlayer());
                serverNotification->mPacket << "You lost" << EventShortNoticeType::majorGameEvent;
                ODServer::getSingleton().queueServerNotification(serverNotification);
                continue;
            }

            ServerNotification *serverNotification = new ServerNotification(
                ServerNotificationType::chatServer, seat->getPlayer());
            serverNotification->mPacket << "An ally has lost" << EventShortNoticeType::majorGameEvent;
            ODServer::getSingleton().queueServerNotification(serverNotification);
        }
    }
}

void Player::notifyTeamFighting(Player* player, Tile* tile)
{
    // We check if there is a fight event currently near this tile. If yes, we update
    // the time event. If not, we create a new fight event
    bool isFirstFight = true;
    for(PlayerEvent* event : mEvents)
    {
        if(event->getType() != PlayerEventType::fight)
            continue;

        // There is already another fight event. We check the distance
        isFirstFight = false;
        if(Pathfinding::squaredDistanceTile(*tile, *event->getTile()) < MIN_DIST_EVENT_SQUARED)
        {
            // A similar event is already on nearly. We only update it
            event->setTimeRemain(BATTLE_TIME_COUNT);
            return;
        }
    }

    if(isFirstFight)
    {
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::playerFighting, this);
        serverNotification->mPacket << player->getId();
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }

    // We add the fight event
    mEvents.push_back(new PlayerEvent(PlayerEventType::fight, tile, BATTLE_TIME_COUNT));
    fireEvents();
}

void Player::notifyNoResearchInQueue()
{
    if(mNoResearchInQueueTime == 0.0f)
    {
        mNoResearchInQueueTime = NO_RESEARCH_TIME_COUNT;

        std::string chatMsg = "Your research queue is empty, while there are still skills that could be unlocked.";
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::chatServer, this);
        serverNotification->mPacket << chatMsg << EventShortNoticeType::genericGameInfo;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

void Player::notifyNoTreasuryAvailable()
{
    if(mNoTreasuryAvailableTime == 0.0f)
    {
        mNoTreasuryAvailableTime = NO_TREASURY_TIME_COUNT;

        std::string chatMsg = "No treasury available. You should build a bigger one.";
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::chatServer, this);
        serverNotification->mPacket << chatMsg << EventShortNoticeType::genericGameInfo;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}

void Player::fireEvents()
{
    // On server side, we update the client
    if(!mGameMap->isServerGameMap())
        return;

    ServerNotification *serverNotification = new ServerNotification(
        ServerNotificationType::playerEvents, this);
    uint32_t nbItems = mEvents.size();
    serverNotification->mPacket << nbItems;
    for(PlayerEvent* event : mEvents)
    {
        PlayerEvent::exportPlayerEventToPacket(event, mGameMap,
            serverNotification->mPacket);
    }

    ODServer::getSingleton().queueServerNotification(serverNotification);
}


void Player::markTilesForDigging(bool marked, const std::vector<Tile*>& tiles, bool asyncMsg)
{
    if(tiles.empty())
        return;

    std::vector<Tile*> tilesMark;
    // If human player, we notify marked tiles
    if(getIsHuman())
    {
        for(Tile* tile : tiles)
        {
            if(!marked)
            {
                getSeat()->tileMarkedDiggingNotifiedToPlayer(tile, marked);
                tile->setMarkedForDigging(marked, this);
                tilesMark.push_back(tile);

                continue;
            }

            // If the tile is diggable for the client, we mark it for him
            if(getSeat()->isTileDiggableForClient(tile))
            {
                getSeat()->tileMarkedDiggingNotifiedToPlayer(tile, marked);
                tilesMark.push_back(tile);
            }

            // If the tile can be marked on server side, we mark it
            if(!tile->isDiggable(getSeat()))
                continue;

            tile->setMarkedForDigging(marked, this);
        }
    }
    else
    {
        for(Tile* tile : tiles)
        {
            if(!marked)
            {
                getSeat()->tileMarkedDiggingNotifiedToPlayer(tile, marked);
                tile->setMarkedForDigging(marked, this);
                continue;
            }

            // If the tile can be marked on server side, we mark it
            if(!tile->isDiggable(getSeat()))
                continue;

            tile->setMarkedForDigging(marked, this);
        }
        return;
    }

    // If no tile to mark, we do not send the message
    if(tilesMark.empty())
        return;

    // On client side, we ask to mark the tile
    if(!asyncMsg)
    {
        ServerNotification* serverNotification = new ServerNotification(
            ServerNotificationType::markTiles, this);
        uint32_t nbTiles = tilesMark.size();
        serverNotification->mPacket << marked << nbTiles;
        for(Tile* tile : tilesMark)
        {
            // On client side, we ask to mark the tile
            mGameMap->tileToPacket(serverNotification->mPacket, tile);
        }
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
    else
    {
        ServerNotification serverNotification(
            ServerNotificationType::markTiles, this);
        uint32_t nbTiles = tilesMark.size();
        serverNotification.mPacket << marked << nbTiles;
        for(Tile* tile : tilesMark)
            mGameMap->tileToPacket(serverNotification.mPacket, tile);

        ODServer::getSingleton().sendAsyncMsg(serverNotification);
    }
}

void Player::upkeepPlayer(double timeSinceLastUpkeep)
{
    decreaseSpellCooldowns();

    // Specific stuff for human players
    if(!getIsHuman())
        return;

    // Handle fighting time
    bool wasFightHappening = false;
    bool isFightHappening = false;
    bool isEventListUpdated = false;
    for(auto it = mEvents.begin(); it != mEvents.end();)
    {
        PlayerEvent* event = *it;
        if(event->getType() != PlayerEventType::fight)
        {
            ++it;
            continue;
        }

        wasFightHappening = true;

        float timeRemain = event->getTimeRemain();
        if(timeRemain > timeSinceLastUpkeep)
        {
            isFightHappening = true;
            timeRemain -= timeSinceLastUpkeep;
            event->setTimeRemain(timeRemain);
            ++it;
            continue;
        }

        // This event is outdated, we remove it
        isEventListUpdated = true;
        delete event;
        it = mEvents.erase(it);
    }

    if(wasFightHappening && !isFightHappening)
    {
        // Notify the player he is no longer under attack.
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::playerNoMoreFighting, this);
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }

    // Do not notify research queue empty if no library
    if(getIsHuman() &&
       (getSeat()->getNbRooms(RoomType::library) > 0))
    {
        if(mNoResearchInQueueTime > timeSinceLastUpkeep)
            mNoResearchInQueueTime -= timeSinceLastUpkeep;
        else
        {
            mNoResearchInQueueTime = 0.0f;

            // Reprint the warning if there is still no research being done
            if(getSeat() != nullptr && !getSeat()->isResearching() && !ResearchManager::isAllResearchesDoneForSeat(getSeat()))
                notifyNoResearchInQueue();
        }
    }

    if(mNoTreasuryAvailableTime > 0.0f)
    {
        if(mNoTreasuryAvailableTime > timeSinceLastUpkeep)
            mNoTreasuryAvailableTime -= timeSinceLastUpkeep;
        else
            mNoTreasuryAvailableTime = 0.0f;
    }

    if(isEventListUpdated)
        fireEvents();
}

void Player::setSpellCooldownTurns(SpellType spellType, uint32_t cooldown)
{
    uint32_t spellIndex = static_cast<uint32_t>(spellType);
    if(spellIndex >= mSpellsCooldown.size())
    {
        OD_LOG_ERR("seatId=" + Helper::toString(getId()) + ", spellType=" + SpellManager::getSpellNameFromSpellType(spellType));
        return;
    }

    mSpellsCooldown[spellIndex] = std::pair<uint32_t, float>(cooldown, 1.0f / ODApplication::turnsPerSecond);
    if(mGameMap->isServerGameMap() && getIsHuman())
    {
        ServerNotification *serverNotification = new ServerNotification(
            ServerNotificationType::setSpellCooldown, this);
        serverNotification->mPacket << spellType << cooldown;
        ODServer::getSingleton().queueServerNotification(serverNotification);
    }
}
