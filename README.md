# ScrollGuard

Block accidental scrolling in other apps while your chosen game/app is focused.

**Use case:** Playing Arma 3 (or any game that uses the mouse wheel for interaction) while Discord/Chrome is open on a second monitor. With ScrollGuard running, when your game is in the foreground, your mouse wheel **won’t** scroll other windows by accident. Alt-Tab away and everything behaves normally again.

---

## Features

* **Foreground-aware:** Only active when your selected app is focused.
* **Global protection:** Cancels wheel events that would hit other apps/monitors.
* **No admin required:** Uses a low-level mouse hook; no drivers/services.
* **Two selection modes:**

  * **List pick:** Choose from visible apps (process name + window title).
  * **Hover-Select:** If a window doesn’t appear in the list (borderless games, etc.), hover the mouse over it and press Enter.

---

## How it works (short version)

* Installs a global `WH_MOUSE_LL` hook.
* On each wheel event, if your chosen app’s **PID** owns the **foreground window** **and** the cursor is **not over** that app, the event is swallowed (return `1`).
* If the app isn’t foreground, or the cursor is over the app, events pass through normally.

Only **wheel** events are touched; movement, clicks, keyboard (including Alt-Tab) are untouched.

---

## Requirements

* Windows 10/11 (x64 or x86)
* Microsoft Visual C++ toolchain (MSVC) + Windows SDK
  Install via **Visual Studio Installer** → *Desktop development with C++* (or **Build Tools for Visual Studio**).
* No external libraries

---

## Run & Use

1. Launch your game/app and any other windows (e.g., Discord, Chrome).
2. Run `ScrollGuard.exe`.
3. Choose your target app:

   * If a **numbered list** appears, type its number and press Enter.
   * If the list is empty or the app isn’t listed, type **0** for **Hover-Select**:

     * Hover your mouse over the game’s main window and press **Enter**.
4. Leave ScrollGuard running (console window open) while you play.

**Exit:** Press **Ctrl+C** in the console (or close the console window).

---

## Testing

* In Windows Settings → **Bluetooth & devices → Mouse**, enable **“Scroll inactive windows when I hover over them.”**
* Focus your game window (foreground). Move the cursor over Discord/Chrome on your second monitor and spin the wheel:

  * **Without ScrollGuard:** the inactive window scrolls.
  * **With ScrollGuard:** nothing scrolls in other apps while the game is foreground.
* Alt-Tab to Discord/Chrome and scroll again: it should work normally.

---

## Troubleshooting

* **No apps in list**
  Some borderless/fullscreen windows don’t report titles. Use **Hover-Select** (`0`), hover the mouse over the game, press Enter.

* **Still scrolling other apps while game is focused**
  Confirm the console shows the correct **PID / process name**. If you changed the game executable (e.g., x64 binary), re-run and reselect it.

* **Nothing happens at all**
  Ensure ScrollGuard is **not** running as admin while your game is not (or vice versa). Keep them at the same privilege level (usually **non-admin**).

* **Anti-cheat concerns**
  ScrollGuard uses a global mouse hook, which many anti-cheats allow, but policies vary. If your anti-cheat complains, close ScrollGuard before joining protected servers.

* **Minimized / background game**
  ScrollGuard only blocks while your selected app is the **foreground** window.

---

## Disclaimer

Use at your own risk. Tested on Windows 11 with common apps (Chrome, Discord) and borderless/fullscreen games. Behavior can vary with overlays or special input modes.
