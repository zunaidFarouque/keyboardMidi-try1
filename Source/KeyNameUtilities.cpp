#include "KeyNameUtilities.h"
#include "MappingTypes.h"

// Windows header must be included first
#include <windows.h>

juce::String KeyNameUtilities::getKeyName(int virtualKeyCode) {
  // Check for pseudo-codes first
  if (virtualKeyCode == InputTypes::ScrollUp)
    return "Scroll Up";
  if (virtualKeyCode == InputTypes::ScrollDown)
    return "Scroll Down";
  if (virtualKeyCode == InputTypes::PointerX)
    return "Trackpad X";
  if (virtualKeyCode == InputTypes::PointerY)
    return "Trackpad Y";

  // Standard key lookup
  int scanCode = MapVirtualKeyA(virtualKeyCode, MAPVK_VK_TO_VSC);
  char name[128] = {0};
  if (GetKeyNameTextA(scanCode << 16, name, 128) > 0)
    return juce::String(name);
  return "Key [" + juce::String(virtualKeyCode) + "]";
}

juce::String KeyNameUtilities::getFriendlyDeviceName(uintptr_t deviceHandle) {
  HANDLE hDevice = reinterpret_cast<HANDLE>(deviceHandle);
  if (hDevice == nullptr)
    return "Device [" + juce::String::toHexString((juce::int64)deviceHandle).toUpperCase() + "]";

  UINT bufferSize = 0;
  GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME, nullptr, &bufferSize);

  if (bufferSize == 0)
    return "Device [" + juce::String::toHexString((juce::int64)deviceHandle).toUpperCase() + "]";

  auto *buffer = new char[bufferSize];
  if (GetRawInputDeviceInfoA(hDevice, RIDI_DEVICENAME, buffer, &bufferSize) ==
      UINT(-1)) {
    delete[] buffer;
    return "Device [" + juce::String::toHexString((juce::int64)deviceHandle).toUpperCase() + "]";
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
  return "Device [" + juce::String::toHexString((juce::int64)deviceHandle).toUpperCase() + "]";
}
