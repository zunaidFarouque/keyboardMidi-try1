#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include <memory>

// Forward declaration
class PointerInputManager;

class RawInputManager {
public:
  // Listener interface for raw input events
  struct Listener {
    virtual ~Listener() = default;
    virtual void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                   bool isDown) = 0;
    virtual void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                 float value) = 0;
  };

  RawInputManager();
  ~RawInputManager();

  // Uses void* to avoid including <windows.h> in the header
  void initialize(void *nativeWindowHandle);
  void shutdown();

  // Listener management
  void addListener(Listener *listener);
  void removeListener(Listener *listener);

  static juce::String getKeyName(int virtualKey);

private:
  void *targetHwnd = nullptr;
  juce::ListenerList<Listener> listeners;
  bool isInitialized = false;
  std::unique_ptr<PointerInputManager> pointerInputManager;
  
  // Forward declaration for helper class (defined in .cpp)
  class PointerEventForwarder;
  std::unique_ptr<PointerEventForwarder> pointerEventForwarder;

  // Static WNDPROC wrapper
  static int64_t __stdcall rawInputWndProc(void *hwnd, unsigned int msg,
                                           uint64_t wParam, int64_t lParam);
  static void *originalWndProc;
  static RawInputManager *globalManagerInstance;
};