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

#include "entities/Building.h"

#include "entities/BuildingObject.h"
#include "entities/RenderedMovableEntity.h"
#include "entities/Tile.h"
#include "game/Seat.h"
#include "gamemap/GameMap.h"
#include "network/ODServer.h"
#include "render/RenderManager.h"
#include "utils/Helper.h"
#include "utils/LogManager.h"

const double Building::DEFAULT_TILE_HP = 10.0;

const Ogre::Vector3 SCALE(RenderManager::BLENDER_UNITS_PER_OGRE_UNIT,
        RenderManager::BLENDER_UNITS_PER_OGRE_UNIT,
        RenderManager::BLENDER_UNITS_PER_OGRE_UNIT);

Building::~Building()
{
    for(std::pair<Tile* const, TileData*>& p : mTileData)
    {
        delete p.second;
    }
    mTileData.clear();
}

void Building::doUpkeep()
{
    // If no more tiles, the trap is removed
    if (numCoveredTiles() <= 0)
    {
        if(canBuildingBeRemoved())
        {
            removeFromGameMap();
            deleteYourself();
            return;
        }
    }

    std::vector<Tile*> tilesToRemove;
    for (Tile* tile : mCoveredTiles)
    {
        if (mTileData[tile]->mHP <= 0.0)
        {
            tilesToRemove.push_back(tile);
            continue;
        }
    }

    if (!tilesToRemove.empty())
    {
        for(Tile* tile : tilesToRemove)
            removeCoveredTile(tile);

        updateActiveSpots();
        createMesh();
    }
}

const Ogre::Vector3& Building::getScale() const
{
    return SCALE;
}

void Building::addBuildingObject(Tile* targetTile, RenderedMovableEntity* obj)
{
    if(obj == nullptr)
        return;

    // We assume the object position has been already set (most of the time in loadBuildingObject)
    mBuildingObjects[targetTile] = obj;
    obj->addToGameMap();
    obj->setPosition(obj->getPosition());
}

void Building::removeBuildingObject(Tile* tile)
{
    if(mBuildingObjects.count(tile) == 0)
        return;

    RenderedMovableEntity* obj = mBuildingObjects[tile];
    obj->removeFromGameMap();
    obj->deleteYourself();
    mBuildingObjects.erase(tile);
}

void Building::removeBuildingObject(RenderedMovableEntity* obj)
{
    std::map<Tile*, RenderedMovableEntity*>::iterator it;

    for (it = mBuildingObjects.begin(); it != mBuildingObjects.end(); ++it)
    {
        if(it->second == obj)
            break;
    }

    if(it != mBuildingObjects.end())
    {
        obj->removeFromGameMap();
        obj->deleteYourself();
        mBuildingObjects.erase(it);
    }
}

bool Building::canBuildingBeRemoved()
{
    // We check if an human player still have vision on one of the building tiles
    bool ret = true;
    for(std::pair<Tile* const, TileData*>& p : mTileData)
    {
        if(!p.second->mSeatsVision.empty())
        {
            ret = false;
            break;
        }
    }

    if(mBuildingObjects.empty())
        return ret;

    for (const std::pair<Tile* const, RenderedMovableEntity*>& p : mBuildingObjects)
    {
        RenderedMovableEntity* obj = p.second;
        if(!obj->notifyRemoveAsked())
            ret = false;
    }

    return ret;
}

void Building::removeAllBuildingObjects()
{
    if(mBuildingObjects.empty())
        return;

    for (std::pair<Tile* const, RenderedMovableEntity*>& p : mBuildingObjects)
    {
        RenderedMovableEntity* obj = p.second;
        obj->removeFromGameMap();
        obj->deleteYourself();
    }
    mBuildingObjects.clear();
}

RenderedMovableEntity* Building::getBuildingObjectFromTile(Tile* tile)
{
    if(mBuildingObjects.count(tile) == 0)
        return nullptr;

    RenderedMovableEntity* obj = mBuildingObjects[tile];
    return obj;
}

RenderedMovableEntity* Building::loadBuildingObject(GameMap* gameMap, const std::string& meshName,
    Tile* targetTile, double rotationAngle, bool hideCoveredTile, float opacity,
    const std::string& initialAnimationState, bool initialAnimationLoop)
{
    if (targetTile == nullptr)
        targetTile = getCentralTile();

    if(targetTile == nullptr)
    {
        OD_LOG_ERR("room=" + getName());
        return nullptr;
    }

    return loadBuildingObject(gameMap, meshName, targetTile, static_cast<double>(targetTile->getX()),
        static_cast<double>(targetTile->getY()), rotationAngle, hideCoveredTile, opacity,
        initialAnimationState, initialAnimationLoop);
}

RenderedMovableEntity* Building::loadBuildingObject(GameMap* gameMap, const std::string& meshName,
    Tile* targetTile, double x, double y, double rotationAngle, bool hideCoveredTile, float opacity,
    const std::string& initialAnimationState, bool initialAnimationLoop)
{
    std::string baseName;
    if(targetTile == nullptr)
        baseName = getName();
    else
        baseName = getName() + "_" + Tile::displayAsString(targetTile);

    Ogre::Vector3 position(static_cast<Ogre::Real>(x), static_cast<Ogre::Real>(y), 0);
    BuildingObject* obj = new BuildingObject(gameMap, getIsOnServerMap(), baseName, meshName,
        position, static_cast<Ogre::Real>(rotationAngle), hideCoveredTile, opacity,
        initialAnimationState, initialAnimationLoop);

    return obj;
}

Tile* Building::getCentralTile()
{
    if (mCoveredTiles.empty() && mCoveredTilesDestroyed.empty())
        return nullptr;

    std::vector<Tile*> allTiles;
    allTiles.insert(allTiles.end(), mCoveredTiles.begin(), mCoveredTiles.end());

    // In editor mode, we do not consider destroyed tiles while computing the central tile
    if(!getGameMap()->isInEditorMode())
        allTiles.insert(allTiles.end(), mCoveredTilesDestroyed.begin(), mCoveredTilesDestroyed.end());

    if(allTiles.empty())
        return nullptr;

    int minX, maxX, minY, maxY;
    minX = maxX = allTiles[0]->getX();
    minY = maxY = allTiles[0]->getY();

    for(unsigned int i = 0, size = allTiles.size(); i < size; ++i)
    {
        int tempX = allTiles[i]->getX();
        int tempY = allTiles[i]->getY();

        if (tempX < minX)
            minX = tempX;
        if (tempY < minY)
            minY = tempY;
        if (tempX > maxX)
            maxX = tempX;
        if (tempY > maxY)
            maxY = tempY;
    }

    return getGameMap()->getTile((minX + maxX) / 2, (minY + maxY) / 2);
}

bool Building::removeCoveredTile(Tile* t)
{
    OD_LOG_INF(getGameMap()->serverStr() + "building=" + getName() + ", removing covered tile=" + Tile::displayAsString(t));
    auto it = std::find(mCoveredTiles.begin(), mCoveredTiles.end(), t);
    if(it == mCoveredTiles.end())
    {
        OD_LOG_ERR("building=" + getName() + ", removing unknown covered tile=" + Tile::displayAsString(t));
        return false;
    }

    mCoveredTiles.erase(it);
    mCoveredTilesDestroyed.push_back(t);
    mTileData[t]->mHP = 0.0;
    t->setCoveringBuilding(nullptr);

    return true;
}

std::vector<Tile*> Building::getCoveredTiles()
{
    return mCoveredTiles;
}

Tile* Building::getCoveredTile(int index)
{
    if(index >= static_cast<int>(mCoveredTiles.size()))
    {
        OD_LOG_ERR("name=" + getName() + ", index=" + Helper::toString(index) + ", size=" + Helper::toString(mCoveredTiles.size()));
        return nullptr;
    }

    return mCoveredTiles[index];
}

void Building::clearCoveredTiles()
{
    mCoveredTiles.clear();
}

double Building::getHP(Tile *tile) const
{
    if (tile != nullptr)
    {
        std::map<Tile*, TileData*>::const_iterator tileSearched = mTileData.find(tile);
        if(tileSearched == mTileData.end())
        {
            OD_LOG_ERR("couldn't find requested tile=" + Tile::displayAsString(tile));
            return 0.0;
        }

        return tileSearched->second->mHP;
    }

    // If the tile given was nullptr, we add the total HP of all the tiles in the room and return that.
    double total = 0.0;

    for(const std::pair<Tile* const, TileData*>& p : mTileData)
    {
        total += p.second->mHP;
    }

    return total;
}

double Building::takeDamage(GameEntity* attacker, double physicalDamage, double magicalDamage, Tile *tileTakingDamage)
{
    if(mTileData.count(tileTakingDamage) <= 0)
    {
        OD_LOG_ERR("building=" + getName() + ", tile=" + Tile::displayAsString(tileTakingDamage));
        return 0.0;
    }

    TileData* tileData = mTileData.at(tileTakingDamage);
    double damageDone = std::min(tileData->mHP, physicalDamage + magicalDamage);
    tileData->mHP -= damageDone;

    GameMap* gameMap = getGameMap();
    if(!getIsOnServerMap())
        return damageDone;

    Seat* seat = getSeat();
    if (seat == nullptr)
        return damageDone;

    Player* player = gameMap->getPlayerBySeatId(seat->getId());
    if (player == nullptr)
        return damageDone;

    // Tells the server game map the player is under attack.
    gameMap->playerIsFighting(player, tileTakingDamage);

    return damageDone;
}

std::string Building::getNameTile(Tile* tile)
{
    return getMeshName() + "_tile_" + Helper::toString(tile->getX())
        + "_" + Helper::toString(tile->getY());
}

bool Building::isAttackable(Tile* tile, Seat* seat) const
{
    if(getHP(tile) <= 0.0)
        return false;

    return true;
}

bool Building::canSeatSellBuilding(Seat* seat) const
{
    if(!getSeat()->canBuildingBeDestroyedBy(seat))
        return false;

    return true;
}

void Building::exportToStream(std::ostream& os) const
{
    const std::string& name = getName();
    int seatId = getSeat()->getId();
    bool isEditorMode = getGameMap()->isInEditorMode();
    uint32_t nbTiles;
    if(isEditorMode)
        nbTiles = mCoveredTiles.size();
    else
        nbTiles = mCoveredTiles.size() + mCoveredTilesDestroyed.size();

    os << name << "\t" << seatId << "\t" << nbTiles << "\n";
    for(Tile* tile : mCoveredTiles)
    {
        if(mTileData.count(tile) <= 0)
        {
            OD_LOG_ERR("building=" + getName() + ", tile=" + Tile::displayAsString(tile));
            continue;
        }
        TileData* tileData = mTileData.at(tile);
        os << tile->getX() << "\t" << tile->getY();
        exportTileDataToStream(os, tile, tileData);
        os << "\n";
    }

    // In editor mode, we don't export destroyed tiles (there might be some if buildings have been deleted)
    if(isEditorMode)
        return;

    for(Tile* tile : mCoveredTilesDestroyed)
    {
        if(mTileData.count(tile) <= 0)
        {
            OD_LOG_ERR("building=" + getName() + ", tile=" + Tile::displayAsString(tile));
            continue;
        }
        TileData* tileData = mTileData.at(tile);
        os << tile->getX() << "\t" << tile->getY();
        exportTileDataToStream(os, tile, tileData);
        os << "\n";
    }
}

void Building::importFromStream(std::istream& is)
{
    std::string line;
    std::getline(is, line);
    std::stringstream ss(line);

    std::string name;
    OD_ASSERT_TRUE(ss >> name);
    setName(name);

    int seatId;
    uint32_t tilesToLoad;

    OD_ASSERT_TRUE(ss >> seatId);
    Seat* seat = getGameMap()->getSeatById(seatId);
    if(seat == nullptr)
    {
        OD_LOG_ERR("seatId=" + Helper::toString(seatId));
    }
    setSeat(seat);

    // Allied seats can always see buildings
    std::vector<Seat*> alliedSeats = seat->getAlliedSeats();
    alliedSeats.push_back(seat);
    OD_ASSERT_TRUE(ss >> tilesToLoad);
    while(tilesToLoad > 0)
    {
        --tilesToLoad;
        std::getline(is, line);
        std::stringstream ss(line);
        int xxx, yyy;
        OD_ASSERT_TRUE(ss >> xxx >> yyy);
        Tile* tile = getGameMap()->getTile(xxx, yyy);
        if (tile == nullptr)
        {
            OD_LOG_ERR("tile=" + Helper::toString(xxx) + "," + Helper::toString(yyy));
            continue;
        }

        tile->setSeat(getSeat());

        TileData* tileData = createTileData(tile);
        mTileData[tile] = tileData;
        tileData->mSeatsVision = alliedSeats;
        importTileDataFromStream(ss, tile, tileData);
    }
}

void Building::notifySeatVision(Tile* tile, Seat* seat)
{
    TileData* tileData = mTileData[tile];
    auto it = std::find(tileData->mSeatsVision.begin(), tileData->mSeatsVision.end(), seat);
    if(tileData->mHP <= 0)
    {
        // We remove the seat
        if(it == tileData->mSeatsVision.end())
        {
            OD_LOG_ERR("building=" + getName() + ", tile=" + Tile::displayAsString(tile));
            return;
        }

        tileData->mSeatsVision.erase(it);
        return;
    }

    // We add vision
    if(it == tileData->mSeatsVision.end())
        tileData->mSeatsVision.push_back(seat);
}

TileData* Building::createTileData(Tile* tile)
{
    return new TileData;
}
