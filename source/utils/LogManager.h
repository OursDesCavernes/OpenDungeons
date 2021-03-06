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

#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <string>

#include <OgreSingleton.h>

#define OD_STRING_S(x) #x
#define OD_STRING_S_(x) OD_STRING_S(x)
#define S__LINE__ OD_STRING_S_(__LINE__)
#define OD_LOG_ERR(a)      LogManager::getSingleton().logMessage("ERROR: " + std::string(__FILE__) + " line " + std::string(S__LINE__) + std::string(" ") + (a), LogMessageLevel::CRITICAL)
#define OD_LOG_WRN(a)      LogManager::getSingleton().logMessage("WARNING: " + std::string(__FILE__) + " line " + std::string(S__LINE__) + std::string(" ") + (a), LogMessageLevel::WARNING)
#define OD_LOG_INF(a)      LogManager::getSingleton().logMessage(a, LogMessageLevel::NORMAL)
#define OD_LOG_DBG(a)      LogManager::getSingleton().logMessage(a, LogMessageLevel::TRIVIAL)
#define OD_ASSERT_TRUE(a)        if(!(a)) LogManager::getSingleton().logMessage("ERROR: Assert failed file " + std::string(__FILE__) + " line " + std::string(S__LINE__), LogMessageLevel::CRITICAL)
#define OD_ASSERT_TRUE_MSG(a,b)  if(!(a)) LogManager::getSingleton().logMessage("ERROR: Assert failed file " + std::string(__FILE__) + " line " + std::string(S__LINE__) + std::string(" info : ") + b, LogMessageLevel::CRITICAL)


enum class LogMessageLevel
{
    TRIVIAL = 1,
    NORMAL,
    WARNING,
    CRITICAL
};

//! \brief Helper/wrapper class to provide thread-safe logging when ogre is compiled without threads.
class LogManager : public Ogre::Singleton<LogManager>
{
public:
    LogManager() {};
    //! \brief Log a message to the game log.
    virtual void logMessage(const std::string& message, LogMessageLevel lml) = 0;

    //! \brief Set the log detail level.
    //Messages lower than this level will be filtered.
    virtual void setLogDetail(LogMessageLevel ll) = 0;

    static const std::string GAMELOG_NAME;
private:
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
};

#endif // LOGMANAGER_H
