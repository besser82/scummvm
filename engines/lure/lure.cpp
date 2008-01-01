/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */



#include "common/config-manager.h"
#include "common/system.h"
#include "common/savefile.h"

#include "lure/luredefs.h"
#include "lure/surface.h"
#include "lure/lure.h"
#include "lure/intro.h"
#include "lure/game.h"
#include "lure/sound.h"

namespace Lure {

static LureEngine *int_engine = NULL;

LureEngine::LureEngine(OSystem *system, const LureGameDescription *gameDesc): Engine(system), _gameDescription(gameDesc) {

	Common::addSpecialDebugLevel(kLureDebugScripts, "scripts", "Scripts debugging");
	Common::addSpecialDebugLevel(kLureDebugAnimations, "animations", "Animations debugging");
	Common::addSpecialDebugLevel(kLureDebugHotspots, "hotspots", "Hotspots debugging");
	Common::addSpecialDebugLevel(kLureDebugFights, "fights", "Fights debugging");
	Common::addSpecialDebugLevel(kLureDebugSounds, "sounds", "Sounds debugging");
	Common::addSpecialDebugLevel(kLureDebugStrings, "strings", "Strings debugging");

	// Setup mixer

	if (!_mixer->isReady()) {
		warning("Sound initialization failed.");
	}
}

int LureEngine::init() {
	int_engine = this;

	_system->beginGFXTransaction();
	initCommonGFX(false);
	_system->initSize(FULL_SCREEN_WIDTH, FULL_SCREEN_HEIGHT);
	_system->endGFXTransaction();

	// Check the version of the lure.dat file
	Common::File f;
	VersionStructure version;
	if (!f.open(SUPPORT_FILENAME))  
		GUIError("Could not locate Lure support file");
	
	f.seek(0xbf * 8);
	f.read(&version, sizeof(VersionStructure));
	f.close();

	if (READ_LE_UINT16(&version.id) != 0xffff)
		GUIError("Error validating %s - file is invalid or out of date", SUPPORT_FILENAME);
	else if ((version.vMajor != LURE_DAT_MAJOR) || (version.vMinor != LURE_DAT_MINOR))
		GUIError("Incorrect version of %s file - expected %d.%d but got %d.%d",
			SUPPORT_FILENAME, LURE_DAT_MAJOR, LURE_DAT_MINOR, 
			version.vMajor, version.vMinor);

	_disk = new Disk();
	_resources = new Resources();
	_strings = new StringData();
	_screen = new Screen(*_system);
	_mouse = new Mouse();
	_events = new Events();
	_menu = new Menu();
	Surface::initialise();
	_room = new Room();
	_fights = new FightsManager();
	return 0;
}

LureEngine::~LureEngine() {
	// Remove all of our debug levels here
	Common::clearAllSpecialDebugLevels();

	// Delete and deinitialise subsystems
	Surface::deinitialise();
	delete _fights;
	delete _room;
	delete _menu;
	delete _events;
	delete _mouse;
	delete _screen;
	delete _strings;
	delete _resources;
	delete _disk;
}

LureEngine &LureEngine::getReference() {
	return *int_engine;
}

int LureEngine::go() {

	if (ConfMan.getBool("copy_protection")) {
		CopyProtectionDialog *dialog = new CopyProtectionDialog();
		bool result = dialog->show();
		delete dialog;
		if (_events->quitFlag)
			return 0;

		if (!result)
			error("Sorry - copy protection failed");
	}

	Game *gameInstance = new Game();

	if (ConfMan.getInt("boot_param") == 0) {
		// Show the introduction
		Sound.loadSection(Sound.isRoland() ? ROLAND_INTRO_SOUND_RESOURCE_ID : ADLIB_INTRO_SOUND_RESOURCE_ID);

		Introduction *intro = new Introduction();
		intro->show();
		delete intro;
	}

	// Play the game
	if (!_events->quitFlag) {
		// Play the game
		Sound.loadSection(Sound.isRoland() ? ROLAND_MAIN_SOUND_RESOURCE_ID : ADLIB_MAIN_SOUND_RESOURCE_ID);
		gameInstance->execute();
	}

	delete gameInstance;
	return 0;
}

void LureEngine::quitGame() {
	_system->quit();
}

const char *LureEngine::generateSaveName(int slotNumber) {
	static char buffer[15];

	sprintf(buffer, "lure.%.3d", slotNumber);
	return buffer;
}

bool LureEngine::saveGame(uint8 slotNumber, Common::String &caption) {
	Common::WriteStream *f = this->_saveFileMan->openForSaving(
		generateSaveName(slotNumber));
	if (f == NULL) 
		return false;

	f->write("lure", 5);
	f->writeByte(getLanguage());
	f->writeByte(LURE_DAT_MINOR);
	f->writeString(caption);
	f->writeByte(0); // End of string terminator

	Resources::getReference().saveToStream(f);
	Game::getReference().saveToStream(f);
	Sound.saveToStream(f);
	Fights.saveToStream(f);
	Room::getReference().saveToStream(f);

	delete f;
	return true;
}

#define FAILED_MSG "loadGame: Failed to load slot %d"

bool LureEngine::loadGame(uint8 slotNumber) {
	Common::ReadStream *f = this->_saveFileMan->openForLoading(
		generateSaveName(slotNumber));
	if (f == NULL) 
		return false;

	// Check for header
	char buffer[5];
	f->read(buffer, 5);
	if (memcmp(buffer, "lure", 5) != 0) {
		warning(FAILED_MSG, slotNumber);
		delete f;
		return false;
	}

	// Check language version
	uint8 language = f->readByte();
	_saveVersion = f->readByte();
	if ((language != getLanguage()) || (_saveVersion < LURE_MIN_SAVEGAME_MINOR)) {
		warning("loadGame: Failed to load slot %d - incorrect version", slotNumber);
		delete f;
		return false;
	}

	// Read in and discard the savegame caption
	while (f->readByte() != 0) ;

	// Load in the data
	Resources::getReference().loadFromStream(f);
	Game::getReference().loadFromStream(f);
	Sound.loadFromStream(f);
	Fights.loadFromStream(f);
	Room::getReference().loadFromStream(f);

	delete f;
	return true;
}

void LureEngine::GUIError(const char *msg, ...) {
	char buffer[STRINGBUFLEN];
	va_list va;

	// Generate the full error message
	va_start(va, msg);
	vsnprintf(buffer, STRINGBUFLEN, msg, va);
	va_end(va);	

	Engine::GUIErrorMessage(buffer);
	exit(1);
}

Common::String *LureEngine::detectSave(int slotNumber) {
	Common::ReadStream *f = this->_saveFileMan->openForLoading(
		generateSaveName(slotNumber));
	if (f == NULL) return NULL;
	Common::String *result = NULL;

	// Check for header
	char buffer[5];
	f->read(&buffer[0], 5);
	if (memcmp(&buffer[0], "lure", 5) == 0) {
		// Check language version
		uint8 language = f->readByte();
		uint8 version = f->readByte();
		if ((language == getLanguage()) && (version >= LURE_MIN_SAVEGAME_MINOR)) {
			// Read in the savegame title
			char saveName[MAX_DESC_SIZE];
			char *p = saveName;
			int decCtr = MAX_DESC_SIZE - 1;
			while ((decCtr > 0) && ((*p++ = f->readByte()) != 0)) --decCtr;
			*p = '\0';
			result = new Common::String(saveName);
		}
	}

	delete f;
	return result;
}

} // End of namespace Lure
