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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

/*
 * This code is based on original Hugo Trilogy source code
 *
 * Copyright (c) 1989-1995 David P. Gray
 *
 */

// This module contains all the scheduling and timing stuff

#include "common/system.h"

#include "hugo/hugo.h"
#include "hugo/schedule.h"
#include "hugo/file.h"
#include "hugo/display.h"
#include "hugo/util.h"
#include "hugo/object.h"
#include "hugo/sound.h"
#include "hugo/parser.h"
#include "hugo/text.h"

namespace Hugo {

Scheduler::Scheduler(HugoEngine *vm) : _vm(vm), _actListArr(0) {
	memset(_events, 0, sizeof(_events));
}

Scheduler::~Scheduler() {
}

/**
* Initialise the timer event queue
*/
void Scheduler::initEventQueue() {
	debugC(1, kDebugSchedule, "initEventQueue");

	// Chain next_p from first to last
	for (int i = kMaxEvents; --i;)
		_events[i - 1].nextEvent = &_events[i];
	_events[kMaxEvents - 1].nextEvent = 0;

	// Chain prev_p from last to first
	for (int i = 1; i < kMaxEvents; i++)
		_events[i].prevEvent = &_events[i - 1];
	_events[0].prevEvent = 0;

	_headEvent = _tailEvent = 0;                    // Event list is empty
	_freeEvent = _events;                           // Free list is full
}

/**
* Return a ptr to an event structure from the free list
*/
event_t *Scheduler::getQueue() {
	debugC(4, kDebugSchedule, "getQueue");

	if (!_freeEvent)                                // Error: no more events available
		error("An error has occurred: %s", "getQueue");
	event_t *resEvent = _freeEvent;
	_freeEvent = _freeEvent->nextEvent;
	resEvent->nextEvent = 0;
	return resEvent;
}

/**
* Call Insert_action for each action in the list supplied
*/
void Scheduler::insertActionList(const uint16 actIndex) {
	debugC(1, kDebugSchedule, "insertActionList(%d)", actIndex);

	if (_actListArr[actIndex]) {
		for (int i = 0; _actListArr[actIndex][i].a0.actType != ANULL; i++)
			insertAction(&_actListArr[actIndex][i]);
	}
}

/**
* Return system time in ticks.  A tick is 1/TICKS_PER_SEC mS
*/
uint32 Scheduler::getWinTicks() const {
	debugC(5, kDebugSchedule, "getWinTicks()");

	return _vm->getGameStatus().tick;
}

/**
* Return system time in ticks.  A tick is 1/TICKS_PER_SEC mS
* If update FALSE, simply return last known time
* Note that this is real time unless a processing cycle takes longer than
* a real tick, in which case the system tick is simply incremented
*/
uint32 Scheduler::getDosTicks(const bool updateFl) {
	debugC(5, kDebugSchedule, "getDosTicks(%s)", (updateFl) ? "TRUE" : "FALSE");

	static  uint32 tick = 0;                        // Current system time in ticks
	static  uint32 t_old = 0;                       // The previous wall time in ticks

	uint32 t_now;                                   // Current wall time in ticks

	if (!updateFl)
		return(tick);

	if (t_old == 0)
		t_old = (uint32) floor((double) (g_system->getMillis() * _vm->getTPS() / 1000));
	// Calculate current wall time in ticks
	t_now = g_system->getMillis() * _vm->getTPS() / 1000	;

	if ((t_now - t_old) > 0) {
		t_old = t_now;
		tick++;
	}
	return(tick);
}

/**
* Add indecated bonus to score if not added already
*/
void Scheduler::processBonus(const int bonusIndex) {
	debugC(1, kDebugSchedule, "processBonus(%d)", bonusIndex);

	if (!_vm->_points[bonusIndex].scoredFl) {
		_vm->adjustScore(_vm->_points[bonusIndex].score);
		_vm->_points[bonusIndex].scoredFl = true;
	}
}

/**
* Transition to a new screen as follows:
* 1. Clear out all non-global events from event list.
* 2. Set the new screen (in the hero object and any carried objects)
* 3. Read in the screen files for the new screen
* 4. Schedule action list for new screen
* 5. Initialise prompt line and status line
*/
void Scheduler::newScreen(const int screenIndex) {
	debugC(1, kDebugSchedule, "newScreen(%d)", screenIndex);

	// Make sure the background file exists!
	if (!_vm->isPacked()) {
		Common::String filename = Common::String(_vm->_text->getScreenNames(screenIndex));
		if (!_vm->_file->fileExists(_vm->_picDir + filename + ".PCX") &&
			!_vm->_file->fileExists(filename + ".ART")) {
				error("Unable to find background file for %s", filename.c_str());
			return;
		}
	}

	// 1. Clear out all local events
	event_t *curEvent = _headEvent;                 // The earliest event
	event_t *wrkEvent;                              // Event ptr
	while (curEvent) {                              // While mature events found
		wrkEvent = curEvent->nextEvent;             // Save p (becomes undefined after Del)
		if (curEvent->localActionFl)
			delQueue(curEvent);                     // Return event to free list
		curEvent = wrkEvent;
	}

	// 2. Set the new screen in the hero object and any being carried
	_vm->setNewScreen(screenIndex);

	// 3. Read in new screen files
	_vm->readScreenFiles(screenIndex);

	// 4. Schedule action list for this screen
	_vm->screenActions(screenIndex);

	// 5. Initialise prompt line and status line
	_vm->_screen->initNewScreenDisplay();
}

/**
* Transition to a new screen as follows:
* 1. Set the new screen (in the hero object and any carried objects)
* 2. Read in the screen files for the new screen
* 3. Initialise prompt line and status line
*/
void Scheduler::restoreScreen(const int screenIndex) {
	debugC(1, kDebugSchedule, "restoreScreen(%d)", screenIndex);

	// 1. Set the new screen in the hero object and any being carried
	_vm->setNewScreen(screenIndex);

	// 2. Read in new screen files
	_vm->readScreenFiles(screenIndex);

	// 3. Initialise prompt line and status line
	_vm->_screen->initNewScreenDisplay();
}

/**
* Wait (if necessary) for next synchronizing tick
* Slow machines won't make it by the end of tick, so will just plod on
* at their own speed, not waiting here, but free running.
* Note: DOS Versions only
*/
void Scheduler::waitForRefresh() {
	debugC(5, kDebugSchedule, "waitForRefresh()");

	static uint32 timeout = 0;
	uint32 t;

	if (timeout == 0)
		timeout = getDosTicks(true);

	while ((t = getDosTicks(true)) < timeout)
		;
	timeout = ++t;
}

/**
* Read kALnewscr used by maze (Hugo 2)
*/
void Scheduler::loadAlNewscrIndex(Common::File &in) {
	debugC(6, kDebugSchedule, "loadAlNewscrIndex(&in)");

	int numElem;
	for (int varnt = 0; varnt < _vm->_numVariant; varnt++) {
		numElem = in.readUint16BE();
		if (varnt == _vm->_gameVariant)
			_alNewscrIndex = numElem;
	}
}

/**
* Load actListArr from Hugo.dat
*/
void Scheduler::loadActListArr(Common::File &in) {
	debugC(6, kDebugSchedule, "loadActListArr(&in)");

	int numElem, numSubElem, numSubAct;
	for (int varnt = 0; varnt < _vm->_numVariant; varnt++) {
		numElem = in.readUint16BE();
		if (varnt == _vm->_gameVariant) {
			_actListArrSize = numElem;
			_actListArr = (act **)malloc(sizeof(act *) * _actListArrSize);
			for (int i = 0; i < _actListArrSize; i++) {
				numSubElem = in.readUint16BE();
				_actListArr[i] = (act *) malloc(sizeof(act) * (numSubElem + 1));
				for (int j = 0; j < numSubElem; j++) {
					_actListArr[i][j].a0.actType = (action_t) in.readByte();
					switch (_actListArr[i][j].a0.actType) {
					case ANULL:              // -1
						break;
					case ASCHEDULE:          // 0
						_actListArr[i][j].a0.timer = in.readSint16BE();
						_actListArr[i][j].a0.actIndex = in.readUint16BE();
						break;
					case START_OBJ:          // 1
						_actListArr[i][j].a1.timer = in.readSint16BE();
						_actListArr[i][j].a1.objIndex = in.readSint16BE();
						_actListArr[i][j].a1.cycleNumb = in.readSint16BE();
						_actListArr[i][j].a1.cycle = (cycle_t) in.readByte();
						break;
					case INIT_OBJXY:         // 2
						_actListArr[i][j].a2.timer = in.readSint16BE();
						_actListArr[i][j].a2.objIndex = in.readSint16BE();
						_actListArr[i][j].a2.x = in.readSint16BE();
						_actListArr[i][j].a2.y = in.readSint16BE();
						break;
					case PROMPT:             // 3
						_actListArr[i][j].a3.timer = in.readSint16BE();
						_actListArr[i][j].a3.promptIndex = in.readSint16BE();
						numSubAct = in.readUint16BE();
						_actListArr[i][j].a3.responsePtr = (int *) malloc(sizeof(int) * numSubAct);
						for (int k = 0; k < numSubAct; k++)
							_actListArr[i][j].a3.responsePtr[k] = in.readSint16BE();
						_actListArr[i][j].a3.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a3.actFailIndex = in.readUint16BE();
						_actListArr[i][j].a3.encodedFl = (in.readByte() == 1) ? true : false;
						break;
					case BKGD_COLOR:         // 4
						_actListArr[i][j].a4.timer = in.readSint16BE();
						_actListArr[i][j].a4.newBackgroundColor = in.readUint32BE();
						break;
					case INIT_OBJVXY:        // 5
						_actListArr[i][j].a5.timer = in.readSint16BE();
						_actListArr[i][j].a5.objIndex = in.readSint16BE();
						_actListArr[i][j].a5.vx = in.readSint16BE();
						_actListArr[i][j].a5.vy = in.readSint16BE();
						break;
					case INIT_CARRY:         // 6
						_actListArr[i][j].a6.timer = in.readSint16BE();
						_actListArr[i][j].a6.objIndex = in.readSint16BE();
						_actListArr[i][j].a6.carriedFl = (in.readByte() == 1) ? true : false;
						break;
					case INIT_HF_COORD:      // 7
						_actListArr[i][j].a7.timer = in.readSint16BE();
						_actListArr[i][j].a7.objIndex = in.readSint16BE();
						break;
					case NEW_SCREEN:         // 8
						_actListArr[i][j].a8.timer = in.readSint16BE();
						_actListArr[i][j].a8.screenIndex = in.readSint16BE();
						break;
					case INIT_OBJSTATE:      // 9
						_actListArr[i][j].a9.timer = in.readSint16BE();
						_actListArr[i][j].a9.objIndex = in.readSint16BE();
						_actListArr[i][j].a9.newState = in.readByte();
						break;
					case INIT_PATH:          // 10
						_actListArr[i][j].a10.timer = in.readSint16BE();
						_actListArr[i][j].a10.objIndex = in.readSint16BE();
						_actListArr[i][j].a10.newPathType = in.readSint16BE();
						_actListArr[i][j].a10.vxPath = in.readByte();
						_actListArr[i][j].a10.vyPath = in.readByte();
						break;
					case COND_R:             // 11
						_actListArr[i][j].a11.timer = in.readSint16BE();
						_actListArr[i][j].a11.objIndex = in.readSint16BE();
						_actListArr[i][j].a11.stateReq = in.readByte();
						_actListArr[i][j].a11.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a11.actFailIndex = in.readUint16BE();
						break;
					case TEXT:               // 12
						_actListArr[i][j].a12.timer = in.readSint16BE();
						_actListArr[i][j].a12.stringIndex = in.readSint16BE();
						break;
					case SWAP_IMAGES:        // 13
						_actListArr[i][j].a13.timer = in.readSint16BE();
						_actListArr[i][j].a13.objIndex1 = in.readSint16BE();
						_actListArr[i][j].a13.objIndex2 = in.readSint16BE();
						break;
					case COND_SCR:           // 14
						_actListArr[i][j].a14.timer = in.readSint16BE();
						_actListArr[i][j].a14.objIndex = in.readSint16BE();
						_actListArr[i][j].a14.screenReq = in.readSint16BE();
						_actListArr[i][j].a14.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a14.actFailIndex = in.readUint16BE();
						break;
					case AUTOPILOT:          // 15
						_actListArr[i][j].a15.timer = in.readSint16BE();
						_actListArr[i][j].a15.objIndex1 = in.readSint16BE();
						_actListArr[i][j].a15.objIndex2 = in.readSint16BE();
						_actListArr[i][j].a15.dx = in.readByte();
						_actListArr[i][j].a15.dy = in.readByte();
						break;
					case INIT_OBJ_SEQ:       // 16
						_actListArr[i][j].a16.timer = in.readSint16BE();
						_actListArr[i][j].a16.objIndex = in.readSint16BE();
						_actListArr[i][j].a16.seqIndex = in.readSint16BE();
						break;
					case SET_STATE_BITS:     // 17
						_actListArr[i][j].a17.timer = in.readSint16BE();
						_actListArr[i][j].a17.objIndex = in.readSint16BE();
						_actListArr[i][j].a17.stateMask = in.readSint16BE();
						break;
					case CLEAR_STATE_BITS:   // 18
						_actListArr[i][j].a18.timer = in.readSint16BE();
						_actListArr[i][j].a18.objIndex = in.readSint16BE();
						_actListArr[i][j].a18.stateMask = in.readSint16BE();
						break;
					case TEST_STATE_BITS:    // 19
						_actListArr[i][j].a19.timer = in.readSint16BE();
						_actListArr[i][j].a19.objIndex = in.readSint16BE();
						_actListArr[i][j].a19.stateMask = in.readSint16BE();
						_actListArr[i][j].a19.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a19.actFailIndex = in.readUint16BE();
						break;
					case DEL_EVENTS:         // 20
						_actListArr[i][j].a20.timer = in.readSint16BE();
						_actListArr[i][j].a20.actTypeDel = (action_t) in.readByte();
						break;
					case GAMEOVER:           // 21
						_actListArr[i][j].a21.timer = in.readSint16BE();
						break;
					case INIT_HH_COORD:      // 22
						_actListArr[i][j].a22.timer = in.readSint16BE();
						_actListArr[i][j].a22.objIndex = in.readSint16BE();
						break;
					case EXIT:               // 23
						_actListArr[i][j].a23.timer = in.readSint16BE();
						break;
					case BONUS:              // 24
						_actListArr[i][j].a24.timer = in.readSint16BE();
						_actListArr[i][j].a24.pointIndex = in.readSint16BE();
						break;
					case COND_BOX:           // 25
						_actListArr[i][j].a25.timer = in.readSint16BE();
						_actListArr[i][j].a25.objIndex = in.readSint16BE();
						_actListArr[i][j].a25.x1 = in.readSint16BE();
						_actListArr[i][j].a25.y1 = in.readSint16BE();
						_actListArr[i][j].a25.x2 = in.readSint16BE();
						_actListArr[i][j].a25.y2 = in.readSint16BE();
						_actListArr[i][j].a25.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a25.actFailIndex = in.readUint16BE();
						break;
					case SOUND:              // 26
						_actListArr[i][j].a26.timer = in.readSint16BE();
						_actListArr[i][j].a26.soundIndex = in.readSint16BE();
						break;
					case ADD_SCORE:          // 27
						_actListArr[i][j].a27.timer = in.readSint16BE();
						_actListArr[i][j].a27.objIndex = in.readSint16BE();
						break;
					case SUB_SCORE:          // 28
						_actListArr[i][j].a28.timer = in.readSint16BE();
						_actListArr[i][j].a28.objIndex = in.readSint16BE();
						break;
					case COND_CARRY:         // 29
						_actListArr[i][j].a29.timer = in.readSint16BE();
						_actListArr[i][j].a29.objIndex = in.readSint16BE();
						_actListArr[i][j].a29.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a29.actFailIndex = in.readUint16BE();
						break;
					case INIT_MAZE:          // 30
						_actListArr[i][j].a30.timer = in.readSint16BE();
						_actListArr[i][j].a30.mazeSize = in.readByte();
						_actListArr[i][j].a30.x1 = in.readSint16BE();
						_actListArr[i][j].a30.y1 = in.readSint16BE();
						_actListArr[i][j].a30.x2 = in.readSint16BE();
						_actListArr[i][j].a30.y2 = in.readSint16BE();
						_actListArr[i][j].a30.x3 = in.readSint16BE();
						_actListArr[i][j].a30.x4 = in.readSint16BE();
						_actListArr[i][j].a30.firstScreenIndex = in.readByte();
						break;
					case EXIT_MAZE:          // 31
						_actListArr[i][j].a31.timer = in.readSint16BE();
						break;
					case INIT_PRIORITY:      // 32
						_actListArr[i][j].a32.timer = in.readSint16BE();
						_actListArr[i][j].a32.objIndex = in.readSint16BE();
						_actListArr[i][j].a32.priority = in.readByte();
						break;
					case INIT_SCREEN:        // 33
						_actListArr[i][j].a33.timer = in.readSint16BE();
						_actListArr[i][j].a33.objIndex = in.readSint16BE();
						_actListArr[i][j].a33.screenIndex = in.readSint16BE();
						break;
					case AGSCHEDULE:         // 34
						_actListArr[i][j].a34.timer = in.readSint16BE();
						_actListArr[i][j].a34.actIndex = in.readUint16BE();
						break;
					case REMAPPAL:           // 35
						_actListArr[i][j].a35.timer = in.readSint16BE();
						_actListArr[i][j].a35.oldColorIndex = in.readSint16BE();
						_actListArr[i][j].a35.newColorIndex = in.readSint16BE();
						break;
					case COND_NOUN:          // 36
						_actListArr[i][j].a36.timer = in.readSint16BE();
						_actListArr[i][j].a36.nounIndex = in.readUint16BE();
						_actListArr[i][j].a36.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a36.actFailIndex = in.readUint16BE();
						break;
					case SCREEN_STATE:       // 37
						_actListArr[i][j].a37.timer = in.readSint16BE();
						_actListArr[i][j].a37.screenIndex = in.readSint16BE();
						_actListArr[i][j].a37.newState = in.readByte();
						break;
					case INIT_LIPS:          // 38
						_actListArr[i][j].a38.timer = in.readSint16BE();
						_actListArr[i][j].a38.lipsObjIndex = in.readSint16BE();
						_actListArr[i][j].a38.objIndex = in.readSint16BE();
						_actListArr[i][j].a38.dxLips = in.readByte();
						_actListArr[i][j].a38.dyLips = in.readByte();
						break;
					case INIT_STORY_MODE:    // 39
						_actListArr[i][j].a39.timer = in.readSint16BE();
						_actListArr[i][j].a39.storyModeFl = (in.readByte() == 1);
						break;
					case WARN:               // 40
						_actListArr[i][j].a40.timer = in.readSint16BE();
						_actListArr[i][j].a40.stringIndex = in.readSint16BE();
						break;
					case COND_BONUS:         // 41
						_actListArr[i][j].a41.timer = in.readSint16BE();
						_actListArr[i][j].a41.BonusIndex = in.readSint16BE();
						_actListArr[i][j].a41.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a41.actFailIndex = in.readUint16BE();
						break;
					case TEXT_TAKE:          // 42
						_actListArr[i][j].a42.timer = in.readSint16BE();
						_actListArr[i][j].a42.objIndex = in.readSint16BE();
						break;
					case YESNO:              // 43
						_actListArr[i][j].a43.timer = in.readSint16BE();
						_actListArr[i][j].a43.promptIndex = in.readSint16BE();
						_actListArr[i][j].a43.actYesIndex = in.readUint16BE();
						_actListArr[i][j].a43.actNoIndex = in.readUint16BE();
						break;
					case STOP_ROUTE:         // 44
						_actListArr[i][j].a44.timer = in.readSint16BE();
						break;
					case COND_ROUTE:         // 45
						_actListArr[i][j].a45.timer = in.readSint16BE();
						_actListArr[i][j].a45.routeIndex = in.readSint16BE();
						_actListArr[i][j].a45.actPassIndex = in.readUint16BE();
						_actListArr[i][j].a45.actFailIndex = in.readUint16BE();
						break;
					case INIT_JUMPEXIT:      // 46
						_actListArr[i][j].a46.timer = in.readSint16BE();
						_actListArr[i][j].a46.jumpExitFl = (in.readByte() == 1);
						break;
					case INIT_VIEW:          // 47
						_actListArr[i][j].a47.timer = in.readSint16BE();
						_actListArr[i][j].a47.objIndex = in.readSint16BE();
						_actListArr[i][j].a47.viewx = in.readSint16BE();
						_actListArr[i][j].a47.viewy = in.readSint16BE();
						_actListArr[i][j].a47.direction = in.readSint16BE();
						break;
					case INIT_OBJ_FRAME:     // 48
						_actListArr[i][j].a48.timer = in.readSint16BE();
						_actListArr[i][j].a48.objIndex = in.readSint16BE();
						_actListArr[i][j].a48.seqIndex = in.readSint16BE();
						_actListArr[i][j].a48.frameIndex = in.readSint16BE();
						break;
					case OLD_SONG:           //49
						_actListArr[i][j].a49.timer = in.readSint16BE();
						_actListArr[i][j].a49.songIndex = in.readUint16BE();
						break;
					default:
						error("Engine - Unknown action type encountered: %d", _actListArr[i][j].a0.actType);
					}
				}
				_actListArr[i][numSubElem].a0.actType = ANULL;
			}
		} else {
			for (int i = 0; i < numElem; i++) {
				numSubElem = in.readUint16BE();
				for (int j = 0; j < numSubElem; j++) {
					numSubAct = in.readByte();
					switch (numSubAct) {
					case ANULL:              // -1
						break;
					case ASCHEDULE:          // 0
						in.readSint16BE();
						in.readUint16BE();
						break;
					case START_OBJ:          // 1
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						break;
					case INIT_OBJXY:         // 2
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case PROMPT:             // 3
						in.readSint16BE();
						in.readSint16BE();
						numSubAct = in.readUint16BE();
						for (int k = 0; k < numSubAct; k++)
							in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						in.readByte();
						break;
					case BKGD_COLOR:         // 4
						in.readSint16BE();
						in.readUint32BE();
						break;
					case INIT_OBJVXY:        // 5
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case INIT_CARRY:         // 6
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						break;
					case INIT_HF_COORD:      // 7
						in.readSint16BE();
						in.readSint16BE();
						break;
					case NEW_SCREEN:         // 8
						in.readSint16BE();
						in.readSint16BE();
						break;
					case INIT_OBJSTATE:      // 9
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						break;
					case INIT_PATH:          // 10
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						in.readByte();
						break;
					case COND_R:             // 11
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case TEXT:               // 12
						in.readSint16BE();
						in.readSint16BE();
						break;
					case SWAP_IMAGES:        // 13
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case COND_SCR:           // 14
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case AUTOPILOT:          // 15
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						in.readByte();
						break;
					case INIT_OBJ_SEQ:       // 16
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case SET_STATE_BITS:     // 17
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case CLEAR_STATE_BITS:   // 18
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case TEST_STATE_BITS:    // 19
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case DEL_EVENTS:         // 20
						in.readSint16BE();
						in.readByte();
						break;
					case GAMEOVER:           // 21
						in.readSint16BE();
						break;
					case INIT_HH_COORD:      // 22
						in.readSint16BE();
						in.readSint16BE();
						break;
					case EXIT:               // 23
						in.readSint16BE();
						break;
					case BONUS:              // 24
						in.readSint16BE();
						in.readSint16BE();
						break;
					case COND_BOX:           // 25
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case SOUND:              // 26
						in.readSint16BE();
						in.readSint16BE();
						break;
					case ADD_SCORE:          // 27
						in.readSint16BE();
						in.readSint16BE();
						break;
					case SUB_SCORE:          // 28
						in.readSint16BE();
						in.readSint16BE();
						break;
					case COND_CARRY:         // 29
						in.readSint16BE();
						in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case INIT_MAZE:          // 30
						in.readSint16BE();
						in.readByte();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						break;
					case EXIT_MAZE:          // 31
						in.readSint16BE();
						break;
					case INIT_PRIORITY:      // 32
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						break;
					case INIT_SCREEN:        // 33
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case AGSCHEDULE:         // 34
						in.readSint16BE();
						in.readUint16BE();
						break;
					case REMAPPAL:           // 35
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case COND_NOUN:          // 36
						in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case SCREEN_STATE:       // 37
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						break;
					case INIT_LIPS:          // 38
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readByte();
						in.readByte();
						break;
					case INIT_STORY_MODE:    // 39
						in.readSint16BE();
						in.readByte();
						break;
					case WARN:               // 40
						in.readSint16BE();
						in.readSint16BE();
						break;
					case COND_BONUS:         // 41
						in.readSint16BE();
						in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case TEXT_TAKE:          // 42
						in.readSint16BE();
						in.readSint16BE();
						break;
					case YESNO:              // 43
						in.readSint16BE();
						in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case STOP_ROUTE:         // 44
						in.readSint16BE();
						break;
					case COND_ROUTE:         // 45
						in.readSint16BE();
						in.readSint16BE();
						in.readUint16BE();
						in.readUint16BE();
						break;
					case INIT_JUMPEXIT:      // 46
						in.readSint16BE();
						in.readByte();
						break;
					case INIT_VIEW:          // 47
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case INIT_OBJ_FRAME:     // 48
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						in.readSint16BE();
						break;
					case OLD_SONG:           //49
						in.readSint16BE();
						in.readUint16BE();
						break;
					default:
						error("Engine - Unknown action type encountered %d - variante %d pos %d.%d", numSubAct, varnt, i, j);
					}
				}
			}
		}
	}
}

void Scheduler::freeActListArr() {
	debugC(6, kDebugSchedule, "freeActListArr()");

	if (_actListArr) {
		for (int i = 0; i < _actListArrSize; i++) {
			for (int j = 0; _actListArr[i][j].a0.actType != ANULL; j++) {
				if (_actListArr[i][j].a0.actType == PROMPT)
					free(_actListArr[i][j].a3.responsePtr);
			}
			free(_actListArr[i]);
		}
		free(_actListArr);
	}
}

/**
* Maze mode is enabled.  Check to see whether hero has crossed the maze
* bounding box, if so, go to the next room
*/
void Scheduler::processMaze(const int x1, const int x2, const int y1, const int y2) {
	debugC(1, kDebugSchedule, "processMaze");

	status_t &gameStatus = _vm->getGameStatus();

	if (x1 < _maze.x1) {
		// Exit west
		_actListArr[_alNewscrIndex][3].a8.screenIndex = *_vm->_screen_p - 1;
		_actListArr[_alNewscrIndex][0].a2.x = _maze.x2 - kShiftSize - (x2 - x1);
		_actListArr[_alNewscrIndex][0].a2.y = _vm->_hero->y;
		gameStatus.routeIndex = -1;
		insertActionList(_alNewscrIndex);
	} else if (x2 > _maze.x2) {
		// Exit east
		_actListArr[_alNewscrIndex][3].a8.screenIndex = *_vm->_screen_p + 1;
		_actListArr[_alNewscrIndex][0].a2.x = _maze.x1 + kShiftSize;
		_actListArr[_alNewscrIndex][0].a2.y = _vm->_hero->y;
		gameStatus.routeIndex = -1;
		insertActionList(_alNewscrIndex);
	} else if (y1 < _maze.y1 - kShiftSize) {
		// Exit north
		_actListArr[_alNewscrIndex][3].a8.screenIndex = *_vm->_screen_p - _maze.size;
		_actListArr[_alNewscrIndex][0].a2.x = _maze.x3;
		_actListArr[_alNewscrIndex][0].a2.y = _maze.y2 - kShiftSize - (y2 - y1);
		gameStatus.routeIndex = -1;
		insertActionList(_alNewscrIndex);
	} else if (y2 > _maze.y2 - kShiftSize / 2) {
		// Exit south
		_actListArr[_alNewscrIndex][3].a8.screenIndex = *_vm->_screen_p + _maze.size;
		_actListArr[_alNewscrIndex][0].a2.x = _maze.x4;
		_actListArr[_alNewscrIndex][0].a2.y = _maze.y1 + kShiftSize;
		gameStatus.routeIndex = -1;
		insertActionList(_alNewscrIndex);
	}
}

/**
* Write the event queue to the file with handle f
* Note that we convert all the event structure ptrs to indexes
* using -1 for NULL.  We can't convert the action ptrs to indexes
* so we save address of first dummy action ptr to compare on restore.
*/
void Scheduler::saveEvents(Common::WriteStream *f) {
	debugC(1, kDebugSchedule, "saveEvents()");

	f->writeUint32BE(getTicks());

	int16 freeIndex = (_freeEvent == 0) ? -1 : _freeEvent - _events;
	int16 headIndex = (_headEvent == 0) ? -1 : _headEvent - _events;
	int16 tailIndex = (_tailEvent == 0) ? -1 : _tailEvent - _events;

	f->writeSint16BE(freeIndex);
	f->writeSint16BE(headIndex);
	f->writeSint16BE(tailIndex);

	// Convert event ptrs to indexes
	event_t  saveEventArr[kMaxEvents];              // Convert event ptrs to indexes
	for (int16 i = 0; i < kMaxEvents; i++) {
		event_t *wrkEvent = &_events[i];
		saveEventArr[i] = *wrkEvent;

 		// fix up action pointer (to do better)
		int16 index, subElem;
		findAction(saveEventArr[i].action, &index, &subElem);
		saveEventArr[i].action = (act*)((index << 16)| subElem);

		saveEventArr[i].prevEvent = (wrkEvent->prevEvent == 0) ? (event_t *) - 1 : (event_t *)(wrkEvent->prevEvent - _events);
		saveEventArr[i].nextEvent = (wrkEvent->nextEvent == 0) ? (event_t *) - 1 : (event_t *)(wrkEvent->nextEvent - _events);
	}

	f->write(saveEventArr, sizeof(saveEventArr));
}

/** 
* Restore the action data from file with handle f
*/

void Scheduler::restoreActions(Common::SeekableReadStream *f) {

	for (int i = 0; i < _actListArrSize; i++) {
	
		// read all the sub elems
		int j = 0;
		do {

			// handle special case for a3, keep list pointer
			int* responsePtr = 0;
			if (_actListArr[i][j].a3.actType == PROMPT) {
				responsePtr = _actListArr[i][j].a3.responsePtr;
			}

			f->read(&_actListArr[i][j], sizeof(act));

			// handle special case for a3, reset list pointer
			if (_actListArr[i][j].a3.actType == PROMPT) {
				_actListArr[i][j].a3.responsePtr = responsePtr;
			}
			j++;
		} while (_actListArr[i][j-1].a0.actType != ANULL);
	}
}

/*
* Save the action data in the file with handle f
*/

void Scheduler::saveActions(Common::WriteStream* f) const {
	for (int i = 0; i < _actListArrSize; i++) {
		// write all the sub elems data

		int j = 0;
		do {
			f->write(&_actListArr[i][j], sizeof(act));
			j++;
		} while (_actListArr[i][j-1].a0.actType != ANULL);
	}
}

/*
* Find the index in the action list to be able to serialize the action to save game
*/

void Scheduler::findAction(act* action, int16* index, int16* subElem) {
	
	assert(index && subElem);
	if (!action) {
		*index = -1;
		*subElem = -1;
		return;
	}

	for (int i = 0; i < _actListArrSize; i++) {
		int j = 0;
		do {
			if (action == &_actListArr[i][j]) {
				*index = i;
				*subElem = j;
				return;
			}
			j++;
		} while (_actListArr[i][j-1].a0.actType != ANULL);
	}
	// action not found ??
	assert(0);
}

/**
* Restore the event list from file with handle f
*/
void Scheduler::restoreEvents(Common::SeekableReadStream *f) {
	debugC(1, kDebugSchedule, "restoreEvents");

	event_t  savedEvents[kMaxEvents];               // Convert event ptrs to indexes

	uint32 saveTime = f->readUint32BE();            // time of save
	int16 freeIndex = f->readSint16BE();            // Free list index
	int16 headIndex = f->readSint16BE();            // Head of list index
	int16 tailIndex = f->readSint16BE();            // Tail of list index
	f->read(savedEvents, sizeof(savedEvents));

	event_t *wrkEvent;
	// Restore events indexes to pointers
	for (int i = 0; i < kMaxEvents; i++) {
		wrkEvent = &savedEvents[i];
		_events[i] = *wrkEvent;
		// fix up action pointer (to do better)
		int32 val = (size_t)_events[i].action;
		if ((val & 0xffff) == 0xffff) {
			_events[i].action = 0;
		} else {
			_events[i].action = (act*)&_actListArr[val >> 16][val & 0xffff];
		}
		_events[i].prevEvent = (wrkEvent->prevEvent == (event_t *) - 1) ? (event_t *)0 : &_events[(size_t)wrkEvent->prevEvent ];
		_events[i].nextEvent = (wrkEvent->nextEvent == (event_t *) - 1) ? (event_t *)0 : &_events[(size_t)wrkEvent->nextEvent ];
	}
	_freeEvent = (freeIndex == -1) ? 0 : &_events[freeIndex];
	_headEvent = (headIndex == -1) ? 0 : &_events[headIndex];
	_tailEvent = (tailIndex == -1) ? 0 : &_events[tailIndex];

	// Adjust times to fit our time
	uint32 curTime = getTicks();
	wrkEvent = _headEvent;                              // The earliest event
	while (wrkEvent) {                              // While mature events found
		wrkEvent->time = wrkEvent->time - saveTime + curTime;
		wrkEvent = wrkEvent->nextEvent;
	}
}

/**
* Insert the action pointed to by p into the timer event queue
* The queue goes from head (earliest) to tail (latest) timewise
*/
void Scheduler::insertAction(act *action) {
	debugC(1, kDebugSchedule, "insertAction() - Action type A%d", action->a0.actType);

	// First, get and initialise the event structure
	event_t *curEvent = getQueue();
	curEvent->action = action;
	switch (action->a0.actType) {                   // Assign whether local or global
	case AGSCHEDULE:
		curEvent->localActionFl = false;            // Lasts over a new screen
		break;
	default:
		curEvent->localActionFl = true;             // Rest are for current screen only
		break;
	}

	curEvent->time = action->a0.timer + getTicks(); // Convert rel to abs time

	// Now find the place to insert the event
	if (!_tailEvent) {                              // Empty queue
		_tailEvent = _headEvent = curEvent;
		curEvent->nextEvent = curEvent->prevEvent = 0;
	} else {
		event_t *wrkEvent = _tailEvent;             // Search from latest time back
		bool found = false;

		while (wrkEvent && !found) {
			if (wrkEvent->time <= curEvent->time) { // Found if new event later
				found = true;
				if (wrkEvent == _tailEvent)         // New latest in list
					_tailEvent = curEvent;
				else
					wrkEvent->nextEvent->prevEvent = curEvent;
				curEvent->nextEvent = wrkEvent->nextEvent;
				wrkEvent->nextEvent = curEvent;
				curEvent->prevEvent = wrkEvent;
			}
			wrkEvent = wrkEvent->prevEvent;
		}

		if (!found) {                               // Must be earliest in list
			_headEvent->prevEvent = curEvent;       // So insert as new head
			curEvent->nextEvent = _headEvent;
			curEvent->prevEvent = 0;
			_headEvent = curEvent;
		}
	}
}

/**
* This function performs the action in the event structure pointed to by p
* It dequeues the event and returns it to the free list.  It returns a ptr
* to the next action in the list, except special case of NEW_SCREEN
*/
event_t *Scheduler::doAction(event_t *curEvent) {
	debugC(1, kDebugSchedule, "doAction - Event action type : %d", curEvent->action->a0.actType);

	status_t &gameStatus = _vm->getGameStatus();
	act *action = curEvent->action;
	object_t *obj1;
	int       dx, dy;
	event_t  *wrkEvent;                             // Save ev_p->next_p for return

	switch (action->a0.actType) {
	case ANULL:                                     // Big NOP from DEL_EVENTS
		break;
	case ASCHEDULE:                                 // act0: Schedule an action list
		insertActionList(action->a0.actIndex);
		break;
	case START_OBJ:                                 // act1: Start an object cycling
		_vm->_object->_objects[action->a1.objIndex].cycleNumb = action->a1.cycleNumb;
		_vm->_object->_objects[action->a1.objIndex].cycling = action->a1.cycle;
		break;
	case INIT_OBJXY:                                // act2: Initialise an object
		_vm->_object->_objects[action->a2.objIndex].x = action->a2.x;          // Coordinates
		_vm->_object->_objects[action->a2.objIndex].y = action->a2.y;
		break;
	case PROMPT:                                    // act3: Prompt user for key phrase
		promptAction(action);
		break;
	case BKGD_COLOR:                                // act4: Set new background color
		_vm->_screen->setBackgroundColor(action->a4.newBackgroundColor);
		break;
	case INIT_OBJVXY:                               // act5: Initialise an object velocity
		_vm->_object->setVelocity(action->a5.objIndex, action->a5.vx, action->a5.vy);
		break;
	case INIT_CARRY:                                // act6: Initialise an object
		_vm->_object->setCarry(action->a6.objIndex, action->a6.carriedFl);  // carried status
		break;
	case INIT_HF_COORD:                             // act7: Initialise an object to hero's "feet" coords
		_vm->_object->_objects[action->a7.objIndex].x = _vm->_hero->x - 1;
		_vm->_object->_objects[action->a7.objIndex].y = _vm->_hero->y + _vm->_hero->currImagePtr->y2 - 1;
		_vm->_object->_objects[action->a7.objIndex].screenIndex = *_vm->_screen_p;  // Don't forget screen!
		break;
	case NEW_SCREEN:                                // act8: Start new screen
		newScreen(action->a8.screenIndex);
		break;
	case INIT_OBJSTATE:                             // act9: Initialise an object state
		_vm->_object->_objects[action->a9.objIndex].state = action->a9.newState;
		break;
	case INIT_PATH:                                 // act10: Initialise an object path and velocity
		_vm->_object->setPath(action->a10.objIndex, (path_t) action->a10.newPathType, action->a10.vxPath, action->a10.vyPath);
		break;
	case COND_R:                                    // act11: action lists conditional on object state
		if (_vm->_object->_objects[action->a11.objIndex].state == action->a11.stateReq)
			insertActionList(action->a11.actPassIndex);
		else
			insertActionList(action->a11.actFailIndex);
		break;
	case TEXT:                                      // act12: Text box (CF WARN)
		Utils::Box(kBoxAny, "%s", _vm->_file->fetchString(action->a12.stringIndex));   // Fetch string from file
		break;
	case SWAP_IMAGES:                               // act13: Swap 2 object images
		_vm->_object->swapImages(action->a13.objIndex1, action->a13.objIndex2);
		break;
	case COND_SCR:                                  // act14: Conditional on current screen
		if (_vm->_object->_objects[action->a14.objIndex].screenIndex == action->a14.screenReq)
			insertActionList(action->a14.actPassIndex);
		else
			insertActionList(action->a14.actFailIndex);
		break;
	case AUTOPILOT:                                 // act15: Home in on a (stationary) object
		_vm->_object->homeIn(action->a15.objIndex1, action->a15.objIndex2, action->a15.dx, action->a15.dy);
		break;
	case INIT_OBJ_SEQ:                              // act16: Set sequence number to use
		// Note: Don't set a sequence at time 0 of a new screen, it causes
		// problems clearing the boundary bits of the object!  t>0 is safe
		_vm->_object->_objects[action->a16.objIndex].currImagePtr = _vm->_object->_objects[action->a16.objIndex].seqList[action->a16.seqIndex].seqPtr;
		break;
	case SET_STATE_BITS:                            // act17: OR mask with curr obj state
		_vm->_object->_objects[action->a17.objIndex].state |= action->a17.stateMask;
		break;
	case CLEAR_STATE_BITS:                          // act18: AND ~mask with curr obj state
		_vm->_object->_objects[action->a18.objIndex].state &= ~action->a18.stateMask;
		break;
	case TEST_STATE_BITS:                           // act19: If all bits set, do apass else afail
		if ((_vm->_object->_objects[action->a19.objIndex].state & action->a19.stateMask) == action->a19.stateMask)
			insertActionList(action->a19.actPassIndex);
		else
			insertActionList(action->a19.actFailIndex);
		break;
	case DEL_EVENTS:                                // act20: Remove all events of this action type
		delEventType(action->a20.actTypeDel);
		break;
	case GAMEOVER:                                  // act21: Game over!
		// NOTE: Must wait at least 1 tick before issuing this action if
		// any objects are to be made invisible!
		gameStatus.gameOverFl = true;
		break;
	case INIT_HH_COORD:                             // act22: Initialise an object to hero's actual coords
		_vm->_object->_objects[action->a22.objIndex].x = _vm->_hero->x;
		_vm->_object->_objects[action->a22.objIndex].y = _vm->_hero->y;
		_vm->_object->_objects[action->a22.objIndex].screenIndex = *_vm->_screen_p;// Don't forget screen!
		break;
	case EXIT:                                      // act23: Exit game back to DOS
		_vm->endGame();
		break;
	case BONUS:                                     // act24: Get bonus score for action
		processBonus(action->a24.pointIndex);
		break;
	case COND_BOX:                                  // act25: Conditional on bounding box
		obj1 = &_vm->_object->_objects[action->a25.objIndex];
		dx = obj1->x + obj1->currImagePtr->x1;
		dy = obj1->y + obj1->currImagePtr->y2;
		if ((dx >= action->a25.x1) && (dx <= action->a25.x2) &&
		        (dy >= action->a25.y1) && (dy <= action->a25.y2))
			insertActionList(action->a25.actPassIndex);
		else
			insertActionList(action->a25.actFailIndex);
		break;
	case SOUND:                                     // act26: Play a sound (or tune)
		if (action->a26.soundIndex < _vm->_tunesNbr)
			_vm->_sound->playMusic(action->a26.soundIndex);
		else
			_vm->_sound->playSound(action->a26.soundIndex, kSoundPriorityMedium);
		break;
	case ADD_SCORE:                                 // act27: Add object's value to score
		_vm->adjustScore(_vm->_object->_objects[action->a27.objIndex].objValue);
		break;
	case SUB_SCORE:                                 // act28: Subtract object's value from score
		_vm->adjustScore(-_vm->_object->_objects[action->a28.objIndex].objValue);
		break;
	case COND_CARRY:                                // act29: Conditional on object being carried
		if (_vm->_object->isCarried(action->a29.objIndex))
			insertActionList(action->a29.actPassIndex);
		else
			insertActionList(action->a29.actFailIndex);
		break;
	case INIT_MAZE:                                 // act30: Enable and init maze structure
		_maze.enabledFl = true;
		_maze.size = action->a30.mazeSize;
		_maze.x1 = action->a30.x1;
		_maze.y1 = action->a30.y1;
		_maze.x2 = action->a30.x2;
		_maze.y2 = action->a30.y2;
		_maze.x3 = action->a30.x3;
		_maze.x4 = action->a30.x4;
		_maze.firstScreenIndex = action->a30.firstScreenIndex;
		break;
	case EXIT_MAZE:                                 // act31: Disable maze mode
		_maze.enabledFl = false;
		break;
	case INIT_PRIORITY:
		_vm->_object->_objects[action->a32.objIndex].priority = action->a32.priority;
		break;
	case INIT_SCREEN:
		_vm->_object->_objects[action->a33.objIndex].screenIndex = action->a33.screenIndex;
		break;
	case AGSCHEDULE:                                // act34: Schedule a (global) action list
		insertActionList(action->a34.actIndex);
		break;
	case REMAPPAL:                                  // act35: Remap a palette color
		_vm->_screen->remapPal(action->a35.oldColorIndex, action->a35.newColorIndex);
		break;
	case COND_NOUN:                                 // act36: Conditional on noun mentioned
		if (_vm->_parser->isWordPresent(_vm->_text->getNounArray(action->a36.nounIndex)))
			insertActionList(action->a36.actPassIndex);
		else
			insertActionList(action->a36.actFailIndex);
		break;
	case SCREEN_STATE:                              // act37: Set new screen state
		_vm->_screenStates[action->a37.screenIndex] = action->a37.newState;
		break;
	case INIT_LIPS:                                 // act38: Position lips on object
		_vm->_object->_objects[action->a38.lipsObjIndex].x = _vm->_object->_objects[action->a38.objIndex].x + action->a38.dxLips;
		_vm->_object->_objects[action->a38.lipsObjIndex].y = _vm->_object->_objects[action->a38.objIndex].y + action->a38.dyLips;
		_vm->_object->_objects[action->a38.lipsObjIndex].screenIndex = *_vm->_screen_p; // Don't forget screen!
		_vm->_object->_objects[action->a38.lipsObjIndex].cycling = kCycleForward;
		break;
	case INIT_STORY_MODE:                           // act39: Init story_mode flag
		// This is similar to the QUIET path mode, except that it is
		// independant of it and it additionally disables the ">" prompt
		gameStatus.storyModeFl = action->a39.storyModeFl;

		// End the game after story if this is special vendor demo mode
		if (gameStatus.demoFl && action->a39.storyModeFl == false)
			_vm->endGame();
		break;
	case WARN:                                      // act40: Text box (CF TEXT)
		Utils::Box(kBoxOk, "%s", _vm->_file->fetchString(action->a40.stringIndex));
		break;
	case COND_BONUS:                                // act41: Perform action if got bonus
		if (_vm->_points[action->a41.BonusIndex].scoredFl)
			insertActionList(action->a41.actPassIndex);
		else
			insertActionList(action->a41.actFailIndex);
		break;
	case TEXT_TAKE:                                 // act42: Text box with "take" message
		Utils::Box(kBoxAny, TAKE_TEXT, _vm->_text->getNoun(_vm->_object->_objects[action->a42.objIndex].nounIndex, TAKE_NAME));
		break;
	case YESNO:                                     // act43: Prompt user for Yes or No
		if (Utils::Box(kBoxYesNo, "%s", _vm->_file->fetchString(action->a43.promptIndex)) != 0)
			insertActionList(action->a43.actYesIndex);
		else
			insertActionList(action->a43.actNoIndex);
		break;
	case STOP_ROUTE:                                // act44: Stop any route in progress
		gameStatus.routeIndex = -1;
		break;
	case COND_ROUTE:                                // act45: Conditional on route in progress
		if (gameStatus.routeIndex >= action->a45.routeIndex)
			insertActionList(action->a45.actPassIndex);
		else
			insertActionList(action->a45.actFailIndex);
		break;
	case INIT_JUMPEXIT:                             // act46: Init status.jumpexit flag
		// This is to allow left click on exit to get there immediately
		// For example the plane crash in Hugo2 where hero is invisible
		// Couldn't use INVISIBLE flag since conflicts with boat in Hugo1
		gameStatus.jumpExitFl = action->a46.jumpExitFl;
		break;
	case INIT_VIEW:                                 // act47: Init object.viewx, viewy, dir
		_vm->_object->_objects[action->a47.objIndex].viewx = action->a47.viewx;
		_vm->_object->_objects[action->a47.objIndex].viewy = action->a47.viewy;
		_vm->_object->_objects[action->a47.objIndex].direction = action->a47.direction;
		break;
	case INIT_OBJ_FRAME:                            // act48: Set seq,frame number to use
		// Note: Don't set a sequence at time 0 of a new screen, it causes
		// problems clearing the boundary bits of the object!  t>0 is safe
		_vm->_object->_objects[action->a48.objIndex].currImagePtr = _vm->_object->_objects[action->a48.objIndex].seqList[action->a48.seqIndex].seqPtr;
		for (dx = 0; dx < action->a48.frameIndex; dx++)
			_vm->_object->_objects[action->a48.objIndex].currImagePtr = _vm->_object->_objects[action->a48.objIndex].currImagePtr->nextSeqPtr;
		break;
	case OLD_SONG:
		// Replaces ACT26 for DOS games.
		_vm->_sound->DOSSongPtr = _vm->_text->getTextData(action->a49.songIndex);
		break;
	default:
		error("An error has occurred: %s", "doAction");
		break;
	}

	if (action->a0.actType == NEW_SCREEN) {         // New_screen() deletes entire list
		return 0;                                   // next_p = 0 since list now empty
	} else {
		wrkEvent = curEvent->nextEvent;
		delQueue(curEvent);                         // Return event to free list
		return wrkEvent;                            // Return next event ptr
	}
}

/**
* Delete an event structure (i.e. return it to the free list)
* Historical note:  Originally event p was assumed to be at head of queue
* (i.e. earliest) since all events were deleted in order when proceeding to
* a new screen.  To delete an event from the middle of the queue, the action
* was overwritten to be ANULL.  With the advent of GLOBAL events, delQueue
* was modified to allow deletes anywhere in the list, and the DEL_EVENT
* action was modified to perform the actual delete.
*/
void Scheduler::delQueue(event_t *curEvent) {
	debugC(4, kDebugSchedule, "delQueue()");

	if (curEvent == _headEvent) {                   // If p was the head ptr
		_headEvent = curEvent->nextEvent;           // then make new head_p
	} else {                                        // Unlink p
		curEvent->prevEvent->nextEvent = curEvent->nextEvent;
		if (curEvent->nextEvent)
			curEvent->nextEvent->prevEvent = curEvent->prevEvent;
		else
			_tailEvent = curEvent->prevEvent;
	}

	if (_headEvent)
		_headEvent->prevEvent = 0;                  // Mark end of list
	else
		_tailEvent = 0;                             // Empty queue

	curEvent->nextEvent = _freeEvent;               // Return p to free list
	if (_freeEvent)                                 // Special case, if free list was empty
		_freeEvent->prevEvent = curEvent;
	_freeEvent = curEvent;
}

void Scheduler::delEventType(const action_t actTypeDel) {
	// Note: actions are not deleted here, simply turned into NOPs!
	event_t *wrkEvent = _headEvent;                 // The earliest event
	event_t *saveEvent;

	while (wrkEvent) {                              // While events found in list
		saveEvent = wrkEvent->nextEvent;
		if (wrkEvent->action->a20.actType == actTypeDel)
			delQueue(wrkEvent);
		wrkEvent = saveEvent;
	}
}

Scheduler_v1d::Scheduler_v1d(HugoEngine *vm) : Scheduler(vm) {
}

Scheduler_v1d::~Scheduler_v1d() {
}

const char *Scheduler_v1d::getCypher() const {
	return "Copyright (c) 1990, Gray Design Associates";
}

uint32 Scheduler_v1d::getTicks() {
	return getDosTicks(false);
}

/**
* This is the scheduler which runs every tick.  It examines the event queue
* for any events whose time has come.  It dequeues these events and performs
* the action associated with the event, returning it to the free queue
*/
void Scheduler_v1d::runScheduler() {
	debugC(6, kDebugSchedule, "runScheduler");

	uint32 ticker = getTicks();                     // The time now, in ticks
	event_t *curEvent = _headEvent;                 // The earliest event

	while (curEvent && (curEvent->time <= ticker))  // While mature events found
		curEvent = doAction(curEvent);              // Perform the action (returns next_p)
}

void Scheduler_v1d::promptAction(act *action) {
	Utils::Box(kBoxPrompt, "%s", _vm->_file->fetchString(action->a3.promptIndex));

	warning("STUB: doAction(act3)");
	// TODO: The answer of the player is not handled currently! Once it'll be read in the messageBox, uncomment this block
#if 0
	char response[256];
	// TODO: Put user input in response

	Utils::strlwr(response);
	if (action->a3.encodedFl) {
		warning("Encrypted flag set");
		decodeString(response);
	}

	if (strstr(response, _vm->_file->fetchString(action->a3.responsePtr[0]))
		insertActionList(action->a3.actPassIndex);
	else
		insertActionList(action->a3.actFailIndex);
#endif

	// HACK: As the answer is not read, currently it's always considered correct
	insertActionList(action->a3.actPassIndex);
}

/**
* Decode a response to a prompt
*/
void Scheduler_v1d::decodeString(char *line) {
	debugC(1, kDebugSchedule, "decodeString(%s)", line);

	static const char *cypher = getCypher();

	for(uint16 i = 0; i < strlen(line); i++) {
		line[i] = (line[i] + cypher[i]) % '~';
		if (line[i] < ' ')
			line[i] += ' ';
	}
}

Scheduler_v2d::Scheduler_v2d(HugoEngine *vm) : Scheduler_v1d(vm) {
}

Scheduler_v2d::~Scheduler_v2d() {
}

const char *Scheduler_v2d::getCypher() const {
	return "Copyright 1991, Gray Design Associates";
}

void Scheduler_v2d::promptAction(act *action) {
	Utils::Box(kBoxPrompt, "%s", _vm->_file->fetchString(action->a3.promptIndex));
	warning("STUB: doAction(act3), expecting answer %s", _vm->_file->fetchString(action->a3.responsePtr[0]));

	// TODO: The answer of the player is not handled currently! Once it'll be read in the messageBox, uncomment this block
#if 0
	char *response = Utils::Box(BOX_PROMPT, "%s", _vm->_file->fetchString(action->a3.promptIndex));

	bool  found = false;
	char *tmpStr;                                   // General purpose string ptr

	for (dx = 0; !found && (action->a3.responsePtr[dx] != -1); dx++) {
		tmpStr = _vm->_file->fetchString(action->a3.responsePtr[dx]);
		if (strstr(Utils::strlwr(response) , tmpStr))
			found = true;
	}

	if (found)
		insertActionList(action->a3.actPassIndex);
	else
		insertActionList(action->a3.actFailIndex);
#endif

	// HACK: As the answer is not read, currently it's always considered correct
	insertActionList(action->a3.actPassIndex);
}

/**
* Decode a string
*/
void Scheduler_v2d::decodeString(char *line) {
	debugC(1, kDebugSchedule, "decodeString(%s)", line);

	static const char *cypher = getCypher();

	for (uint16 i = 0; i < strlen(line); i++)
		line[i] -= cypher[i % strlen(cypher)];
	debugC(1, kDebugSchedule, "result : %s", line);
}

Scheduler_v3d::Scheduler_v3d(HugoEngine *vm) : Scheduler_v2d(vm) {
}

Scheduler_v3d::~Scheduler_v3d() {
}

const char *Scheduler_v3d::getCypher() const {
	return "Copyright 1992, Gray Design Associates";
}

Scheduler_v1w::Scheduler_v1w(HugoEngine *vm) : Scheduler_v3d(vm) {
}

Scheduler_v1w::~Scheduler_v1w() {
}

uint32 Scheduler_v1w::getTicks() {
	return getWinTicks();
}

/**
* This is the scheduler which runs every tick.  It examines the event queue
* for any events whose time has come.  It dequeues these events and performs
* the action associated with the event, returning it to the free queue
*/
void Scheduler_v1w::runScheduler() {
	debugC(6, kDebugSchedule, "runScheduler");

	uint32 ticker = getTicks();                     // The time now, in ticks
	event_t *curEvent = _headEvent;                 // The earliest event

	while (curEvent && (curEvent->time <= ticker))  // While mature events found
		curEvent = doAction(curEvent);              // Perform the action (returns next_p)

	_vm->getGameStatus().tick++;                    // Accessed elsewhere via getTicks()
}
} // End of namespace Hugo
