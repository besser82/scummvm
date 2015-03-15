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
 */

#ifndef SHERLOCK_EVENTS_H
#define SHERLOCK_EVENTS_H

#include "common/scummsys.h"
#include "common/events.h"
#include "common/stack.h"

namespace Sherlock {

enum CursorType { CURSOR_NONE = 0 };

#define GAME_FRAME_RATE 50
#define GAME_FRAME_TIME (1000 / GAME_FRAME_RATE)

class SherlockEngine;

class EventsManager {
private:
	SherlockEngine *_vm;
	uint32 _frameCounter;
	uint32 _priorFrameTime;
	Common::Point _mousePos;

	bool checkForNextFrameCounter();
public:
	CursorType _cursorId;
	byte _mouseButtons;
	bool _mouseClicked;
	Common::Stack<Common::KeyState> _pendingKeys;
public:
	EventsManager(SherlockEngine *vm);
	~EventsManager();

	void setCursor(CursorType cursorId);

	void showCursor();

	void hideCursor();

	bool isCursorVisible();

	void pollEvents();

	Common::Point mousePos() const { return _mousePos; }

	uint32 getFrameCounter() const { return _frameCounter; }

	bool isKeyPressed() const { return !_pendingKeys.empty(); }

	Common::KeyState getKey() { return _pendingKeys.pop(); }

	void delay(int amount);

	void waitForNextFrame();

};

} // End of namespace Sherlock

#endif /* SHERLOCK_EVENTS_H */
