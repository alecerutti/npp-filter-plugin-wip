#pragma once

#include <string>
#include <vector>
#include <Windows.h>
#include <commctrl.h>
#include "tinyxml2.h"

void buildXmlRecursive(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* xmlParent, HTREEITEM hItem, HWND hTreeView);
void saveTreeToXml(const std::string& filename, HWND hTreeView);
std::string convertWStringToUtf8(const std::wstring& wstr);
std::wstring convertUtf8ToWString(const char* str);
void loadXmlRecursive(tinyxml2::XMLElement* xmlElement, HTREEITEM hParent, HWND hTreeView);
void loadTreeFromXml(const std::string& filename, HWND hTreeView);