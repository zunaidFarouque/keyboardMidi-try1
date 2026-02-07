#include "RawInputManager.h"
#include "MappingTypes.h"
#include "PointerInputManager.h"
#include "SettingsManager.h"
#include "TouchpadHidParser.h"

#include <algorithm>

// 1. Windows Header Only
// (We removed the #defines because CMake already sets them)
#include <windows.h>

// Static storage (file-scope for safety during destruction)
static WNDPROC globalOriginalWndProc =
    nullptr; // Store the old proc here (survives class destruction)
RawInputManager *RawInputManager::globalManagerInstance = nullptr;

namespace {
bool isPrecisionTouchpadDevice(HANDLE deviceHandle) {
  if (deviceHandle == nullptr)
    return false;

  RID_DEVICE_INFO deviceInfo{};
  deviceInfo.cbSize = sizeof(RID_DEVICE_INFO);
  UINT deviceInfoSize = deviceInfo.cbSize;

  if (GetRawInputDeviceInfo(deviceHandle, RIDI_DEVICEINFO, &deviceInfo,
                            &deviceInfoSize) == static_cast<UINT>(-1)) {
    return false;
  }

  return deviceInfo.dwType == RIM_TYPEHID &&
         deviceInfo.hid.usUsagePage == 0x000D &&
         deviceInfo.hid.usUsage == 0x0005;
}
} // namespace

// Helper class to forward pointer events to RawInputManager listeners
class RawInputManager::PointerEventForwarder
    : public PointerInputManager::Listener {
public:
  explicit PointerEventForwarder(RawInputManager *manager) : manager(manager) {}
  void onPointerEvent(uintptr_t device, int axisID, float value) override {
    if (manager) {
      manager->listeners.call(
          [device, axisID, value](RawInputManager::Listener &l) {
            l.handleAxisEvent(device, axisID, value);
          });
    }
  }

private:
  RawInputManager *manager;
};

RawInputManager::RawInputManager()
    : pointerInputManager(std::make_unique<PointerInputManager>()),
      pointerEventForwarder(std::make_unique<PointerEventForwarder>(this)) {
  pointerInputManager->addListener(pointerEventForwarder.get());
  // Note: globalManagerInstance is set in initialize() after window subclassing
}

RawInputManager::~RawInputManager() { shutdown(); }

void RawInputManager::initialize(void *nativeWindowHandle,
                                 SettingsManager *settingsMgr) {
  if (isInitialized || nativeWindowHandle == nullptr)
    return;

  HWND hwnd = static_cast<HWND>(nativeWindowHandle);
  targetHwnd = nativeWindowHandle;
  settingsManager = settingsMgr;

  // Log the Handle
  DBG("RawInputManager: Initializing with HWND: " +
      juce::String::toHexString((juce::int64)hwnd));

  // RAW INPUT REGISTRATION
  RAWINPUTDEVICE keyboardRid[1];

  // Page 1, Usage 6 = Keyboard
  keyboardRid[0].usUsagePage = 0x01;
  keyboardRid[0].usUsage = 0x06;

  // CRITICAL FIX: RIDEV_INPUTSINK enables background monitoring.
  // We also keep RIDEV_DEVNOTIFY to handle plug/unplug events.
  keyboardRid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;

  // CRITICAL FIX: Target must be explicit for InputSink to work.
  keyboardRid[0].hwndTarget = hwnd;

  if (RegisterRawInputDevices(keyboardRid, 1, sizeof(keyboardRid[0])) ==
      FALSE) {
    DWORD error = GetLastError();
    DBG("RawInputManager: CRITICAL ERROR - Registration Failed! Error Code: " +
        juce::String(error));
    return;
  } else {
    DBG("RawInputManager: Registration Success. Flags: RIDEV_INPUTSINK");
  }

  RAWINPUTDEVICE touchpadRid[1];
  touchpadRid[0].usUsagePage = 0x000D; // Digitizer
  touchpadRid[0].usUsage = 0x0005;     // Touch Pad
  touchpadRid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
  touchpadRid[0].hwndTarget = hwnd;

  if (RegisterRawInputDevices(touchpadRid, 1, sizeof(touchpadRid[0])) ==
      FALSE) {
    DWORD error = GetLastError();
    DBG("RawInputManager: Touchpad registration failed. Error Code: " +
        juce::String(error));
  } else {
    DBG("RawInputManager: Touchpad registration success.");
  }

  // Subclass the window
  LONG_PTR oldProc =
      SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)rawInputWndProc);
  globalOriginalWndProc = (WNDPROC)oldProc;

  // Store global pointer
  globalManagerInstance = this;

  if (globalOriginalWndProc) {
    isInitialized = true;
    DBG("RawInputManager: Window subclassed successfully. Initialized with "
        "RIDEV_INPUTSINK.");
  }
}

void RawInputManager::shutdown() {
  // 1. CUT THE CORD IMMEDIATELY
  // This prevents the static proc from touching 'this' ever again.
  globalManagerInstance = nullptr;

  // 2. Restore the window (Unsubclass)
  if (isInitialized && targetHwnd && globalOriginalWndProc) {
    HWND hwnd = static_cast<HWND>(targetHwnd);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)globalOriginalWndProc);
    isInitialized = false;
  }

  // 3. Clean up static state
  // globalOriginalWndProc = nullptr; // Optional, maybe keep it safe in case a
  // lagging message hits

  // Clear device key states
  deviceKeyStates.clear();
}

void RawInputManager::resetState() { deviceKeyStates.clear(); }

void RawInputManager::addListener(Listener *listener) {
  listeners.add(listener);
}

void RawInputManager::removeListener(Listener *listener) {
  listeners.remove(listener);
}

void RawInputManager::setFocusTargetCallback(std::function<void *()> cb) {
  focusTargetCallback = cb;
}

void RawInputManager::setOnDeviceChangeCallback(std::function<void()> cb) {
  onDeviceChangeCallback = cb;
}

// Helper function to force a window to the foreground, bypassing Windows
// restrictions
static void forceForegroundWindow(HWND hwnd) {
  if (GetForegroundWindow() == hwnd)
    return;

  HWND currentForeground = GetForegroundWindow();
  DWORD foregroundThread = 0;
  if (currentForeground != nullptr) {
    foregroundThread = GetWindowThreadProcessId(currentForeground, NULL);
  }
  DWORD appThread = GetCurrentThreadId();

  // 1. Restore if minimized
  if (IsIconic(hwnd)) {
    ShowWindow(hwnd, SW_RESTORE);
  }

  // 2. Attach threads to bypass focus lock
  if (foregroundThread != 0 && foregroundThread != appThread) {
    AttachThreadInput(foregroundThread, appThread, TRUE);

    // Trick: Sometimes sending a dummy ALT key helps unlock the state
    // keybd_event(VK_MENU, 0, 0, 0);
    // keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

    SetForegroundWindow(hwnd);
    SetFocus(hwnd); // Ensure keyboard focus too

    AttachThreadInput(foregroundThread, appThread, FALSE);
  } else {
    SetForegroundWindow(hwnd);
  }
}

int64_t __stdcall RawInputManager::rawInputWndProc(void *hwnd, unsigned int msg,
                                                   uint64_t wParam,
                                                   int64_t lParam) {
  // 1. SAFETY CHECK: If the class is dead, just pass through.
  if (globalManagerInstance == nullptr) {
    if (globalOriginalWndProc) {
      return CallWindowProc(globalOriginalWndProc, (HWND)hwnd, (UINT)msg,
                            (WPARAM)wParam, (LPARAM)lParam);
    }
    return DefWindowProc((HWND)hwnd, (UINT)msg, (WPARAM)wParam, (LPARAM)lParam);
  }

  // 2. Normal Processing
  if (msg == WM_INPUT) {
    // DIAGNOSTIC LOG
    // GET_RAWINPUT_CODE_WPARAM is a macro to extract the input type
    int code = GET_RAWINPUT_CODE_WPARAM(wParam);
    juce::String typeStr =
        (code == RIM_INPUT) ? "FOREGROUND"
                            : (code == RIM_INPUTSINK ? "BACKGROUND" : "OTHER");

    // Only log if it is BACKGROUND to prove it works (avoid flooding)
#if JUCE_DEBUG
    if (code == RIM_INPUTSINK) {
      DBG("RawInputManager: Received BACKGROUND Event! (wParam=" +
          juce::String((int)wParam) + ", code=" + juce::String(code) + ")");
    }
#endif

    // Focus Guard: If MIDI mode is active, steal focus when key is pressed
    if (globalManagerInstance && globalManagerInstance->settingsManager) {
      if (globalManagerInstance->settingsManager->isMidiModeActive()) {
        HWND shield = static_cast<HWND>(
            globalManagerInstance->targetHwnd); // Default to main window
        if (globalManagerInstance->focusTargetCallback) {
          void *callbackResult = globalManagerInstance->focusTargetCallback();
          if (callbackResult != nullptr) {
            shield = static_cast<HWND>(callbackResult);
          }
        }
        if (shield != nullptr) {
          forceForegroundWindow(shield);
        }
      }
    }

    UINT dwSize = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize,
                    sizeof(RAWINPUTHEADER));

    if (dwSize > 0) {
      auto *lpb = new BYTE[dwSize];
      if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize,
                          sizeof(RAWINPUTHEADER)) == dwSize) {
        auto *raw = (RAWINPUT *)lpb;

        if (raw->header.dwType == RIM_TYPEKEYBOARD) {
          HANDLE deviceHandle = raw->header.hDevice;
          UINT vKey = raw->data.keyboard.VKey;
          UINT makeCode = raw->data.keyboard.MakeCode;
          UINT flags = raw->data.keyboard.Flags;
          bool isDown = (flags & RI_KEY_BREAK) == 0;

          // --- VKEY REPAIR START ---
          /* TEMPORARILY DISABLED FOR STABILITY
          if (vKey == 0 || vKey == 0xFF) {
            // 1. Check Flags for Special Keys
            if (flags & RI_KEY_E1) {
              // Only Pause/Break uses E1
              vKey = VK_PAUSE;
            }
            else if (makeCode == 0x37 && (flags & RI_KEY_E0)) {
              // Print Screen (E0 + 37)
              vKey = VK_SNAPSHOT;
            }
            else if (makeCode == 0x45) {
              // Num Lock (Scan 45, No E1)
              vKey = VK_NUMLOCK;
            }
            else {
              // 2. Fallback to Windows Mapping
              // MAPVK_VSC_TO_VK_EX distinguishes Left/Right Shift/Ctrl/Alt
              vKey = MapVirtualKey(makeCode, MAPVK_VSC_TO_VK_EX);

              if (vKey == 0) vKey = MapVirtualKey(makeCode, MAPVK_VSC_TO_VK);
            }
          }
          */
          // --- VKEY REPAIR END ---

          if (globalManagerInstance) {
            // Check if MIDI mode is active - only broadcast for MIDI when
            // active (Toggle key detection still works because hook allows it
            // through)
            bool shouldBroadcast = false;
            if (globalManagerInstance->settingsManager) {
              // Always broadcast if MIDI mode is active
              // Also broadcast toggle key (turn off) and performance key (turn
              // on both) even when MIDI is off
              int toggleKey =
                  globalManagerInstance->settingsManager->getToggleKey();
              int perfKey =
                  globalManagerInstance->settingsManager->getPerformanceModeKey();
              shouldBroadcast =
                  globalManagerInstance->settingsManager->isMidiModeActive() ||
                  (vKey == toggleKey) || (vKey == perfKey);
            } else {
              // If no settings manager, always broadcast (backward
              // compatibility)
              shouldBroadcast = true;
            }

            if (shouldBroadcast) {
              uintptr_t handle = reinterpret_cast<uintptr_t>(deviceHandle);

              // Anti-ghosting and autorepeat filtering
              auto &stateSet = globalManagerInstance->deviceKeyStates[handle];

              if (isDown) {
                // Key Down event
                if (stateSet.find(vKey) != stateSet.end()) {
                  // Key is already in the set - this is an autorepeat, IGNORE
                  // Do not broadcast
                } else {
                  // Fresh press - add to set and broadcast
                  stateSet.insert(vKey);
                  globalManagerInstance->listeners.call(
                      [handle, vKey, isDown](Listener &l) {
                        l.handleRawKeyEvent(handle, vKey, isDown);
                      });
                }
              } else {
                // Key Up event
                if (stateSet.find(vKey) != stateSet.end()) {
                  // Valid release - remove from set and broadcast
                  stateSet.erase(vKey);
                  globalManagerInstance->listeners.call(
                      [handle, vKey, isDown](Listener &l) {
                        l.handleRawKeyEvent(handle, vKey, isDown);
                      });
                } else {
                  // Edge case: Key not in set (maybe app started with key held)
                  // Broadcast anyway to ensure NoteOff is sent
                  globalManagerInstance->listeners.call(
                      [handle, vKey, isDown](Listener &l) {
                        l.handleRawKeyEvent(handle, vKey, isDown);
                      });
                }
              }
            }
          }
        } else if (raw->header.dwType == RIM_TYPEMOUSE) {
          HANDLE deviceHandle = raw->header.hDevice;
          USHORT buttonFlags = raw->data.mouse.usButtonFlags;

          if (buttonFlags & RI_MOUSE_WHEEL) {
            SHORT wheelDelta = (SHORT)raw->data.mouse.usButtonData;

            if (globalManagerInstance) {
              uintptr_t handle = reinterpret_cast<uintptr_t>(deviceHandle);

              // Split scroll into discrete up/down events
              if (wheelDelta > 0) {
                // Scroll Up - treat as key press
                globalManagerInstance->listeners.call([handle](Listener &l) {
                  l.handleRawKeyEvent(handle, InputTypes::ScrollUp, true);
                  l.handleRawKeyEvent(handle, InputTypes::ScrollUp, false);
                });
              } else if (wheelDelta < 0) {
                // Scroll Down - treat as key press
                globalManagerInstance->listeners.call([handle](Listener &l) {
                  l.handleRawKeyEvent(handle, InputTypes::ScrollDown, true);
                  l.handleRawKeyEvent(handle, InputTypes::ScrollDown, false);
                });
              }
            }
          }
        } else if (raw->header.dwType == RIM_TYPEHID) {
          HANDLE deviceHandle = raw->header.hDevice;
          if (isPrecisionTouchpadDevice(deviceHandle)) {
            auto contacts = parsePrecisionTouchpadReport(
                reinterpret_cast<void *>(lParam),
                reinterpret_cast<void *>(deviceHandle));
            if (globalManagerInstance) {
              uintptr_t handle = reinterpret_cast<uintptr_t>(deviceHandle);
              juce::ScopedLock lock(
                  globalManagerInstance->touchpadContactsLock);
              auto &acc =
                  globalManagerInstance->touchpadContactsByDevice[handle];
              if (contacts.empty()) {
                acc.clear();
              } else {
                // Update or add each contact from this report. Do not mark
                // contacts missing from this report as lifted: many PTPs send
                // one contact per WM_INPUT (alternating), which would otherwise
                // flicker the other finger to "-". Lift is shown only when the
                // parser reports that contact with Tip Switch = 0.
                for (const auto &c : contacts) {
                  auto it = std::find_if(acc.begin(), acc.end(),
                                         [&c](const TouchpadContact &a) {
                                           return a.contactId == c.contactId;
                                         });
                  if (it != acc.end())
                    *it = c;
                  else
                    acc.push_back(c);
                }
              }
              auto contactsCopy = acc;
              globalManagerInstance->listeners.call(
                  [handle, contactsCopy](Listener &l) {
                    l.handleTouchpadContacts(handle, contactsCopy);
                  });
            }
          }
        }
      }
      delete[] lpb;
    }
  } else if (msg == WM_POINTERUPDATE) {
    if (globalManagerInstance && globalManagerInstance->pointerInputManager) {
      globalManagerInstance->pointerInputManager->processPointerMessage(
          wParam, lParam, hwnd);
    }
  } else if (msg == WM_INPUT_DEVICE_CHANGE) {
    // Device plug/unplug event (0x00FE)
    if (globalManagerInstance &&
        globalManagerInstance->onDeviceChangeCallback) {
      // Call the callback asynchronously to avoid blocking the message loop
      juce::MessageManager::callAsync([instance = globalManagerInstance]() {
        if (instance && instance->onDeviceChangeCallback) {
          instance->onDeviceChangeCallback();
        }
      });
    }
    // Return 0 to indicate we handled it
    if (globalOriginalWndProc) {
      return CallWindowProc(globalOriginalWndProc, (HWND)hwnd, (UINT)msg,
                            (WPARAM)wParam, (LPARAM)lParam);
    }
    return DefWindowProc((HWND)hwnd, (UINT)msg, (WPARAM)wParam, (LPARAM)lParam);
  }

  // 3. Pass to original
  if (globalOriginalWndProc) {
    return CallWindowProc(globalOriginalWndProc, (HWND)hwnd, (UINT)msg,
                          (WPARAM)wParam, (LPARAM)lParam);
  }
  return DefWindowProc((HWND)hwnd, (UINT)msg, (WPARAM)wParam, (LPARAM)lParam);
}

juce::String RawInputManager::getKeyName(int virtualKey) {
  // Prefer explicit names for modifier left/right so UI can distinguish them.
  switch (virtualKey) {
  case VK_LSHIFT:
    return "Left Shift";
  case VK_RSHIFT:
    return "Right Shift";
  case VK_LCONTROL:
    return "Left Ctrl";
  case VK_RCONTROL:
    return "Right Ctrl";
  case VK_LMENU:
    return "Left Alt";
  case VK_RMENU:
    return "Right Alt";
  default:
    break;
  }

  int scanCode = MapVirtualKeyA(virtualKey, MAPVK_VK_TO_VSC);
  char name[128] = {0};
  if (GetKeyNameTextA(scanCode << 16, name, 128) > 0)
    return juce::String(name);
  return "Unknown Key (" + juce::String(virtualKey) + ")";
}