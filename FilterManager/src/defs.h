#pragma once

#include <string>

const wchar_t PLUGIN_NAME[] = L"FilterManager";

enum ItemType { TYPE_FILTER, TYPE_FILE };

struct TreeItemData {
    ItemType type;
    std::wstring filePath;
};
