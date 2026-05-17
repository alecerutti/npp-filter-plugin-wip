#pragma once

#include "PluginInterface.h"

#include <vector>

void loadPlugin();
void unloadPlugin();

void initializeMenu();
void addMenuItem(const wchar_t* title, PFUNCPLUGINCMD action, bool checked = false, ShortcutKey* shortcut = NULL);
ShortcutKey* createShortcut(unsigned char key, bool enableALT = true, bool enableCTRL = true, bool enableSHIFT = true);
void dummyFunc();