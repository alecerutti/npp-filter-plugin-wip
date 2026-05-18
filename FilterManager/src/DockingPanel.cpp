#include "DockingPanel.h"
#include "ConfigManager.h"

DockingPanel::DockingPanel() :
    hPanel(nullptr),
    cmdId(0),
    hFilterMenu(nullptr),
    hFileMenu(nullptr),
    nppData(),
    dock()
{
}

DockingPanel::~DockingPanel() {
    if (hFilterMenu) DestroyMenu(hFilterMenu);
    if (hFileMenu) DestroyMenu(hFileMenu);
}

void DockingPanel::initMenus() {
    hFilterMenu = CreatePopupMenu();
    AppendMenu(hFilterMenu, MF_STRING, ID_ADD_CURRENT_DOC, L"Add Current Document");
    AppendMenu(hFilterMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hFilterMenu, MF_STRING, ID_ADD_CHILD_FILTER, L"Add Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_RENAME_FILTER, L"Rename Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_DELETE_FILTER, L"Delete Filter");
    AppendMenu(hFilterMenu, MF_STRING, ID_DELETE_FILTER_AND_CHILDREN, L"Delete Filter AND Children");
    //AppendMenu(hFilterMenu, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(hFilterMenu, MF_STRING, ID_SAVE_CONFIG, L"Save configuration"); // todo move in main plugin menu

    hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_REMOVE_FILE, L"Remove");
}

void DockingPanel::init(NppData nppDataIn, int cmdIdIn, const std::string& configPath) {
    nppData = nppDataIn;
    cmdId = cmdIdIn;
    currentConfigPath = configPath;
    initMenus();

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = staticWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"FilterManagerContainer";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassEx(&wc);

    hPanel = CreateWindowEx(0, L"FilterManagerContainer", L"", WS_CHILD | WS_VISIBLE,
        0, 0, 300, 400, nppData._nppHandle, nullptr, GetModuleHandle(nullptr), this);

    SetWindowLongPtr(hPanel, GWLP_USERDATA, (LONG_PTR)this);
    tree.create(hPanel, 300, 400);

    ConfigManager cm;
    cm.loadConfig(currentConfigPath, tree);

    dock = {};
    dock.hClient = hPanel;
    dock.pszName = L"Filter Manager";
    dock.dlgID = cmdId;
    dock.uMask = 0;
    dock.pszAddInfo = nullptr;
    dock.hIconTab = nullptr;
    dock.pszModuleName = L"FilterManager.dll";

    // register docking panel and forcing hide
    ::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&dock);
    ::SendMessage(nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)dock.hClient);
}

void DockingPanel::toggle() {
    UINT msg = isVisible() ? NPPM_DMMHIDE : NPPM_DMMSHOW;
    ::SendMessage(nppData._nppHandle, msg, 0, (LPARAM)hPanel);
}

bool DockingPanel::isVisible() const {
    return IsWindowVisible(hPanel);
}

void DockingPanel::saveConfiguration() {
    ConfigManager cm;
    cm.saveConfig(currentConfigPath, tree);
}

LRESULT CALLBACK DockingPanel::staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DockingPanel* panel = (DockingPanel*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (panel) return panel->wndProc(hwnd, msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT DockingPanel::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (tree.getHandle()) {
            SetWindowPos(tree.getHandle(), nullptr, 0, 0, rc.right, rc.bottom, SWP_NOZORDER);
        }
        return 0;
    }
    case WM_SETFOCUS: {
        if (!tree.selection()) {
            HTREEITEM root = TreeView_GetRoot(tree.getHandle());
            if (root) TreeView_SelectItem(tree.getHandle(), root);
        }
        return 0;
    }
    case WM_COMMAND: {
        int cmd = LOWORD(wParam);
        HTREEITEM item = tree.selection();

        switch (cmd) {
        case ID_ADD_CHILD_FILTER: {
            HTREEITEM parent = item ? item : TVI_ROOT;
            HTREEITEM newItem = tree.addFilter(parent);
            if (newItem) tree.renameItem(newItem);
            return 0;
        }
        case ID_RENAME_FILTER:
            if (item && tree.getItemType(item) == TYPE_FILTER) tree.renameItem(item);
            return 0;
        case ID_DELETE_FILTER:
            if (item && tree.getItemType(item) == TYPE_FILTER) tree.deleteFilter(item);
            return 0;
        case ID_DELETE_FILTER_AND_CHILDREN:
            if (item && tree.getItemType(item) == TYPE_FILTER) {
                int res = MessageBox(hwnd, L"Delete selected filter AND all its children?", L"Confirm deletion", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (res == IDYES) {
                    HTREEITEM parent = TreeView_GetParent(tree.getHandle(), item);
                    SetFocus(tree.getHandle());
                    HTREEITEM sel = parent ? parent : TreeView_GetRoot(tree.getHandle());
                    if (sel) TreeView_SelectItem(tree.getHandle(), sel);
                    TreeView_DeleteItem(tree.getHandle(), item);
                }
            }
            return 0;
        case ID_ADD_CURRENT_DOC:
            if (item && tree.getItemType(item) == TYPE_FILTER) tree.addCurrentDocument(item, nppData._nppHandle, hwnd);
            return 0;
        case ID_REMOVE_FILE:
            if (item && tree.getItemType(item) == TYPE_FILE) TreeView_DeleteItem(tree.getHandle(), item);
            return 0;
        //case ID_SAVE_CONFIG:
        //    saveConfiguration();
            //return 0;
        }
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR lpnmh = (LPNMHDR)lParam;
        if (lpnmh->hwndFrom == tree.getHandle()) {
            if (lpnmh->code == NM_DBLCLK) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(tree.getHandle(), &pt);
                TVHITTESTINFO hit = {};
                hit.pt = pt;
                HTREEITEM item = TreeView_HitTest(tree.getHandle(), &hit);
                if (item) {
                    if (tree.getItemType(item) == TYPE_FILTER) SendMessage(hwnd, WM_COMMAND, ID_RENAME_FILTER, 0);
                    else tree.openFile(item, nppData._nppHandle);
                }
                return TRUE;
            }
            if (lpnmh->code == NM_RCLICK) {
                POINT screenPt, clientPt;
                GetCursorPos(&screenPt);
                clientPt = screenPt;
                ScreenToClient(tree.getHandle(), &clientPt);

                TVHITTESTINFO hit = {};
                hit.pt = clientPt;
                HTREEITEM clicked = TreeView_HitTest(tree.getHandle(), &hit);

                if (clicked) {
                    TreeView_SelectItem(tree.getHandle(), clicked);
                    if (tree.getItemType(clicked) == TYPE_FILTER)
                        TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, hwnd, nullptr);
                    else
                        TrackPopupMenu(hFileMenu, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, hwnd, nullptr);
                }
                else {
                    TreeView_SelectItem(tree.getHandle(), nullptr);
                    EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_GRAYED);
                    EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_GRAYED);
                    EnableMenuItem(hFilterMenu, ID_DELETE_FILTER_AND_CHILDREN, MF_GRAYED);
                    EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_GRAYED);
                    TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, hwnd, nullptr);
                    EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_ENABLED);
                    EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_ENABLED);
                    EnableMenuItem(hFilterMenu, ID_DELETE_FILTER_AND_CHILDREN, MF_ENABLED);
                    EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_ENABLED);
                }
                return TRUE;
            }
            // Delegate other notifications (drag, edit, custom draw) to the tree
            return tree.handleNotify(lpnmh);
        }
        break;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}