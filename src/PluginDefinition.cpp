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
//#include "menuCmdID.h"

#include "DockingFeature/Docking.h"

//#include <commctrl.h>
//#pragma comment(lib, "comctl32.lib")
//#include <windowsx.h>
#include <string>
//#include <fstream>
//#include <WinUser.h>
//#include <iomanip>

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE /*hModule*/)
{
	// TODO parse xaml
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
	setCommand(0, TEXT("Show/Hide Panel"), panel, NULL, false);
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
// Item cliccato
HTREEITEM hItemClicked = NULL;

bool isVisible = false;

tTbData myDock = { 0 };

const wchar_t* CONTAINER_CLASS = L"FilterManagerContainer";

// Forward declarations
LRESULT CALLBACK ContainerProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void addRootFilter();
void addChildFilter(HTREEITEM parent);
void deleteFilter(HTREEITEM item);
void renameFilter(HTREEITEM item);
void addCurrentDocument(HTREEITEM filterItem);
void removeFile(HTREEITEM fileItem);
void openFile(HTREEITEM fileItem);
TreeItemData* getItemData(HTREEITEM item);
ItemType getItemType(HTREEITEM item);

void panel()
{
	if (!hContainer)
	{
		// Registra la classe della finestra contenitore
		WNDCLASSEX wc = { 0 };
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.lpfnWndProc = ContainerProc;
		wc.hInstance = GetModuleHandle(nullptr);
		wc.lpszClassName = CONTAINER_CLASS;
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		RegisterClassEx(&wc);

		// Crea la finestra contenitore
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

		// Crea il TreeView DENTRO il contenitore
		hTreeView = CreateWindowEx(
			WS_EX_CLIENTEDGE,
			WC_TREEVIEW,
			L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_EDITLABELS,
			0, 0, 300, 400,
			hContainer,
			nullptr,
			GetModuleHandle(nullptr),
			nullptr
		);

		// Crea il menu popup per FILTRI
		hFilterMenu = CreatePopupMenu();
		AppendMenu(hFilterMenu, MF_STRING, ID_ADD_FILTER, L"Add Filter");
		AppendMenu(hFilterMenu, MF_STRING, ID_RENAME_FILTER, L"Rename Filter");
		AppendMenu(hFilterMenu, MF_STRING, ID_DELETE_FILTER, L"Delete Filter");
		AppendMenu(hFilterMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hFilterMenu, MF_STRING, ID_ADD_CURRENT_DOC, L"Add Current Document");

		// Crea il menu popup per FILE
		hFileMenu = CreatePopupMenu();
		AppendMenu(hFileMenu, MF_STRING, ID_REMOVE_FILE, L"Remove");

		// Configura il docking
		myDock.hClient = hContainer;
		myDock.pszName = L"Filter Manager";
		myDock.dlgID = 0;
		myDock.uMask = 0;
		myDock.pszAddInfo = nullptr;
		myDock.hIconTab = nullptr;
		myDock.pszModuleName = L"MyPlugin.dll";

		::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&myDock);
	}

	// Mostra/nascondi
	if (!isVisible)
	{
		::SendMessage(nppData._nppHandle, NPPM_DMMSHOW, 0, (LPARAM)myDock.hClient);
		isVisible = true;
	}
	else
	{
		::SendMessage(nppData._nppHandle, NPPM_DMMHIDE, 0, (LPARAM)myDock.hClient);
		isVisible = false;
	}
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

void deleteItemRecursive(HTREEITEM item)
{
	// Libera memoria dei figli prima
	HTREEITEM child = TreeView_GetChild(hTreeView, item);
	while (child)
	{
		HTREEITEM next = TreeView_GetNextSibling(hTreeView, child);
		deleteItemRecursive(child);
		child = next;
	}

	// Libera la memoria di questo item
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
	// Ottieni il path del documento corrente in Notepad++
	wchar_t filePath[MAX_PATH] = { 0 };
	::SendMessage(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, MAX_PATH, (LPARAM)filePath);

	if (wcslen(filePath) == 0)
	{
		MessageBox(hContainer, L"No document is currently open!", L"Error", MB_OK | MB_ICONWARNING);
		return;
	}

	// Estrai solo il nome del file dal path completo
	wchar_t* fileName = wcsrchr(filePath, L'\\');
	if (!fileName)
		fileName = wcsrchr(filePath, L'/');

	if (fileName)
		fileName++; // Salta il separatore
	else
		fileName = filePath; // Nessun separatore trovato, usa tutto

	// Crea la struttura dati per il file
	TreeItemData* data = new TreeItemData();
	data->type = TYPE_FILE;
	data->filePath = filePath;

	// Inserisci il file come figlio del filtro
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
		// Apri il file in Notepad++
		::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)data->filePath.c_str());
	}
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

	return TYPE_FILTER; // Default
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

		if (cmd == ID_ADD_FILTER)
		{
			if (hItemClicked)
			{
				addChildFilter(hItemClicked);
			}
			else
			{
				addRootFilter();
			}
			return 0;
		}
		else if (cmd == ID_DELETE_FILTER)
		{
			if (hItemClicked)
			{
				deleteFilter(hItemClicked);
				hItemClicked = NULL;
			}
			return 0;
		}
		else if (cmd == ID_RENAME_FILTER)
		{
			if (hItemClicked)
			{
				renameFilter(hItemClicked);
			}
			return 0;
		}
		else if (cmd == ID_ADD_CURRENT_DOC)
		{
			if (hItemClicked)
			{
				addCurrentDocument(hItemClicked);
			}
			return 0;
		}
		else if (cmd == ID_REMOVE_FILE)
		{
			if (hItemClicked)
			{
				removeFile(hItemClicked);
				hItemClicked = NULL;
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
				// Doppio click su un item
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

				POINT pointClient = point;
				ScreenToClient(hTreeView, &pointClient);

				TVHITTESTINFO thti = {};
				thti.pt = pointClient;

				hItemClicked = TreeView_HitTest(hTreeView, &thti);

				if (hItemClicked)
				{
					TreeView_SelectItem(hTreeView, hItemClicked);

					// Mostra menu diverso in base al tipo
					ItemType type = getItemType(hItemClicked);

					if (type == TYPE_FILTER)
					{
						// Menu per filtri
						EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_ENABLED);
						EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_ENABLED);
						EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_ENABLED);
						TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
					}
					else // TYPE_FILE
					{
						// Menu per file
						TrackPopupMenu(hFileMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
					}
				}
				else
				{
					// Click su area vuota - mostra solo "Add Filter"
					EnableMenuItem(hFilterMenu, ID_RENAME_FILTER, MF_GRAYED);
					EnableMenuItem(hFilterMenu, ID_DELETE_FILTER, MF_GRAYED);
					EnableMenuItem(hFilterMenu, ID_ADD_CURRENT_DOC, MF_GRAYED);
					TrackPopupMenu(hFilterMenu, TPM_RIGHTBUTTON, point.x, point.y, 0, hwnd, NULL);
				}
				return TRUE;
			}

			case TVN_BEGINLABELEDIT:
			{
				// Permetti l'editing solo sui filtri, non sui file
				LPNMTVDISPINFO pInfo = (LPNMTVDISPINFO)lParam;
				ItemType type = getItemType(pInfo->item.hItem);

				if (type == TYPE_FILE)
				{
					return TRUE; // TRUE = blocca editing
				}

				return FALSE; // FALSE = permetti editing
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
				// Already free in WM_COMMAND ID_DELETE_FILTER

				//// Libera la memoria quando un item viene eliminato
				//LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
				//TreeItemData* data = (TreeItemData*)pnmtv->itemOld.lParam;
				//if (data)
				//{
				//	delete data;
				//}
				//return 0;
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