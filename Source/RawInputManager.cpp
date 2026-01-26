#include "RawInputManager.h"
#include "MappingTypes.h"
#include "PointerInputManager.h"
#include "SettingsManager.h"


// 1. Windows Header Only
// (We removed the #defines because CMake already sets them)
#include <windows.h>

// Static storage
void *RawInputManager::originalWndProc = nullptr;
RawInputManager *RawInputManager::globalManagerInstance = nullptr;

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
  globalManagerInstance = this;
  pointerInputManager->addListener(pointerEventForwarder.get());
}

RawInputManager::~RawInputManager() { shutdown(); }

void RawInputManager::initialize(void *nativeWindowHandle, SettingsManager* settingsMgr) {
  if (isInitialized || nativeWindowHandle == nullptr)
    return;

  HWND hwnd = static_cast<HWND>(nativeWindowHandle);
  targetHwnd = nativeWindowHandle;
  settingsManager = settingsMgr;

  // Log the Handle
  DBG("RawInputManager: Initializing with HWND: " + juce::String::toHexString((juce::int64)hwnd));

  // RAW INPUT REGISTRATION
  RAWINPUTDEVICE rid[1];
  
  // Page 1, Usage 6 = Keyboard
  rid[0].usUsagePage = 0x01;
  rid[0].usUsage = 0x06;
  
  // CRITICAL FIX: RIDEV_INPUTSINK enables background monitoring.
  // We also keep RIDEV_DEVNOTIFY to handle plug/unplug events.
  rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
  
  // CRITICAL FIX: Target must be explicit for InputSink to work.
  rid[0].hwndTarget = hwnd;

  if (RegisterRawInputDevices(rid, 1, sizeof(rid[0])) == FALSE) {
    DWORD error = GetLastError();
    DBG("RawInputManager: CRITICAL ERROR - Registration Failed! Error Code: " + juce::String(error));
    return;
  } else {
    DBG("RawInputManager: Registration Success. Flags: RIDEV_INPUTSINK");
  }

  // Subclass the window
  originalWndProc =
      (void *)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)rawInputWndProc);

  if (originalWndProc) {
    isInitialized = true;
    DBG("RawInputManager: Window subclassed successfully. Initialized with RIDEV_INPUTSINK.");
  }
}

void RawInputManager::shutdown() {
  if (isInitialized && targetHwnd) {
    HWND hwnd = static_cast<HWND>(targetHwnd);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
    isInitialized = false;
  }

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

void RawInputManager::setFocusTargetCallback(std::function<void*()> cb) {
  focusTargetCallback = cb;
}

// Helper function to force a window to the foreground, bypassing Windows restrictions
static void forceForegroundWindow(HWND hwnd) {
  if (GetForegroundWindow() == hwnd) return;

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
  if (msg == WM_INPUT) {
    // DIAGNOSTIC LOG
    // GET_RAWINPUT_CODE_WPARAM is a macro to extract the input type
    int code = GET_RAWINPUT_CODE_WPARAM(wParam);
    juce::String typeStr = (code == RIM_INPUT) ? "FOREGROUND" : (code == RIM_INPUTSINK ? "BACKGROUND" : "OTHER");
    
    // Only log if it is BACKGROUND to prove it works (avoid flooding)
    if (code == RIM_INPUTSINK) {
      DBG("RawInputManager: Received BACKGROUND Event! (wParam=" + juce::String((int)wParam) + ", code=" + juce::String(code) + ")");
    }
    
    // Focus Guard: If MIDI mode is active, steal focus when key is pressed
    if (globalManagerInstance && globalManagerInstance->settingsManager) {
      if (globalManagerInstance->settingsManager->isMidiModeActive()) {
        HWND shield = static_cast<HWND>(globalManagerInstance->targetHwnd); // Default to main window
        if (globalManagerInstance->focusTargetCallback) {
          void* callbackResult = globalManagerInstance->focusTargetCallback();
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
          int vKey = raw->data.keyboard.VKey;
          bool isDown = (raw->data.keyboard.Flags & RI_KEY_BREAK) == 0;

          if (globalManagerInstance) {
            // Check if MIDI mode is active - only broadcast for MIDI when active
            // (Toggle key detection still works because hook allows it through)
            bool shouldBroadcast = false;
            if (globalManagerInstance->settingsManager) {
              // Always broadcast if MIDI mode is active
              // Also always broadcast toggle key so it can be detected to turn mode off
              int toggleKey = globalManagerInstance->settingsManager->getToggleKey();
              shouldBroadcast = globalManagerInstance->settingsManager->isMidiModeActive() || 
                               (vKey == toggleKey);
            } else {
              // If no settings manager, always broadcast (backward compatibility)
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
        }
      }
      delete[] lpb;
    }
  } else if (msg == WM_POINTERUPDATE) {
    if (globalManagerInstance && globalManagerInstance->pointerInputManager) {
      globalManagerInstance->pointerInputManager->processPointerMessage(
          wParam, lParam, hwnd);
    }
  }

  return CallWindowProc((WNDPROC)originalWndProc, (HWND)hwnd, msg, wParam,
                        lParam);
}

juce::String RawInputManager::getKeyName(int virtualKey) {
  int scanCode = MapVirtualKeyA(virtualKey, MAPVK_VK_TO_VSC);
  char name[128] = {0};
  if (GetKeyNameTextA(scanCode << 16, name, 128) > 0)
    return juce::String(name);
  return "Unknown Key (" + juce::String(virtualKey) + ")";
}