#include "KeyNameUtilities.h"
#include "MappingTypes.h"

// Windows header must be included first
#include <windows.h>

juce::String KeyNameUtilities::getKeyName(int virtualKeyCode) {
  // 1. Check for Internal Pseudo-Codes
  if (virtualKeyCode == InputTypes::ScrollUp)
    return "Scroll Up";
  if (virtualKeyCode == InputTypes::ScrollDown)
    return "Scroll Down";
  if (virtualKeyCode == InputTypes::PointerX)
    return "Trackpad X";
  if (virtualKeyCode == InputTypes::PointerY)
    return "Trackpad Y";

  // 2. Manual Overrides for Windows Ambiguities
  // These keys often map to Numpad names if we don't handle them explicitly.
  switch (virtualKeyCode) {
  // Modifiers (Phase 49.1 / 39.9 follow-up): distinguish Left/Right
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

  // Navigation
  case VK_LEFT:
    return "Left Arrow";
  case VK_RIGHT:
    return "Right Arrow";
  case VK_UP:
    return "Up Arrow";
  case VK_DOWN:
    return "Down Arrow";
  case VK_PRIOR:
    return "Page Up";
  case VK_NEXT:
    return "Page Down";
  case VK_HOME:
    return "Home";
  case VK_END:
    return "End";
  case VK_INSERT:
    return "Insert";
  case VK_DELETE:
    return "Delete";

  // System / Locks
  case VK_SNAPSHOT:
    return "Print Screen";
  case VK_SCROLL:
    return "Scroll Lock";
  case VK_PAUSE:
    return "Pause/Break";
  case VK_NUMLOCK:
    return "Num Lock";
  case VK_CAPITAL:
    return "Caps Lock";
  case VK_APPS:
    return "Ctx Menu";

  // Windows Keys
  case VK_LWIN:
    return "Left Windows";
  case VK_RWIN:
    return "Right Windows";

    //     // Misc
    //     case VK_CLEAR:      return "Clear (Num 5)";
    //     case 0xFF:          return "Fn / Hardware";

  default:
    break;
  }

  // 3. Fallback to Windows API for standard keys (A-Z, 0-9, F-Keys, Symbols)
  int scanCode = MapVirtualKeyA(virtualKeyCode, MAPVK_VK_TO_VSC);

  // Safety for unmapped keys
  if (scanCode == 0)
    return "Key " + juce::String(virtualKeyCode);

  // Shift left to high word
  int lParam = (scanCode << 16);

  char name[128] = {0};
  if (GetKeyNameTextA(lParam, name, 128) > 0) {
    return juce::String(name);
  }

  return "Key " + juce::String(virtualKeyCode);
}

juce::String KeyNameUtilities::getFriendlyDeviceName(uintptr_t deviceHandle) {
  HANDLE hDevice = reinterpret_cast<HANDLE>(deviceHandle);
  if (hDevice == nullptr)
    return "Device [" +
           juce::String::toHexString((juce::int64)deviceHandle).toUpperCase() +
           "]";

  UINT bufferSize = 0;
  GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME, nullptr, &bufferSize);

  if (bufferSize == 0)
    return "Device [" +
           juce::String::toHexString((juce::int64)deviceHandle).toUpperCase() +
           "]";

  auto *buffer = new char[bufferSize];
  if (GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME, buffer, &bufferSize) ==
      UINT(-1)) {
    delete[] buffer;
    return "Device [" +
           juce::String::toHexString((juce::int64)deviceHandle).toUpperCase() +
           "]";
  }

  juce::String deviceName(buffer);
  delete[] buffer;

  // Extract VID/PID from the messy hardware ID string
  // Format is typically: \\?\HID#VID_046D&PID_C52B#...
  int vidStart = deviceName.indexOf("VID_");
  if (vidStart >= 0) {
    int vidEnd = deviceName.indexOf(vidStart, "&");
    if (vidEnd < 0)
      vidEnd = deviceName.indexOf(vidStart, "#");
    if (vidEnd > vidStart) {
      juce::String vid = deviceName.substring(vidStart, vidEnd);
      return "Device [" + vid + "]";
    }
  }

  // Fallback to hex handle if extraction fails
  return "Device [" +
         juce::String::toHexString((juce::int64)deviceHandle).toUpperCase() +
         "]";
}
