#include "RawInputManager.h"
#include "PointerInputManager.h"
#include "MappingTypes.h"

// 1. Windows Header Only
// (We removed the #defines because CMake already sets them)
#include <windows.h>

// Static storage
void *RawInputManager::originalWndProc = nullptr;
RawInputManager *RawInputManager::globalManagerInstance = nullptr;

// Helper class to forward pointer events to RawInputManager listeners
class RawInputManager::PointerEventForwarder : public PointerInputManager::Listener {
public:
  explicit PointerEventForwarder(RawInputManager *manager) : manager(manager) {}
  void onPointerEvent(uintptr_t device, int axisID, float value) override {
    if (manager) {
      manager->listeners.call([device, axisID, value](RawInputManager::Listener &l) {
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

void RawInputManager::initialize(void *nativeWindowHandle) {
  if (isInitialized || nativeWindowHandle == nullptr)
    return;

  HWND hwnd = static_cast<HWND>(nativeWindowHandle);
  targetHwnd = nativeWindowHandle;

  // Raw Input Registration (Keyboard + Mouse)
  RAWINPUTDEVICE rid[2];
  // Keyboard
  rid[0].usUsagePage = 0x01; // Generic Desktop
  rid[0].usUsage = 0x06;     // Keyboard
  rid[0].dwFlags = RIDEV_INPUTSINK;
  rid[0].hwndTarget = hwnd;
  // Mouse
  rid[1].usUsagePage = 0x01; // Generic Desktop
  rid[1].usUsage = 0x02;     // Mouse
  rid[1].dwFlags = RIDEV_INPUTSINK;
  rid[1].hwndTarget = hwnd;

  if (RegisterRawInputDevices(rid, 2, sizeof(rid[0])) == FALSE) {
    DBG("Failed to register raw input device.");
    return;
  }

  // Subclass the window
  originalWndProc =
      (void *)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)rawInputWndProc);

  if (originalWndProc) {
    isInitialized = true;
  }
}

void RawInputManager::shutdown() {
  if (isInitialized && targetHwnd) {
    HWND hwnd = static_cast<HWND>(targetHwnd);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
    isInitialized = false;
  }
}

void RawInputManager::addListener(Listener *listener) {
  listeners.add(listener);
}

void RawInputManager::removeListener(Listener *listener) {
  listeners.remove(listener);
}

int64_t __stdcall RawInputManager::rawInputWndProc(void *hwnd, unsigned int msg,
                                                   uint64_t wParam,
                                                   int64_t lParam) {
  if (msg == WM_INPUT) {
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
            uintptr_t handle = reinterpret_cast<uintptr_t>(deviceHandle);
            globalManagerInstance->listeners.call(
                [handle, vKey, isDown](Listener &l) {
                  l.handleRawKeyEvent(handle, vKey, isDown);
                });
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
                globalManagerInstance->listeners.call(
                    [handle](Listener &l) {
                      l.handleRawKeyEvent(handle, InputTypes::ScrollUp, true);
                      l.handleRawKeyEvent(handle, InputTypes::ScrollUp, false);
                    });
              } else if (wheelDelta < 0) {
                // Scroll Down - treat as key press
                globalManagerInstance->listeners.call(
                    [handle](Listener &l) {
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