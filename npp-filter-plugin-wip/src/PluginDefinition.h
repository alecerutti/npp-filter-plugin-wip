#pragma once

#include "PluginInterface.h"
#include <commctrl.h>
#include "tinyxml2.h"
#include <string>

enum ItemType
{
    TYPE_FILTER = 0,
    TYPE_FILE = 1
};

struct TreeItemData
{
    ItemType type;
    std::wstring filePath;
};

const TCHAR NPP_PLUGIN_NAME[] = TEXT("Filter Manager");
static const wchar_t* CONTAINER_CLASS = L"FilterManagerContainer";

// Number of commands reduced to 1 (panel only)
const int nbFunc = 1;

void pluginInit(HANDLE hModule);
void pluginCleanUp();
void commandMenuInit();
void commandMenuCleanUp();
bool setCommand(size_t index, TCHAR* cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey* sk = nullptr, bool check0nInit = false);

void panel();
std::wstring getConfigPath();

void expandNode(HTREEITEM item);
void collapseNode(HTREEITEM item);
void expandAllNodes();
void collapseAllNodes();

HTREEITEM copyItemRecursive(HTREEITEM src, HTREEITEM dstParent);
void moveItemRecursive(HTREEITEM item, HTREEITEM newParent);
void detachDataFromItemRecursive(HTREEITEM item);
void deleteFilter(HTREEITEM item);

void collapseNodeRecursive(HTREEITEM item);
void collapseAllNodesRecursive();
void expandNodeRecursive(HTREEITEM item);
void expandAllNodesRecursive();

void insertItemBefore(HTREEITEM item, HTREEITEM target);
void insertItemAfter(HTREEITEM item, HTREEITEM target);
HTREEITEM copyItemToPosition(HTREEITEM src, HTREEITEM dstParent, HTREEITEM insertAfter);