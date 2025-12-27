#pragma once

#include <string>
#include "tinyxml2.h"

using namespace tinyxml2;

class XmlConfiguration
{
public:
	void load();

	void save();

	void saveTreeNode(XMLElement* xmlParent, HTREEITEM hItem, XMLDocument& doc);

	void loadTreeNode(XMLElement* xmlNode, HTREEITEM hParent);

	void setPath(const std::wstring &filePath);

	std::string wstringToUtf8(const std::wstring& wstr);

	std::wstring utf8ToWstring(const std::string& str);

private:
	std::wstring filePath;
};
