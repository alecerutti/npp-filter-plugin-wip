////this file is part of notepad++
////Copyright (C)2022 Don HO <don.h@free.fr>
////
////This program is free software; you can redistribute it and/or
////modify it under the terms of the GNU General Public License
////as published by the Free Software Foundation; either
////version 2 of the License, or (at your option) any later version.
////
////This program is distributed in the hope that it will be useful,
////but WITHOUT ANY WARRANTY; without even the implied warranty of
////MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
////GNU General Public License for more details.
////
////You should have received a copy of the GNU General Public License
////along with this program; if not, write to the Free Software
////Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#pragma once

#include "PluginInterface.h"
#include <commctrl.h> // TreeView
#include "tinyxml2.h"

// Const stuff
const TCHAR NPP_PLUGIN_NAME[] = TEXT("FilterManager");
static const wchar_t* CONTAINER_CLASS = L"FilterManagerContainer";

// Numero di comandi
const int nbFunc = 2;

// Ciclo vita plugin
void pluginInit(HANDLE hModule);
void pluginCleanUp();
void commandMenuInit();
void commandMenuCleanUp();
bool setCommand(size_t index, TCHAR* cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey* sk = nullptr, bool check0nInit = false);

// Comandi plugin
void panel();

// TreeView
void expandNode(HTREEITEM item);
void collapseNode(HTREEITEM item);
void expandAllNodes();
void collapseAllNodes();