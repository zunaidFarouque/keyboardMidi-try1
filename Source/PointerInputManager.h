#pragma once
#include <JuceHeader.h>
#include <cstdint>

class PointerInputManager {
public:
  // Listener interface for pointer events
  struct Listener {
    virtual ~Listener() = default;
    virtual void onPointerEvent(uintptr_t device, int axisID, float value) = 0;
  };

  PointerInputManager();
  ~PointerInputManager();

  // Process WM_POINTER messages (wParam and lParam from Windows)
  void processPointerMessage(uintptr_t wParam, uintptr_t lParam, void *windowHandle);

  // Listener management
  void addListener(Listener *listener);
  void removeListener(Listener *listener);

private:
  juce::ListenerList<Listener> listeners;
};
