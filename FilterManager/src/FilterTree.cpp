#include "FilterTree.h"
#include "Notepadpp/Notepad_plus_msgs.h" //NPPM_GETFULLCURRENTPATH NPPM_DOOPEN

#pragma comment(lib, "comctl32.lib")

static const UINT_PTR TREEVIEW_SUBCLASS_ID = 1;

FilterTree::FilterTree() : hTree(nullptr), isDragging(false), dragActive(false), dragItem(nullptr) {}

FilterTree::~FilterTree() {
    destroy();
}

void FilterTree::create(HWND parent, int width, int height) {
    hTree = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT |
        TVS_HASBUTTONS | TVS_EDITLABELS | TVS_FULLROWSELECT | TVS_NOTOOLTIPS,
        0, 0, width, height, parent, nullptr, GetModuleHandle(nullptr), nullptr);

    SetWindowSubclass(hTree, TreeViewSubclassProc, TREEVIEW_SUBCLASS_ID, (DWORD_PTR)this);
}

void FilterTree::destroy() {
    if (hTree) {
        RemoveWindowSubclass(hTree, TreeViewSubclassProc, TREEVIEW_SUBCLASS_ID);
        hTree = nullptr;
    }
}

HWND FilterTree::getHandle() const { return hTree; }

HTREEITEM FilterTree::selection() const { return TreeView_GetSelection(hTree); }

TreeItemData* FilterTree::getItemData(HTREEITEM item) const {
    if (!item) return nullptr;
    TVITEM tvi = {};
    tvi.mask = TVIF_PARAM | TVIF_HANDLE;
    tvi.hItem = item;
    return TreeView_GetItem(hTree, &tvi) ? (TreeItemData*)tvi.lParam : nullptr;
}

ItemType FilterTree::getItemType(HTREEITEM item) const {
    TreeItemData* d = getItemData(item);
    return d ? d->type : TYPE_FILTER;
}

std::wstring FilterTree::getItemText(HTREEITEM item) const {
    wchar_t buf[MAX_PATH] = {};
    TVITEM tvi = {};
    tvi.hItem = item;
    tvi.mask = TVIF_TEXT;
    tvi.pszText = buf;
    tvi.cchTextMax = MAX_PATH;
    TreeView_GetItem(hTree, &tvi);
    return buf;
}

HTREEITEM FilterTree::addFilter(HTREEITEM parent) {
    TreeItemData* data = new TreeItemData{ TYPE_FILTER };
    wchar_t label[] = L"New Filter";

    TVINSERTSTRUCT tvis = {};
    tvis.hParent = parent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = label;
    tvis.item.lParam = (LPARAM)data;

    HTREEITEM newItem = TreeView_InsertItem(hTree, &tvis);
    if (parent && parent != TVI_ROOT) TreeView_Expand(hTree, parent, TVE_EXPAND);
    if (newItem) TreeView_SelectItem(hTree, newItem);
    return newItem;
}

void FilterTree::addCurrentDocument(HTREEITEM filter, HWND nppHandle, HWND msgParent) {
    wchar_t filePath[MAX_PATH] = {};
    ::SendMessage(nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)filePath);

    if (!filePath[0]) {
        MessageBox(msgParent, L"No document is currently open!", L"Error", MB_OK | MB_ICONWARNING);
        return;
    }

    wchar_t* fileName = wcsrchr(filePath, L'\\');
    if (!fileName) fileName = wcsrchr(filePath, L'/');
    fileName = fileName ? fileName + 1 : filePath;

    TreeItemData* data = new TreeItemData{ TYPE_FILE, filePath };
    TVINSERTSTRUCT tvis = {};
    tvis.hParent = filter;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = fileName;
    tvis.item.lParam = (LPARAM)data;

    TreeView_InsertItem(hTree, &tvis);
    TreeView_Expand(hTree, filter, TVE_EXPAND);
}

void FilterTree::deleteFilter(HTREEITEM item) {
    if (!item) return;
    HTREEITEM parent = TreeView_GetParent(hTree, item);
    HTREEITEM child = TreeView_GetChild(hTree, item);
    while (child) {
        HTREEITEM next = TreeView_GetNextSibling(hTree, child);
        moveItem(child, parent ? parent : TVI_ROOT);
        child = next;
    }
    TreeView_DeleteItem(hTree, item);
}

void FilterTree::renameItem(HTREEITEM item) {
    TreeView_SelectItem(hTree, item);
    TreeView_EnsureVisible(hTree, item);
    SetFocus(hTree);
    TreeView_EditLabel(hTree, item);
}

void FilterTree::openFile(HTREEITEM item, HWND nppHandle) {
    TreeItemData* data = getItemData(item);
    if (data && data->type == TYPE_FILE) {
        ::SendMessage(nppHandle, NPPM_DOOPEN, 0, (LPARAM)data->filePath.c_str());
    }
}

void FilterTree::moveItem(HTREEITEM item, HTREEITEM newParent) {
    HTREEITEM newItem = copyItem(item, newParent);
    detachData(item);
    TreeView_DeleteItem(hTree, item);
    TreeView_Expand(hTree, newParent, TVE_EXPAND);
    TreeView_SelectItem(hTree, newItem);
}

void FilterTree::moveItemBefore(HTREEITEM item, HTREEITEM target) {
    HTREEITEM targetParent = TreeView_GetParent(hTree, target);
    HTREEITEM insertAfter = TVI_FIRST;
    HTREEITEM first = targetParent ? TreeView_GetChild(hTree, targetParent) : TreeView_GetRoot(hTree);

    if (first != target) {
        for (HTREEITEM s = first; s; s = TreeView_GetNextSibling(hTree, s)) {
            HTREEITEM next = TreeView_GetNextSibling(hTree, s);
            if (next == target) { insertAfter = s; break; }
        }
    }
    HTREEITEM newItem = copyItem(item, targetParent, insertAfter);
    detachData(item);
    TreeView_DeleteItem(hTree, item);
    TreeView_SelectItem(hTree, newItem);
}

void FilterTree::moveItemAfter(HTREEITEM item, HTREEITEM target) {
    HTREEITEM targetParent = TreeView_GetParent(hTree, target);
    HTREEITEM newItem = copyItem(item, targetParent, target);
    detachData(item);
    TreeView_DeleteItem(hTree, item);
    TreeView_SelectItem(hTree, newItem);
}

HTREEITEM FilterTree::copyItem(HTREEITEM src, HTREEITEM dstParent, HTREEITEM insertAfter) {
    wchar_t text[MAX_PATH] = {};
    TVITEM tvi = {};
    tvi.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.hItem = src;
    tvi.pszText = text;
    tvi.cchTextMax = MAX_PATH;
    TreeView_GetItem(hTree, &tvi);

    TVINSERTSTRUCT ins = {};
    ins.hParent = dstParent;
    ins.hInsertAfter = insertAfter;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM;
    ins.item.pszText = text;
    ins.item.lParam = tvi.lParam;

    HTREEITEM newItem = TreeView_InsertItem(hTree, &ins);
    for (HTREEITEM c = TreeView_GetChild(hTree, src); c; c = TreeView_GetNextSibling(hTree, c)) {
        copyItem(c, newItem);
    }
    return newItem;
}

void FilterTree::detachData(HTREEITEM item) {
    TVITEM tvi = {};
    tvi.mask = TVIF_PARAM;
    tvi.hItem = item;
    tvi.lParam = 0;
    TreeView_SetItem(hTree, &tvi);
    for (HTREEITEM c = TreeView_GetChild(hTree, item); c; c = TreeView_GetNextSibling(hTree, c)) {
        detachData(c);
    }
}

LRESULT CALLBACK FilterTree::TreeViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    FilterTree* tree = (FilterTree*)dwRefData;

    switch (uMsg) {
    case WM_MOUSEMOVE: {
        if (!tree->dragActive || !tree->dragItem) break;
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        if (!tree->isDragging) {
            if (abs(pt.x - tree->dragStartPt.x) > 5 || abs(pt.y - tree->dragStartPt.y) > 5) {
                tree->isDragging = true;
                SetCapture(hWnd);
            }
        }

        if (!tree->isDragging) break;

        TVHITTESTINFO hit = {};
        hit.pt = pt;
        TreeView_HitTest(hWnd, &hit);

        if (hit.hItem && hit.hItem != tree->dragItem) {
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
            else if (tree->getItemType(hit.hItem) == TYPE_FILTER) {
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
        if (!tree->dragActive) break;

        if (tree->isDragging) {
            ReleaseCapture();
            TreeView_SelectDropTarget(hWnd, nullptr);
            TreeView_SetInsertMark(hWnd, nullptr, FALSE);

            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            TVHITTESTINFO hit = {};
            hit.pt = pt;
            TreeView_HitTest(hWnd, &hit);

            if (hit.hItem && hit.hItem != tree->dragItem) {
                RECT r;
                TreeView_GetItemRect(hWnd, hit.hItem, &r, FALSE);
                int h = r.bottom - r.top;
                int rel = pt.y - r.top;

                if (rel < h / 3) tree->moveItemBefore(tree->dragItem, hit.hItem);
                else if (rel > h * 2 / 3) tree->moveItemAfter(tree->dragItem, hit.hItem);
                else if (tree->getItemType(hit.hItem) == TYPE_FILTER) tree->moveItem(tree->dragItem, hit.hItem);
                else tree->moveItemAfter(tree->dragItem, hit.hItem);
            }
        }

        tree->dragItem = nullptr;
        tree->isDragging = false;
        tree->dragActive = false;
        break;
    }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT FilterTree::onCustomDraw(LPNMTVCUSTOMDRAW pcd) {
    if (pcd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (pcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        TreeItemData* data = getItemData((HTREEITEM)pcd->nmcd.dwItemSpec);
        pcd->clrTextBk = (data && data->type == TYPE_FILTER) ? RGB(235, 235, 235) : RGB(255, 255, 255);
        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

LRESULT FilterTree::handleNotify(LPNMHDR pnmh) {
    switch (pnmh->code) {
    case TVN_BEGINDRAG: {
        LPNMTREEVIEW lpnmtv = (LPNMTREEVIEW)pnmh;
        dragItem = lpnmtv->itemNew.hItem;
        dragStartPt = lpnmtv->ptDrag;
        dragActive = true;
        isDragging = false;
        TreeView_SelectItem(hTree, dragItem);
        SetFocus(hTree);
        return 0;
    }
    case TVN_BEGINLABELEDIT: {
        LPNMTVDISPINFO pInfo = (LPNMTVDISPINFO)pnmh;
        return getItemType(pInfo->item.hItem) == TYPE_FILE ? TRUE : FALSE;
    }
    case TVN_ENDLABELEDIT: {
        LPNMTVDISPINFO pInfo = (LPNMTVDISPINFO)pnmh;
        if (pInfo->item.pszText && pInfo->item.pszText[0] != L'\0') {
            TVITEM item = pInfo->item;
            item.mask = TVIF_TEXT | TVIF_HANDLE;
            TreeView_SetItem(hTree, &item);
            return TRUE;
        }
        return FALSE;
    }
    case TVN_DELETEITEM: {
        TreeItemData* data = (TreeItemData*)((LPNMTREEVIEW)pnmh)->itemOld.lParam;
        delete data;
        return 0;
    }
    case NM_CUSTOMDRAW:
        return onCustomDraw((LPNMTVCUSTOMDRAW)pnmh);
    }
    return 0;
}