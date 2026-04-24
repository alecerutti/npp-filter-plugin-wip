#include "FilterManager.h"

/// DEBUG
#include <fstream>
///

// todo remove
const int   nbFunc = 1;
FuncItem funcItem[nbFunc];
//

const wchar_t PLUGIN_NAME[] = L"FilterManager";
NppData nppData;
std::vector<FuncItem> nppMenu;

void loadPlugin() {
    // DEBUG
    std::ofstream("load_working.txt");
}

void unloadPlugin() {
    std::ofstream("unlodad_working.txt");
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

    //*nbF = static_cast<int>(nppMenu.size());
    //return nppMenu.data();
    *nbF = nbFunc;
    return funcItem;
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