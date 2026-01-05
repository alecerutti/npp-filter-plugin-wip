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
#include "ConfigurationManager.h"
#pragma comment(lib, "comctl32.lib")

std::string configPath = "c:\\users\\user\\downloads\\filtermanager.xml";
HTREEITEM g_dragItem = NULL;
bool g_isDragging = false;
bool g_dragActive = false;
POINT g_dragStartPt = {};
const UINT_PTR TREEVIEW_SUBCLASS_ID = 1;

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;


// Tipo di item nel TreeView

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

	//// configuration file path
	//wchar_t configDir[MAX_PATH];
	//::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);
	//
	//std::wstring configPathW = configDir;
	//configPathW += L"\\FilterManager.xml";

	//// Converti in std::string se necessario
	//configPath = convertWStringToUtf8(configPathW);
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
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

	WCHAR cmdName[] = L"Show/Hide Panel";
	setCommand(0, cmdName, panel, &sk, false);
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
// declarations
LRESULT CALLBACK ContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TreeViewSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
HTREEITEM addRootFilter();
HTREEITEM addChildFilter(HTREEITEM parent);
void deleteFilter(HTREEITEM item);
void renameFilter(HTREEITEM item);
void addCurrentDocument(HTREEITEM filterItem);
void removeFile(HTREEITEM fileItem);
void openFile(HTREEITEM fileItem);
HTREEITEM getSelectedItem();
TreeItemData* getItemData(HTREEITEM item);
ItemType getItemType(HTREEITEM item);

// Aggiungi queste funzioni helper prima di pluginInit()

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

		SetWindowSubclass(hTreeView, TreeViewSubclassProc, TREEVIEW_SUBCLASS_ID, 0);

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

void expandNode(HTREEITEM item)
{
	if (!item) return;

	TreeView_Expand(hTreeView, item, TVE_EXPAND);
}

void collapseNode(HTREEITEM item)
{
	if (!item) return;

	TreeView_Expand(hTreeView, item, TVE_COLLAPSE);
}

void expandAllNodes()
{
	HTREEITEM item = TreeView_GetRoot(hTreeView);

	while (item)
	{
		expandNode(item);
		item = TreeView_GetNextSibling(hTreeView, item);
	}
}

void collapseAllNodes()
{
	HTREEITEM item = TreeView_GetRoot(hTreeView);

	while (item)
	{
		collapseNode(item);
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

void unselectItem()
{
	TreeView_SelectItem(hTreeView, NULL);
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
				// Calcola la distanza dal punto di inizio
				int dx = abs(pt.x - g_dragStartPt.x);
				int dy = abs(pt.y - g_dragStartPt.y);

				if (dx > 5 || dy > 5)
				{
					g_isDragging = true;
					SetCapture(hWnd); // Cattura il mouse sulla TreeView
				}
			}

			if (g_isDragging)
			{
				TVHITTESTINFO hit = {};
				hit.pt = pt;
				TreeView_HitTest(hWnd, &hit);
				TreeView_SelectDropTarget(hWnd, hit.hItem);
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

				POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				TVHITTESTINFO hit = {};
				hit.pt = pt;
				TreeView_HitTest(hWnd, &hit);

				if (hit.hItem && hit.hItem != g_dragItem)
				{
					if (getItemType(hit.hItem) == TYPE_FILTER)
					{
						// Chiamata alla tua funzione di spostamento logico
						moveItemRecursive(g_dragItem, hit.hItem);
					}
				}
			}

			// Reset stati
			g_dragItem = NULL;
			g_isDragging = false;
			g_dragActive = false;
		}
		break;
	}

	case WM_NCDESTROY:
		// Rimuovi il subclass quando la finestra viene distrutta
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
	// 1. Disabilitiamo temporaneamente la pulizia automatica dei dati
	// Oppure, più semplicemente, resettiamo l'lParam dell'item originale
	// in modo che TreeView_DeleteItem non trovi nulla da cancellare.

	// Scolleghiamo i dati dall'item originale prima di eliminarlo
	// Dobbiamo farlo ricorsivamente per tutto il ramo se vogliamo essere sicuri.

	HTREEITEM newItem = copyItemRecursive(item, newParent);

	// Per evitare che TVN_DELETEITEM cancelli i dati che abbiamo appena "passato"
	// al nuovo item, dobbiamo resettare i parametri dell'item vecchio.
	detachDataFromItemRecursive(item);

	TreeView_DeleteItem(hTreeView, item);

	TreeView_Expand(hTreeView, newParent, TVE_EXPAND);
	TreeView_SelectItem(hTreeView, newItem);
}

// Funzione helper per "staccare" i puntatori senza cancellarli
void detachDataFromItemRecursive(HTREEITEM item)
{
	TVITEM tvi = {};
	tvi.mask = TVIF_PARAM;
	tvi.hItem = item;
	tvi.lParam = 0; // Reset
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

		if (cmd == ID_ADD_FILTER)
		{
			if (item)
			{
				item = addChildFilter(item);
			}
			else
			{
				item = addRootFilter();
			}

			if (item) renameFilter(item);

			return 0;
		}
		else if (cmd == ID_DELETE_FILTER)
		{
			if (item && getItemType(item) == TYPE_FILTER)
			{
				int res = MessageBox(
					hContainer,
					L"Are you sure to delete the selected filter and all its subfilters?",
					L"Confirm deletion",
					MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
				);

				if (res == IDYES)
				{
					HTREEITEM parent = TreeView_GetParent(hTreeView, item);

					/* Restore focus */
					SetFocus(hTreeView);

					if (parent)
					{
						TreeView_SelectItem(hTreeView, parent);
					}
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
			case TVN_BEGINDRAG:
			{
				LPNMTREEVIEW lpnmtv = (LPNMTREEVIEW)lParam;

				// Inizializza lo stato globale del drag
				g_dragItem = lpnmtv->itemNew.hItem;
				g_dragStartPt = lpnmtv->ptDrag; // Coordinate relative alla TreeView
				g_dragActive = true;
				g_isDragging = false;

				// Seleziona l'item che si sta trascinando per feedback visivo
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
					unselectItem();
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
				case 'D':

					if (getItemType(item) == TYPE_FILTER)
					{
						SendMessage(hwnd, WM_COMMAND, ID_DELETE_FILTER, 0);
					}
					else if (getItemType(item) == TYPE_FILE)
					{
						SendMessage(hwnd, WM_COMMAND, ID_REMOVE_FILE, 0);
					}

					return TRUE;
				case VK_SPACE:
				{
					/* SPACE goes to first element of the tree; MAIUSC + SPACE goes to the last element of the tree */

					bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
					bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					bool isUpper = ((capsLockOn && !shiftPressed) || (!capsLockOn && shiftPressed));

					HTREEITEM root = TreeView_GetRoot(hTreeView);

					if (isUpper)
					{
						HTREEITEM item = root;
						HTREEITEM last = NULL;

						while (item)
						{
							last = item;
							item = TreeView_GetNextSibling(hTreeView, item);
						}

						if (last) TreeView_SelectItem(hTreeView, last);
					}
					else
					{

						if (root) TreeView_SelectItem(hTreeView, root);
					}

					return TRUE;
				}
				case 'F':
				{
					bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
					bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					bool isUpper = ((capsLockOn && !shiftPressed) || (!capsLockOn && shiftPressed));

					if (isUpper)
					{
						unselectItem();
						SendMessage(hwnd, WM_COMMAND, ID_ADD_FILTER, 0);
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
				case 'A':
					if (getItemType(item) == TYPE_FILTER)
					{
						SendMessage(hwnd, WM_COMMAND, ID_ADD_CURRENT_DOC, 0);
					}
					return TRUE;

					/* Vim motions */
				case 'H':
					SendMessage(hTreeView, WM_KEYDOWN, VK_LEFT, 0);
					return TRUE;

				case 'L':
					SendMessage(hTreeView, WM_KEYDOWN, VK_RIGHT, 0);
					return TRUE;

				case 'K':
					SendMessage(hTreeView, WM_KEYDOWN, VK_UP, 0);
					return TRUE;

				case 'J':
					SendMessage(hTreeView, WM_KEYDOWN, VK_DOWN, 0);
					return TRUE;

				case 'E':
				case 'e':
				{
					if (getItemType(item) != TYPE_FILTER)
						return FALSE;

					bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
					bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					bool isUpper = ((capsLockOn && !shiftPressed) || (!capsLockOn && shiftPressed));

					if (isUpper)
					{
						expandAllNodes();
					}
					else
					{
						expandNode(item);
					}
					return TRUE;
				}
				case 'C':
				case 'c':
				{
					if (getItemType(item) != TYPE_FILTER)
						return FALSE;

					bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
					bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					bool isUpper = ((capsLockOn && !shiftPressed) || (!capsLockOn && shiftPressed));

					if (isUpper)
					{
						collapseAllNodes();
					}
					else
					{
						collapseNode(item);
					}
					return TRUE;
				}
				case 'S':
					saveTreeToXml(configPath, hTreeView);
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