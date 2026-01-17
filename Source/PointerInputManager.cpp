#include "PointerInputManager.h"

// Windows header must be included first
#include <windows.h>

PointerInputManager::PointerInputManager() {}

PointerInputManager::~PointerInputManager() {}

void PointerInputManager::addListener(Listener *listener) {
  listeners.add(listener);
}

void PointerInputManager::removeListener(Listener *listener) {
  listeners.remove(listener);
}

void PointerInputManager::processPointerMessage(uintptr_t wParam,
                                                uintptr_t lParam,
                                                void *windowHandle) {
  UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
  HWND hwnd = static_cast<HWND>(windowHandle);

  POINTER_INFO pointerInfo;
  if (GetPointerInfo(pointerId, &pointerInfo) == FALSE)
    return;

  // Get window client rect for normalization
  RECT clientRect;
  if (GetClientRect(hwnd, &clientRect) == FALSE)
    return;

  int windowWidth = clientRect.right - clientRect.left;
  int windowHeight = clientRect.bottom - clientRect.top;

  if (windowWidth <= 0 || windowHeight <= 0)
    return;

  // Convert screen coordinates to client coordinates
  POINT pt = {pointerInfo.ptPixelLocation.x, pointerInfo.ptPixelLocation.y};
  ScreenToClient(hwnd, &pt);

  // Normalize X/Y to 0.0f - 1.0f
  float normalizedX = std::clamp(static_cast<float>(pt.x) / windowWidth, 0.0f, 1.0f);
  float normalizedY = std::clamp(static_cast<float>(pt.y) / windowHeight, 0.0f, 1.0f);

  // Use device handle as uintptr_t (from pointerInfo.sourceDevice)
  uintptr_t deviceHandle = reinterpret_cast<uintptr_t>(pointerInfo.sourceDevice);

  // Notify listeners for both X and Y axes
  listeners.call([deviceHandle, normalizedX, normalizedY](Listener &l) {
    l.onPointerEvent(deviceHandle, 0x2000, normalizedX); // PointerX
    l.onPointerEvent(deviceHandle, 0x2001, normalizedY); // PointerY
  });
}
