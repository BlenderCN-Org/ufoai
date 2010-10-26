/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(INCLUDED_COMMANDS_H)
#define INCLUDED_COMMANDS_H

#include "gtkutil/accelerator.h"
#include <string>

void KeyEvent_connect(const std::string& name);
void KeyEvent_disconnect(const std::string& name);

const Accelerator& GlobalShortcuts_insert (const std::string& name, const Accelerator& accelerator);
void GlobalShortcuts_register (const std::string& name, int type); // 1 = command, 2 = toggle
void GlobalShortcuts_reportUnregistered ();

class CommandVisitor
{
	public:
		virtual ~CommandVisitor ()
		{
		}
		virtual void visit (const std::string& name, Accelerator& accelerator) = 0;
};

void GlobalCommands_insert (const std::string& name, const Callback& callback, const Accelerator& accelerator =
		accelerator_null());
const Command& GlobalCommands_find (const std::string& name);

void GlobalToggles_insert (const std::string& name, const Callback& callback, const BoolExportCallback& exportCallback,
		const Accelerator& accelerator = accelerator_null());
const Toggle& GlobalToggles_find (const std::string& name);

void GlobalKeyEvents_insert (const std::string& name, const Accelerator& accelerator, const Callback& keyDown,
		const Callback& keyUp);
const KeyEvent& GlobalKeyEvents_find (const std::string& name);

void DoCommandListDlg ();

void LoadCommandMap (const std::string& path);
void SaveCommandMap (const std::string& path);
void GlobalShortcuts_foreach (CommandVisitor& visitor);
void disconnect_accelerator(const std::string&);
void connect_accelerator(const std::string&);

#endif
