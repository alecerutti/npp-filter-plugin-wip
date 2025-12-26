//this file is part of notepad++
//Copyright (C)2022 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"

#include "DockingFeature/Docking.h"
#include <string>
#include <fstream>
#include <windowsx.h>

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;


// Tipo di item nel TreeView
enum ItemType
{
	TYPE_FILTER = 0,
	TYPE_FILE = 1
};

// Struttura dati per ogni item del TreeView
struct TreeItemData
{
	ItemType type;
	std::wstring filePath; // Solo per TYPE_FILE
};

enum MenuCommands
{
	ID_ADD_FILTER = 1001,
	ID_RENAME_FILTER = 1002,
	ID_DELETE_FILTER = 1003,
	ID_ADD_CURRENT_DOC = 1004,
	ID_REMOVE_FILE = 1005
};

// Finestra contenitore
HWND hContainer = NULL;
// TreeView
HWND hTreeView = NULL;
// Menu popup per filtri
HMENU hFilterMenu = NULL;
// Menu popup per file
HMENU hFileMenu = NULL;

tTbData myDock = { 0 };

//std::ofstream fileOut("c:\\users\\user\\downloads\\debug.txt");

const wchar_t* CONTAINER_CLASS = L"FilterManagerContainer";

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE /*hModule*/)
{
	// Creazione menu popup (non dipendono dalla finestra)
	hFilterMenu = CreatePopupMenu();
	AppendMenu(hFilterMenu, MF_STRING, ID_ADD_FILTER, L"Add Filter");
	AppendMenu(hFilterMenu, MF_STRING, ID_RENAME_FILTER, L"Rename Filter");
	AppendMenu(hFilterMenu, MF_STRING, ID_DELETE_FILTER, L"Delete Filter");
	AppendMenu(hFilterMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(hFilterMenu, MF_STRING, ID_ADD_CURRENT_DOC, L"Add Current Document");

	hFileMenu = CreatePopupMenu();
	AppendMenu(hFileMenu, MF_STRING, ID_REMOVE_FILE, L"Remove");

	// Inizializzazione dati del docking (solo struttura)
	ZeroMemory(&myDock, sizeof(tTbData));
	myDock.pszName = L"Filter Manager";
	myDock.dlgID = 0;
	myDock.uMask = 0;
	myDock.pszAddInfo = nullptr;
	myDock.hIconTab = nullptr;
	myDock.pszModuleName = L"MyPlugin.dll";

	// loadXmlConfiguration();
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
	// TODO save xml
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{
	ShortcutKey sk;
	sk._isCtrl = true;
	sk._isAlt = true;
	sk._isShift = true;
	sk._key = 'M';

	setCommand(0, TEXT("Show/Hide Panel"), panel, &sk, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
}

//
// This function help you to initialize your plugin commands
//
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

//----------------------------------------------//
//-- DEFINIZIONI --//
//----------------------------------------------//
// Forward declarations
LRESULT CALLBACK ContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void addRootFilter();
void addChildFilter(HTREEITEM parent);
void deleteFilter(HTREEITEM item);
void renameFilter(HTREEITEM item);
void addCurrentDocument(HTREEITEM filterItem);
void removeFile(HTREEITEM fileItem);
void openFile(HTREEITEM fileItem);
HTREEITEM getSelectedItem();
TreeItemData* getItemData(HTREEITEM item);
ItemType getItemType(HTREEITEM item);

void panel()
{
	if (!hContainer)
	{
		/* Callback for container class */
		WNDCLASSEX wc = { 0 };
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.lpfnWndProc = ContainerProc;
		wc.hInstance = GetModuleHandle(nullptr);
		wc.lpszClassName = CONTAINER_CLASS;
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		RegisterClassEx(&wc);

		/* Container */
		hContainer = CreateWindowEx(
			0,
			CONTAINER_CLASS,
			L"",
			WS_CHILD | WS_VISIBLE,
			0, 0, 300, 400,
			nppData._nppHandle,
			nullptr,
			GetModuleHandle(nullptr),
			nullptr
		);

		/* TreeView within container */
		hTreeView = CreateWindowEx(
			WS_EX_CLIENTEDGE,
			WC_TREEVIEW,
			L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_EDITLABELS | TVS_FULLROWSELECT,
			0, 0, 300, 400,
			hContainer,
			nullptr,
			GetModuleHandle(nullptr),
			nullptr
		);

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

void addRootFilter()
{
	TreeItemData* data = new TreeItemData();
	data->type = TYPE_FILTER;

	TVINSERTSTRUCT tvis = {};
	tvis.hParent = TVI_ROOT;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
	tvis.item.pszText = L"New Filter";
	tvis.item.lParam = (LPARAM)data;

	TreeView_InsertItem(hTreeView, &tvis);
}

void addChildFilter(HTREEITEM parent)
{
	TreeItemData* data = new TreeItemData();
	data->type = TYPE_FILTER;

	TVINSERTSTRUCT tvis = {};
	tvis.hParent = parent;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
	tvis.item.pszText = L"New Filter";
	tvis.item.lParam = (LPARAM)data;

	TreeView_InsertItem(hTreeView, &tvis);
	TreeView_Expand(hTreeView, parent, TVE_EXPAND);
}

// TODO remove
void deleteItemRecursive(HTREEITEM item)
{
	HTREEITEM child = TreeView_GetChild(hTreeView, item);
	while (child)
	{
		HTREEITEM next = TreeView_GetNextSibling(hTreeView, child);
		deleteItemRecursive(child);
		child = next;
	}

	TreeItemData* data = getItemData(item);
	if (data)
	{
		delete data;
	}
}

void deleteFilter(HTREEITEM item)
{
	deleteItemRecursive(item);
	TreeView_DeleteItem(hTreeView, item);
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
	/* Get path of doc open in Notepad++ */
	wchar_t filePath[MAX_PATH] = { 0 };
	::SendMessage(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)filePath);

	if (wcslen(filePath) == 0)
	{
		MessageBox(hContainer, L"No document is currently open!", L"Error", MB_OK | MB_ICONWARNING);
		return;
	}

	/* Extract name froma path */
	wchar_t* fileName = wcsrchr(filePath, L'\\');
	if (!fileName)
		fileName = wcsrchr(filePath, L'/');

	if (fileName)
		fileName++; /* Skip separator */
	else
		fileName = filePath;

	TreeItemData* data = new TreeItemData();
	data->type = TYPE_FILE;
	data->filePath = filePath;

	/* Insert file as child of filer */
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
	if (data)
	{
		delete data;
	}
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

TreeItemData* getItemData(HTREEITEM item)
{
	if (!item) return nullptr;

	TVITEM tvi = {};
	tvi.mask = TVIF_PARAM | TVIF_HANDLE;
	tvi.hItem = item;

	if (TreeView_GetItem(hTreeView, &tvi))
	{
		return (TreeItemData*)tvi.lParam;
	}

	return nullptr;
}

ItemType getItemType(HTREEITEM item)
{
	TreeItemData* data = getItemData(item);
	if (data)
		return data->type;

	return TYPE_FILTER;
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

	case WM_COMMAND:
	{
		int cmd = LOWORD(wParam);
		HTREEITEM item = getSelectedItem();

		if (cmd == ID_ADD_FILTER)
		{
			if (item)
			{
				addChildFilter(item);
			}
			else
			{
				addRootFilter();
			}
			return 0;
		}
		else if (cmd == ID_DELETE_FILTER)
		{
			if (item && getItemType(item) == TYPE_FILTER)
			{
				//deleteFilter(hItemClicked);
				TreeView_DeleteItem(hTreeView, item);
			}
			return 0;
		}
		else if (cmd == ID_RENAME_FILTER)
		{
			if (item && getItemType(item) == TYPE_FILTER)
			{
				renameFilter(item);
			}
			return 0;
		}
		else if (cmd == ID_ADD_CURRENT_DOC)
		{
			if (item && getItemType(item) == TYPE_FILTER)
			{
				addCurrentDocument(item);
			}
			return 0;
		}
		else if (cmd == ID_REMOVE_FILE)
		{
			if (item && getItemType(item) == TYPE_FILE)
			{
				//removeFile(hItemClicked);
				TreeView_DeleteItem(hTreeView, item);
			}
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
			case NM_CLICK:
			{
				DWORD pos = GetMessagePos();

				POINT pt = {
					GET_X_LPARAM(pos),
					GET_Y_LPARAM(pos)
				};

				ScreenToClient(hTreeView, &pt);

				TVHITTESTINFO ht = {};
				ht.pt = pt;

				HTREEITEM item = TreeView_HitTest(hTreeView, &ht);

				if (item)
				{
					TreeView_SelectItem(hTreeView, item);
					return TRUE;
				}

				break;
			}

			case NM_DBLCLK:
			{
				POINT point;
				GetCursorPos(&point);
				ScreenToClient(hTreeView, &point);

				TVHITTESTINFO thti = {};
				thti.pt = point;
				HTREEITEM item = TreeView_HitTest(hTreeView, &thti);

				if (item && getItemType(item) == TYPE_FILE)
				{
					openFile(item);
				}
				return TRUE;
			}

			case NM_RCLICK:
			{
				POINT point;
				GetCursorPos(&point);
				//ScreenToClient(hTreeView, &point);

				TVHITTESTINFO thti = {};
				thti.pt = point;
				HTREEITEM hItemClicked = TreeView_HitTest(hTreeView, &thti);

				if (hItemClicked)
				{
					TreeView_SelectItem(hTreeView, hItemClicked);

					/* Show different menu depending on item type */
					ItemType type = getItemType(hItemClicked);

					if (type == TYPE_FILTER)
					{
						EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_ENABLED);
						EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_ENABLED);
						EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_ENABLED);
						TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
					}
					else if (type == TYPE_FILE)
					{
						TrackPopupMenu(hFileMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
					}
				}
				else
				{
					/* Click on empty zone shows "Add Filter" only */
					EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_GRAYED);
					EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_GRAYED);
					EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_GRAYED);
					TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
				}
				return TRUE;
			}

			case TVN_BEGINLABELEDIT:
			{
				/* Allow renaming filters only */
				LPNMTVDISPINFO pInfo = (LPNMTVDISPINFO)lParam;
				ItemType type = getItemType(pInfo->item.hItem);

				if (type == TYPE_FILE)
				{
					return TRUE;
				}

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
				/* TODO: check if this is called for each item recursevly otherwise there is memory leak */
				LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
				TreeItemData* data = (TreeItemData*)pnmtv->itemOld.lParam;

				if (data)
				{
					delete data;
				}

				break;
			}

			case TVN_KEYDOWN:
			{
				LPNMTVKEYDOWN pKeyDown = (LPNMTVKEYDOWN)lParam;
				HTREEITEM item = getSelectedItem();

				if (!item) break;

				switch (pKeyDown->wVKey)
				{
				case 'R':
					if (getItemType(item) == TYPE_FILTER)
					{
						SendMessage(hwnd, WM_COMMAND, ID_RENAME_FILTER, 0);
					}
					return TRUE; /* Avoids default Windows beep */
				case VK_RETURN:
					if (getItemType(item) == TYPE_FILE)
					{
						openFile(item);
					}
					return TRUE;

				case VK_ESCAPE:
					TreeView_SelectItem(hTreeView, NULL);
					return TRUE;

				case VK_DELETE:
					/* Deleting filters is very dangerous and should be done via menu */
/*					if (getItemType(item) == TYPE_FILTER)
					{
						SendMessage(hwnd, WM_COMMAND, ID_DELETE_FILTER, 0);
					}
					else*/ if (getItemType(item) == TYPE_FILE)
					{
						SendMessage(hwnd, WM_COMMAND, ID_REMOVE_FILE, 0);
					}
					return TRUE;
				case VK_SPACE:
				{
					HTREEITEM root = TreeView_GetRoot(hTreeView);
					if (root)
						TreeView_SelectItem(hTreeView, root);
					return TRUE;
				}
				case 'F':
				{
					bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
					bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					bool isUpper = ((capsLockOn && !shiftPressed) || (!capsLockOn && shiftPressed));

					if (isUpper)
					{
						addRootFilter();
					}
					else
					{
						if (getItemType(item) == TYPE_FILTER)
						{
							SendMessage(hwnd, WM_COMMAND, ID_ADD_FILTER, 0);
						}
					}
					return TRUE;
				}
				case 'D':
					if (getItemType(item) == TYPE_FILTER)
					{
						SendMessage(hwnd, WM_COMMAND, ID_ADD_CURRENT_DOC, 0);
					}
					return TRUE;
				}
			}
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