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

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "comctl32.lib")
std::ofstream debug("c:\\users\\user\\downloads\\debug.txt");

HTREEITEM g_dragItem = NULL;
bool g_isDragging = false;
bool g_dragActive = false;
POINT g_dragStartPt = {};
const UINT_PTR TREEVIEW_SUBCLASS_ID = 1;
std::string configPath;

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
	ID_ADD_CHILD_FILTER = 1001,
	ID_RENAME_FILTER = 1002,
	ID_DELETE_FILTER = 1003,
	ID_DELETE_FILTER_AND_CHILDREN = 1004,
	ID_ADD_CURRENT_DOC = 1005,
	ID_REMOVE_FILE = 1006,
	ID_SAVE_CONFIG = 1007
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

	// Inizializzazione dati del docking (solo struttura)
	ZeroMemory(&myDock, sizeof(tTbData));
	myDock.pszName = L"Filter Manager";
	myDock.dlgID = 0;
	myDock.uMask = 0;
	myDock.pszAddInfo = nullptr;
	myDock.hIconTab = nullptr;
	myDock.pszModuleName = L"MyPlugin.dll";
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
	static ShortcutKey sk;
	sk._isCtrl = true;
	sk._isAlt = true;
	sk._isShift = false;
	sk._key = 'M';

	WCHAR cmdName[] = L"Show/Hide Panel";
	setCommand(0, cmdName, panel, &sk, false);

	WCHAR shortcutsName[] = L"Ugly shortcuts list";
	setCommand(1, shortcutsName, showShortcutsPopup, nullptr, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	if (hTreeView)
		saveTreeToXml(configPath, hTreeView);
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

				if (hit.hItem && hit.hItem != g_dragItem)
				{
					RECT itemRect;
					TreeView_GetItemRect(hWnd, hit.hItem, &itemRect, FALSE);

					int itemHeight = itemRect.bottom - itemRect.top;
					int relativeY = pt.y - itemRect.top;

					//bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
					//bool isTargetExpanded = (TreeView_GetItemState(hWnd, hit.hItem, TVIS_EXPANDED) & TVIS_EXPANDED);

					// Decide quale feedback mostrare
					//if (ctrlPressed || (getItemType(hit.hItem) == TYPE_FILTER && !isTargetExpanded))
					//{
					//	// CTRL premuto o filtro chiuso  mostra HIGHLIGHT (inserimento dentro)
					//	TreeView_SetInsertMark(hWnd, NULL, FALSE);
					//	TreeView_SelectDropTarget(hWnd, hit.hItem);
					//}
					//else 
					if (relativeY < itemHeight / 3)
					{
						// Terzo superiore  mostra LINEA PRIMA
						TreeView_SelectDropTarget(hWnd, NULL);
						TreeView_SetInsertMark(hWnd, hit.hItem, FALSE); // FALSE = prima
					}
					else if (relativeY > itemHeight * 2 / 3)
					{
						// Terzo inferiore  mostra LINEA DOPO
						TreeView_SelectDropTarget(hWnd, NULL);
						TreeView_SetInsertMark(hWnd, hit.hItem, TRUE); // TRUE = dopo
					}
					else
					{
						// Terzo centrale  mostra HIGHLIGHT (inserimento dentro, se è filtro)
						if (getItemType(hit.hItem) == TYPE_FILTER)
						{
							TreeView_SetInsertMark(hWnd, NULL, FALSE);
							TreeView_SelectDropTarget(hWnd, hit.hItem);
						}
						else
						{
							// Se è un file, non fare nulla
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

				//TreeView_SelectDropTarget(hWnd, hit.hItem);
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
				TreeView_SetInsertMark(hWnd, NULL, FALSE); // <-- AGGIUNGI QUESTA RIGA

				POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				TVHITTESTINFO hit = {};
				hit.pt = pt;
				TreeView_HitTest(hWnd, &hit);

				if (hit.hItem && hit.hItem != g_dragItem)
				{
					// new
					RECT itemRect;
					TreeView_GetItemRect(hWnd, hit.hItem, &itemRect, FALSE);

					int itemHeight = itemRect.bottom - itemRect.top;
					int relativeY = pt.y - itemRect.top;

					if (relativeY < itemHeight / 3)
					{
						// Terzo superiore inserisci PRIMA (come fratello)
						insertItemBefore(g_dragItem, hit.hItem);
					}
					else if (relativeY > itemHeight * 2 / 3)
					{
						// Terzo inferiore inserisci DOPO (come fratello)
						insertItemAfter(g_dragItem, hit.hItem);
					}
					//
					else/*if(getItemType(hit.hItem) == TYPE_FILTER)*/
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

		if (cmd == ID_ADD_CHILD_FILTER)
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
				deleteFilter(item);
			}

			return 0;
		}
		else if (cmd == ID_DELETE_FILTER_AND_CHILDREN)
		{
			if (item && getItemType(item) == TYPE_FILTER)
			{
				int res = MessageBox(
					hContainer,
					L"Are you sure to delete the selected filter AND all its children?",
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
				/* Dobule click on file opens file, double click on filter renames filter */
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
				default:
					break;
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
					/* Click on empty zone*/
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
					{
						// Colore di sfondo per i filtri
						pcd->clrTextBk = RGB(235, 235, 235);
					}
					else
					{
						// File sfondo standard
						pcd->clrTextBk = RGB(255, 255, 255);
					}

					return CDRF_DODEFAULT;
				}
				}

				break;
			}

			case TVN_KEYDOWN:
			{
				LPNMTVKEYDOWN pKeyDown = (LPNMTVKEYDOWN)lParam;
				HTREEITEM item = getSelectedItem();

				if (!item) return TRUE;

				switch (pKeyDown->wVKey)
				{
				case 'R':
				{
					if (getItemType(item) == TYPE_FILTER)
					{
						SendMessage(hwnd, WM_COMMAND, ID_RENAME_FILTER, 0);
					}
				}
				return TRUE; /* Avoids default Windows beep */
				case VK_RETURN:
				{
					if (getItemType(item) == TYPE_FILE)
					{
						openFile(item);
						return TRUE;
					}
				}

				case VK_ESCAPE:
				{
					TreeView_SelectItem(hTreeView, NULL);
					return TRUE;
				}

				case VK_DELETE:
				case 'D':
				{
					if (getItemType(item) == TYPE_FILTER)
					{
						/* Delete filter only, to delete also children use the menu */
						SendMessage(hwnd, WM_COMMAND, ID_DELETE_FILTER, 0);
					}
					else if (getItemType(item) == TYPE_FILE)
					{
						SendMessage(hwnd, WM_COMMAND, ID_REMOVE_FILE, 0);
					}

					return TRUE;
				}
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
					/* Shift + F: adds sibling; F: adds child */
					bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
					bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					bool isUpper = ((capsLockOn && !shiftPressed) || (!capsLockOn && shiftPressed));

					if (getItemType(item) != TYPE_FILTER) return FALSE;

					if (isUpper)
					{
						//unselectItem();
						HTREEITEM parent = TreeView_GetParent(hTreeView, item);
						selectItem(parent);

						SendMessage(hwnd, WM_COMMAND, ID_ADD_CHILD_FILTER, 0);
					}
					else
					{
						SendMessage(hwnd, WM_COMMAND, ID_ADD_CHILD_FILTER, 0);
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
				{
					if (getItemType(item) != TYPE_FILTER)
						return FALSE;

					bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
					bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					bool isUpper = ((capsLockOn && !shiftPressed) || (!capsLockOn && shiftPressed));

					if (isUpper)
					{
						expandAllNodesRecursive();
					}
					else
					{
						expandNodeRecursive(item);
					}
					return TRUE;
				}
				case 'C':
				{
					if (getItemType(item) != TYPE_FILTER)
						return FALSE;

					bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
					bool capsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					bool isUpper = ((capsLockOn && !shiftPressed) || (!capsLockOn && shiftPressed));

					if (isUpper)
					{
						collapseAllNodesRecursive();
					}
					else
					{
						collapseNodeRecursive(item);
					}
					return TRUE;
				}
				case 'S':
					saveTreeToXml(configPath, hTreeView);
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

void showShortcutsPopup()
{
	const wchar_t* shortcutsText =
		L"Ctrl+Alt+Shift+M : Show/Hide Panel\n"
		L"R : Rename selected filter\n"
		L"Delete / D : Delete selected file or filter\n"
		L"A : Add current document to selected filter\n"
		L"F : Add new filter\n"
		L"Enter : Open selected file\n"
		L"Space : Select first item (Shift+Space = last item)\n"
		L"E / Shift+E : Expand node / expand all\n"
		L"C / Shift+C : Collapse node / collapse all\n"
		L"S : Save configuration\n"
		L"H / L / K / J : Vim-style navigation\n";

	MessageBox(nppData._nppHandle, shortcutsText, L"Plugin Shortcuts", MB_OK | MB_ICONINFORMATION);
}

void insertItemBefore(HTREEITEM item, HTREEITEM target)
{
	HTREEITEM targetParent = TreeView_GetParent(hTreeView, target);

	// Copia l'item e i suoi figli
	HTREEITEM newItem = copyItemToPosition(item, targetParent, target);

	// Elimina il vecchio item
	detachDataFromItemRecursive(item);
	TreeView_DeleteItem(hTreeView, item);

	// Seleziona il nuovo item
	TreeView_SelectItem(hTreeView, newItem);
}

void insertItemAfter(HTREEITEM item, HTREEITEM target)
{
	HTREEITEM targetParent = TreeView_GetParent(hTreeView, target);
	HTREEITEM insertAfter = target;

	// Copia l'item e i suoi figli
	HTREEITEM newItem = copyItemToPosition(item, targetParent, insertAfter);

	// Elimina il vecchio item
	detachDataFromItemRecursive(item);
	TreeView_DeleteItem(hTreeView, item);

	// Seleziona il nuovo item
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

	// Copia ricorsivamente i figli
	HTREEITEM child = TreeView_GetChild(hTreeView, src);
	while (child)
	{
		copyItemRecursive(child, newItem);
		child = TreeView_GetNextSibling(hTreeView, child);
	}

	return newItem;
}