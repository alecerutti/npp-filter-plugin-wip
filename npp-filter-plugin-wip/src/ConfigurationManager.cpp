#include "ConfigurationManager.h"
#include "PluginDefinition.h"

void buildXmlRecursive(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* xmlParent, HTREEITEM hItem, HWND hTreeView) {
    while (hItem != nullptr) {
        wchar_t buffer[MAX_PATH];
        TVITEMW tvi = { 0 };
        tvi.hItem = hItem;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvi.pszText = buffer;
        tvi.cchTextMax = MAX_PATH;

        if (TreeView_GetItem(hTreeView, &tvi)) {
            std::string nameUtf8 = convertWStringToUtf8(buffer);
            TreeItemData* data = reinterpret_cast<TreeItemData*>(tvi.lParam);
            tinyxml2::XMLElement* currentElement = nullptr;

            if (data && data->type == TYPE_FILE) {
                currentElement = doc.NewElement("file");
                currentElement->SetAttribute("name", nameUtf8.c_str());
                std::string pathUtf8 = convertWStringToUtf8(data->filePath);
                currentElement->SetAttribute("path", pathUtf8.c_str());
            }
            else {
                currentElement = doc.NewElement("filter");
                currentElement->SetAttribute("name", nameUtf8.c_str());
                HTREEITEM hChild = TreeView_GetChild(hTreeView, hItem);
                if (hChild != nullptr) {
                    buildXmlRecursive(doc, currentElement, hChild, hTreeView);
                }
            }

            if (currentElement) {
                xmlParent->InsertEndChild(currentElement);
            }
        }
        hItem = TreeView_GetNextSibling(hTreeView, hItem);
    }
}

void saveTreeToXml(const std::string& filename, HWND hTreeView) {
    tinyxml2::XMLDocument doc;
    doc.InsertEndChild(doc.NewDeclaration());

    tinyxml2::XMLElement* root = doc.NewElement("FilterManager");
    doc.InsertEndChild(root);

    HTREEITEM hRoot = TreeView_GetRoot(hTreeView);
    if (hRoot) {
        buildXmlRecursive(doc, root, hRoot, hTreeView);
    }
    doc.SaveFile(filename.c_str());
}

std::string convertWStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::wstring convertUtf8ToWString(const char* str) {
    if (!str) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &wstrTo[0], size_needed);
    return wstrTo.c_str();
}

void loadXmlRecursive(tinyxml2::XMLElement* xmlElement, HTREEITEM hParent, HWND hTreeView) {
    for (tinyxml2::XMLElement* e = xmlElement->FirstChildElement(); e != nullptr; e = e->NextSiblingElement()) {
        std::string tagName = e->Value();
        const char* nameAttr = e->Attribute("name");
        std::wstring wName = convertUtf8ToWString(nameAttr);
        TreeItemData* data = new TreeItemData();

        if (tagName == "filter") {
            data->type = TYPE_FILTER;
            TVINSERTSTRUCT tvis = { 0 };
            tvis.hParent = hParent;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
            tvis.item.pszText = const_cast<wchar_t*>(wName.c_str());
            tvis.item.lParam = reinterpret_cast<LPARAM>(data);
            HTREEITEM hNewFilter = TreeView_InsertItem(hTreeView, &tvis);
            loadXmlRecursive(e, hNewFilter, hTreeView);
            TreeView_Expand(hTreeView, hNewFilter, TVE_EXPAND);
        }
        else if (tagName == "file") {
            data->type = TYPE_FILE;
            const char* pathAttr = e->Attribute("path");
            data->filePath = convertUtf8ToWString(pathAttr);
            TVINSERTSTRUCT tvis = { 0 };
            tvis.hParent = hParent;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
            tvis.item.pszText = const_cast<wchar_t*>(wName.c_str());
            tvis.item.lParam = reinterpret_cast<LPARAM>(data);
            TreeView_InsertItem(hTreeView, &tvis);
        }
    }
}

void loadTreeFromXml(const std::string& filename, HWND hTreeView) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS) return;

    tinyxml2::XMLElement* root = doc.FirstChildElement("FilterManager");
    if (!root) return;

    TreeView_DeleteAllItems(hTreeView);
    loadXmlRecursive(root, TVI_ROOT, hTreeView);
}