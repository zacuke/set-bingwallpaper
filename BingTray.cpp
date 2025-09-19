#include <windows.h>
#include <shellapi.h>
#include <urlmon.h>
#include <wininet.h>
#include <string>
#include <sstream>
#include <fstream>
#include <shlobj.h>
#include <shobjidl.h>
#include <atlbase.h>

#include "resource.h"

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")

#define WM_TRAY           (WM_USER + 1)
#define ID_TRAY_EXIT      100
#define ID_TRAY_UPDATE    101
#define ID_TRAY_AUTOSTART 102
#define ID_TRAY_ICON      1
#define ID_TIMER          1

NOTIFYICONDATA nid = {};
HWND hWnd;

// ========== logging helper ==========

std::wstring GetLogPath() {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"bingtray.log");
    return path;
}

void Log(const std::wstring& msg) {
    std::wofstream f(GetLogPath(), std::ios::app);
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        f << L"[" << st.wYear << L"-" << st.wMonth << L"-" << st.wDay
          << L" " << st.wHour << L":" << st.wMinute << L":" << st.wSecond << L"] "
          << msg << std::endl;
    }
}

// ========== helpers ==========

bool DownloadFile(const std::wstring& url, const std::wstring& path) {
    return URLDownloadToFileW(NULL, url.c_str(), path.c_str(), 0, NULL) == S_OK;
}

std::string DownloadUrlToString(const std::wstring& url) {
    HINTERNET hInternet = InternetOpen(L"BingTray", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";
    HINTERNET hConnect = InternetOpenUrl(hInternet, url.c_str(),
        NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    char buffer[4096];
    DWORD read;
    std::string data;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &read) && read > 0) {
        data.append(buffer, buffer + read);
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return data;
}

struct BingImageInfo {
    std::wstring fullUrl;
    std::wstring urlBase;
};

// Parse Bing JSON to extract url and urlbase
BingImageInfo GetBingImageInfo() {
    BingImageInfo info;
    std::string data = DownloadUrlToString(
        L"https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=1&mkt=en-US");

    if (data.empty()) return info;

    size_t pos = data.find("\"url\":\"");
    if (pos != std::string::npos) {
        pos += 7;
        size_t end = data.find("\"", pos);
        if (end != std::string::npos) {
            std::string urlPart = data.substr(pos, end - pos);
            info.fullUrl = L"https://www.bing.com" + std::wstring(urlPart.begin(), urlPart.end());
        }
    }

    pos = data.find("\"urlbase\":\"");
    if (pos != std::string::npos) {
        pos += 11;
        size_t end = data.find("\"", pos);
        if (end != std::string::npos) {
            std::string urlBasePart = data.substr(pos, end - pos);
            info.urlBase = std::wstring(urlBasePart.begin(), urlBasePart.end());
        }
    }

    return info;
}

void SetWallpaper(const std::wstring& file) {
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)file.c_str(),
        SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
}

// ========== local cache tracking ==========

std::wstring GetMetaCachePath() {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"bingwallpaper.txt");
    return path;
}

std::wstring GetImageCachePath() {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"bingwallpaper.jpg");
    return path;
}

std::wstring LoadLastUrlBase() {
    std::wstring path = GetMetaCachePath();
    std::wifstream f(path);
    std::wstring line;
    if (f) std::getline(f, line);
    return line;
}

void SaveLastUrlBase(const std::wstring& urlbase) {
    std::wstring path = GetMetaCachePath();
    std::wofstream f(path, std::ios::trunc);
    if (f) f << urlbase;
}

// ========== update logic ==========

void UpdateWallpaper() {
    BingImageInfo info = GetBingImageInfo();
    if (info.fullUrl.empty() || info.urlBase.empty()) {
        Log(L"Failed to retrieve Bing image info.");
        return;
    }

    auto imagePath = GetImageCachePath();
    std::wstring lastBase = LoadLastUrlBase();

    if (info.urlBase == lastBase) {
        // Already the same wallpaper
        if (GetFileAttributesW(imagePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            SetWallpaper(imagePath);
            Log(L"Reused cached wallpaper, no download.");
        } else {
            Log(L"No image file found, redownloading current wallpaper.");
            if (DownloadFile(info.fullUrl, imagePath)) {
                SetWallpaper(imagePath);
                SaveLastUrlBase(info.urlBase);
                Log(L"Downloaded current wallpaper again.");
            }
        }
        return;
    }

    // New image
    if (DownloadFile(info.fullUrl, imagePath)) {
        SetWallpaper(imagePath);
        SaveLastUrlBase(info.urlBase);
        Log(L"Downloaded NEW wallpaper and set it.");
    } else {
        Log(L"Failed to download NEW wallpaper.");
    }
}

// ========== Startup shortcut helpers ==========

std::wstring GetStartupShortcutPath() {
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, path);
    wcscat_s(path, L"\\BingTray.lnk");
    return path;
}

bool ShortcutExists() {
    std::wstring link = GetStartupShortcutPath();
    return GetFileAttributesW(link.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool CreateStartupShortcut() {
    std::wstring shortcutPath = GetStartupShortcutPath();
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    CComPtr<IShellLink> psl;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl))))
        return false;

    psl->SetPath(exePath);

    CComPtr<IPersistFile> ppf;
    if (FAILED(psl->QueryInterface(IID_PPV_ARGS(&ppf))))
        return false;

    return SUCCEEDED(ppf->Save(shortcutPath.c_str(), TRUE));
}

void DeleteStartupShortcut() {
    std::wstring shortcutPath = GetStartupShortcutPath();
    DeleteFileW(shortcutPath.c_str());
}

// ========== main window proc ==========

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = ID_TRAY_ICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAY;
        nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_TRAY_ICON));
        lstrcpyW(nid.szTip, L"Bing Wallpaper");
        Shell_NotifyIcon(NIM_ADD, &nid);

        // Set timer to 1 day
        SetTimer(hwnd, ID_TIMER, 24 * 60 * 60 * 1000, NULL);

        // Update once at startup
        UpdateWallpaper();
        break;

    case WM_TRAY:
        if (lParam == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_UPDATE, L"Update Now");

            UINT autoStartFlags = MF_STRING;
            if (ShortcutExists())
                autoStartFlags |= MF_CHECKED;
            AppendMenu(hMenu, autoStartFlags, ID_TRAY_AUTOSTART, L"Start with Windows");

            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

            POINT pt; GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_UPDATE:
            UpdateWallpaper();
            break;
        case ID_TRAY_AUTOSTART:
            if (ShortcutExists()) {
                DeleteStartupShortcut();
            } else {
                CoInitialize(NULL);
                CreateStartupShortcut();
                CoUninitialize();
            }
            break;
        case ID_TRAY_EXIT:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_TIMER:
        if (wParam == ID_TIMER) {
            UpdateWallpaper();
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ========== WinMain ==========

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BingTrayClass";
    RegisterClassEx(&wc);

    hWnd = CreateWindowEx(0, L"BingTrayClass", L"BingTray",
        0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}