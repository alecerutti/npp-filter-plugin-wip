#pragma once

#include "FilterTree.h"
#include "Notepadpp/PluginInterface.h"
#include "Notepadpp/DockingFeature/Docking.h" // tTbData

#include <windows.h>

class DockingPanel {
public:
    DockingPanel();
    ~DockingPanel();

    void init(NppData nppData, int cmdId, const std::string& configPath);
    void toggle();
    bool isVisible() const;
    void saveConfiguration();

private:
    static LRESULT CALLBACK staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void initMenus();

    HWND hPanel;
    FilterTree tree;
    NppData nppData;
    int cmdId;
    std::string currentConfigPath;
    tTbData dock;

    HMENU hFilterMenu;
    HMENU hFileMenu;

    enum MenuCommands {
        ID_ADD_CHILD_FILTER = 1001,
        ID_RENAME_FILTER = 1002,
        ID_DELETE_FILTER = 1003,
        ID_DELETE_FILTER_AND_CHILDREN = 1004,
        ID_ADD_CURRENT_DOC = 1005,
        ID_REMOVE_FILE = 1006,
        ID_SAVE_CONFIG = 1007
    };
};