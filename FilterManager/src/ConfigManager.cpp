#include "ConfigManager.h"
#include "FilterTree.h"
#include "tinyxml2.h"

std::string ConfigManager::toUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring ConfigManager::fromUtf8(const char* str) {
    if (!str) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &out[0], n);
    out.resize(wcslen(out.c_str()));
    return out;
}

static void buildXml(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* xmlParent, HTREEITEM hItem, FilterTree& tree, ConfigManager* cm) {
    for (; hItem; hItem = TreeView_GetNextSibling(tree.getHandle(), hItem)) {
        std::wstring text = tree.getItemText(hItem);
        TreeItemData* data = tree.getItemData(hItem);
        if (!data) continue;

        tinyxml2::XMLElement* el;
        if (data->type == TYPE_FILE) {
            el = doc.NewElement("file");
            el->SetAttribute("name", cm->toUtf8(text).c_str());
            el->SetAttribute("path", cm->toUtf8(data->filePath).c_str());
        }
        else {
            el = doc.NewElement("filter");
            el->SetAttribute("name", cm->toUtf8(text).c_str());
            HTREEITEM child = TreeView_GetChild(tree.getHandle(), hItem);
            if (child) buildXml(doc, el, child, tree, cm);
        }
        xmlParent->InsertEndChild(el);
    }
}

void ConfigManager::saveConfig(const std::string& filename, FilterTree& tree) {
    tinyxml2::XMLDocument doc;
    doc.InsertEndChild(doc.NewDeclaration());
    auto* root = doc.NewElement("FilterManager");
    doc.InsertEndChild(root);

    HTREEITEM hRoot = TreeView_GetRoot(tree.getHandle());
    if (hRoot) buildXml(doc, root, hRoot, tree, this);
    doc.SaveFile(filename.c_str());
}

static void loadXmlNode(tinyxml2::XMLElement* xmlEl, HTREEITEM hParent, FilterTree& tree, ConfigManager* cm) {
    for (auto* e = xmlEl->FirstChildElement(); e; e = e->NextSiblingElement()) {
        std::wstring name = cm->fromUtf8(e->Attribute("name"));
        TreeItemData* data = new TreeItemData();

        TVINSERTSTRUCT tvis = {};
        tvis.hParent = hParent;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = const_cast<wchar_t*>(name.data());
        tvis.item.lParam = (LPARAM)data;

        if (strcmp(e->Value(), "filter") == 0) {
            data->type = TYPE_FILTER;
            HTREEITEM hNew = TreeView_InsertItem(tree.getHandle(), &tvis);
            loadXmlNode(e, hNew, tree, cm);
            TreeView_Expand(tree.getHandle(), hNew, TVE_EXPAND);
        }
        else {
            data->type = TYPE_FILE;
            data->filePath = cm->fromUtf8(e->Attribute("path"));
            TreeView_InsertItem(tree.getHandle(), &tvis);
        }
    }
}

void ConfigManager::loadConfig(const std::string& filename, FilterTree& tree) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS) return;
    auto* root = doc.FirstChildElement("FilterManager");
    if (!root) return;

    TreeView_DeleteAllItems(tree.getHandle());
    loadXmlNode(root, TVI_ROOT, tree, this);
}