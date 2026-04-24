#pragma once
#include "PluginInterface.h"
#include "FilterTree.h"
#include <commctrl.h>

const TCHAR NPP_PLUGIN_NAME[] = TEXT("Filter Manager");
const int   nbFunc = 1;

extern FuncItem funcItem[nbFunc];
extern NppData  nppData;

// Initialization/cleanup functions
void commandMenuInit();
void commandMenuCleanUp();
void pluginInit(HANDLE hModule);
void pluginCleanUp();

// Make pFunc visible and invokable from Notepad++ Plugins tab
bool setCommand(size_t index, TCHAR* cmdName, PFUNCPLUGINCMD pFunc,
    ShortcutKey* sk = nullptr, bool check0nInit = false);

// Custom functions
void panel();
std::wstring getConfigPath();