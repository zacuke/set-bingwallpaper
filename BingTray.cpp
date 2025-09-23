#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shellapi.h>
#include <urlmon.h>
#include <wininet.h>
#include <string>
#include <sstream>
#include <shlobj.h>
#include <shobjidl.h>  // COM interfaces
#include <objbase.h>   // CoInitializeEx, CoCreateInstance
#include <cstdio>      // _wfopen, fwprintf

#include "resource.h"

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")

#define WM_TRAY           (WM_USER + 1)
#define ID_TRAY_EXIT      100
#define ID_TRAY_UPDATE    101
#define ID_TRAY_AUTOSTART 102
#define ID_TRAY_ICON      1
#define ID_TIMER          1
#define ID_IDLE_TIMER     2   // idle check timer every 10s

#define DO_DEBUG_LOG      0   // Set to 1 to enable debug logging

NOTIFYICONDATA nid = {};
HWND hWnd;

DWORD lastUpdateTick = 0;
bool wasIdle = false;

// ---------- logging helper ----------

std::wstring GetLogPath() {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"bingtray.log");
    return path;
}

void Log(const std::wstring& msg) {
    if (!DO_DEBUG_LOG) return;

    FILE* fp = _wfopen(GetLogPath().c_str(), L"a, ccs=UTF-8");
    if (!fp) return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    fwprintf(fp, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond,
             msg.c_str());
    fclose(fp);
}

// ---------- Bing helpers ----------

bool DownloadFile(const std::wstring& url, const std::wstring& path) {
    return URLDownloadToFileW(NULL, url.c_str(), path.c_str(), 0, NULL) == S_OK;
}

std::string DownloadUrlToString(const std::wstring& url) {
    HINTERNET hInternet = InternetOpen(L"BingTray", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";
    HINTERNET hConnect = InternetOpenUrl(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
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

// ---------- cache tracking ----------

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
    FILE* fp = _wfopen(path.c_str(), L"rt, ccs=UTF-8");
    if (!fp) return L"";
    wchar_t buffer[512]; std::wstring line;
    if (fgetws(buffer, 512, fp)) {
        line = buffer;
        while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r'))
            line.pop_back();
    }
    fclose(fp);
    return line;
}
void SaveLastUrlBase(const std::wstring& urlbase) {
    std::wstring path = GetMetaCachePath();
    FILE* fp = _wfopen(path.c_str(), L"wt, ccs=UTF-8");
    if (!fp) return;
    fputws(urlbase.c_str(), fp);
    fclose(fp);
}

// ---------- update logic ----------

void UpdateWallpaperThrottled() {
    DWORD now = GetTickCount();
    if (now - lastUpdateTick < 60 * 60 * 1000) {
        Log(L"Skipped update (throttled, less than 1h).");
        return;
    }
    lastUpdateTick = now;

    BingImageInfo info = GetBingImageInfo();
    if (info.fullUrl.empty() || info.urlBase.empty()) {
        Log(L"Failed to retrieve Bing image info.");
        return;
    }

    auto imagePath = GetImageCachePath();
    std::wstring lastBase = LoadLastUrlBase();
    if (info.urlBase == lastBase) {
        if (GetFileAttributesW(imagePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            SetWallpaper(imagePath);
            Log(L"Reused cached wallpaper, no download.");
        } else {
            Log(L"No image file found, redownloading.");
            if (DownloadFile(info.fullUrl, imagePath)) {
                SetWallpaper(imagePath);
                SaveLastUrlBase(info.urlBase);
                Log(L"Downloaded current wallpaper again.");
            }
        }
        return;
    }
    if (DownloadFile(info.fullUrl, imagePath)) {
        SetWallpaper(imagePath);
        SaveLastUrlBase(info.urlBase);
        Log(L"Downloaded NEW wallpaper and set it.");
    } else {
        Log(L"Failed to download NEW wallpaper.");
    }
}

// ---------- Startup shortcut ----------

std::wstring GetStartupShortcutPath() {
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, path);
    wcscat_s(path, L"\\BingTray.lnk");
    return path;
}
bool ShortcutExists() {
    return GetFileAttributesW(GetStartupShortcutPath().c_str()) != INVALID_FILE_ATTRIBUTES;
}
bool CreateStartupShortcut() {
    std::wstring shortcutPath = GetStartupShortcutPath();
    wchar_t exePath[MAX_PATH]; GetModuleFileNameW(NULL, exePath, MAX_PATH);

    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, (void**)&psl)))
        return false;
    psl->SetPath(exePath);

    IPersistFile* ppf = nullptr;
    bool success = false;
    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
        if (SUCCEEDED(ppf->Save(shortcutPath.c_str(), TRUE)))
            success = true;
        ppf->Release();
    }
    psl->Release();
    return success;
}
void DeleteStartupShortcut() {
    DeleteFileW(GetStartupShortcutPath().c_str());
}

// ---------- window proc ----------

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

        SetTimer(hwnd, ID_TIMER, 24 * 60 * 60 * 1000, NULL);   // daily
        SetTimer(hwnd, ID_IDLE_TIMER, 10 * 1000, NULL);        // every 10s
        Log(L"App started, running initial update...");
        UpdateWallpaperThrottled();
        break;

    case WM_TRAY:
        if (lParam == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_UPDATE,   L"Update Now");
            UINT autoStartFlags = MF_STRING;
            if (ShortcutExists()) autoStartFlags |= MF_CHECKED;
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
        case ID_TRAY_UPDATE: UpdateWallpaperThrottled(); break;
        case ID_TRAY_AUTOSTART:
            if (ShortcutExists()) {
                DeleteStartupShortcut();
                Log(L"Disabled autostart.");
            } else {
                if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))) {
                    if (CreateStartupShortcut()) Log(L"Enabled autostart.");
                    CoUninitialize();
                }
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
            Log(L"Daily timer fired, updating wallpaper...");
            UpdateWallpaperThrottled();
        } else if (wParam == ID_IDLE_TIMER) {
            LASTINPUTINFO li{ sizeof(LASTINPUTINFO) };
            if (GetLastInputInfo(&li)) {
                DWORD idleMs = GetTickCount() - li.dwTime;
                if (idleMs >= 15 * 60 * 1000) {
                    if (!wasIdle) { wasIdle = true; Log(L"User idle >=15 min."); }
                } else {
                    if (wasIdle) {
                        wasIdle = false;
                        Log(L"User active again, trigger update (throttled).");
                        UpdateWallpaperThrottled();
                    }
                }
            }
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        KillTimer(hwnd, ID_IDLE_TIMER);
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;

    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ---------- entry point ----------

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
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