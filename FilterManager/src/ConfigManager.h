#pragma once

#include <string>

class FilterTree;

class ConfigManager {
public:
    void loadConfig(const std::string& filepath, FilterTree& tree);
    void saveConfig(const std::string& filepath, FilterTree& tree);

    std::string toUtf8(const std::wstring& wstr);
    std::wstring fromUtf8(const char* str);
};