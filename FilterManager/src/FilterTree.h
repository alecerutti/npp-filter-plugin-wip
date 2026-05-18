#pragma once

#include "defs.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

class FilterTree {
public:
    FilterTree();
    ~FilterTree();

    void create(HWND parent, int width, int height);
    void destroy();
    HWND getHandle() const;

    // Tree operations
    HTREEITEM addFilter(HTREEITEM parent);
    void addCurrentDocument(HTREEITEM filter, HWND nppHandle, HWND msgParent);
    void deleteFilter(HTREEITEM item);
    void renameItem(HTREEITEM item);
    void openFile(HTREEITEM item, HWND nppHandle);

    HTREEITEM selection() const;
    ItemType getItemType(HTREEITEM item) const;
    TreeItemData* getItemData(HTREEITEM item) const;
    std::wstring getItemText(HTREEITEM item) const;

    // Move operations
    void moveItem(HTREEITEM item, HTREEITEM newParent);
    void moveItemBefore(HTREEITEM item, HTREEITEM target);
    void moveItemAfter(HTREEITEM item, HTREEITEM target);

    LRESULT handleNotify(LPNMHDR pnmh);

private:
    HWND hTree;
    bool isDragging;
    bool dragActive;
    HTREEITEM dragItem;
    POINT dragStartPt;

    static LRESULT CALLBACK TreeViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    LRESULT onCustomDraw(LPNMTVCUSTOMDRAW pcd);

    HTREEITEM copyItem(HTREEITEM src, HTREEITEM dstParent, HTREEITEM insertAfter = TVI_LAST);
    void detachData(HTREEITEM item);
};