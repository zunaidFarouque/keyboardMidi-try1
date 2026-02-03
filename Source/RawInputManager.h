#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "TouchpadTypes.h"

// Forward declaration
class PointerInputManager;
class SettingsManager;

class RawInputManager {
public:
  // Listener interface for raw input events
  struct Listener {
    virtual ~Listener() = default;
    virtual void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                   bool isDown) = 0;
    virtual void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                 float value) = 0;
    virtual void
    handleTouchpadContacts(uintptr_t deviceHandle,
                           const std::vector<TouchpadContact> &contacts) {}
  };

  RawInputManager();
  ~RawInputManager();

  // Uses void* to avoid including <windows.h> in the header
  void initialize(void *nativeWindowHandle,
                  SettingsManager *settingsMgr = nullptr);
  void shutdown();

  // Listener management
  void addListener(Listener *listener);
  void removeListener(Listener *listener);

  // State management for anti-ghosting and autorepeat filtering
  void resetState();

  // Focus target callback for dynamic window selection
  void setFocusTargetCallback(std::function<void *()> cb);

  // Device change callback for hardware hygiene
  void setOnDeviceChangeCallback(std::function<void()> cb);

  static juce::String getKeyName(int virtualKey);

private:
  void *targetHwnd = nullptr;
  SettingsManager *settingsManager = nullptr;
  std::function<void *()> focusTargetCallback;
  std::function<void()> onDeviceChangeCallback;
  juce::ListenerList<Listener> listeners;
  bool isInitialized = false;
  std::unique_ptr<PointerInputManager> pointerInputManager;

  // Forward declaration for helper class (defined in .cpp)
  class PointerEventForwarder;
  std::unique_ptr<PointerEventForwarder> pointerEventForwarder;

  // Anti-ghosting and autorepeat filtering: Track pressed keys per device
  std::map<uintptr_t, std::set<int>> deviceKeyStates;

  // Accumulated touchpad contacts per device (merge across WM_INPUT messages)
  std::map<uintptr_t, std::vector<TouchpadContact>> touchpadContactsByDevice;
  juce::CriticalSection touchpadContactsLock;

  // Static WNDPROC wrapper
  static int64_t __stdcall rawInputWndProc(void *hwnd, unsigned int msg,
                                           uint64_t wParam, int64_t lParam);
  static RawInputManager *globalManagerInstance;
};