#include "FilterManager.h"

/// DEBUG
#include <fstream>
#include <iostream>
#include <ctime>
#include <string>

std::string get_simple_timestamp() {
    std::time_t now = std::time(nullptr);
    std::tm tm_struct;
    char buf[20];

    // Versione sicura per MSVC (Windows)
    localtime_s(&tm_struct, &now);

    std::strftime(buf, sizeof(buf), "%Y_%m_%d_%H_%M_%S", &tm_struct);

    return std::string(buf);
}
//

const wchar_t PLUGIN_NAME[] = L"FilterManager";
NppData nppData;
std::vector<FuncItem> nppMenu;

void loadPlugin() {
    // DEBUG
    std::ofstream("c:\\users\\user\\downloads\\filtermanager\\load_working.txt");

    initializeMenu();
}

void unloadPlugin() {
    std::ofstream("c:\\users\\user\\downloads\\filtermanager\\unlodad_working.txt");
}

void initializeMenu()
{
    addMenuItem(L"dummy_fucntion", dummyFunc, false, createShortcut('J'));
}

void addMenuItem(const wchar_t* title, PFUNCPLUGINCMD action, bool checked, ShortcutKey* shortcut)
{
    FuncItem item;

    wcscpy_s(item._itemName, _countof(item._itemName), title);
    item._pFunc = action;
    item._init2Check = checked;
    item._pShKey = shortcut;

    nppMenu.push_back(item);
}

ShortcutKey* createShortcut(unsigned char key, bool enableALT, bool enableCTRL, bool enableSHIFT) {
    auto shortcut = new ShortcutKey();
    shortcut->_isAlt = enableALT;
    shortcut->_isCtrl = enableCTRL;
    shortcut->_isShift = enableSHIFT;
    shortcut->_key = key;

    return shortcut;
}

void dummyFunc()
{
    std::string filename = "c:\\users\\user\\downloads\\filtermanager\\" + get_simple_timestamp() + ".txt";
    std::ofstream(filename.c_str());
}

///////////////////////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
    nppData = notpadPlusData;
    loadPlugin();
}

// The getName function tells Notepad++ plugins system its name
extern "C" __declspec(dllexport) const TCHAR* getName() {
    return PLUGIN_NAME;
}

// The getFuncsArray function gives Notepad++ plugins system the pointer FuncItem Array
// and the size of this array (the number of functions)
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF) {

    *nbF = static_cast<int>(nppMenu.size());
    return nppMenu.data();
}

// For v.3.3 compatibility
extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam) {
    return TRUE;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode)
{
    switch (notifyCode->nmhdr.code)
    {
    case NPPN_SHUTDOWN:
    {
        unloadPlugin();
    }
    break;

    default:
        return;
    }
}

#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}
#endif //UNICODE