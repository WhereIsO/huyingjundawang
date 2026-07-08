#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "backup.hpp"

namespace {

constexpr int kWindowWidth = 920;
constexpr int kWindowHeight = 640;

enum ControlId {
    ID_TAB = 100,

    ID_BACKUP_SOURCE = 201,
    ID_BACKUP_SOURCE_BROWSE = 202,
    ID_BACKUP_ARCHIVE = 203,
    ID_BACKUP_ARCHIVE_BROWSE = 204,
    ID_BACKUP_COMPRESS = 205,
    ID_BACKUP_PASSWORD = 206,
    ID_BACKUP_INCLUDE_EXT = 207,
    ID_BACKUP_EXCLUDE_EXT = 208,
    ID_BACKUP_NAME_CONTAINS = 209,
    ID_BACKUP_MIN_SIZE = 210,
    ID_BACKUP_MAX_SIZE = 211,
    ID_BACKUP_MODIFIED_AFTER = 212,
    ID_BACKUP_RUN = 213,

    ID_RESTORE_ARCHIVE = 301,
    ID_RESTORE_ARCHIVE_BROWSE = 302,
    ID_RESTORE_TARGET = 303,
    ID_RESTORE_TARGET_BROWSE = 304,
    ID_RESTORE_PASSWORD = 305,
    ID_RESTORE_OVERWRITE = 306,
    ID_RESTORE_RUN = 307,

    ID_LIST_ARCHIVE = 401,
    ID_LIST_ARCHIVE_BROWSE = 402,
    ID_LIST_RUN = 403,
    ID_LIST_VIEW = 404,

    ID_STATUS = 900
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND tab = nullptr;
    HWND status = nullptr;
    HWND listView = nullptr;
    HFONT font = nullptr;
    std::vector<HWND> backupControls;
    std::vector<HWND> restoreControls;
    std::vector<HWND> listControls;
};

AppState gApp;

HWND makeControl(const char* className,
                 const char* text,
                 DWORD style,
                 DWORD exStyle,
                 int x,
                 int y,
                 int width,
                 int height,
                 int id) {
    HWND handle = CreateWindowExA(exStyle,
                                  className,
                                  text,
                                  style | WS_CHILD,
                                  x,
                                  y,
                                  width,
                                  height,
                                  gApp.window,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                  gApp.instance,
                                  nullptr);
    if (handle == nullptr) {
        throw std::runtime_error("failed to create window control");
    }
    SendMessageA(handle, WM_SETFONT, reinterpret_cast<WPARAM>(gApp.font), TRUE);
    return handle;
}

HWND makeLabel(const char* text, int x, int y, int width, int height, std::vector<HWND>& page) {
    HWND handle = makeControl("STATIC", text, WS_VISIBLE, 0, x, y, width, height, 0);
    page.push_back(handle);
    return handle;
}

HWND makeEdit(int id, int x, int y, int width, int height, std::vector<HWND>& page, DWORD extra = 0) {
    HWND handle = makeControl("EDIT",
                              "",
                              WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | extra,
                              WS_EX_CLIENTEDGE,
                              x,
                              y,
                              width,
                              height,
                              id);
    page.push_back(handle);
    return handle;
}

HWND makeButton(const char* text, int id, int x, int y, int width, int height, std::vector<HWND>& page) {
    HWND handle = makeControl("BUTTON",
                              text,
                              WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              0,
                              x,
                              y,
                              width,
                              height,
                              id);
    page.push_back(handle);
    return handle;
}

HWND makeCheckbox(const char* text, int id, int x, int y, int width, int height, std::vector<HWND>& page) {
    HWND handle = makeControl("BUTTON",
                              text,
                              WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                              0,
                              x,
                              y,
                              width,
                              height,
                              id);
    page.push_back(handle);
    return handle;
}

std::string getText(int id) {
    HWND control = GetDlgItem(gApp.window, id);
    int length = GetWindowTextLengthA(control);
    std::string text(static_cast<std::size_t>(length + 1), '\0');
    if (length > 0) {
        GetWindowTextA(control, &text[0], length + 1);
    }
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void setText(int id, const std::string& text) {
    SetWindowTextA(GetDlgItem(gApp.window, id), text.c_str());
}

bool checked(int id) {
    return SendMessageA(GetDlgItem(gApp.window, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void setStatus(const std::string& message) {
    SetWindowTextA(gApp.status, message.c_str());
}

void showError(const std::exception& ex) {
    setStatus(std::string("Error: ") + ex.what());
    MessageBoxA(gApp.window, ex.what(), "Operation failed", MB_OK | MB_ICONERROR);
}

std::string formatBytes(std::uint64_t value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

std::string formatTime(std::int64_t value) {
    if (value <= 0) {
        return "-";
    }
    std::time_t raw = static_cast<std::time_t>(value);
    std::tm* local = std::localtime(&raw);
    if (local == nullptr) {
        return "-";
    }
    std::ostringstream out;
    out << std::put_time(local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::uint64_t parseOptionalBytes(const std::string& value,
                                 const char* label,
                                 std::uint64_t fallback) {
    if (value.empty()) {
        return fallback;
    }
    std::size_t consumed = 0;
    std::uint64_t parsed = 0;
    try {
        parsed = std::stoull(value, &consumed, 10);
    } catch (const std::exception&) {
        throw std::runtime_error(std::string("invalid ") + label + ": " + value);
    }
    if (consumed != value.size()) {
        throw std::runtime_error(std::string("invalid ") + label + ": " + value);
    }
    return parsed;
}

void requirePath(const std::string& path, const char* label) {
    if (path.empty()) {
        throw std::runtime_error(std::string(label) + " is required");
    }
}

std::string browseFolder(const char* title) {
    BROWSEINFOA info{};
    info.hwndOwner = gApp.window;
    info.lpszTitle = title;
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE item = SHBrowseForFolderA(&info);
    if (item == nullptr) {
        return "";
    }

    char path[MAX_PATH]{};
    std::string result;
    if (SHGetPathFromIDListA(item, path)) {
        result = path;
    }
    CoTaskMemFree(item);
    return result;
}

std::string browseArchiveOpen() {
    char path[MAX_PATH]{};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = gApp.window;
    ofn.lpstrFilter = "SDB archive (*.sdb)\0*.sdb\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "sdb";
    if (!GetOpenFileNameA(&ofn)) {
        return "";
    }
    return path;
}

std::string browseArchiveSave() {
    char path[MAX_PATH]{};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = gApp.window;
    ofn.lpstrFilter = "SDB archive (*.sdb)\0*.sdb\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "sdb";
    if (!GetSaveFileNameA(&ofn)) {
        return "";
    }
    return path;
}

void showPage(int index) {
    const std::vector<std::vector<HWND>*> pages = {
        &gApp.backupControls,
        &gApp.restoreControls,
        &gApp.listControls,
    };

    for (std::size_t i = 0; i < pages.size(); ++i) {
        int command = (static_cast<int>(i) == index) ? SW_SHOW : SW_HIDE;
        for (HWND control : *pages[i]) {
            ShowWindow(control, command);
        }
    }
}

void addTab(const char* text, int index) {
    TCITEMA item{};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<char*>(text);
    TabCtrl_InsertItem(gApp.tab, index, &item);
}

void insertListColumn(int index, const char* title, int width) {
    LVCOLUMNA column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<char*>(title);
    column.cx = width;
    column.iSubItem = index;
    ListView_InsertColumn(gApp.listView, index, &column);
}

void createBackupPage() {
    std::vector<HWND>& page = gApp.backupControls;
    makeLabel("Source directory", 28, 62, 120, 22, page);
    makeEdit(ID_BACKUP_SOURCE, 160, 58, 610, 26, page);
    makeButton("Browse...", ID_BACKUP_SOURCE_BROWSE, 785, 56, 90, 30, page);

    makeLabel("Archive file", 28, 102, 120, 22, page);
    makeEdit(ID_BACKUP_ARCHIVE, 160, 98, 610, 26, page);
    makeButton("Save as...", ID_BACKUP_ARCHIVE_BROWSE, 785, 96, 90, 30, page);

    makeCheckbox("Compress", ID_BACKUP_COMPRESS, 160, 138, 110, 24, page);
    makeLabel("Password", 290, 142, 72, 22, page);
    makeEdit(ID_BACKUP_PASSWORD, 365, 138, 180, 26, page, ES_PASSWORD);

    HWND group = makeControl("BUTTON",
                             "Filters",
                             WS_VISIBLE | BS_GROUPBOX,
                             0,
                             28,
                             180,
                             847,
                             188,
                             0);
    page.push_back(group);

    makeLabel("Include extensions", 50, 214, 135, 22, page);
    makeEdit(ID_BACKUP_INCLUDE_EXT, 190, 210, 230, 26, page);
    makeLabel("e.g. .cpp,.h", 430, 214, 115, 22, page);

    makeLabel("Exclude extensions", 50, 254, 135, 22, page);
    makeEdit(ID_BACKUP_EXCLUDE_EXT, 190, 250, 230, 26, page);
    makeLabel("e.g. .tmp,.log", 430, 254, 125, 22, page);

    makeLabel("Name contains", 50, 294, 135, 22, page);
    makeEdit(ID_BACKUP_NAME_CONTAINS, 190, 290, 230, 26, page);

    makeLabel("Min bytes", 510, 214, 80, 22, page);
    makeEdit(ID_BACKUP_MIN_SIZE, 595, 210, 120, 26, page);
    makeLabel("Max bytes", 510, 254, 80, 22, page);
    makeEdit(ID_BACKUP_MAX_SIZE, 595, 250, 120, 26, page);
    makeLabel("Modified after", 510, 294, 95, 22, page);
    makeEdit(ID_BACKUP_MODIFIED_AFTER, 615, 290, 120, 26, page);
    makeLabel("YYYY-MM-DD", 745, 294, 95, 22, page);

    makeButton("Start Backup", ID_BACKUP_RUN, 740, 386, 135, 36, page);
}

void createRestorePage() {
    std::vector<HWND>& page = gApp.restoreControls;
    makeLabel("Archive file", 28, 62, 120, 22, page);
    makeEdit(ID_RESTORE_ARCHIVE, 160, 58, 610, 26, page);
    makeButton("Browse...", ID_RESTORE_ARCHIVE_BROWSE, 785, 56, 90, 30, page);

    makeLabel("Target directory", 28, 102, 120, 22, page);
    makeEdit(ID_RESTORE_TARGET, 160, 98, 610, 26, page);
    makeButton("Browse...", ID_RESTORE_TARGET_BROWSE, 785, 96, 90, 30, page);

    makeLabel("Password", 28, 146, 120, 22, page);
    makeEdit(ID_RESTORE_PASSWORD, 160, 142, 220, 26, page, ES_PASSWORD);
    makeCheckbox("Overwrite existing files", ID_RESTORE_OVERWRITE, 410, 142, 190, 24, page);

    makeButton("Restore", ID_RESTORE_RUN, 740, 188, 135, 36, page);
}

void createListPage() {
    std::vector<HWND>& page = gApp.listControls;
    makeLabel("Archive file", 28, 62, 120, 22, page);
    makeEdit(ID_LIST_ARCHIVE, 160, 58, 610, 26, page);
    makeButton("Browse...", ID_LIST_ARCHIVE_BROWSE, 785, 56, 90, 30, page);
    makeButton("Load Archive", ID_LIST_RUN, 740, 98, 135, 32, page);

    gApp.listView = makeControl(WC_LISTVIEWA,
                                "",
                                WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                WS_EX_CLIENTEDGE,
                                28,
                                148,
                                847,
                                304,
                                ID_LIST_VIEW);
    ListView_SetExtendedListViewStyle(gApp.listView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    insertListColumn(0, "Type", 90);
    insertListColumn(1, "Original", 90);
    insertListColumn(2, "Stored", 90);
    insertListColumn(3, "Modified", 150);
    insertListColumn(4, "Flags", 70);
    insertListColumn(5, "Path", 340);
    page.push_back(gApp.listView);
}

void createControls() {
    gApp.tab = makeControl(WC_TABCONTROLA,
                           "",
                           WS_VISIBLE | WS_TABSTOP | TCS_FIXEDWIDTH,
                           0,
                           18,
                           16,
                           857,
                           30,
                           ID_TAB);
    SendMessageA(gApp.tab, TCM_SETITEMSIZE, 0, MAKELPARAM(150, 24));
    addTab("Backup", 0);
    addTab("Restore", 1);
    addTab("Archive List", 2);

    createBackupPage();
    createRestorePage();
    createListPage();

    gApp.status = makeControl("EDIT",
                              "Ready.",
                              WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                              WS_EX_CLIENTEDGE,
                              28,
                              472,
                              847,
                              105,
                              ID_STATUS);
    showPage(0);
}

void runBackup() {
    try {
        std::string source = getText(ID_BACKUP_SOURCE);
        std::string archive = getText(ID_BACKUP_ARCHIVE);
        requirePath(source, "Source directory");
        requirePath(archive, "Archive file");

        sdb::BackupOptions options;
        options.compress = checked(ID_BACKUP_COMPRESS);
        options.password = getText(ID_BACKUP_PASSWORD);
        options.filter.includeExtensions = sdb::splitCommaList(getText(ID_BACKUP_INCLUDE_EXT));
        options.filter.excludeExtensions = sdb::splitCommaList(getText(ID_BACKUP_EXCLUDE_EXT));
        options.filter.nameContains = getText(ID_BACKUP_NAME_CONTAINS);
        options.filter.minSize = parseOptionalBytes(getText(ID_BACKUP_MIN_SIZE), "minimum size", 0);
        options.filter.maxSize = parseOptionalBytes(getText(ID_BACKUP_MAX_SIZE),
                                                    "maximum size",
                                                    std::numeric_limits<std::uint64_t>::max());

        std::string date = getText(ID_BACKUP_MODIFIED_AFTER);
        if (!date.empty()) {
            options.filter.hasModifiedAfter = true;
            if (!sdb::parseDate(date, options.filter.modifiedAfter)) {
                throw std::runtime_error("invalid modified-after date: " + date);
            }
        }

        setStatus("Creating backup...");
        sdb::OperationStats stats = sdb::createBackup(source, archive, options);

        std::ostringstream message;
        message << "Backup created successfully.\r\n"
                << "Archive: " << archive << "\r\n"
                << "Directories: " << stats.directories << "\r\n"
                << "Files: " << stats.files << "\r\n"
                << "Original bytes: " << stats.originalBytes << "\r\n"
                << "Stored bytes: " << stats.storedBytes;
        setStatus(message.str());
        MessageBoxA(gApp.window, "Backup created successfully.", "Done", MB_OK | MB_ICONINFORMATION);
    } catch (const std::exception& ex) {
        showError(ex);
    }
}

void runRestore() {
    try {
        std::string archive = getText(ID_RESTORE_ARCHIVE);
        std::string target = getText(ID_RESTORE_TARGET);
        requirePath(archive, "Archive file");
        requirePath(target, "Target directory");

        sdb::RestoreOptions options;
        options.password = getText(ID_RESTORE_PASSWORD);
        options.overwrite = checked(ID_RESTORE_OVERWRITE);

        setStatus("Restoring archive...");
        sdb::OperationStats stats = sdb::restoreBackup(archive, target, options);

        std::ostringstream message;
        message << "Archive restored successfully.\r\n"
                << "Target: " << target << "\r\n"
                << "Directories: " << stats.directories << "\r\n"
                << "Files: " << stats.files << "\r\n"
                << "Restored bytes: " << stats.originalBytes;
        setStatus(message.str());
        MessageBoxA(gApp.window, "Archive restored successfully.", "Done", MB_OK | MB_ICONINFORMATION);
    } catch (const std::exception& ex) {
        showError(ex);
    }
}

void runListArchive() {
    try {
        std::string archive = getText(ID_LIST_ARCHIVE);
        requirePath(archive, "Archive file");
        setStatus("Loading archive entries...");

        std::vector<sdb::ArchiveEntryInfo> entries = sdb::listArchive(archive);
        ListView_DeleteAllItems(gApp.listView);

        for (std::size_t i = 0; i < entries.size(); ++i) {
            const sdb::ArchiveEntryInfo& entry = entries[i];
            LVITEMA item{};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(i);
            item.iSubItem = 0;
            item.pszText = const_cast<char*>(entry.type.c_str());
            ListView_InsertItem(gApp.listView, &item);

            std::string original = formatBytes(entry.originalSize);
            std::string stored = formatBytes(entry.storedSize);
            std::string modified = formatTime(entry.modifiedTime);
            std::string flags;
            if (entry.compressed) {
                flags += "C";
            }
            if (entry.encrypted) {
                flags += "E";
            }
            if (flags.empty()) {
                flags = "-";
            }

            ListView_SetItemText(gApp.listView, static_cast<int>(i), 1, const_cast<char*>(original.c_str()));
            ListView_SetItemText(gApp.listView, static_cast<int>(i), 2, const_cast<char*>(stored.c_str()));
            ListView_SetItemText(gApp.listView, static_cast<int>(i), 3, const_cast<char*>(modified.c_str()));
            ListView_SetItemText(gApp.listView, static_cast<int>(i), 4, const_cast<char*>(flags.c_str()));
            ListView_SetItemText(gApp.listView, static_cast<int>(i), 5, const_cast<char*>(entry.path.c_str()));
        }

        std::ostringstream message;
        message << "Loaded " << entries.size() << " archive entries.";
        setStatus(message.str());
    } catch (const std::exception& ex) {
        showError(ex);
    }
}

void handleBrowse(int id) {
    std::string value;
    switch (id) {
        case ID_BACKUP_SOURCE_BROWSE:
            value = browseFolder("Select source directory");
            if (!value.empty()) {
                setText(ID_BACKUP_SOURCE, value);
            }
            break;
        case ID_BACKUP_ARCHIVE_BROWSE:
            value = browseArchiveSave();
            if (!value.empty()) {
                setText(ID_BACKUP_ARCHIVE, value);
            }
            break;
        case ID_RESTORE_ARCHIVE_BROWSE:
            value = browseArchiveOpen();
            if (!value.empty()) {
                setText(ID_RESTORE_ARCHIVE, value);
            }
            break;
        case ID_RESTORE_TARGET_BROWSE:
            value = browseFolder("Select restore target directory");
            if (!value.empty()) {
                setText(ID_RESTORE_TARGET, value);
            }
            break;
        case ID_LIST_ARCHIVE_BROWSE:
            value = browseArchiveOpen();
            if (!value.empty()) {
                setText(ID_LIST_ARCHIVE, value);
            }
            break;
        default:
            break;
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case ID_BACKUP_SOURCE_BROWSE:
                case ID_BACKUP_ARCHIVE_BROWSE:
                case ID_RESTORE_ARCHIVE_BROWSE:
                case ID_RESTORE_TARGET_BROWSE:
                case ID_LIST_ARCHIVE_BROWSE:
                    handleBrowse(id);
                    return 0;
                case ID_BACKUP_RUN:
                    runBackup();
                    return 0;
                case ID_RESTORE_RUN:
                    runRestore();
                    return 0;
                case ID_LIST_RUN:
                    runListArchive();
                    return 0;
                default:
                    return 0;
            }
        }
        case WM_NOTIFY: {
            NMHDR* header = reinterpret_cast<NMHDR*>(lParam);
            if (header->idFrom == ID_TAB && header->code == TCN_SELCHANGE) {
                showPage(TabCtrl_GetCurSel(gApp.tab));
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (gApp.font != nullptr) {
                DeleteObject(gApp.font);
                gApp.font = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(hwnd, message, wParam, lParam);
}

void centerWindow(HWND hwnd) {
    RECT rect{};
    GetWindowRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    gApp.instance = instance;
    CoInitialize(nullptr);

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    WNDCLASSEXA windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = "SdbGuiWindow";

    if (!RegisterClassExA(&windowClass)) {
        MessageBoxA(nullptr, "Failed to register window class.", "Startup failed", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    gApp.font = CreateFontA(18,
                            0,
                            0,
                            0,
                            FW_NORMAL,
                            FALSE,
                            FALSE,
                            FALSE,
                            DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE,
                            "Segoe UI");

    gApp.window = CreateWindowExA(0,
                                  "SdbGuiWindow",
                                  "Simple Data Backup",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  kWindowWidth,
                                  kWindowHeight,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  nullptr);
    if (gApp.window == nullptr) {
        MessageBoxA(nullptr, "Failed to create main window.", "Startup failed", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    try {
        createControls();
    } catch (const std::exception& ex) {
        MessageBoxA(nullptr, ex.what(), "Startup failed", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    centerWindow(gApp.window);
    ShowWindow(gApp.window, showCommand);
    UpdateWindow(gApp.window);

    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(gApp.window, &message)) {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
    }

    CoUninitialize();
    return static_cast<int>(message.wParam);
}
