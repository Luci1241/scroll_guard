// ScrollGuard.cpp (v4 – NOMINMAX fix, clean strings, robust input; list or hover‑select)
// Purpose: When your chosen app is the foreground window, block mouse-wheel
// events from scrolling other (inactive) windows on other monitors.
// When you Alt+Tab away, everything scrolls normally again.
//
// Build in "Developer Command Prompt for VS":
//   cl /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE ScrollGuard.cpp user32.lib kernel32.lib psapi.lib
// Run:
//   ScrollGuard.exe
// Exit:
//   Press Ctrl+C in the console.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX // avoid Windows macros clobbering std::numeric_limits::max
#include <windows.h>
#include <psapi.h>

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <limits>

struct AppEntry {
  HWND hwnd{};
  DWORD pid{};
  std::wstring processName;
  std::wstring windowTitle;
};

// Globals for the hook
static HHOOK g_mouseHook = nullptr;
static DWORD g_targetPid = 0;             // The process we protect when in foreground
static volatile bool g_running = true;

// Get base process name from PID
static std::wstring GetProcessNameFromPid(DWORD pid) {
  std::wstring name = L"(unknown)";
  HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (hProc) {
    wchar_t buf[MAX_PATH] = {};
    if (GetModuleBaseNameW(hProc, nullptr, buf, MAX_PATH)) {
      name = buf;
    } else {
      DWORD sz = MAX_PATH;
      if (QueryFullProcessImageNameW(hProc, 0, buf, &sz)) {
        std::wstring full = buf;
        // Find last path separator (either '\' or '/')
        size_t p1 = full.find_last_of(L'\\');
        size_t p2 = full.find_last_of(L'/');
        size_t pos = (p1 == std::wstring::npos) ? p2 : (p2 == std::wstring::npos ? p1 : (p1 > p2 ? p1 : p2));
        name = (pos == std::wstring::npos) ? full : full.substr(pos + 1);
      }
    }
    CloseHandle(hProc);
  }
  return name;
}

// Collect a list of visible top-level windows (one entry per PID)
static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  if (!IsWindowVisible(hwnd)) return TRUE; // only consider visible top-level windows

  std::vector<AppEntry>& out = *(std::vector<AppEntry>*)lParam;

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (!pid) return TRUE;

  // De-duplicate by PID (prefer the first window we find)
  auto it = std::find_if(out.begin(), out.end(), [pid](const AppEntry& e){ return e.pid == pid; });
  if (it != out.end()) return TRUE;

  // Title (allow empty titles — common for borderless games)
  int len = GetWindowTextLengthW(hwnd);
  std::wstring title;
  if (len > 0) {
    title.resize(static_cast<size_t>(len) + 1);
    int written = GetWindowTextW(hwnd, &title[0], static_cast<int>(title.size()));
    if (written < 0) written = 0;
    title.resize(static_cast<size_t>(written));
    if (title.empty()) title = L"[No Title]";
  } else {
    title = L"[No Title]";
  }

  AppEntry e{};
  e.hwnd = hwnd;
  e.pid = pid;
  e.processName = GetProcessNameFromPid(pid);
  e.windowTitle = std::move(title);
  out.push_back(std::move(e));
  return TRUE;
}

static std::vector<AppEntry> EnumerateApps() {
  std::vector<AppEntry> apps;
  apps.reserve(256);
  EnumWindows(EnumWindowsProc, (LPARAM)&apps);

  // Sort by process name then title for a stable, readable list
  std::sort(apps.begin(), apps.end(), [](const AppEntry& a, const AppEntry& b){
    int c = _wcsicmp(a.processName.c_str(), b.processName.c_str());
    if (c != 0) return c < 0;
    return _wcsicmp(a.windowTitle.c_str(), b.windowTitle.c_str()) < 0;
  });
  return apps;
}

// Return the PID of the top-level window under the cursor point
static DWORD PidFromPoint(POINT pt) {
  HWND h = WindowFromPoint(pt);
  if (!h) return 0;
  HWND root = GetAncestor(h, GA_ROOT);
  if (!root) root = h;
  DWORD pid = 0;
  GetWindowThreadProcessId(root, &pid);
  return pid;
}

// Low-level mouse hook: swallow wheel events when target app is focused and mouse is NOT over it
static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == HC_ACTION && g_targetPid != 0) {
    if (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) {
      HWND fg = GetForegroundWindow();
      DWORD fgPid = 0;
      if (fg) GetWindowThreadProcessId(fg, &fgPid);

      if (fgPid == g_targetPid) {
        const MSLLHOOKSTRUCT* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        POINT pt = info->pt; // screen coords
        DWORD underPid = PidFromPoint(pt);
        if (underPid != g_targetPid) {
          return 1; // block event globally for other apps
        }
      }
    }
  }
  return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

// Clean shutdown on Ctrl+C
static BOOL WINAPI ConsoleCtrlHandler(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
    g_running = false;
    if (g_mouseHook) { UnhookWindowsHookEx(g_mouseHook); g_mouseHook = nullptr; }
    return TRUE;
  }
  return FALSE;
}

static void FlushInputLine() {
  // Clear through newline to allow subsequent getline()
  std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
}

// Helper: hover-select PID under the mouse
static DWORD HoverSelectPid() {
  std::wcout << L"\nHover your mouse over the target app (its main window) and press Enter...\n";
  std::wstring dummy;
  std::getline(std::wcin, dummy); // wait for Enter
  POINT pt{};
  GetCursorPos(&pt);
  DWORD pid = PidFromPoint(pt);
  if (pid == 0) {
    std::wcerr << L"Could not resolve a window under the cursor. Try again with the window visible." << std::endl;
  }
  return pid;
}
int wmain() {
  std::wcout << L"ScrollGuard - block inactive-window scrolling when your chosen app is focused\n";
  std::wcout << L"--------------------------------------------------------------------------------\n\n";

  // 1) Enumerate candidates and let the user pick (or hover-select fallback)
  auto apps = EnumerateApps();

  if (!apps.empty()) {
    std::wcout << L"Pick the application to protect (enter the number).\n";
    std::wcout << L"Or type 0 to use Hover-Select.\n\n";
    for (size_t i = 0; i < apps.size(); ++i) {
      std::wcout << std::setw(3) << i + 1 << L". "
                 << apps[i].processName << L"  -  " << apps[i].windowTitle << L"\n";
    }
    std::wcout << L"\nSelection (0 for Hover-Select): ";
    size_t choice = 0;
    if (!(std::wcin >> choice)) { std::wcerr << L"Invalid input." << std::endl; return 2; }
    FlushInputLine(); // eat trailing newline

    if (choice == 0) {
      g_targetPid = HoverSelectPid();
      if (g_targetPid == 0) return 2;
    } else if (choice >= 1 && choice <= apps.size()) {
      g_targetPid = apps[choice - 1].pid;
    } else {
      std::wcerr << L"Invalid selection." << std::endl; return 2;
    }
  } else {
    std::wcout << L"No visible apps found to list. We'll use Hover-Select instead." << std::endl;
    g_targetPid = HoverSelectPid();
    if (g_targetPid == 0) return 2;
  }

  std::wcout << L"\nMonitoring PID: " << g_targetPid
             << L" (" << GetProcessNameFromPid(g_targetPid) << L")\n";
  std::wcout << L"When this app is in the foreground, scrolling over other apps will be blocked." << std::endl;
  std::wcout << L"Press Ctrl+C to quit.\n" << std::endl;

  // 2) Install the low-level mouse hook
  SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

  g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
  if (!g_mouseHook) {
    std::wcerr << L"Failed to install mouse hook." << std::endl;
    return 3;
  }

  // 3) Standard message loop to keep the hook alive
  MSG msg;
  while (g_running && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  if (g_mouseHook) { UnhookWindowsHookEx(g_mouseHook); g_mouseHook = nullptr; }
  std::wcout << L"Goodbye." << std::endl;
  return 0;
}
