#include "PluginDefinition.h"
#include "ConfigurationManager.h"
#include "DockingFeature/Docking.h"
#include <windowsx.h>
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "comctl32.lib")

static const wchar_t* CONTAINER_CLASS = L"FilterManagerContainer";
static const UINT_PTR TREEVIEW_SUBCLASS_ID = 1;

FuncItem funcItem[nbFunc];
NppData  nppData;

static FilterTree  filterTree;
static HWND        hContainer = nullptr;

// Menu appearing when right clicking on FilterManager panel
static HMENU       hFilterMenu = nullptr;
// Menu appearing when right clicking on a file listed in FilterManager panel
static HMENU       hFileMenu = nullptr;

static tTbData     myDock = {};
static std::string configPath;

enum MenuCommands {
    ID_ADD_CHILD_FILTER = 1001,
    ID_RENAME_FILTER = 1002,
    ID_DELETE_FILTER = 1003,
    ID_DELETE_FILTER_AND_CHILDREN = 1004,
    ID_ADD_CURRENT_DOC = 1005,
    ID_REMOVE_FILE = 1006,
    ID_SAVE_CONFIG = 1007
};

LRESULT CALLBACK ContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TreeViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

void pluginInit(HANDLE /*hModule*/) {
    hFilterMenu = CreatePopupMenu();
    AppendMenu(hFilterMenu, MF_STRING, ID_ADD_CURRENT_DOC, L"Add Current Document");
    AppendMenu(hFilterMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hFilterMenu, MF_STRING, ID_ADD_CHILD_FILTER, L"Add Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_RENAME_FILTER, L"Rename Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_DELETE_FILTER, L"Delete Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_DELETE_FILTER_AND_CHILDREN, L"Delete Filter AND Children");
    AppendMenu(hFilterMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hFilterMenu, MF_STRING, ID_SAVE_CONFIG, L"Save configuration");

    hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_REMOVE_FILE, L"Remove");
}

void pluginCleanUp() {}

void commandMenuInit() {
    WCHAR name[] = L"Show/Hide Panel";
    //setCommand(0, name, panel, nullptr, false);
    
    // Memory will be reclaimed by the OS when Notepad++ closes.
    ShortcutKey* sk = new ShortcutKey;
    sk->_isCtrl = true;
    sk->_isAlt = false;
    sk->_isShift = true;
    sk->_key = 'P'; // Use the virtual key code (e.g., 'P' for Ctrl+Shift+P)

    // Pass the ShortcutKey pointer to setCommand
    setCommand(0, name, panel, sk, false);
}

void commandMenuCleanUp() {
    if (filterTree.hwnd)
        saveTree(configPath, filterTree);
}

bool setCommand(size_t index, TCHAR* cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey* sk, bool check0nInit) {
    if (index >= nbFunc || !pFunc) return false;
    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;
    return true;
}

void panel() {
    if (!hContainer) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = ContainerProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = CONTAINER_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassEx(&wc);

        hContainer = CreateWindowEx(
            0, CONTAINER_CLASS, L"", WS_CHILD | WS_VISIBLE,
            0, 0, 300, 400, nppData._nppHandle, nullptr, GetModuleHandle(nullptr), nullptr);

        filterTree.hwnd = CreateWindowEx(
            WS_EX_CLIENTEDGE, WC_TREEVIEW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER
            | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS
            | TVS_EDITLABELS | TVS_FULLROWSELECT | TVS_NOTOOLTIPS,
            0, 0, 300, 400, hContainer, nullptr, GetModuleHandle(nullptr), nullptr);

        SetWindowSubclass(filterTree.hwnd, TreeViewSubclassProc, TREEVIEW_SUBCLASS_ID, 0);

        configPath = toUtf8(getConfigPath());
        loadTree(configPath, filterTree);

        myDock.hClient = hContainer;
        myDock.pszName = L"Filter Manager";
        myDock.dlgID = 0;
        myDock.uMask = 0;
        myDock.pszAddInfo = nullptr;
        myDock.hIconTab = nullptr;
        //myDock.pszModuleName = L"MyPlugin.dll";

        ::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&myDock);
        return;
    }

    UINT msg = IsWindowVisible(myDock.hClient) ? NPPM_DMMHIDE : NPPM_DMMSHOW;
    ::SendMessage(nppData._nppHandle, msg, 0, (LPARAM)myDock.hClient);
}

std::wstring getConfigPath() {
    wchar_t dir[MAX_PATH] = {};
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)dir);
    return std::wstring(dir) + L"\\FilterManager.xml";
}

LRESULT CALLBACK TreeViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR /*dwRefData*/)
{
    switch (uMsg) {
    case WM_MOUSEMOVE: {
        if (!filterTree.dragActive || !filterTree.dragItem) break;
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        if (!filterTree.isDragging) {
            if (abs(pt.x - filterTree.dragStartPt.x) > 5 || abs(pt.y - filterTree.dragStartPt.y) > 5) {
                filterTree.isDragging = true;
                SetCapture(hWnd);
            }
        }

        if (!filterTree.isDragging) break;

        TVHITTESTINFO hit = {};
        hit.pt = pt;
        TreeView_HitTest(hWnd, &hit);

        if (hit.hItem && hit.hItem != filterTree.dragItem) {
            RECT r;
            TreeView_GetItemRect(hWnd, hit.hItem, &r, FALSE);
            int h = r.bottom - r.top;
            int rel = pt.y - r.top;

            if (rel < h / 3) {
                TreeView_SelectDropTarget(hWnd, nullptr);
                TreeView_SetInsertMark(hWnd, hit.hItem, FALSE);
            }
            else if (rel > h * 2 / 3) {
                TreeView_SelectDropTarget(hWnd, nullptr);
                TreeView_SetInsertMark(hWnd, hit.hItem, TRUE);
            }
            else if (filterTree.getItemType(hit.hItem) == TYPE_FILTER) {
                TreeView_SetInsertMark(hWnd, nullptr, FALSE);
                TreeView_SelectDropTarget(hWnd, hit.hItem);
            }
            else {
                TreeView_SetInsertMark(hWnd, nullptr, FALSE);
                TreeView_SelectDropTarget(hWnd, nullptr);
            }
        }
        else {
            TreeView_SetInsertMark(hWnd, nullptr, FALSE);
            TreeView_SelectDropTarget(hWnd, nullptr);
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (!filterTree.dragActive) break;

        if (filterTree.isDragging) {
            ReleaseCapture();
            TreeView_SelectDropTarget(hWnd, nullptr);
            TreeView_SetInsertMark(hWnd, nullptr, FALSE);

            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            TVHITTESTINFO hit = {};
            hit.pt = pt;
            TreeView_HitTest(hWnd, &hit);

            if (hit.hItem && hit.hItem != filterTree.dragItem) {
                RECT r;
                TreeView_GetItemRect(hWnd, hit.hItem, &r, FALSE);
                int h = r.bottom - r.top;
                int rel = pt.y - r.top;

                if (rel < h / 3)
                    filterTree.moveItemBefore(filterTree.dragItem, hit.hItem);
                else if (rel > h * 2 / 3)
                    filterTree.moveItemAfter(filterTree.dragItem, hit.hItem);
                else if (filterTree.getItemType(hit.hItem) == TYPE_FILTER)
                    filterTree.moveItem(filterTree.dragItem, hit.hItem);
                else
                    filterTree.moveItemAfter(filterTree.dragItem, hit.hItem);
            }
        }

        filterTree.dragItem = nullptr;
        filterTree.isDragging = false;
        filterTree.dragActive = false;
        break;
    }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, TreeViewSubclassProc, uIdSubclass);
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (filterTree.hwnd)
            SetWindowPos(filterTree.hwnd, nullptr, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
        return 0;
    }

    case WM_SETFOCUS: {
        if (!TreeView_GetSelection(filterTree.hwnd)) {
            HTREEITEM root = TreeView_GetRoot(filterTree.hwnd);
            if (root) TreeView_SelectItem(filterTree.hwnd, root);
        }
        return 0;
    }

    case WM_COMMAND: {
        int cmd = LOWORD(wParam);
        HTREEITEM item = filterTree.selection();

        switch (cmd) {
        case ID_ADD_CHILD_FILTER: {
            HTREEITEM parent = item ? item : TVI_ROOT;
            HTREEITEM newItem = filterTree.addFilter(parent);
            if (newItem) filterTree.renameItem(newItem);
            return 0;
        }
        case ID_RENAME_FILTER:
            if (item && filterTree.getItemType(item) == TYPE_FILTER) filterTree.renameItem(item);
            return 0;
        case ID_DELETE_FILTER:
            if (item && filterTree.getItemType(item) == TYPE_FILTER) filterTree.deleteFilter(item);
            return 0;
        case ID_DELETE_FILTER_AND_CHILDREN:
            if (item && filterTree.getItemType(item) == TYPE_FILTER) {
                int res = MessageBox(hwnd, L"Delete selected filter AND all its children?",
                    L"Confirm deletion", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (res == IDYES) {
                    HTREEITEM parent = TreeView_GetParent(filterTree.hwnd, item);
                    SetFocus(filterTree.hwnd);
                    HTREEITEM sel = parent ? parent : TreeView_GetRoot(filterTree.hwnd);
                    if (sel) TreeView_SelectItem(filterTree.hwnd, sel);
                    TreeView_DeleteItem(filterTree.hwnd, item);
                }
            }
            return 0;
        case ID_ADD_CURRENT_DOC:
            if (item && filterTree.getItemType(item) == TYPE_FILTER)
                filterTree.addCurrentDocument(item, nppData._nppHandle, hwnd);
            return 0;
        case ID_REMOVE_FILE:
            if (item && filterTree.getItemType(item) == TYPE_FILE)
                TreeView_DeleteItem(filterTree.hwnd, item);
            return 0;
        case ID_SAVE_CONFIG:
            saveTree(configPath, filterTree);
            return 0;
        }
        break;
    }

    case WM_NOTIFY: {
        LPNMHDR lpnmh = (LPNMHDR)lParam;
        if (lpnmh->hwndFrom != filterTree.hwnd) break;

        switch (lpnmh->code) {
        case NM_DBLCLK: {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(filterTree.hwnd, &pt);
            TVHITTESTINFO hit = {};
            hit.pt = pt;
            HTREEITEM item = TreeView_HitTest(filterTree.hwnd, &hit);
            if (!item) break;
            if (filterTree.getItemType(item) == TYPE_FILTER)
                SendMessage(hwnd, WM_COMMAND, ID_RENAME_FILTER, 0);
            else
                filterTree.openFile(item, nppData._nppHandle);
            return TRUE;
        }
        case TVN_BEGINDRAG: {
            LPNMTREEVIEW lpnmtv = (LPNMTREEVIEW)lParam;
            filterTree.dragItem = lpnmtv->itemNew.hItem;
            filterTree.dragStartPt = lpnmtv->ptDrag;
            filterTree.dragActive = true;
            filterTree.isDragging = false;
            TreeView_SelectItem(filterTree.hwnd, filterTree.dragItem);
            SetFocus(filterTree.hwnd);
            return 0;
        }
        case NM_RCLICK: {
            POINT screenPt, clientPt;
            GetCursorPos(&screenPt);
            clientPt = screenPt;
            ScreenToClient(filterTree.hwnd, &clientPt);

            TVHITTESTINFO hit = {};
            hit.pt = clientPt;
            HTREEITEM clicked = TreeView_HitTest(filterTree.hwnd, &hit);

            if (clicked) {
                TreeView_SelectItem(filterTree.hwnd, clicked);
                if (filterTree.getItemType(clicked) == TYPE_FILTER)
                    TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, hwnd, nullptr);
                else
                    TrackPopupMenu(hFileMenu, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, hwnd, nullptr);
            }
            else {
                TreeView_SelectItem(filterTree.hwnd, nullptr);
                // Grey out item-specific actions when nothing is selected
                EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_GRAYED);
                EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_GRAYED);
                EnableMenuItem(hFilterMenu, ID_DELETE_FILTER_AND_CHILDREN, MF_GRAYED);
                EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_GRAYED);
                TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, hwnd, nullptr);
                // Re-enable for next time
                EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_ENABLED);
                EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_ENABLED);
                EnableMenuItem(hFilterMenu, ID_DELETE_FILTER_AND_CHILDREN, MF_ENABLED);
                EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_ENABLED);
            }
            return TRUE;
        }
        case TVN_BEGINLABELEDIT: {
            LPNMTVDISPINFO pInfo = (LPNMTVDISPINFO)lParam;
            return filterTree.getItemType(pInfo->item.hItem) == TYPE_FILE ? TRUE : FALSE;
        }
        case TVN_ENDLABELEDIT: {
            LPNMTVDISPINFO pInfo = (LPNMTVDISPINFO)lParam;
            if (pInfo->item.pszText && pInfo->item.pszText[0] != L'\0') {
                TVITEM item = pInfo->item;
                item.mask = TVIF_TEXT | TVIF_HANDLE;
                TreeView_SetItem(filterTree.hwnd, &item);
                return TRUE;
            }
            return FALSE;
        }
        case TVN_DELETEITEM: {
            TreeItemData* data = (TreeItemData*)((LPNMTREEVIEW)lParam)->itemOld.lParam;
            delete data;
            break;
        }
        case NM_CUSTOMDRAW: {
            LPNMTVCUSTOMDRAW pcd = (LPNMTVCUSTOMDRAW)lParam;
            if (pcd->nmcd.dwDrawStage == CDDS_PREPAINT)    return CDRF_NOTIFYITEMDRAW;
            if (pcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                TreeItemData* data = filterTree.getItemData((HTREEITEM)pcd->nmcd.dwItemSpec);
                pcd->clrTextBk = (data && data->type == TYPE_FILTER) ? RGB(235, 235, 235)
                    : RGB(255, 255, 255);
                return CDRF_DODEFAULT;
            }
            break;
        }
        }
        break;
    }

    case WM_DESTROY:
        DestroyMenu(hFilterMenu); hFilterMenu = nullptr;
        DestroyMenu(hFileMenu);   hFileMenu = nullptr;
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}