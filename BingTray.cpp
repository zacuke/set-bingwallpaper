#include <windows.h>
#include <shellapi.h>
#include <urlmon.h>
#include <wininet.h>
#include <string>
#include <sstream>
#include "resource.h"   // include resource definitions
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")

#define WM_TRAY        (WM_USER + 1)
#define ID_TRAY_EXIT   100
#define ID_TRAY_UPDATE 101   // new menu item
#define ID_TRAY_ICON   1
#define ID_TIMER       1

NOTIFYICONDATA nid = {};
HWND hWnd;

// ===== helper functions stay the same =====

bool DownloadFile(const std::wstring& url, const std::wstring& path) {
    return URLDownloadToFileW(NULL, url.c_str(), path.c_str(), 0, NULL) == S_OK;
}

std::wstring GetBingImageUrl() {
    HINTERNET hInternet = InternetOpen(L"BingTray", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return L"";
    HINTERNET hConnect = InternetOpenUrl(hInternet,
        L"https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=1&mkt=en-US",
        NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return L""; }
    char buffer[4096];
    DWORD read;
    std::string data;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &read) && read > 0) {
        data.append(buffer, buffer + read);
    }
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    size_t pos = data.find("\"url\":\"");
    if (pos == std::string::npos) return L"";
    pos += 7;
    size_t end = data.find("\"", pos);
    std::string urlPart = data.substr(pos, end - pos);
    std::wstring fullUrl = L"https://www.bing.com" + std::wstring(urlPart.begin(), urlPart.end());
    return fullUrl;
}

void SetWallpaper(const std::wstring& file) {
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)file.c_str(),
        SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE);
}

void UpdateWallpaper() {
    std::wstring url = GetBingImageUrl();
    if (url.empty()) return;
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"bingwallpaper.jpg");
    if (DownloadFile(url, path)) {
        SetWallpaper(path);
    }
}

// ===== main window proc =====

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = ID_TRAY_ICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAY;
        nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_TRAY_ICON)); // embedded custom icon
        lstrcpyW(nid.szTip, L"Bing Wallpaper");
        Shell_NotifyIcon(NIM_ADD, &nid);

        SetTimer(hwnd, ID_TIMER, 30 * 1000, NULL);
        UpdateWallpaper();
        break;

    case WM_TRAY:
        if (lParam == WM_RBUTTONUP) {
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_UPDATE, L"Update Now");
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

// ===== WinMain =====

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