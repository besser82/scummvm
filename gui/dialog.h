/* ScummVM - Scumm Interpreter
 * Copyright (C) 2002 The ScummVM project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header$
 */

#ifndef DIALOG_H
#define DIALOG_H

#include "scummsys.h"


class Widget;
class NewGui;

#define RES_STRING(id)		_gui->queryResString(id)
#define CUSTOM_STRING(id)	_gui->queryCustomString(id)

// Some "common" commands sent to handleCommand()
enum {
	kCloseCmd = 'clos'
};

class Dialog {
	friend class Widget;
protected:
	NewGui	*_gui;
	Widget	*_firstWidget;
	int16	_x, _y;
	uint16	_w, _h;
	Widget	*_mouseWidget;
public:
	Dialog(NewGui *gui, int x, int y, int w, int h)
		: _gui(gui), _firstWidget(0), _x(x), _y(y), _w(w), _h(h), _mouseWidget(0)
		{}

	virtual void draw();

	//virtual void handleIdle(); // Called periodically
	virtual void handleClick(int x, int y, int button);
	virtual void handleKey(char key, int modifiers) // modifiers = alt/shift/ctrl etc.
		{ if (key == 27) close(); }
	virtual void handleMouseMoved(int x, int y, int button);
	virtual void handleCommand(uint32 cmd);
	
	NewGui	*getGui()	{ return _gui; }

protected:
	Widget* findWidget(int x, int y); // Find the widget at pos x,y if any
	void close();

	void addResText(int x, int y, int w, int h, int resID);
	void addButton(int x, int y, int w, int h, char hotkey, const char *label, uint32 cmd);
};

class SaveLoadDialog : public Dialog {
public:
	SaveLoadDialog(NewGui *gui);

	virtual void handleCommand(uint32 cmd);
};

class OptionsDialog : public Dialog {
public:
	OptionsDialog(NewGui *gui);

	virtual void handleCommand(uint32 cmd);
};

class PauseDialog : public Dialog {
public:
	PauseDialog(NewGui *gui);

	virtual void handleClick(int x, int y, int button)
		{ close(); }
	virtual void handleKey(char key, int modifiers)
		{
			if (key == 32)
				close();
			else
				Dialog::handleKey(key, modifiers);
		}
};

#endif
