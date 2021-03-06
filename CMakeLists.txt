cmake_minimum_required(VERSION 2.8)

##################################
#### Project settings ############
##################################

project(opendungeons)

# Set a capitalized exe name on Windows
if(WIN32)
    set(PROJECT_BINARY_NAME "OpenDungeons")
else()
    set(PROJECT_BINARY_NAME "opendungeons")
endif()

set(AS_LIBRARY_NAME "angelscript")

# Project version
set(OD_MAJOR_VERSION 0)
set(OD_MINOR_VERSION 6)
set(OD_PATCH_LEVEL   0)

# Set the project version ready to be used in the code.
if(NOT MSVC)
    add_definitions(-DOD_VERSION="${OD_MAJOR_VERSION}.${OD_MINOR_VERSION}.${OD_PATCH_LEVEL}")
endif()

# Set the data path depending on the platform
if(WIN32)
    set(OD_DATA_PATH ".")
    set(OD_PLUGINS_CFG_PATH ".")
else()
    # Set binary and data install locations if we want to use the installer
    set(OD_BIN_PATH ${CMAKE_INSTALL_PREFIX}/games CACHE PATH "Absolute path to the game binary directory")
    set(OD_DATA_PATH ${CMAKE_INSTALL_PREFIX}/share/games/${PROJECT_NAME} CACHE PATH "Absolute path to the game data directory")
    set(OD_SHARE_PATH ${CMAKE_INSTALL_PREFIX}/share CACHE PATH "Absolute path to the shared data directory (desktop file, icons, etc.)")
    # Set the plugins.cfg file path to a common but architecture-dependent location.
    # Because the plugins.cfg Ogre plugins path path may vary depending on the architecture used.
    set(OD_PLUGINS_CFG_PATH /etc/${PROJECT_NAME} CACHE PATH "Absolute path to the Ogre plugins.cfg file")
endif()

if(NOT MSVC)
    # Set the data paths
    add_definitions(-DOD_DATA_PATH="${OD_DATA_PATH}")
    add_definitions(-DOD_PLUGINS_CFG_PATH="${OD_PLUGINS_CFG_PATH}")
endif()

# Where we want the binary
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

# Compilation options
option(OD_ENABLE_WARNINGS "Compile the game with all standard warnings enabled" ON)

# enable/disable unit tests
option(OD_BUILD_TESTING "Compile unit tests (to enable unit tests both this and BUILD_TESTING has to be on." OFF)

if(OD_BUILD_TESTING)
    include(CTest)
endif()

if (UNIX AND NOT APPLE)
    # Linux option - Do not grab the keyboard when using OIS
    # This is breaking the game's input on certain linux distributions and thus, must stay an option for now...
    option(OD_LINUX_NO_KEYBOARD_GRAB "Do not grab keyboard input when using OIS under Linux. Permits input in other applications." ON)

    # Defines the value for the preprocessor.
    if(OD_LINUX_NO_KEYBOARD_GRAB)
        message(STATUS "Linux (OIS): x11_keyboard_grab = false")
        add_definitions(-DOD_LINUX_NO_KEYBOARD_GRAB)
    else()
        message(STATUS "Linux (OIS): x11_keyboard_grab = true")
    endif()
endif()

##################################
#### Useful variables ############
##################################

#project paths
set(SRC "${CMAKE_SOURCE_DIR}/source")
#set(DEPENDENCIES_DIR "${CMAKE_SOURCE_DIR}/dependencies")
#set(ANGELSCRIPT_DIR "${DEPENDENCIES_DIR}/angelscript")
#set(ANGELSCRIPT_SRC_DIR "${ANGELSCRIPT_DIR}/angelscript/source")
#set(ANGELSCRIPT_ADDON_DIR "${ANGELSCRIPT_DIR}/add_on")

#cmake paths
set(CMAKE_CONFIG_DIR "${CMAKE_SOURCE_DIR}/cmake/config")
set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake/modules
    ${CMAKE_SOURCE_DIR}/cmake/modules/BoostTestTargets
)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMake build type is not set, defaulting to 'RelWithDebInfo'")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
# For MSVC builds, the debug flags is added below.
    add_definitions("-DOD_DEBUG")
endif()

##################################
#### ExplicitCompilerFlags #######
##################################

# TODO: Use add_compiler_options instead of setting CMAKE_CXX_FLAGS

# CMAKE_CXX_FLAGS are meant to be defined using these two public strings
# This makes it possible to have them explicitly displayed in CMake GUI
# CMAKE_CXX_FLAGS can still be used to override our default defintions
set(OD_OPT_FLAGS CACHE STRING "Optimization and optional compilation flags")
set(OD_CXX11_FLAGS CACHE STRING "Compilation flags to enable C++11 support")

if(MSVC)
    # C++11 compilation flag is activated by default
    # Optimisation flags can be added if deemed useful
else()
    include(CheckCXXCompilerFlag)
    if(NOT OD_CXX11_FLAGS)
        CHECK_CXX_COMPILER_FLAG("-std=c++11" OD_CXX11_FLAG_SUPPORTED)
        # MinGW supports CXX11 flag but do not compile with it. However, it works with gnu++11
        if(OD_CXX11_FLAG_SUPPORTED AND NOT MINGW)
            set(OD_CXX11_FLAGS "-std=c++11" CACHE STRING "Compilation flags to enable C++11 support" FORCE)
        else()
            set(OD_CXX11_FLAGS "-std=gnu++11" CACHE STRING "Compilation flags to enable C++11 support" FORCE)
        endif()
    endif()
    if(OD_ENABLE_WARNINGS)
        # Help getting compilation warnings
        set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Wall -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wold-style-cast")
        set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Woverloaded-virtual -Wredundant-decls -Wswitch-default -Wstrict-null-sentinel -Winvalid-pch")
        set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Wswitch-default -Wstack-protector")
        #for later https://stackoverflow.com/questions/5088460/flags-to-enable-thorough-and-verbose-g-warnings/9862800#9862800:
        #set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -pedantic -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization ")
        #set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept")
        #set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo")
        #set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused")
        #set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Winline -Winvalid-pch -Wswitch-enum -Wzero-as-null-pointer-constant -Wuseless-cast")
        #set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Weffc++")
    endif()
    if(MINGW)
        # Disable some warnings on MinGW
        if(OD_ENABLE_WARNINGS)
            set(OD_OPT_FLAGS "${OD_OPT_FLAGS} -Wno-unused-local-typedefs -Wno-format")
        endif()

        # This includes enough debug information to get something useful
        # from Dr. Mingw while keeping binary size down. Almost useless
        # with gdb, though.
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -ggdb0 -gstabs2")
    endif()
endif()

# Set compiler options in MSVC
if(WIN32 AND MSVC)
    #Set some extra compiler flags
    #TODO - investigate if these are what they should be
    set(PLATFORM_C_FLAGS "/W3 /MD /Od /DWIN32 /D_WINDOWS /Gm /Gy /fp:fast /ZI /EHsc")
    set(PLATFORM_C_FLAGS_DEBUG "/W3 /MDd /Od /Gm /Gy /fp:fast /ZI /DOD_DEBUG" )

    # Set the application version
    add_definitions(/DOD_VERSION="${OD_MAJOR_VERSION}.${OD_MINOR_VERSION}.${OD_PATCH_LEVEL}")

    # Set the data paths
    add_definitions(/DOD_DATA_PATH="${OD_DATA_PATH}")
    add_definitions(/DOD_PLUGINS_CFG_PATH="${OD_PLUGINS_CFG_PATH}")

    set(CMAKE_CXX_FLAGS "${PLATFORM_C_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${PLATFORM_C_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${PLATFORM_C_FLAGS_DEBUG}")
endif()

if(WIN32)
    #Tell OIS we are linking dynamically for dllexport/dllimport attributes.
    #TODO: Find out if this is needed.
    add_definitions(-DOIS_DYNAMIC_LIB)
endif()

set(CMAKE_CXX_FLAGS "${OD_CXX11_FLAGS} ${OD_OPT_FLAGS} ${CMAKE_CXX_FLAGS}")
message(STATUS "CMake CXX Flags: " ${CMAKE_CXX_FLAGS})

##################################
#### Source files (.cpp) #########
##################################

#Add new .cpp files here so that they get compiled
#set(AS_SOURCEFILES
    #AngelScript sources
#    ${ANGELSCRIPT_SRC_DIR}/as_atomic.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_builder.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_bytecode.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc.cpp
#Disabling exotic platforms for now
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_arm.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_mips.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_ppc.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_ppc_64.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_sh4.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_x64_gcc.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_x64_msvc.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_x64_mingw.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_x86.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_callfunc_xenon.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_compiler.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_configgroup.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_context.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_datatype.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_gc.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_generic.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_globalproperty.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_memory.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_module.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_objecttype.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_outputbuffer.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_parser.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_restore.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_scriptcode.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_scriptengine.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_scriptfunction.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_scriptnode.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_scriptobject.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_string.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_string_util.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_thread.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_tokenizer.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_typeinfo.cpp
#    ${ANGELSCRIPT_SRC_DIR}/as_variablescope.cpp

    #AngelScript Addon sources
#    ${ANGELSCRIPT_ADDON_DIR}/scriptarray/scriptarray.cpp
#    ${ANGELSCRIPT_ADDON_DIR}/scriptbuilder/scriptbuilder.cpp
#    ${ANGELSCRIPT_ADDON_DIR}/scripthelper/scripthelper.cpp
#    ${ANGELSCRIPT_ADDON_DIR}/scriptstdstring/scriptstdstring.cpp
#)
set(OD_SOURCEFILES

    #OpenDungeons sources
    ${SRC}/ai/AIManager.cpp
    ${SRC}/ai/BaseAI.cpp
    ${SRC}/ai/KeeperAI.cpp

    ${SRC}/camera/CameraManager.cpp
    ${SRC}/camera/HermiteCatmullSpline.cpp

    ${SRC}/creatureeffect/CreatureEffect.cpp
    ${SRC}/creatureeffect/CreatureEffectExplosion.cpp
    ${SRC}/creatureeffect/CreatureEffectHeal.cpp
    ${SRC}/creatureeffect/CreatureEffectSlap.cpp
    ${SRC}/creatureeffect/CreatureEffectSpeedChange.cpp

    ${SRC}/creaturemood/CreatureMood.cpp
    ${SRC}/creaturemood/CreatureMoodAwakness.cpp
    ${SRC}/creaturemood/CreatureMoodCreature.cpp
    ${SRC}/creaturemood/CreatureMoodFee.cpp
    ${SRC}/creaturemood/CreatureMoodHunger.cpp
    ${SRC}/creaturemood/CreatureMoodHpLoss.cpp
    ${SRC}/creaturemood/CreatureMoodTurnsWithoutFight.cpp

    ${SRC}/entities/Building.cpp
    ${SRC}/entities/BuildingObject.cpp
    ${SRC}/entities/ChickenEntity.cpp
    ${SRC}/entities/CraftedTrap.cpp
    ${SRC}/entities/Creature.cpp
    ${SRC}/entities/CreatureAction.cpp
    ${SRC}/entities/CreatureDefinition.cpp
    ${SRC}/entities/DoorEntity.cpp
    ${SRC}/entities/EntityBase.cpp
    ${SRC}/entities/EntityLoading.cpp
    ${SRC}/entities/GameEntity.cpp
    ${SRC}/entities/GameEntityType.cpp
    ${SRC}/entities/GiftBoxEntity.cpp
    ${SRC}/entities/MapLight.cpp
    ${SRC}/entities/MissileBoulder.cpp
    ${SRC}/entities/MissileObject.cpp
    ${SRC}/entities/MissileOneHit.cpp
    ${SRC}/entities/MovableGameEntity.cpp
    ${SRC}/entities/PersistentObject.cpp
    ${SRC}/entities/RenderedMovableEntity.cpp
    ${SRC}/entities/ResearchEntity.cpp
    ${SRC}/entities/SmallSpiderEntity.cpp
    ${SRC}/entities/Tile.cpp
    ${SRC}/entities/TileFunctions.cpp
    ${SRC}/entities/TrapEntity.cpp
    ${SRC}/entities/TreasuryObject.cpp
    ${SRC}/entities/Weapon.cpp

    ${SRC}/game/Player.cpp
    ${SRC}/game/PlayerFunctions.cpp
    ${SRC}/game/PlayerSelection.cpp
    ${SRC}/game/Research.cpp
    ${SRC}/game/ResearchManager.cpp
    ${SRC}/game/Seat.cpp
    ${SRC}/game/SeatFunctions.cpp

    ${SRC}/gamemap/GameMap.cpp
    ${SRC}/gamemap/MapLoader.cpp
    ${SRC}/gamemap/MiniMap.cpp
    ${SRC}/gamemap/MiniMapCamera.cpp
    ${SRC}/gamemap/TileContainer.cpp
    ${SRC}/gamemap/TileSet.cpp

    ${SRC}/giftboxes/GiftBoxResearch.cpp

    ${SRC}/goals/Goal.cpp
    ${SRC}/goals/GoalClaimNTiles.cpp
    ${SRC}/goals/GoalLoading.cpp
    ${SRC}/goals/GoalKillAllEnemies.cpp
    ${SRC}/goals/GoalMineNGold.cpp
    ${SRC}/goals/GoalProtectCreature.cpp
    ${SRC}/goals/GoalProtectDungeonTemple.cpp

    ${SRC}/modes/AbstractApplicationMode.cpp
    ${SRC}/modes/Command.cpp
    ${SRC}/modes/ConsoleCommands.cpp
    ${SRC}/modes/ConsoleInterface.cpp
    ${SRC}/modes/EditorMode.cpp
    ${SRC}/modes/GameMode.cpp
    ${SRC}/modes/GameEditorModeBase.cpp
    ${SRC}/modes/GameEditorModeConsole.cpp
    ${SRC}/modes/InputManager.cpp
    ${SRC}/modes/MenuModeMain.cpp
    ${SRC}/modes/MenuModeConfigureSeats.cpp
    ${SRC}/modes/MenuModeEditor.cpp
    ${SRC}/modes/MenuModeLoad.cpp
    ${SRC}/modes/MenuModeMultiplayerClient.cpp
    ${SRC}/modes/MenuModeMultiplayerServer.cpp
    ${SRC}/modes/MenuModeReplay.cpp
    ${SRC}/modes/MenuModeSkirmish.cpp
    ${SRC}/modes/ModeManager.cpp
    ${SRC}/modes/SettingsWindow.cpp

    ${SRC}/network/ChatEventMessage.cpp
    ${SRC}/network/ClientNotification.cpp
    ${SRC}/network/ODClient.cpp
    ${SRC}/network/ODPacket.cpp
    ${SRC}/network/ODServer.cpp
    ${SRC}/network/ODSocketClient.cpp
    ${SRC}/network/ODSocketServer.cpp
    ${SRC}/network/ServerNotification.cpp

    ${SRC}/render/CreatureOverlayStatus.cpp
    ${SRC}/render/Gui.cpp
    ${SRC}/render/MovableTextOverlay.cpp
    ${SRC}/render/ODFrameListener.cpp
    ${SRC}/render/RenderManager.cpp
    ${SRC}/render/TextRenderer.cpp

    ${SRC}/rooms/Room.cpp
    ${SRC}/rooms/RoomCrypt.cpp
    ${SRC}/rooms/RoomDormitory.cpp
    ${SRC}/rooms/RoomDungeonTemple.cpp
    ${SRC}/rooms/RoomHatchery.cpp
    ${SRC}/rooms/RoomLibrary.cpp
    ${SRC}/rooms/RoomManager.cpp
    ${SRC}/rooms/RoomPortal.cpp
    ${SRC}/rooms/RoomPortalWave.cpp
    ${SRC}/rooms/RoomType.cpp
    ${SRC}/rooms/RoomTrainingHall.cpp
    ${SRC}/rooms/RoomTreasury.cpp
    ${SRC}/rooms/RoomWorkshop.cpp

    ${SRC}/sound/MusicPlayer.cpp
    ${SRC}/sound/SoundEffectsManager.cpp

    ${SRC}/spawnconditions/SpawnCondition.cpp
    ${SRC}/spawnconditions/SpawnConditionCreature.cpp
    ${SRC}/spawnconditions/SpawnConditionGold.cpp
    ${SRC}/spawnconditions/SpawnConditionRoom.cpp

    ${SRC}/spells/Spell.cpp
    ${SRC}/spells/SpellCallToWar.cpp
    ${SRC}/spells/SpellCreatureExplosion.cpp
    ${SRC}/spells/SpellCreatureHaste.cpp
    ${SRC}/spells/SpellCreatureHeal.cpp
    ${SRC}/spells/SpellManager.cpp
    ${SRC}/spells/SpellSummonWorker.cpp
    ${SRC}/spells/SpellType.cpp

    ${SRC}/traps/Trap.cpp
    ${SRC}/traps/TrapBoulder.cpp
    ${SRC}/traps/TrapCannon.cpp
    ${SRC}/traps/TrapDoor.cpp
    ${SRC}/traps/TrapManager.cpp
    ${SRC}/traps/TrapSpike.cpp
    ${SRC}/traps/TrapType.cpp

    ${SRC}/utils/ConfigManager.cpp
    ${SRC}/utils/FrameRateLimiter.cpp
    ${SRC}/utils/Helper.cpp
    ${SRC}/utils/LogManager.cpp
    ${SRC}/utils/LogManagerFile.cpp
    ${SRC}/utils/LogManagerOgre.cpp
    ${SRC}/utils/Random.cpp
    ${SRC}/utils/ResourceManager.cpp

    ${SRC}/ODApplication.cpp
    ${SRC}/main.cpp
)

IF (MINGW)
    SET(OD_SOURCEFILES ${OD_SOURCEFILES} ${SRC}/utils/StackTraceWinMinGW.cpp)
ELSEIF(MSVC)
    SET(OD_SOURCEFILES ${OD_SOURCEFILES} ${SRC}/utils/StackTraceWinMSVC.cpp)
ELSEIF(UNIX)
    SET(OD_SOURCEFILES ${OD_SOURCEFILES} ${SRC}/utils/StackTraceUnix.cpp)
ELSE()
    SET(OD_SOURCEFILES ${OD_SOURCEFILES} ${SRC}/utils/StackTraceStub.cpp)
ENDIF ()

# Adds the Windows icon resource file when building on windows.
IF (WIN32)
    SET(OD_SOURCEFILES ${OD_SOURCEFILES} ${CMAKE_SOURCE_DIR}/dist/icon.rc)
ENDIF ()

##################################
#### Find packages ###############
##################################

find_package(Threads REQUIRED)
find_package(OIS REQUIRED)
find_package(OGRE REQUIRED)
find_package(CEGUI REQUIRED)
find_package(SFML 2 REQUIRED COMPONENTS Audio System Network)

if((OGRE_VERSION_MAJOR LESS 1) AND (OGRE_VERSION_MINOR LESS 9))
    message(FATAL_ERROR "OGRE version >= 1.9.0 required")
endif()

if("${CEGUI_VERSION}" VERSION_LESS "0.8.0")
    message(FATAL_ERROR "CEGUI version >= 0.8.0 required")
endif()

if (SFML_VERSION_MAJOR LESS 2)
    message(FATAL_ERROR "SFML version >= 2.0 required")
else()
    message(STATUS "SFML include directory: ${SFML_INCLUDE_DIR}; SFML audio library: ${SFML_AUDIO_LIBRARY_DEBUG} ${SFML_AUDIO_LIBRARY_RELEASE}")
endif()

#This has to cover the versions not already known by CMake
set(Boost_ADDITIONAL_VERSIONS 1.47 1.47.0 1.47.1 1.55.0)

set(OD_BOOST_COMPONENTS system filesystem locale program_options)

if(WIN32 AND ${OGRE_FOUND})
    # On windows we can try to use boost from the Ogre SDK instead
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} ${ENV_OGRE_HOME}/boost_1_44/lib)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${ENV_OGRE_HOME}/boost_1_44)

    # We set these instead of searching if using MSVC
    # This is because the libs in the ogre dir has a lib prefix making findboost not find them
    set(OD_BOOST_LIB_DIRS  ${ENV_OGRE_HOME}/boost_1_44/lib)
    set(OD_BOOST_INCLUDE_DIRS ${ENV_OGRE_HOME}/boost_1_44)

    find_package(Boost)
    if(Boost_FOUND)
        set(OD_BOOST_LIB_DIRS  ${Boost_INCLUDE_DIRS}/stage/lib)
        set(OD_BOOST_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
    endif()
endif()

if(MINGW OR UNIX)
    # We need to specify the compiler here that Ogre used to compile boost
    find_package(Boost REQUIRED COMPONENTS ${OD_BOOST_COMPONENTS} OPTIONAL_COMPONENTS unit_test_framework)
endif()

##################################
#### Headers and linking #########
##################################

#add all project specific include directorieS
include_directories(
    #OpenDungeons includes
    ${CMAKE_SOURCE_DIR}/source

    #Angelscript includes
#    SYSTEM ${ANGELSCRIPT_DIR}/angelscript/include
#    SYSTEM ${ANGELSCRIPT_SRC_DIR}

    #AngelScript Addons, in-comment only the ones we actually use
#    SYSTEM ${ANGELSCRIPT_ADDON_DIR}/scriptarray
#    SYSTEM ${ANGELSCRIPT_ADDON_DIR}/scriptbuilder
#    SYSTEM ${ANGELSCRIPT_ADDON_DIR}/scripthelper
#    SYSTEM ${ANGELSCRIPT_ADDON_DIR}/scriptstdstring

    #external packages includes
    SYSTEM ${CEGUI_INCLUDE_DIR}
    SYSTEM ${SFML_INCLUDE_DIR}
    SYSTEM ${OGRE_INCLUDE_DIRS}
    SYSTEM ${OIS_INCLUDE_DIRS}
)

if(WIN32)
    if(MINGW)
        #TODO: Why are we linking boost here? It's linked again later.
        # Boost
        link_libraries(${Boost_LIBRARIES})
        include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
    endif()

    if(MSVC)
        include_directories(SYSTEM ${OD_BOOST_INCLUDE_DIRS})
        link_directories(${OD_BOOST_LIB_DIRS})
    endif()
endif()

##################################
#### Binary ######################
##################################

# Create the angelscript library file
#add_library(${AS_LIBRARY_NAME} ${AS_SOURCEFILES})

# Treat warnings as errors for main build, but not angelscript, since it would fail.
if(NOT MSVC)
    add_compile_options("-Werror")
endif()

# Create the binary file (WIN32 makes sure there is no console window on windows.)
add_executable(${PROJECT_BINARY_NAME} WIN32 ${OD_SOURCEFILES})

##################################
#### Link libraries ##############
##################################

# Add threads when linking AngelScript
#target_link_libraries(${AS_LIBRARY_NAME} ${CMAKE_THREAD_LIBS_INIT})

# Link angelscript
#target_link_libraries(${PROJECT_BINARY_NAME} ${AS_LIBRARY_NAME})

# Link libraries
target_link_libraries(
    #target
    ${PROJECT_BINARY_NAME}

    #libraries
    ${OGRE_LIBRARIES}
    ${OGRE_RTShaderSystem_LIBRARIES}
    ${OIS_LIBRARIES}
    ${CEGUI_LIBRARIES}
    ${CEGUI_OgreRenderer_LIBRARIES}
)

# Set linker options in MSVC
if(WIN32 AND MSVC)
    # We need to force output because of the boost lib used, defining two times the
    # same set of functions, once for Ogre, once for OD. It is harmless in our case
    # to select this option.
    SET_TARGET_PROPERTIES(${PROJECT_BINARY_NAME} PROPERTIES LINK_FLAGS " /FORCE:MULTIPLE")
endif()

# The name of the OGRE Overlay library is available as CMAKE variable, also discovering debug versions correctly; please leave it like that!
target_link_libraries(${PROJECT_BINARY_NAME} ${OGRE_Overlay_LIBRARY})

# We link needed libraries to display stacktrace on uncatched exception
if(MINGW)
# MinGW32 needs intl while not MinGW64. To differentiate them, we test the compiler version as it does not change very often for MinGW32
    if(${CMAKE_CXX_COMPILER_VERSION} STREQUAL "4.8.1")
        message(STATUS "MINGW32 COMPILER VERSION: " ${CMAKE_CXX_COMPILER_VERSION})
        target_link_libraries(${PROJECT_BINARY_NAME} imagehlp bfd intl iberty z)
    else()
        message(STATUS "MINGW64 COMPILER VERSION: " ${CMAKE_CXX_COMPILER_VERSION})
        target_link_libraries(${PROJECT_BINARY_NAME} imagehlp bfd iberty z)
    endif()
elseif(MSVC)
    target_link_libraries(${PROJECT_BINARY_NAME} imagehlp)
endif()

# MSVC automatically links boost
if(NOT MSVC)
    target_link_libraries(${PROJECT_BINARY_NAME} ${Boost_LIBRARIES})
endif()

# Link sfml
# No need to make a difference when release/debug is found because even
# if only one is found, the other is set to the same value
target_link_libraries(${PROJECT_BINARY_NAME} ${SFML_LIBRARIES})

##################################
#### Unit testing ################
##################################

if(BUILD_TESTING AND OD_BUILD_TESTING)
    if(Boost_UNIT_TEST_FRAMEWORK_FOUND)
        message(STATUS "boost testing framework found, enabling tests")
        add_subdirectory("${SRC}/tests")
    else()
        message(STATUS "boost testing framework not found, not enabling tests")
    endif()
endif()

##################################
#### Configure settings files ####
##################################

if(WIN32)
    #On windows, use current directory for plugins and data for now
    set(OD_OGRE_PLUGIN_DIR_REL "")
    set(OD_OGRE_PLUGIN_DIR_DBG "")
else()
    set(OD_OGRE_PLUGIN_DIR_REL ${OGRE_PLUGIN_DIR_REL})
    set(OD_OGRE_PLUGIN_DIR_DBG ${OGRE_PLUGIN_DIR_DBG})

    message(STATUS "Plugin path rel: " ${OD_OGRE_PLUGIN_DIR_REL})
    message(STATUS "Plugin path dbg: " ${OD_OGRE_PLUGIN_DIR_DBG})
endif()

#Do the configuration
configure_file(${CMAKE_CONFIG_DIR}/plugins.cfg.in ${CMAKE_BINARY_DIR}/plugins.cfg)
if(WIN32)
    configure_file(${CMAKE_CONFIG_DIR}/plugins_d.cfg.in ${CMAKE_BINARY_DIR}/plugins_d.cfg)
endif()
configure_file(${CMAKE_CONFIG_DIR}/resources.cfg.in ${CMAKE_BINARY_DIR}/resources.cfg)
# Link icon with its full path when not installed in the /usr prefix since it might not be caught by the hicolor theme
if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    set(OD_ICON_FULLPATH ${PROJECT_BINARY_NAME})
else()
    set(OD_ICON_FULLPATH ${OD_SHARE_PATH}/icons/hicolor/scalable/apps/opendungeons.svg)
endif()
configure_file(${CMAKE_CONFIG_DIR}/opendungeons.desktop.in ${CMAKE_BINARY_DIR}/opendungeons.desktop)
configure_file(${CMAKE_CONFIG_DIR}/opendungeons.6.in ${CMAKE_BINARY_DIR}/opendungeons.6)

##################################
### Run-in-place customisation ###
##################################

# If the project is not built in the source directory, create symbolic links
# pointing to the game resources so that the game can be run in place
# NOTE: This won't work on FAT filesystems
if(NOT CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    foreach(RES config gui levels materials models music particles scripts sounds)
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/${RES})
            message(STATUS "Creating symlink to ${CMAKE_SOURCE_DIR}/${RES} in ${CMAKE_BINARY_DIR}")
            if(UNIX)
                execute_process(COMMAND ln -s -r -T ${CMAKE_SOURCE_DIR}/${RES} ${CMAKE_BINARY_DIR}/${RES})
            elseif(WIN32 AND ${CMAKE_SYSTEM_VERSION} GREATER 6.0) # Windows Vista or newer
                # For some reason "COMMAND cmd.exe /c mklink /j" does not work as expected
                # so we use a batch file to workaround it (cf. https://github.com/OpenDungeons/OpenDungeons/pull/206 for details)
                execute_process(COMMAND cmd.exe /c ${CMAKE_SOURCE_DIR}/cmake/winsymlink.bat ${CMAKE_BINARY_DIR}/${RES} ${CMAKE_SOURCE_DIR}/${RES})
            endif()
        endif()
    endforeach(RES)
endif()

##################################
#### Installation ################
##################################

if(UNIX)
    set(OD_PLUGINSCFGFILE ${CMAKE_BINARY_DIR}/plugins.cfg)

    set(OD_RESOURCESFILE ${CMAKE_BINARY_DIR}/resources.cfg)

    set(OD_RESOURCES   ${CMAKE_SOURCE_DIR}/config
                       ${CMAKE_SOURCE_DIR}/gui
                       ${CMAKE_SOURCE_DIR}/levels
                       ${CMAKE_SOURCE_DIR}/materials
                       ${CMAKE_SOURCE_DIR}/models
                       ${CMAKE_SOURCE_DIR}/music
                       ${CMAKE_SOURCE_DIR}/particles
                       ${CMAKE_SOURCE_DIR}/scripts
                       ${CMAKE_SOURCE_DIR}/sounds)

    set(OD_DOC         ${CMAKE_SOURCE_DIR}/AUTHORS
                       ${CMAKE_SOURCE_DIR}/CREDITS
                       ${CMAKE_SOURCE_DIR}/LICENSE.md
                       ${CMAKE_SOURCE_DIR}/README.md
                       ${CMAKE_SOURCE_DIR}/RELEASE-NOTES.md)

    # Install required game files: binary, configuration files and resources
    install(TARGETS ${PROJECT_BINARY_NAME}
            DESTINATION ${OD_BIN_PATH}
            PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    install(FILES ${OD_PLUGINSCFGFILE}
            DESTINATION ${OD_PLUGINS_CFG_PATH})
    install(FILES ${OD_RESOURCESFILE}
            DESTINATION ${OD_DATA_PATH})
    install(DIRECTORY ${OD_RESOURCES}
            DESTINATION ${OD_DATA_PATH})

    # Additional distribution content (desktop file, icons, man page, doc)
    install(FILES ${CMAKE_SOURCE_DIR}/dist/opendungeons.appdata.xml
            DESTINATION ${OD_SHARE_PATH}/appdata)
    install(FILES ${CMAKE_BINARY_DIR}/opendungeons.desktop
            DESTINATION ${OD_SHARE_PATH}/applications)
    install(FILES ${CMAKE_BINARY_DIR}/opendungeons.6
            DESTINATION ${OD_SHARE_PATH}/man/man6)
    install(FILES ${OD_DOC}
            DESTINATION ${OD_SHARE_PATH}/doc/${PROJECT_NAME})
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/dist/hicolor
            DESTINATION ${OD_SHARE_PATH}/icons)
endif()

##################################
#### Packaging ###################
##################################

#Not used at the moment, more flexible to do the deb packages manually
#TODO - set up for making tarballs
# set(CPACK_SOURCE_IGNORE_FILES "~$" "${CMAKE_SOURCE_DIR}.*/.svn/" "${CMAKE_SOURCE_DIR}/debian/")
# set(CPACK_PACKAGE_NAME opendungeons)
# set(CPACK_PACKAGE_VERSION ${${PROJECT_NAME}_MAJOR_VERSION}.${${PROJECT_NAME}_MINOR_VERSION}.${${PROJECT_NAME}_PATCH_LEVEL})
# set(CPACK_PACKAGE_CONTACT OpenDungeons Team)
# set(CPACK_GENERATOR "DEB")
# set(CPACK_SOURCE_GENERATOR "DEB")
# set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcegui (>= 0.8.3), libois-1.2.0, libsfml-audio2.0, libopenal1, libogremain-1.9.0")
# include(CPack)
