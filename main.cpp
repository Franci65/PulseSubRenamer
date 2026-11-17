#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

enum ControlID {
    ID_BTN_REN_VID = 101,
    ID_BTN_ADD_FILES = 102,
    ID_BTN_ADD_FOLDER = 103,
    ID_BTN_REN_SUB = 104
};

const COLORREF COL_BG      = RGB(20, 21, 25);    // #141519
const COLORREF COL_PANEL   = RGB(26, 28, 34);    // #1A1C22
const COLORREF COL_HEADER  = RGB(32, 35, 43);    // #20232B
const COLORREF COL_BORDER  = RGB(48, 52, 64);    // #303440
const COLORREF COL_SEL     = RGB(42, 56, 82);    // #2A3852
const COLORREF COL_ACCENT  = RGB(59, 130, 246);  // #3B82F6
const COLORREF COL_ACC_HOV = RGB(37, 99, 235);   // #2563EB
const COLORREF COL_BTN     = RGB(38, 41, 50);    // #262932
const COLORREF COL_BTN_HOV = RGB(52, 56, 68);    // #343844
const COLORREF COL_TEXT    = RGB(228, 231, 238); // #E4E7EE
const COLORREF COL_MUTED   = RGB(138, 145, 160); // #8A91A0

struct FileItem {
    fs::path fullPath;
    std::wstring displayName;
};

std::vector<FileItem> g_videos;
std::vector<FileItem> g_subs;
int g_activePane = 0;
std::array<int, 2> g_selIdx = { -1, -1 };
int g_scrollY = 0;
int g_hoverBtn = 0;
bool g_isDragging = false;
bool g_isDone = false;
int g_dragStartY = 0;
std::wstring g_status = L"Drop video/subtitle files or folders here, or use + Add Files / + Add Folder.";

HWND g_hWnd = nullptr;
HWND g_btnRenVid = nullptr, g_btnAddFiles = nullptr, g_btnAddFolder = nullptr, g_btnRenSub = nullptr;
HFONT g_fontUI = nullptr, g_fontBold = nullptr;
RECT g_rcLeftPane{}, g_rcRightPane{};

const int ROW_H = 26;
const int HDR_H = 30;
const int TOP_H = 56;
const int BOT_H = 30;

bool IsVideoExt(const std::wstring& ext) {
    static const wchar_t* exts[] = { L".mkv", L".mp4", L".avi", L".mov", L".wmv", L".m4v", L".webm", L".ts" };
    for (auto e : exts) if (_wcsicmp(ext.c_str(), e) == 0) return true;
    return false;
}

bool IsSubExt(const std::wstring& ext) {
    static const wchar_t* exts[] = { L".srt", L".ass", L".ssa", L".sub", L".vtt", L".idx" };
    for (auto e : exts) if (_wcsicmp(ext.c_str(), e) == 0) return true;
    return false;
}

void SortLists() {
    auto cmp = [](const FileItem& a, const FileItem& b) {
        return StrCmpLogicalW(a.displayName.c_str(), b.displayName.c_str()) < 0;
    };
    std::sort(g_videos.begin(), g_videos.end(), cmp);
    std::sort(g_subs.begin(), g_subs.end(), cmp);
}

void AddPath(const fs::path& p, bool scanDir = true) {
    std::error_code ec;
    if (fs::is_directory(p, ec)) {
        if (!scanDir) return;
        for (const auto& entry : fs::directory_iterator(p, ec)) {
            if (entry.is_regular_file(ec)) AddPath(entry.path(), false);
        }
        return;
    }
    if (!fs::is_regular_file(p, ec)) return;
    std::wstring ext = p.extension().wstring();
    FileItem item{ p, p.filename().wstring() };
    auto exists = [&](const std::vector<FileItem>& vec) {
        for (const auto& x : vec) if (x.fullPath == p) return true;
        return false;
    };
    if (IsVideoExt(ext) && !exists(g_videos)) g_videos.push_back(item);
    else if (IsSubExt(ext) && !exists(g_subs)) g_subs.push_back(item);
}
void UpdateStatus(const std::wstring& msg) {
    g_status = msg;
    InvalidateRect(g_hWnd, nullptr, FALSE);
}

void DoAddFiles() {
    if (g_isDone) return;
    std::vector<wchar_t> buf(65536, 0);
    OPENFILENAMEW ofn{ sizeof(ofn) };
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = L"Video & Subtitles\0*.mkv;*.mp4;*.avi;*.mov;*.wmv;*.m4v;*.webm;*.ts;*.srt;*.ass;*.ssa;*.sub;*.vtt\0All Files\0*.*\0";
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = (DWORD)buf.size();
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    wchar_t* ptr = buf.data();
    fs::path dir = ptr;
    ptr += wcslen(ptr) + 1;
    if (*ptr == 0) AddPath(dir, false);
    else { while (*ptr) { AddPath(dir / ptr, false); ptr += wcslen(ptr) + 1; } }
    SortLists();
    UpdateStatus(L"Loaded: " + std::to_wstring(g_videos.size()) + L" videos, " + std::to_wstring(g_subs.size()) + L" subtitles.");
}

void DoAddFolder() {
    if (g_isDone) return;
    IFileOpenDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
        DWORD opts; pfd->GetOptions(&opts); pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        if (SUCCEEDED(pfd->Show(g_hWnd))) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) { AddPath(path, true); CoTaskMemFree(path); }
                psi->Release();
            }
        }
        pfd->Release();
    }
    SortLists();
    UpdateStatus(L"Loaded: " + std::to_wstring(g_videos.size()) + L" videos, " + std::to_wstring(g_subs.size()) + L" subtitles.");
}

void ExecuteRename(bool renameSubtitles) {
    if (g_isDone) { PostQuitMessage(0); return; }
    if (g_videos.empty() || g_videos.size() != g_subs.size()) {
        MessageBoxW(g_hWnd, L"Videos and subtitles do not match.", L"PulseSubRenamer", MB_OK | MB_ICONWARNING);
        return;
    }
    int okCount = 0;
    std::error_code ec;
    for (size_t i = 0; i < g_videos.size(); ++i) {
        auto& target = renameSubtitles ? g_subs.at(i) : g_videos.at(i);
        const auto& source = renameSubtitles ? g_videos.at(i) : g_subs.at(i);
        fs::path newPath = target.fullPath.parent_path() / (source.fullPath.stem().wstring() + target.fullPath.extension().wstring());
        if (target.fullPath != newPath) {
            fs::rename(target.fullPath, newPath, ec);
            if (!ec) { target.fullPath = newPath; target.displayName = newPath.filename().wstring(); okCount++; }
        }
    }
    g_isDone = true;
    g_selIdx.fill(-1);
    SetWindowTextW(g_btnRenSub, L"Exit");
    EnableWindow(g_btnRenVid, FALSE);
    EnableWindow(g_btnAddFiles, FALSE);
    EnableWindow(g_btnAddFolder, FALSE);
    UpdateStatus(L"Done! Renamed " + std::to_wstring(okCount) + L" files. Click Exit to close.");
}

void LayoutControls(int w, int h) {
    int pad = 14, btnW = 136, btnH = 32, topY = 12;
    MoveWindow(g_btnRenVid, pad, topY, btnW, btnH, TRUE);
    MoveWindow(g_btnRenSub, w - pad - btnW, topY, btnW, btnH, TRUE);
    int midX = (w - (btnW * 2 + 10)) / 2;
    MoveWindow(g_btnAddFiles, midX, topY, btnW, btnH, TRUE);
    MoveWindow(g_btnAddFolder, midX + btnW + 10, topY, btnW, btnH, TRUE);
    int paneW = (w - pad * 2) / 2, paneH = h - TOP_H - BOT_H;
    g_rcLeftPane  = { pad, TOP_H, pad + paneW + 1, TOP_H + paneH };
    g_rcRightPane = { pad + paneW, TOP_H, w - pad, TOP_H + paneH };
}


void DrawPane(HDC hdc, const RECT& rc, int paneIdx, const wchar_t* title) {
    HBRUSH brPanel = CreateSolidBrush(COL_PANEL); FillRect(hdc, &rc, brPanel); DeleteObject(brPanel);
    RECT rcHdr = { rc.left, rc.top, rc.right, rc.top + HDR_H };
    HBRUSH brHdr = CreateSolidBrush(COL_HEADER); FillRect(hdc, &rcHdr, brHdr); DeleteObject(brHdr);

    SelectObject(hdc, g_fontBold);
    SetTextColor(hdc, (!g_isDone && g_activePane == paneIdx) ? COL_ACCENT : COL_MUTED);
    RECT rcHdrTxt = { rcHdr.left + 12, rcHdr.top, rcHdr.right - 12, rcHdr.bottom };
    DrawTextW(hdc, title, -1, &rcHdrTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HRGN rgn = CreateRectRgn(rc.left + 1, rc.top + HDR_H, rc.right - 1, rc.bottom - 1);
    SelectClipRgn(hdc, rgn);
    SelectObject(hdc, g_fontUI);

    HPEN penRow = CreatePen(PS_SOLID, 1, RGB(34, 37, 45));
    HGDIOBJ oldRowPen = SelectObject(hdc, penRow);

    const auto& list = (paneIdx == 0) ? g_videos : g_subs;
    const auto& other = (paneIdx == 0) ? g_subs : g_videos;
    bool canPreview = !g_isDone && !g_videos.empty() && (g_videos.size() == g_subs.size());
    int previewPane = (g_hoverBtn == ID_BTN_REN_VID) ? 0 : 1;

    for (int i = 0; i < (int)list.size(); ++i) {
        int y = rc.top + HDR_H + i * ROW_H - g_scrollY;
        if (y + ROW_H < rc.top + HDR_H || y > rc.bottom) continue;
        RECT rcRow = { rc.left + 1, y, rc.right - 1, y + ROW_H };
        if (!g_isDone && g_selIdx.at(paneIdx) == i) {
            HBRUSH brSel = CreateSolidBrush(COL_SEL); FillRect(hdc, &rcRow, brSel); DeleteObject(brSel);
        }
        MoveToEx(hdc, rcRow.left, rcRow.bottom - 1, nullptr);
        LineTo(hdc, rcRow.right, rcRow.bottom - 1);

        std::wstring text = list.at(i).displayName;
        bool willRename = false;
        if (canPreview && paneIdx == previewPane) {
            std::wstring newName = other.at(i).fullPath.stem().wstring() + list.at(i).fullPath.extension().wstring();
            willRename = (newName != text);
        }
        SetTextColor(hdc, willRename ? COL_ACCENT : COL_TEXT);
        RECT rcTxt = { rcRow.left + 12, rcRow.top, rcRow.right - 12, rcRow.bottom };
        DrawTextW(hdc, text.c_str(), -1, &rcTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    SelectObject(hdc, oldRowPen); DeleteObject(penRow);
    SelectClipRgn(hdc, nullptr); DeleteObject(rgn);

    HPEN pen = CreatePen(PS_SOLID, 1, (!g_isDone && g_activePane == paneIdx) ? COL_ACCENT : COL_BORDER);
    HGDIOBJ oldPen = SelectObject(hdc, pen), oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBr); SelectObject(hdc, oldPen); DeleteObject(pen);
}

void DrawButton(DRAWITEMSTRUCT* dis) {
    bool isDisabled = (dis->itemState & ODS_DISABLED) != 0;
    bool isAccent = (dis->CtlID == ID_BTN_REN_SUB);
    bool isHov = (!isDisabled && g_hoverBtn == (int)dis->CtlID);
    COLORREF bg = isDisabled ? COL_PANEL : (isAccent ? (isHov ? COL_ACC_HOV : COL_ACCENT) : (isHov ? COL_BTN_HOV : COL_BTN));

    HBRUSH br = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, isAccent ? COL_ACCENT : COL_BORDER);
    SelectObject(dis->hDC, br); SelectObject(dis->hDC, pen);
    RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom, 6, 6);
    DeleteObject(br); DeleteObject(pen);

    std::wstring text(64, L'\0');
    text.resize(GetWindowTextW(dis->hwndItem, text.data(), 64));
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, isDisabled ? COL_MUTED : (isAccent ? RGB(255, 255, 255) : COL_TEXT));
    SelectObject(dis->hDC, isAccent ? g_fontBold : g_fontUI);
    DrawTextW(dis->hDC, text.c_str(), -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void HandleKeyDown(WPARAM vk) {
    if (g_isDone) {
        if (vk == VK_ESCAPE || vk == VK_RETURN) PostQuitMessage(0);
        return;
    }
    if (vk == VK_ESCAPE) {
        g_videos.clear(); g_subs.clear(); g_selIdx.fill(-1); g_scrollY = 0;
        UpdateStatus(L"Lists cleared. Drop video/subtitle files or folders here.");
        return;
    }
    auto& list = (g_activePane == 0) ? g_videos : g_subs;
    int& sel = g_selIdx.at(g_activePane);
    if (list.empty()) return;

    if (vk == VK_DELETE && sel >= 0 && sel < (int)list.size()) {
        list.erase(list.begin() + sel);
        if (sel >= (int)list.size()) sel = (int)list.size() - 1;
        UpdateStatus(L"Removed from list. Total: " + std::to_wstring(g_videos.size()) + L" videos, " + std::to_wstring(g_subs.size()) + L" subtitles.");
    } else if ((GetKeyState(VK_MENU) & 0x8000) && (vk == VK_UP || vk == VK_DOWN) && sel >= 0) {
        int target = (vk == VK_UP) ? sel - 1 : sel + 1;
        if (target >= 0 && target < (int)list.size()) { std::swap(list.at(sel), list.at(target)); sel = target; InvalidateRect(g_hWnd, nullptr, FALSE); }
    } else if (vk == VK_UP && sel > 0) { sel--; InvalidateRect(g_hWnd, nullptr, FALSE); }
    else if (vk == VK_DOWN && sel + 1 < (int)list.size()) { sel++; InvalidateRect(g_hWnd, nullptr, FALSE); }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: LayoutControls(LOWORD(lParam), HIWORD(lParam)); InvalidateRect(hWnd, nullptr, FALSE); return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_REN_VID) ExecuteRename(false);
        else if (LOWORD(wParam) == ID_BTN_REN_SUB) ExecuteRename(true);
        else if (LOWORD(wParam) == ID_BTN_ADD_FILES) DoAddFiles();
        else if (LOWORD(wParam) == ID_BTN_ADD_FOLDER) DoAddFolder();
        SetFocus(hWnd); return 0;
    case WM_DRAWITEM: DrawButton((DRAWITEMSTRUCT*)lParam); return TRUE;
    case WM_SETCURSOR: {
        HWND hT = (HWND)wParam;
        int nh = (hT == g_btnRenVid) ? ID_BTN_REN_VID : (hT == g_btnAddFiles) ? ID_BTN_ADD_FILES : (hT == g_btnAddFolder) ? ID_BTN_ADD_FOLDER : (hT == g_btnRenSub) ? ID_BTN_REN_SUB : 0;
        if (nh != g_hoverBtn) {
            g_hoverBtn = nh;
            InvalidateRect(g_btnRenVid, nullptr, FALSE); InvalidateRect(g_btnAddFiles, nullptr, FALSE);
            InvalidateRect(g_btnAddFolder, nullptr, FALSE); InvalidateRect(g_btnRenSub, nullptr, FALSE);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        break;
    }
    case WM_DROPFILES: {
        if (g_isDone) return 0;
        HDROP hDrop = (HDROP)wParam; UINT cnt = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < cnt; ++i) { wchar_t path[MAX_PATH]; if (DragQueryFileW(hDrop, i, path, MAX_PATH)) AddPath(path, true); }
        DragFinish(hDrop); SortLists();
        UpdateStatus(L"Loaded: " + std::to_wstring(g_videos.size()) + L" videos, " + std::to_wstring(g_subs.size()) + L" subtitles.");
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        if (g_isDone) return 0;
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (PtInRect(&g_rcLeftPane, pt) || PtInRect(&g_rcRightPane, pt)) {
            g_activePane = PtInRect(&g_rcLeftPane, pt) ? 0 : 1;
            const RECT& rc = (g_activePane == 0) ? g_rcLeftPane : g_rcRightPane;
            auto& list = (g_activePane == 0) ? g_videos : g_subs;
            int idx = (pt.y - (rc.top + HDR_H) + g_scrollY) / ROW_H;
            int validIdx = (idx >= 0 && idx < (int)list.size()) ? idx : -1;
            g_selIdx.at(g_activePane) = validIdx;
            if (msg == WM_LBUTTONDOWN && validIdx >= 0) {
                g_isDragging = true; g_dragStartY = pt.y; SetCapture(hWnd);
            } else if (msg == WM_RBUTTONDOWN && validIdx >= 0) {
                InvalidateRect(hWnd, nullptr, FALSE);
                HMENU hMenu = CreatePopupMenu(); AppendMenuW(hMenu, MF_STRING, 1, L"Remove from list\tDel");
                POINT scrPt = pt; ClientToScreen(hWnd, &scrPt);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, scrPt.x, scrPt.y, 0, hWnd, nullptr);
                DestroyMenu(hMenu);
                if (cmd == 1) {
                    list.erase(list.begin() + validIdx);
                    g_selIdx.at(g_activePane) = std::min(validIdx, (int)list.size() - 1);
                    UpdateStatus(L"Removed from list. Total: " + std::to_wstring(g_videos.size()) + L" videos, " + std::to_wstring(g_subs.size()) + L" subtitles.");
                }
            }
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        SetFocus(hWnd); return 0;
    }
    case WM_MOUSEMOVE:
        if (!g_isDone && g_isDragging) {
            int& sel = g_selIdx.at(g_activePane); auto& list = (g_activePane == 0) ? g_videos : g_subs;
            if (sel >= 0 && !list.empty()) {
                int y = (short)HIWORD(lParam);
                const RECT& rc = (g_activePane == 0) ? g_rcLeftPane : g_rcRightPane;
                int target = std::clamp((int)((y - (rc.top + HDR_H) + g_scrollY) / ROW_H), 0, (int)list.size() - 1);
                if (target != sel) {
                    FileItem moved = std::move(list.at(sel)); list.erase(list.begin() + sel);
                    list.insert(list.begin() + target, std::move(moved)); sel = target; InvalidateRect(hWnd, nullptr, FALSE);
                }
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_isDragging) { g_isDragging = false; ReleaseCapture(); InvalidateRect(hWnd, nullptr, FALSE); }
        return 0;
    case WM_MOUSEWHEEL: {
        int maxItems = (int)std::max(g_videos.size(), g_subs.size());
        int maxScroll = std::max(0, (int)(maxItems * ROW_H - ((g_rcLeftPane.bottom - g_rcLeftPane.top) - HDR_H)));
        g_scrollY = std::clamp(g_scrollY - (GET_WHEEL_DELTA_WPARAM(wParam) / 120) * ROW_H * 2, 0, maxScroll);
        InvalidateRect(hWnd, nullptr, FALSE); return 0;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: HandleKeyDown(wParam); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps); RECT rc; GetClientRect(hWnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc); HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom); SelectObject(memDC, memBmp);
        HBRUSH brBg = CreateSolidBrush(COL_BG); FillRect(memDC, &rc, brBg); DeleteObject(brBg); SetBkMode(memDC, TRANSPARENT);
                if (g_activePane == 0) {
            DrawPane(memDC, g_rcRightPane, 1, L"SUBTITLE FILES");
            DrawPane(memDC, g_rcLeftPane, 0, L"VIDEO FILES");
        } else {
            DrawPane(memDC, g_rcLeftPane, 0, L"VIDEO FILES");
            DrawPane(memDC, g_rcRightPane, 1, L"SUBTITLE FILES");
        }

        SelectObject(memDC, g_fontUI); SetTextColor(memDC, COL_MUTED);
        RECT rcBotL = { 14, rc.bottom - BOT_H, rc.right / 2 + 10, rc.bottom };
        RECT rcBotR = { rc.right / 2 + 10, rc.bottom - BOT_H, rc.right - 14, rc.bottom };
        DrawTextW(memDC, g_status.c_str(), -1, &rcBotL, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        DrawTextW(memDC, g_isDone ? L"Renaming complete  |  Click Exit or press Esc to close" : L"Drag / Alt+Up/Down = Reorder  |  Right-Click / Del = Remove  |  Esc = Clear", -1, &rcBotR, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY); DeleteObject(memBmp); DeleteDC(memDC); EndPaint(hWnd, &ps); return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSEXW wc{ sizeof(wc), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst, LoadIcon(hInst, MAKEINTRESOURCE(1)), LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr, L"PulseSubRenamerCls", nullptr };
    RegisterClassExW(&wc);
    g_fontUI = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBold = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_hWnd = CreateWindowExW(WS_EX_ACCEPTFILES, wc.lpszClassName, L"PulseSubRenamer", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1080, 560, nullptr, nullptr, hInst, nullptr);
    BOOL dark = TRUE; DwmSetWindowAttribute(g_hWnd, 20, &dark, sizeof(dark));
    auto mkBtn = [&](const wchar_t* txt, int id) { return CreateWindowW(L"BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, g_hWnd, (HMENU)(INT_PTR)id, hInst, nullptr); };
    g_btnRenVid = mkBtn(L"Rename Videos", ID_BTN_REN_VID); g_btnAddFiles = mkBtn(L"+ Add Files", ID_BTN_ADD_FILES);
    g_btnAddFolder = mkBtn(L"+ Add Folder", ID_BTN_ADD_FOLDER); g_btnRenSub = mkBtn(L"Rename Subtitles", ID_BTN_REN_SUB);
    ShowWindow(g_hWnd, nCmdShow);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
