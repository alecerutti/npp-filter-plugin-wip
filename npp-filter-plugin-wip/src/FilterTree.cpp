#include "FilterTree.h"
#include "PluginInterface.h"

TreeItemData* FilterTree::getItemData(HTREEITEM item) const {
    if (!item) return nullptr;
    TVITEM tvi = {};
    tvi.mask = TVIF_PARAM | TVIF_HANDLE;
    tvi.hItem = item;
    return TreeView_GetItem(hwnd, &tvi) ? (TreeItemData*)tvi.lParam : nullptr;
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
    TreeView_GetItem(hwnd, &tvi);
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

    HTREEITEM newItem = TreeView_InsertItem(hwnd, &tvis);
    if (parent && parent != TVI_ROOT)
        TreeView_Expand(hwnd, parent, TVE_EXPAND);
    if (newItem)
        TreeView_SelectItem(hwnd, newItem);
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

    TreeView_InsertItem(hwnd, &tvis);
    TreeView_Expand(hwnd, filter, TVE_EXPAND);
}

void FilterTree::deleteFilter(HTREEITEM item) {
    if (!item) return;
    HTREEITEM parent = TreeView_GetParent(hwnd, item);
    HTREEITEM child = TreeView_GetChild(hwnd, item);
    while (child) {
        HTREEITEM next = TreeView_GetNextSibling(hwnd, child);
        moveItem(child, parent ? parent : TVI_ROOT);
        child = next;
    }
    TreeView_DeleteItem(hwnd, item);
}

void FilterTree::renameItem(HTREEITEM item) {
    TreeView_SelectItem(hwnd, item);
    TreeView_EnsureVisible(hwnd, item);
    SetFocus(hwnd);
    TreeView_EditLabel(hwnd, item);
}

void FilterTree::openFile(HTREEITEM item, HWND nppHandle) {
    TreeItemData* data = getItemData(item);
    if (data && data->type == TYPE_FILE)
        ::SendMessage(nppHandle, NPPM_DOOPEN, 0, (LPARAM)data->filePath.c_str());
}

void FilterTree::moveItem(HTREEITEM item, HTREEITEM newParent) {
    HTREEITEM newItem = copyItem(item, newParent);
    detachData(item);
    TreeView_DeleteItem(hwnd, item);
    TreeView_Expand(hwnd, newParent, TVE_EXPAND);
    TreeView_SelectItem(hwnd, newItem);
}

void FilterTree::moveItemBefore(HTREEITEM item, HTREEITEM target) {
    HTREEITEM targetParent = TreeView_GetParent(hwnd, target);
    HTREEITEM insertAfter = TVI_FIRST;

    // Walk siblings to find the one just before target
    HTREEITEM first = targetParent
        ? TreeView_GetChild(hwnd, targetParent)
        : TreeView_GetRoot(hwnd);

    if (first != target) {
        for (HTREEITEM s = first; s; s = TreeView_GetNextSibling(hwnd, s)) {
            HTREEITEM next = TreeView_GetNextSibling(hwnd, s);
            if (next == target) { insertAfter = s; break; }
        }
    }

    HTREEITEM newItem = copyItem(item, targetParent, insertAfter);
    detachData(item);
    TreeView_DeleteItem(hwnd, item);
    TreeView_SelectItem(hwnd, newItem);
}

void FilterTree::moveItemAfter(HTREEITEM item, HTREEITEM target) {
    HTREEITEM targetParent = TreeView_GetParent(hwnd, target);
    HTREEITEM newItem = copyItem(item, targetParent, target);
    detachData(item);
    TreeView_DeleteItem(hwnd, item);
    TreeView_SelectItem(hwnd, newItem);
}

void FilterTree::expandAll() {
    for (HTREEITEM item = TreeView_GetRoot(hwnd); item; item = TreeView_GetNextSibling(hwnd, item))
        expandRecursive(item);
}

void FilterTree::collapseAll() {
    for (HTREEITEM item = TreeView_GetRoot(hwnd); item; item = TreeView_GetNextSibling(hwnd, item))
        collapseRecursive(item);
}

void FilterTree::expandRecursive(HTREEITEM item) {
    if (!item) return;
    for (HTREEITEM c = TreeView_GetChild(hwnd, item); c; c = TreeView_GetNextSibling(hwnd, c))
        expandRecursive(c);
    TreeView_Expand(hwnd, item, TVE_EXPAND);
}

void FilterTree::collapseRecursive(HTREEITEM item) {
    if (!item) return;
    for (HTREEITEM c = TreeView_GetChild(hwnd, item); c; c = TreeView_GetNextSibling(hwnd, c))
        collapseRecursive(c);
    TreeView_Expand(hwnd, item, TVE_COLLAPSE);
}

// Deep-copies src under dstParent, inserted after insertAfter (TVI_LAST by default).
HTREEITEM FilterTree::copyItem(HTREEITEM src, HTREEITEM dstParent, HTREEITEM insertAfter) {
    wchar_t text[MAX_PATH] = {};
    TVITEM tvi = {};
    tvi.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.hItem = src;
    tvi.pszText = text;
    tvi.cchTextMax = MAX_PATH;
    TreeView_GetItem(hwnd, &tvi);

    TVINSERTSTRUCT ins = {};
    ins.hParent = dstParent;
    ins.hInsertAfter = insertAfter;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM;
    ins.item.pszText = text;
    ins.item.lParam = tvi.lParam;

    HTREEITEM newItem = TreeView_InsertItem(hwnd, &ins);
    for (HTREEITEM c = TreeView_GetChild(hwnd, src); c; c = TreeView_GetNextSibling(hwnd, c))
        copyItem(c, newItem);

    return newItem;
}

// Zeroes lParam recursively so TVN_DELETEITEM won't double-free during a move.
void FilterTree::detachData(HTREEITEM item) {
    TVITEM tvi = {};
    tvi.mask = TVIF_PARAM;
    tvi.hItem = item;
    tvi.lParam = 0;
    TreeView_SetItem(hwnd, &tvi);
    for (HTREEITEM c = TreeView_GetChild(hwnd, item); c; c = TreeView_GetNextSibling(hwnd, c))
        detachData(c);
}