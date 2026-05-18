#pragma once

#include "Notepadpp/PluginInterface.h"

#include <vector>
#include <string>

void loadPlugin();
void unloadPlugin();

void initializeMenu();
void addMenuItem(const wchar_t* title, PFUNCPLUGINCMD action, bool checked = false, ShortcutKey* shortcut = NULL);
ShortcutKey* createShortcut(unsigned char key, bool enableALT = true, bool enableCTRL = true, bool enableSHIFT = true);

std::string getPluginConfigPath();
void togglePanel();
void saveTree();