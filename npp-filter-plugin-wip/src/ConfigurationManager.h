#pragma once
#include <string>
#include "FilterTree.h"

// UTF-8 <-> wstring conversion (Windows-only, CP_UTF8)
std::string  toUtf8(const std::wstring& wstr);
std::wstring fromUtf8(const char* str);

void saveTree(const std::string& filename, FilterTree& tree);
void loadTree(const std::string& filename, FilterTree& tree);