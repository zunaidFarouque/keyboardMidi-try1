# 🤖 Cursor Prompt: Phase 6 - Mouse & Trackpad Integration

**Role:** Expert C++ Audio Developer (Win32 API Specialist).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 5 Complete. We have `RawInputManager` (Keyboard) and `InputProcessor` (Notes).
*   **Phase Goal:** Add Mouse Scroll (Relative CC) and Precision Trackpad (Absolute XY).

**Strict Constraints:**
1.  **Single Hook Rule:** Only `RawInputManager` owns the `WndProc` hook. Do **not** create a second class that calls `SetWindowLongPtr`.
2.  **No Windows Headers in Headers:** Use `void*` or `uintptr_t`.
3.  **Pseudo-Codes:** Since Mouse/Touch don't have "KeyCodes", use these virtual constants in `MappingTypes.h`:
    *   `0x1000`: Scroll Vertical
    *   `0x2000`: Pointer X
    *   `0x2001`: Pointer Y

---

### Step 1: Update `MappingTypes.h`
Add the pseudo-codes for non-key inputs so we can store them in our `InputID` map.
```cpp
namespace InputTypes {
    constexpr int ScrollVertical = 0x1000;
    constexpr int PointerX       = 0x2000;
    constexpr int PointerY       = 0x2001;
}
```

### Step 2: Create `PointerInputManager` (`Source/PointerInputManager.h/cpp`)
A logic class to parse `WM_POINTER` messages. **It does not hook the window itself.**

**Requirements:**
1.  **Header:** No Windows headers.
2.  **Method:** `void processPointerMessage(uintptr_t wParam, uintptr_t lParam);`
    *   *Implementation:* In .cpp, include `<windows.h>`. Use `GET_POINTERID_WPARAM`. Call `GetPointerInfo`.
    *   Normalize X/Y to `0.0f - 1.0f` based on `rcTarget` (Window Rect).
3.  **Broadcasting:** Add a `Listener` interface with `void onPointerEvent(uintptr_t device, int axisID, float value)`.

### Step 3: Refactor `RawInputManager`
Expand the manager to handle Mouse and delegate Pointer events.

**Requirements:**
1.  **Registration:** Update `initialize` to register Mouse (`UsagePage 1, Usage 2`).
2.  **Members:** Add an instance of `PointerInputManager`.
3.  **WndProc Update:**
    *   **Case `WM_INPUT`**:
        *   If `header.dwType == RIM_TYPEMOUSE`:
        *   Check `usButtonFlags` for `RI_MOUSE_WHEEL`.
        *   Extract wheel delta.
        *   Notify listeners via `handleRawMouseEvent(device, InputTypes::ScrollVertical, delta)`.
    *   **Case `WM_POINTERUPDATE`**:
        *   Forward `wParam` and `lParam` to `pointerInputManager.processPointerMessage(...)`.
4.  **Listener Interface:** Update `RawInputManager::Listener`:
    *   Add `virtual void handleAxisEvent(uintptr_t device, int inputCode, float value) = 0;`
    *   *Note:* The `RawInputManager` should forward events from its internal `PointerInputManager` to its own listeners, simplifying the connection to the rest of the app.

### Step 4: Update `InputProcessor`
Handle the new "Continuous" events.

**Requirements:**
1.  **Method:** `handleAxisEvent(uintptr_t device, int inputCode, float value)` (Implement Listener).
2.  **Logic:**
    *   Look up `InputID { device, inputCode }`.
    *   If mapping exists and `type == CC`:
    *   **Relative (Scroll):** `currentVal = std::clamp(currentVal + (value * sensitivity), 0, 127)`. Send CC.
    *   **Absolute (Pointer):** `currentVal = value * 127`. Send CC.

### Step 5: `MainComponent` (Cursor Lock)
To use the Trackpad as an XY pad, we must lock the mouse.

**Requirements:**
1.  **UI:** Add `juce::ToggleButton performanceModeButton` ("Performance Mode").
2.  **Logic:**
    *   When `true`: Call `::ShowCursor(FALSE)` and `::ClipCursor(&bounds)`.
    *   When `false`: Call `::ShowCursor(TRUE)` and `::ClipCursor(NULL)`.
3.  **Wiring:** Connect `RawInputManager`'s new axis callbacks to `InputProcessor`.

### Step 6: Update `CMakeLists.txt`
Add `Source/PointerInputManager.cpp`.

---

**Generate code for: `MappingTypes.h` (update), `PointerInputManager`, `RawInputManager` (update), `InputProcessor` (update), `MainComponent` (update), and `CMakeLists.txt`.**