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

#include "sound/SoundEffectsManager.h"

#include "utils/Helper.h"
#include "utils/ResourceManager.h"
#include "utils/Random.h"
#include "utils/LogManager.h"
#include "network/ODPacket.h"

#include <OgreQuaternion.h>

// class GameSound
GameSound::GameSound(const std::string& filename, bool spatialSound):
    mSound(nullptr),
    mSoundBuffer(nullptr)
{
    // Loads the buffer
    mSoundBuffer = new sf::SoundBuffer();
    if (mSoundBuffer->loadFromFile(filename) == false)
    {
        delete mSoundBuffer;
        mSoundBuffer = nullptr;
        return;
    }

    mFilename = filename;

    mSound = new sf::Sound();
    // Loads the main sound object
    mSound->setBuffer(*mSoundBuffer);

    mSound->setLoop(false);

    // Sets a correct attenuation value if the sound is spatial
    if (spatialSound == true)
    {
        // Set convenient spatial fading unit.
        mSound->setVolume(100.0f);
        mSound->setAttenuation(3.0f);
        mSound->setMinDistance(3.0f);
    }
    else // Disable attenuation for sounds that must heard the same way everywhere
    {
        // Prevents the sound from being too loud
        mSound->setVolume(30.0f);
        mSound->setAttenuation(0.0f);
    }
}

GameSound::~GameSound()
{
    // The sound object must be stopped and destroyed before its corresponding
    // buffer to ensure detaching the sound stream from the sound source.
    // This prevents a lot of warnings at app quit.
    if (mSound != nullptr)
    {
        mSound->stop();
        delete mSound;
    }

    if (mSoundBuffer != nullptr)
        delete mSoundBuffer;
}

void GameSound::play(float x, float y, float z)
{
    // Check whether the sound is spatial
    if (mSound->getAttenuation() > 0.0f)
    {
        // Check the distance against the listener height and cull the sound accordingly.
        // This permits to hear only the sound of the area seen in game,
        // and avoid glitches in heard sounds.
        sf::Vector3f lis = sf::Listener::getPosition();
        float distance2 = (lis.x - x) * (lis.x - x) + (lis.y - y) * (lis.y - y);
        double height2 = (lis.z * lis.z) + (lis.z * lis.z);

        if (distance2 > height2)
            return;
    }

    mSound->setPosition(x, y, z);
    mSound->play();
}

// SoundEffectsManager class
template<> SoundEffectsManager* Ogre::Singleton<SoundEffectsManager>::msSingleton = nullptr;

SoundEffectsManager::SoundEffectsManager() :
    mSpatialSounds(static_cast<uint32_t>(SpatialSoundType::nbSounds))
{
    initializeSpatialSounds();
}

SoundEffectsManager::~SoundEffectsManager()
{
    // Clear up every cached sounds...
    std::map<std::string, GameSound*>::iterator it = mGameSoundCache.begin();
    std::map<std::string, GameSound*>::iterator it_end = mGameSoundCache.end();
    for (; it != it_end; ++it)
    {
        if (it->second != nullptr)
            delete it->second;
    }
}

void SoundEffectsManager::initializeSpatialSounds()
{

    // We read the sound directory
    for(uint32_t i = 0; i < static_cast<uint32_t>(SpatialSoundType::nbSounds); ++i)
    {
        SpatialSoundType sound = static_cast<SpatialSoundType>(i);
        readSounds(sound);
    }
}

void SoundEffectsManager::readSounds(SpatialSoundType soundType)
{
    uint32_t indexSound = static_cast<uint32_t>(soundType);
    if(indexSound >= mSpatialSounds.size())
    {
        OD_LOG_ERR("soundType=" + Helper::toString(indexSound) + ", size=" + Helper::toString(mSpatialSounds.size()));
        return;
    }

    std::map<std::string, std::vector<GameSound*>>& soundsFamily = mSpatialSounds[indexSound];
    const std::string& soundFolderPath = ResourceManager::getSingletonPtr()->getSoundPath();
    std::string path;
    bool spatialSound = true;
    switch(soundType)
    {
        case SpatialSoundType::Interface:
            path = "Interface";
            spatialSound = false;
            break;
        case SpatialSoundType::Game:
            path = "Game";
            break;
        case SpatialSoundType::Creatures:
            path = "Creatures";
            break;
        case SpatialSoundType::Rooms:
            path = "Rooms";
            break;
        case SpatialSoundType::Traps:
            path = "Traps";
            break;
        case SpatialSoundType::Spells:
            path = "Spells";
            break;
        default:
            OD_LOG_ERR("Unexpected soundType=" + Helper::toString(indexSound));
            return;
    }

    OD_LOG_INF("Loading sounds for path=" + path);
    readSounds(soundsFamily, soundFolderPath + path, "", spatialSound);
}

void SoundEffectsManager::readSounds(std::map<std::string, std::vector<GameSound*>>& soundsFamily,
        const std::string& parentPath, const std::string& parentFamily, bool spatialSound)
{
    std::vector<std::string> directories;
    if(!Helper::fillDirList(parentPath, directories, false))
    {
        OD_LOG_ERR("Error while loading sounds in directory=" + parentPath);
        return;
    }

    for(const std::string& directory : directories)
    {
        // We read sub directories
        std::string fullDir = parentPath + "/" + directory;
        std::string fullFamily = parentFamily.empty() ? directory : parentFamily + "/" + directory;
        readSounds(soundsFamily, fullDir, fullFamily, spatialSound);

        std::vector<std::string> soundFilenames;
        if(!Helper::fillFilesList(fullDir, soundFilenames, ".ogg"))
        {
            OD_LOG_ERR("Cannot load sounds from=" + fullDir);
            continue;
        }

        // We do not create entries for empty directories
        if(soundFilenames.empty())
            continue;

        std::vector<GameSound*>& sounds = soundsFamily[fullFamily];
        for(const std::string& soundFilename : soundFilenames)
        {
            GameSound* gm = getGameSound(soundFilename, spatialSound);
            if (gm == nullptr)
            {
                OD_LOG_ERR("Cannot load sound=" + soundFilename);
                continue;
            }

            OD_LOG_INF("Sound loaded family=" + fullFamily + ", filename=" + soundFilename);
            sounds.push_back(gm);
        }
    }
}

void SoundEffectsManager::setListenerPosition(const Ogre::Vector3& position, const Ogre::Quaternion& orientation)
{
    sf::Listener::setPosition(static_cast<float> (position.x),
                              static_cast<float> (position.y),
                              static_cast<float> (position.z));

    Ogre::Vector3 vDir = orientation.zAxis();
    sf::Listener::setDirection(-vDir.x, -vDir.y, -vDir.z);
}

void SoundEffectsManager::playSpatialSound(SpatialSoundType soundType, const std::string& family,
        float XPos, float YPos, float height)
{
    uint32_t indexSound = static_cast<uint32_t>(soundType);
    if(indexSound >= mSpatialSounds.size())
    {
        OD_LOG_ERR("sound=" + Helper::toString(indexSound) + ", size=" + Helper::toString(mSpatialSounds.size()));
        return;
    }

    std::map<std::string, std::vector<GameSound*>>& soundsFamily = mSpatialSounds[indexSound];
    auto it = soundsFamily.find(family);
    if(it == soundsFamily.end())
    {
        OD_LOG_ERR("Couldn't find sound=" + Helper::toString(indexSound) + " family=" + family);
        return;
    }

    std::vector<GameSound*>& sounds = it->second;
    if(sounds.empty())
    {
        OD_LOG_ERR("No sound found for sound=" + Helper::toString(indexSound) + " family=" + family);
        return;
    }

    unsigned int soundId = Random::Uint(0, sounds.size() - 1);
    sounds[soundId]->play(XPos, YPos, height);
}

GameSound* SoundEffectsManager::getGameSound(const std::string& filename, bool spatialSound)
{
    // We add a suffix to the sound filename as two versions of it can be kept, one spatial,
    // and one which isn't.
    std::string soundFile = filename + (spatialSound ? "_spatial": "");

    std::map<std::string, GameSound*>::iterator it = mGameSoundCache.find(soundFile);
    // Create a new game sound instance when the sound doesn't exist and register it.
    if (it == mGameSoundCache.end())
    {
        GameSound* gm = new GameSound(filename, spatialSound);
        if (gm->isInitialized() == false)
        {
            // Invalid sound filename
            delete gm;
            return nullptr;
        }

        mGameSoundCache.insert(std::make_pair(soundFile, gm));
        return gm;
    }

    return it->second;
}

ODPacket& operator<<(ODPacket& os, const SpatialSoundType& st)
{
    os << static_cast<int32_t>(st);
    return os;
}

ODPacket& operator>>(ODPacket& is, SpatialSoundType& st)
{
    int32_t tmp;
    is >> tmp;
    st = static_cast<SpatialSoundType>(tmp);
    return is;
}
