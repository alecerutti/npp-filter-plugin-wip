#pragma once
#include <string>
#include <Windows.h>
#include <commctrl.h>

enum ItemType { TYPE_FILTER = 0, TYPE_FILE = 1 };

struct TreeItemData {
    ItemType     type;
    std::wstring filePath;
};

// Wraps an HWND treeview and all operations on it.
class FilterTree {
public:
    HWND hwnd = nullptr;

    // Drag state — managed by the subclass proc
    HTREEITEM dragItem = nullptr;
    bool      isDragging = false;
    bool      dragActive = false;
    POINT     dragStartPt = {};

    // Queries
    TreeItemData* getItemData(HTREEITEM item) const;
    ItemType      getItemType(HTREEITEM item) const;
    std::wstring  getItemText(HTREEITEM item) const;
    HTREEITEM     selection() const { return TreeView_GetSelection(hwnd); }

    // Operations
    HTREEITEM addFilter(HTREEITEM parent);   // pass TVI_ROOT for a root-level filter
    void      addCurrentDocument(HTREEITEM filter, HWND nppHandle, HWND msgParent);
    void      deleteFilter(HTREEITEM item);  // promotes children, then deletes the node
    void      renameItem(HTREEITEM item);
    void      openFile(HTREEITEM item, HWND nppHandle);

    // Move (used by drag-drop)
    void moveItem(HTREEITEM item, HTREEITEM newParent);
    void moveItemBefore(HTREEITEM item, HTREEITEM target);
    void moveItemAfter(HTREEITEM item, HTREEITEM target);

    // Expand / collapse all nodes
    void expandAll();
    void collapseAll();

private:
    HTREEITEM copyItem(HTREEITEM src, HTREEITEM dstParent, HTREEITEM insertAfter = TVI_LAST);
    void      detachData(HTREEITEM item);  // zeroes lParam without freeing (for move operations)
    void      expandRecursive(HTREEITEM item);
    void      collapseRecursive(HTREEITEM item);
};