#include "FilterManager.h"
#include "DockingPanel.h"

DockingPanel* pluginPanel = nullptr;
NppData nppData;
std::vector<FuncItem> nppMenu;

void loadPlugin() {
    pluginPanel = new DockingPanel();
    pluginPanel->init(nppData, 0, getPluginConfigPath());
}

void unloadPlugin() {
    if (pluginPanel) {
        pluginPanel->saveConfiguration();
        delete pluginPanel;
        pluginPanel = nullptr;
    }
    for (auto& item : nppMenu) {
        if (item._pShKey) delete item._pShKey;
    }
}

void initializeMenu()
{
    addMenuItem(L"Show/Hide Panel", togglePanel, false, createShortcut('J'));
    addMenuItem(L"Save", saveTree);
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

std::string getPluginConfigPath()
{
    wchar_t dir[MAX_PATH] = {};
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)dir);
    std::wstring wPath = std::wstring(dir) + L"\\FilterManager.xml";
    int n = WideCharToMultiByte(CP_UTF8, 0, wPath.c_str(), (int)wPath.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wPath.c_str(), (int)wPath.size(), &out[0], n, nullptr, nullptr);
    return out;
}

void togglePanel()
{
    if (pluginPanel) pluginPanel->toggle();
}

void saveTree()
{
    if (pluginPanel) pluginPanel->saveConfiguration();
}

///////////////////////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData) {
    nppData = notpadPlusData;
    initializeMenu();
    //loadPlugin();
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
    case NPPN_READY:
    {
        loadPlugin();
        break;
    }
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