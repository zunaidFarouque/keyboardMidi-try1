# 🤖 Cursor Prompt: Phase 27.8 - Dedicated Input Thread & Kernel Injection

**Role:** Expert C++ Audio Developer (Win32 API Threading).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 27.7 Complete.
*   **Problem:** Background Raw Input is unreliable when Hooks are active. The Message Loop on the main GUI thread gets deprioritized or conflicted.
*   **Phase Goal:** Move `RawInputManager` to a **Dedicated std::thread**. This ensures a robust `RIDEV_INPUTSINK` that works even when the app is minimized or backgrounded.

**Strict Constraints:**
1.  **Threading:** Create a `std::thread` in `RawInputManager`. All window creation (`CreateWindowEx`) and Message Pumping (`GetMessage`) MUST happen inside this thread's entry point.
2.  **Inter-Thread Communication:** The Input Thread detects keys -> Calls `InputInjector` -> Updates Audio State. Ensure thread safety (use `juce::MessageManager::callAsync` only for UI updates, but Audio/Logic can run directly if locked).
3.  **Blocking:** The Hook (in `SystemIntegrator`) remains on the Main Thread (standard for Hooks). It blocks everything *except* Injected keys.

---

### Step 1: Update `RawInputManager.h`
Structure for threading.

**Requirements:**
1.  **Members:**
    *   `std::thread inputThread;`
    *   `std::atomic<bool> threadRunning { false };`
    *   `HWND messageWindow = nullptr;`
2.  **Methods:**
    *   `void startThread();`
    *   `void stopThread();`
    *   `void threadEntryPoint();` (The loop).

### Step 2: Update `RawInputManager.cpp`
Implement the Threaded Loop.

**1. Constructor:** Call `startThread()`. Destructor calls `stopThread()`.

**2. `threadEntryPoint`:**
*   **Create Window:** `CreateWindowEx(0, L"Message", ... HWND_MESSAGE ...)`
    *   *Note:* You must register the WNDCLASS inside this thread first.
*   **Register Input:** Call `RegisterRawInputDevices` with `RIDEV_INPUTSINK`.
*   **Message Loop:**
    ```cpp
    MSG msg;
    while (threadRunning && GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    ```
*   **Cleanup:** `DestroyWindow`.

**3. `rawInputWndProc`:**
*   Same logic as Phase 27.7 (Check Signature -> Check Device -> Inject/Broadcast).
*   **Crucial:** This now runs on a background thread. When you broadcast to listeners (MainComponent/InputProcessor), ensure those classes handle thread safety (they should already use `callAsync` or Locks).

### Step 3: Verify `SystemIntegrator.cpp`
Ensure the Hook logic (Main Thread) is solid.
*   It blocks everything without the signature.
*   It allows the signature.

### Step 4: Update `MainComponent.cpp`
*   Remove any manual initialization. The `RawInputManager` constructor starts the thread automatically.

---

**Generate code for: Updated `RawInputManager.h`, Updated `RawInputManager.cpp`.**