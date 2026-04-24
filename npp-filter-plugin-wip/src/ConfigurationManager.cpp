#include "ConfigurationManager.h"
#include "tinyxml2.h"

std::string toUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring fromUtf8(const char* str) {
    if (!str) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &out[0], n);
    out.resize(wcslen(out.c_str()));   // strip null terminator from size
    return out;
}

static void buildXml(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* xmlParent,
    HTREEITEM hItem, FilterTree& tree)
{
    for (; hItem; hItem = TreeView_GetNextSibling(tree.hwnd, hItem)) {
        wchar_t buf[MAX_PATH] = {};
        TVITEMW tvi = {};
        tvi.hItem = hItem;
        tvi.mask = TVIF_TEXT | TVIF_PARAM;
        tvi.pszText = buf;
        tvi.cchTextMax = MAX_PATH;
        if (!TreeView_GetItem(tree.hwnd, &tvi)) continue;

        TreeItemData* data = (TreeItemData*)tvi.lParam;
        tinyxml2::XMLElement* el;

        if (data && data->type == TYPE_FILE) {
            el = doc.NewElement("file");
            el->SetAttribute("name", toUtf8(buf).c_str());
            el->SetAttribute("path", toUtf8(data->filePath).c_str());
        }
        else {
            el = doc.NewElement("filter");
            el->SetAttribute("name", toUtf8(buf).c_str());
            HTREEITEM child = TreeView_GetChild(tree.hwnd, hItem);
            if (child) buildXml(doc, el, child, tree);
        }
        xmlParent->InsertEndChild(el);
    }
}

void saveTree(const std::string& filename, FilterTree& tree) {
    tinyxml2::XMLDocument doc;
    doc.InsertEndChild(doc.NewDeclaration());

    auto* root = doc.NewElement("FilterManager");
    doc.InsertEndChild(root);

    HTREEITEM hRoot = TreeView_GetRoot(tree.hwnd);
    if (hRoot) buildXml(doc, root, hRoot, tree);

    doc.SaveFile(filename.c_str());
}

static void loadXml(tinyxml2::XMLElement* xmlEl, HTREEITEM hParent, FilterTree& tree) {
    for (auto* e = xmlEl->FirstChildElement(); e; e = e->NextSiblingElement()) {
        std::wstring name = fromUtf8(e->Attribute("name"));
        TreeItemData* data = new TreeItemData();

        TVINSERTSTRUCT tvis = {};
        tvis.hParent = hParent;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = const_cast<wchar_t*>(name.data()); // TODO remove const_cast
        tvis.item.lParam = (LPARAM)data;

        if (strcmp(e->Value(), "filter") == 0) {
            data->type = TYPE_FILTER;
            HTREEITEM hNew = TreeView_InsertItem(tree.hwnd, &tvis);
            loadXml(e, hNew, tree);
            TreeView_Expand(tree.hwnd, hNew, TVE_EXPAND);
        }
        else {    // "file"
            data->type = TYPE_FILE;
            data->filePath = fromUtf8(e->Attribute("path"));
            TreeView_InsertItem(tree.hwnd, &tvis);
        }
    }
}

void loadTree(const std::string& filename, FilterTree& tree) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS) return;

    auto* root = doc.FirstChildElement("FilterManager");
    if (!root) return;

    TreeView_DeleteAllItems(tree.hwnd);
    loadXml(root, TVI_ROOT, tree);
}