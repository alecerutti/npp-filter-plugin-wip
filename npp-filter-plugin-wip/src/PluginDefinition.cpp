#include "PluginDefinition.h"
#include "DockingFeature/Docking.h"
#include "ConfigurationManager.h"
#include <string>
#include <fstream>
#include <windowsx.h>
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "comctl32.lib")

HTREEITEM g_dragItem = NULL;
bool g_isDragging = false;
bool g_dragActive = false;
POINT g_dragStartPt = {};
const UINT_PTR TREEVIEW_SUBCLASS_ID = 1;
std::string configPath;

FuncItem funcItem[nbFunc];
NppData nppData;

enum MenuCommands
{
    ID_ADD_CHILD_FILTER = 1001,
    ID_RENAME_FILTER = 1002,
    ID_DELETE_FILTER = 1003,
    ID_DELETE_FILTER_AND_CHILDREN = 1004,
    ID_ADD_CURRENT_DOC = 1005,
    ID_REMOVE_FILE = 1006,
    ID_SAVE_CONFIG = 1007
};

HWND hContainer = NULL;
HWND hTreeView = NULL;
HMENU hFilterMenu = NULL;
HMENU hFileMenu = NULL;
tTbData myDock = { 0 };

void pluginInit(HANDLE /*hModule*/)
{
    hFilterMenu = CreatePopupMenu();
    AppendMenu(hFilterMenu, MF_STRING, ID_ADD_CURRENT_DOC, L"Add Current Document");
    AppendMenu(hFilterMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFilterMenu, MF_STRING, ID_ADD_CHILD_FILTER, L"Add Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_RENAME_FILTER, L"Rename Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_DELETE_FILTER, L"Delete Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_DELETE_FILTER_AND_CHILDREN, L"Delete Filter AND Children");
    AppendMenu(hFilterMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFilterMenu, MF_STRING, ID_SAVE_CONFIG, L"Save configuration");

    hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_REMOVE_FILE, L"Remove");

    ZeroMemory(&myDock, sizeof(tTbData));
    myDock.pszName = L"Filter Manager";
    myDock.dlgID = 0;
    myDock.uMask = 0;
    myDock.pszAddInfo = nullptr;
    myDock.hIconTab = nullptr;
    myDock.pszModuleName = L"MyPlugin.dll";
}

void pluginCleanUp()
{
}

void commandMenuInit()
{
    WCHAR cmdName[] = L"Show/Hide Panel";
    setCommand(0, cmdName, panel, nullptr, false);
}

void commandMenuCleanUp()
{
    if (hTreeView)
        saveTreeToXml(configPath, hTreeView);
}

bool setCommand(size_t index, TCHAR* cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey* sk, bool check0nInit)
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

LRESULT CALLBACK ContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TreeViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
HTREEITEM addRootFilter();
HTREEITEM addChildFilter(HTREEITEM parent);
void renameFilter(HTREEITEM item);
void addCurrentDocument(HTREEITEM filterItem);
void removeFile(HTREEITEM fileItem);
void openFile(HTREEITEM fileItem);
HTREEITEM getSelectedItem();
TreeItemData* getItemData(HTREEITEM item);
ItemType getItemType(HTREEITEM item);

std::wstring getTreeItemText(HTREEITEM hItem)
{
    wchar_t buffer[MAX_PATH] = { 0 };
    TVITEM tvi = { 0 };
    tvi.hItem = hItem;
    tvi.mask = TVIF_TEXT;
    tvi.pszText = buffer;
    tvi.cchTextMax = MAX_PATH;
    TreeView_GetItem(hTreeView, &tvi);
    return std::wstring(buffer);
}

void panel()
{
    if (!hContainer)
    {
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = ContainerProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = CONTAINER_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassEx(&wc);

        hContainer = CreateWindowEx(
            0, CONTAINER_CLASS, L"", WS_CHILD | WS_VISIBLE,
            0, 0, 300, 400, nppData._nppHandle, nullptr, GetModuleHandle(nullptr), nullptr
        );

        hTreeView = CreateWindowEx(
            WS_EX_CLIENTEDGE, WC_TREEVIEW, L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_EDITLABELS | TVS_FULLROWSELECT | TVS_NOTOOLTIPS,
            0, 0, 300, 400, hContainer, nullptr, GetModuleHandle(nullptr), nullptr
        );

        SetWindowSubclass(hTreeView, TreeViewSubclassProc, TREEVIEW_SUBCLASS_ID, 0);

        configPath = convertWStringToUtf8(getConfigPath());
        loadTreeFromXml(configPath, hTreeView);

        myDock.hClient = hContainer;
        myDock.pszName = L"Filter Manager";
        myDock.dlgID = 0;
        myDock.uMask = 0;
        myDock.pszAddInfo = nullptr;
        myDock.hIconTab = nullptr;
        myDock.pszModuleName = L"MyPlugin.dll";

        ::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&myDock);
        return;
    }

    const bool isVisibleNow = IsWindowVisible(myDock.hClient);
    ::SendMessage(nppData._nppHandle, isVisibleNow ? NPPM_DMMHIDE : NPPM_DMMSHOW, 0, (LPARAM)myDock.hClient);
}

std::wstring getConfigPath()
{
    wchar_t pluginConfigPath[MAX_PATH] = { 0 };
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)pluginConfigPath);
    std::wstring path(pluginConfigPath);
    path += L"\\FilterManager.xml";
    return path;
}

void collapseNodeRecursive(HTREEITEM item)
{
    if (!item) return;
    HTREEITEM next_item = TreeView_GetChild(hTreeView, item);
    while (next_item)
    {
        collapseNodeRecursive(next_item);
        next_item = TreeView_GetNextSibling(hTreeView, next_item);
    }
    TreeView_Expand(hTreeView, item, TVE_COLLAPSE);
}

void collapseAllNodesRecursive()
{
    HTREEITEM item = TreeView_GetRoot(hTreeView);
    while (item)
    {
        collapseNodeRecursive(item);
        item = TreeView_GetNextSibling(hTreeView, item);
    }
}

void expandNodeRecursive(HTREEITEM item)
{
    if (!item) return;
    HTREEITEM next_item = TreeView_GetChild(hTreeView, item);
    while (next_item)
    {
        expandNodeRecursive(next_item);
        next_item = TreeView_GetNextSibling(hTreeView, next_item);
    }
    TreeView_Expand(hTreeView, item, TVE_EXPAND);
}

void expandAllNodesRecursive()
{
    HTREEITEM item = TreeView_GetRoot(hTreeView);
    while (item)
    {
        expandNodeRecursive(item);
        item = TreeView_GetNextSibling(hTreeView, item);
    }
}

HTREEITEM addRootFilter()
{
    TreeItemData* data = new TreeItemData();
    data->type = TYPE_FILTER;

    TVINSERTSTRUCT tvis = {};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    wchar_t newFilterText[] = L"New Filter";
    tvis.item.pszText = newFilterText;
    tvis.item.lParam = (LPARAM)data;

    HTREEITEM newItem = TreeView_InsertItem(hTreeView, &tvis);
    if (newItem) TreeView_SelectItem(hTreeView, newItem);
    return newItem;
}

HTREEITEM addChildFilter(HTREEITEM parent)
{
    TreeItemData* data = new TreeItemData();
    data->type = TYPE_FILTER;

    TVINSERTSTRUCT tvis = {};
    tvis.hParent = parent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    wchar_t newFilterText[] = L"New Filter";
    tvis.item.pszText = newFilterText;
    tvis.item.lParam = (LPARAM)data;

    HTREEITEM newItem = TreeView_InsertItem(hTreeView, &tvis);
    TreeView_Expand(hTreeView, parent, TVE_EXPAND);
    if (newItem) TreeView_SelectItem(hTreeView, newItem);
    return newItem;
}

void renameFilter(HTREEITEM item)
{
    TreeView_SelectItem(hTreeView, item);
    TreeView_EnsureVisible(hTreeView, item);
    SetFocus(hTreeView);
    TreeView_EditLabel(hTreeView, item);
}

void addCurrentDocument(HTREEITEM filterItem)
{
    wchar_t filePath[MAX_PATH] = { 0 };
    ::SendMessage(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)filePath);

    if (wcslen(filePath) == 0)
    {
        MessageBox(hContainer, L"No document is currently open!", L"Error", MB_OK | MB_ICONWARNING);
        return;
    }

    wchar_t* fileName = wcsrchr(filePath, L'\\');
    if (!fileName) fileName = wcsrchr(filePath, L'/');
    if (fileName) fileName++;
    else fileName = filePath;

    TreeItemData* data = new TreeItemData();
    data->type = TYPE_FILE;
    data->filePath = filePath;

    TVINSERTSTRUCT tvis = {};
    tvis.hParent = filterItem;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = fileName;
    tvis.item.lParam = (LPARAM)data;

    TreeView_InsertItem(hTreeView, &tvis);
    TreeView_Expand(hTreeView, filterItem, TVE_EXPAND);
}

void removeFile(HTREEITEM fileItem)
{
    TreeItemData* data = getItemData(fileItem);
    if (data) delete data;
    TreeView_DeleteItem(hTreeView, fileItem);
}

void openFile(HTREEITEM fileItem)
{
    TreeItemData* data = getItemData(fileItem);
    if (data && data->type == TYPE_FILE)
    {
        ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)data->filePath.c_str());
    }
}

HTREEITEM getSelectedItem()
{
    return TreeView_GetSelection(hTreeView);
}

void unselectItem()
{
    TreeView_SelectItem(hTreeView, NULL);
}

void selectItem(HTREEITEM item)
{
    TreeView_SelectItem(hTreeView, item);
}

TreeItemData* getItemData(HTREEITEM item)
{
    if (!item) return nullptr;

    TVITEM tvi = {};
    tvi.mask = TVIF_PARAM | TVIF_HANDLE;
    tvi.hItem = item;

    if (TreeView_GetItem(hTreeView, &tvi))
        return (TreeItemData*)tvi.lParam;

    return nullptr;
}

ItemType getItemType(HTREEITEM item)
{
    TreeItemData* data = getItemData(item);
    if (data) return data->type;
    return TYPE_FILTER;
}

LRESULT CALLBACK TreeViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
    case WM_MOUSEMOVE:
    {
        if (g_dragActive && g_dragItem)
        {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            if (!g_isDragging)
            {
                int dx = abs(pt.x - g_dragStartPt.x);
                int dy = abs(pt.y - g_dragStartPt.y);

                if (dx > 5 || dy > 5)
                {
                    g_isDragging = true;
                    SetCapture(hWnd);
                }
            }

            if (g_isDragging)
            {
                TVHITTESTINFO hit = {};
                hit.pt = pt;
                TreeView_HitTest(hWnd, &hit);

                if (hit.hItem && hit.hItem != g_dragItem)
                {
                    RECT itemRect;
                    TreeView_GetItemRect(hWnd, hit.hItem, &itemRect, FALSE);

                    int itemHeight = itemRect.bottom - itemRect.top;
                    int relativeY = pt.y - itemRect.top;

                    if (relativeY < itemHeight / 3)
                    {
                        TreeView_SelectDropTarget(hWnd, NULL);
                        TreeView_SetInsertMark(hWnd, hit.hItem, FALSE);
                    }
                    else if (relativeY > itemHeight * 2 / 3)
                    {
                        TreeView_SelectDropTarget(hWnd, NULL);
                        TreeView_SetInsertMark(hWnd, hit.hItem, TRUE);
                    }
                    else
                    {
                        if (getItemType(hit.hItem) == TYPE_FILTER)
                        {
                            TreeView_SetInsertMark(hWnd, NULL, FALSE);
                            TreeView_SelectDropTarget(hWnd, hit.hItem);
                        }
                        else
                        {
                            TreeView_SetInsertMark(hWnd, NULL, FALSE);
                            TreeView_SelectDropTarget(hWnd, NULL);
                        }
                    }
                }
                else
                {
                    TreeView_SetInsertMark(hWnd, NULL, FALSE);
                    TreeView_SelectDropTarget(hWnd, NULL);
                }
            }
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        if (g_dragActive)
        {
            if (g_isDragging)
            {
                ReleaseCapture();
                TreeView_SelectDropTarget(hWnd, NULL);
                TreeView_SetInsertMark(hWnd, NULL, FALSE);

                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                TVHITTESTINFO hit = {};
                hit.pt = pt;
                TreeView_HitTest(hWnd, &hit);

                if (hit.hItem && hit.hItem != g_dragItem)
                {
                    RECT itemRect;
                    TreeView_GetItemRect(hWnd, hit.hItem, &itemRect, FALSE);

                    int itemHeight = itemRect.bottom - itemRect.top;
                    int relativeY = pt.y - itemRect.top;

                    if (relativeY < itemHeight / 3)
                    {
                        insertItemBefore(g_dragItem, hit.hItem);
                    }
                    else if (relativeY > itemHeight * 2 / 3)
                    {
                        insertItemAfter(g_dragItem, hit.hItem);
                    }
                    else
                    {
                        if (getItemType(hit.hItem) == TYPE_FILTER)
                        {
                            moveItemRecursive(g_dragItem, hit.hItem);
                        }
                        else
                        {
                            insertItemAfter(g_dragItem, hit.hItem);
                        }
                    }
                }
            }

            g_dragItem = NULL;
            g_isDragging = false;
            g_dragActive = false;
        }
        break;
    }

    case WM_NCDESTROY:
        RemoveWindowSubclass(hWnd, TreeViewSubclassProc, uIdSubclass);
        break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

HTREEITEM copyItemRecursive(HTREEITEM src, HTREEITEM dstParent)
{
    wchar_t text[260];

    TVITEM tvi = {};
    tvi.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.hItem = src;
    tvi.pszText = text;
    tvi.cchTextMax = 260;

    TreeView_GetItem(hTreeView, &tvi);

    TVINSERTSTRUCT ins = {};
    ins.hParent = dstParent;
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM;
    ins.item.pszText = text;
    ins.item.lParam = tvi.lParam;

    HTREEITEM newItem = TreeView_InsertItem(hTreeView, &ins);

    HTREEITEM child = TreeView_GetChild(hTreeView, src);
    while (child)
    {
        copyItemRecursive(child, newItem);
        child = TreeView_GetNextSibling(hTreeView, child);
    }

    return newItem;
}

void moveItemRecursive(HTREEITEM item, HTREEITEM newParent)
{
    HTREEITEM newItem = copyItemRecursive(item, newParent);
    detachDataFromItemRecursive(item);
    TreeView_DeleteItem(hTreeView, item);
    TreeView_Expand(hTreeView, newParent, TVE_EXPAND);
    TreeView_SelectItem(hTreeView, newItem);
}

void detachDataFromItemRecursive(HTREEITEM item)
{
    TVITEM tvi = {};
    tvi.mask = TVIF_PARAM;
    tvi.hItem = item;
    tvi.lParam = 0;
    TreeView_SetItem(hTreeView, &tvi);

    HTREEITEM child = TreeView_GetChild(hTreeView, item);
    while (child)
    {
        detachDataFromItemRecursive(child);
        child = TreeView_GetNextSibling(hTreeView, child);
    }
}

LRESULT CALLBACK ContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (hTreeView)
        {
            SetWindowPos(hTreeView, NULL, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
        }
        return 0;
    }

    case WM_SETFOCUS:
    {
        HTREEITEM selected = TreeView_GetSelection(hTreeView);
        if (!selected)
        {
            HTREEITEM root = TreeView_GetRoot(hTreeView);
            if (root)
                TreeView_SelectItem(hTreeView, root);
        }
        return 0;
    }

    case WM_COMMAND:
    {
        int cmd = LOWORD(wParam);
        HTREEITEM item = getSelectedItem();

        if (cmd == ID_ADD_CHILD_FILTER)
        {
            if (item) item = addChildFilter(item);
            else item = addRootFilter();
            if (item) renameFilter(item);
            return 0;
        }
        else if (cmd == ID_DELETE_FILTER)
        {
            if (item && getItemType(item) == TYPE_FILTER) deleteFilter(item);
            return 0;
        }
        else if (cmd == ID_DELETE_FILTER_AND_CHILDREN)
        {
            if (item && getItemType(item) == TYPE_FILTER)
            {
                int res = MessageBox(
                    hContainer, L"Are you sure to delete the selected filter AND all its children?",
                    L"Confirm deletion", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
                );

                if (res == IDYES)
                {
                    HTREEITEM parent = TreeView_GetParent(hTreeView, item);
                    SetFocus(hTreeView);
                    if (parent) TreeView_SelectItem(hTreeView, parent);
                    else
                    {
                        HTREEITEM root = TreeView_GetRoot(hTreeView);
                        if (root) TreeView_SelectItem(hTreeView, root);
                    }
                    TreeView_DeleteItem(hTreeView, item);
                }
            }
            return 0;
        }
        else if (cmd == ID_RENAME_FILTER)
        {
            if (item && getItemType(item) == TYPE_FILTER) renameFilter(item);
            return 0;
        }
        else if (cmd == ID_ADD_CURRENT_DOC)
        {
            if (item && getItemType(item) == TYPE_FILTER) addCurrentDocument(item);
            return 0;
        }
        else if (cmd == ID_REMOVE_FILE)
        {
            if (item && getItemType(item) == TYPE_FILE) TreeView_DeleteItem(hTreeView, item);
            return 0;
        }
        else if (cmd == ID_SAVE_CONFIG)
        {
            saveTreeToXml(configPath, hTreeView);
            return 0;
        }
        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR lpnmh = (LPNMHDR)lParam;

        if (lpnmh->hwndFrom == hTreeView)
        {
            switch (lpnmh->code)
            {
            case NM_DBLCLK:
            {
                POINT point;
                GetCursorPos(&point);
                ScreenToClient(hTreeView, &point);

                TVHITTESTINFO thti = {};
                thti.pt = point;
                HTREEITEM item = TreeView_HitTest(hTreeView, &thti);

                if (!item) break;

                ItemType it = getItemType(item);
                switch (it)
                {
                case TYPE_FILTER:
                    SendMessage(hwnd, WM_COMMAND, ID_RENAME_FILTER, 0);
                    return TRUE;
                case TYPE_FILE:
                    openFile(item);
                    return TRUE;
                default: break;
                }
                return TRUE;
            }
            case TVN_BEGINDRAG:
            {
                LPNMTREEVIEW lpnmtv = (LPNMTREEVIEW)lParam;
                g_dragItem = lpnmtv->itemNew.hItem;
                g_dragStartPt = lpnmtv->ptDrag;
                g_dragActive = true;
                g_isDragging = false;
                TreeView_SelectItem(hTreeView, g_dragItem);
                SetFocus(hTreeView);
                return 0;
            }
            case NM_RCLICK:
            {
                POINT point;
                GetCursorPos(&point);
                POINT clientPt = point;
                ScreenToClient(hTreeView, &clientPt);

                TVHITTESTINFO thti = {};
                thti.pt = clientPt;
                HTREEITEM hItemClicked = TreeView_HitTest(hTreeView, &thti);

                if (hItemClicked)
                {
                    TreeView_SelectItem(hTreeView, hItemClicked);
                    ItemType type = getItemType(hItemClicked);

                    if (type == TYPE_FILTER)
                    {
                        EnableMenuItem(hFilterMenu, ID_ADD_CHILD_FILTER, MF_ENABLED);
                        EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_ENABLED);
                        EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_ENABLED);
                        EnableMenuItem(hFilterMenu, ID_DELETE_FILTER_AND_CHILDREN, MF_ENABLED);
                        EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_ENABLED);
                        EnableMenuItem(hFilterMenu, ID_SAVE_CONFIG, MF_ENABLED);
                        TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
                    }
                    else if (type == TYPE_FILE)
                    {
                        EnableMenuItem(hFilterMenu, ID_REMOVE_FILE, MF_ENABLED);
                        TrackPopupMenu(hFileMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
                    }
                }
                else
                {
                    unselectItem();
                    EnableMenuItem(hFilterMenu, ID_ADD_CHILD_FILTER, MF_ENABLED);
                    EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_GRAYED);
                    EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_GRAYED);
                    EnableMenuItem(hFilterMenu, ID_DELETE_FILTER_AND_CHILDREN, MF_GRAYED);
                    EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_GRAYED);
                    EnableMenuItem(hFilterMenu, ID_SAVE_CONFIG, MF_ENABLED);
                    TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
                }
                return TRUE;
            }

            case TVN_BEGINLABELEDIT:
            {
                LPNMTVDISPINFO pInfo = (LPNMTVDISPINFO)lParam;
                ItemType type = getItemType(pInfo->item.hItem);
                if (type == TYPE_FILE) return TRUE;
                return FALSE;
            }

            case TVN_ENDLABELEDIT:
            {
                LPNMTVDISPINFO pInfo = (LPNMTVDISPINFO)lParam;
                if (pInfo->item.pszText != NULL && pInfo->item.pszText[0] != L'\0')
                {
                    TVITEM item = pInfo->item;
                    item.mask = TVIF_TEXT | TVIF_HANDLE;
                    TreeView_SetItem(hTreeView, &item);
                    return TRUE;
                }
                return FALSE;
            }

            case TVN_DELETEITEM:
            {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                TreeItemData* data = (TreeItemData*)pnmtv->itemOld.lParam;
                if (data) delete data;
                break;
            }

            case NM_CUSTOMDRAW:
            {
                LPNMTVCUSTOMDRAW pcd = (LPNMTVCUSTOMDRAW)lParam;
                switch (pcd->nmcd.dwDrawStage)
                {
                case CDDS_PREPAINT:
                    return CDRF_NOTIFYITEMDRAW;
                case CDDS_ITEMPREPAINT:
                {
                    HTREEITEM hItem = (HTREEITEM)pcd->nmcd.dwItemSpec;
                    TreeItemData* data = getItemData(hItem);

                    if (data && data->type == TYPE_FILTER)
                        pcd->clrTextBk = RGB(235, 235, 235);
                    else
                        pcd->clrTextBk = RGB(255, 255, 255);

                    return CDRF_DODEFAULT;
                }
                }
                break;
            }
            // Removed TVN_KEYDOWN mapping block entirely
            }
        }
        break;
    }

    case WM_DESTROY:
    {
        if (hFilterMenu)
        {
            DestroyMenu(hFilterMenu);
            hFilterMenu = NULL;
        }
        if (hFileMenu)
        {
            DestroyMenu(hFileMenu);
            hFileMenu = NULL;
        }
        break;
    }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void deleteFilter(HTREEITEM item)
{
    if (!item) return;

    HTREEITEM parent = TreeView_GetParent(hTreeView, item);
    HTREEITEM child = TreeView_GetChild(hTreeView, item);

    while (child)
    {
        HTREEITEM next = TreeView_GetNextSibling(hTreeView, child);
        moveItemRecursive(child, parent);
        child = next;
    }

    TreeView_DeleteItem(hTreeView, item);
}

void insertItemBefore(HTREEITEM item, HTREEITEM target)
{
    HTREEITEM targetParent = TreeView_GetParent(hTreeView, target);
    HTREEITEM insertAfter = TVI_FIRST;
    HTREEITEM firstChild = (targetParent == NULL)
        ? TreeView_GetRoot(hTreeView)
        : TreeView_GetChild(hTreeView, targetParent);

    if (firstChild != target)
    {
        HTREEITEM sibling = firstChild;
        while (sibling)
        {
            HTREEITEM next = TreeView_GetNextSibling(hTreeView, sibling);
            if (next == target)
            {
                insertAfter = sibling;
                break;
            }
            sibling = next;
        }
    }

    HTREEITEM newItem = copyItemToPosition(item, targetParent, insertAfter);
    detachDataFromItemRecursive(item);
    TreeView_DeleteItem(hTreeView, item);
    TreeView_SelectItem(hTreeView, newItem);
}

void insertItemAfter(HTREEITEM item, HTREEITEM target)
{
    HTREEITEM targetParent = TreeView_GetParent(hTreeView, target);
    HTREEITEM insertAfter = target;

    HTREEITEM newItem = copyItemToPosition(item, targetParent, insertAfter);
    detachDataFromItemRecursive(item);
    TreeView_DeleteItem(hTreeView, item);
    TreeView_SelectItem(hTreeView, newItem);
}

HTREEITEM copyItemToPosition(HTREEITEM src, HTREEITEM dstParent, HTREEITEM insertAfter)
{
    wchar_t text[260];

    TVITEM tvi = {};
    tvi.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.hItem = src;
    tvi.pszText = text;
    tvi.cchTextMax = 260;
    TreeView_GetItem(hTreeView, &tvi);

    TVINSERTSTRUCT ins = {};
    ins.hParent = dstParent;
    ins.hInsertAfter = insertAfter;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM;
    ins.item.pszText = text;
    ins.item.lParam = tvi.lParam;

    HTREEITEM newItem = TreeView_InsertItem(hTreeView, &ins);

    HTREEITEM child = TreeView_GetChild(hTreeView, src);
    while (child)
    {
        copyItemRecursive(child, newItem);
        child = TreeView_GetNextSibling(hTreeView, child);
    }

    return newItem;
}