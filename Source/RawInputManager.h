#pragma once
#include <JuceHeader.h>
#include <functional>

// Standard callback definition
using RawInputCallback =
    std::function<void(void *deviceHandle, int keyCode, bool isDown)>;

class RawInputManager {
public:
  RawInputManager();
  ~RawInputManager();

  // Uses void* to avoid including <windows.h> in the header
  void initialize(void *nativeWindowHandle);
  void shutdown();
  void setCallback(RawInputCallback cb);

  static juce::String getKeyName(int virtualKey);

private:
  void *targetHwnd = nullptr;
  RawInputCallback callback;
  bool isInitialized = false;

  // Static WNDPROC wrapper
  static int64_t __stdcall rawInputWndProc(void *hwnd, unsigned int msg,
                                           uint64_t wParam, int64_t lParam);
  static void *originalWndProc;
  static RawInputManager *globalManagerInstance;
};