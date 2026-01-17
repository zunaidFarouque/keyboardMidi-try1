# 🤖 Cursor Prompt: Phase 15.2 - UI Scrollbars & Layout Fixes

**Role:** Expert C++ Audio Developer (JUCE UI Specialist).

**Context:**
*   **Current State:** Phase 15.1 Complete. The app works, but the UI is rigid.
*   **Problem:** On smaller screens (or when panels are resized), the `ZonePropertiesPanel` and `MappingInspector` contents get cut off at the bottom.
*   **Phase Goal:** Implement `juce::Viewport` for these panels to enable vertical scrolling.

**Strict Constraints:**
1.  **UX:** Vertical scrolling only (`setScrollBarsShown(true, false)`).
2.  **Responsiveness:** The content inside the viewport must update its height dynamically (e.g., if the `KeyChipList` grows, the panel must get taller to push the content down).

---

### Step 1: Update `ZonePropertiesPanel` (Self-Sizing)
The panel needs to tell its parent how tall it wants to be.

**Updates in `Source/ZonePropertiesPanel.cpp`:**
1.  **Refactor `resized()`:**
    *   Instead of relying on `getHeight()`, calculate the `y` position incrementally for every control.
    *   Example: `int y = 10; ... nameEditor.setBounds(..., y, ...); y += 30;`
    *   At the end of `resized()`, store this total `y` as `requiredHeight`.
2.  **New Method:** `int getRequiredHeight()`. Returns the calculated height.
3.  **Callback:** Add `std::function<void()> onResizeRequested;`. Call this whenever the Chip List changes (add/remove key) so the parent knows to refresh the scrollbar.

### Step 2: Update `ZoneEditorComponent` (The Viewport Wrapper)
This component holds the properties panel.

**Updates in `Source/ZoneEditorComponent.h/cpp`:**
1.  **Member:** Add `juce::Viewport propertiesViewport;`.
2.  **Constructor:**
    *   `addAndMakeVisible(propertiesViewport);`
    *   `propertiesViewport.setViewedComponent(&propertiesPanel, false);` (false = don't delete, we own it).
    *   `propertiesViewport.setScrollBarsShown(true, false);` (Vertical only).
    *   Bind `propertiesPanel.onResizeRequested = [this] { resized(); };`
3.  **Resized:**
    *   Set `propertiesViewport.setBounds(...)` to fill the center area.
    *   **Critical:** Inside `resized`, explicitly set the bounds of the content:
        `propertiesPanel.setBounds(0, 0, propertiesViewport.getWidth() - 15, propertiesPanel.getRequiredHeight());`
        *(The -15 accounts for the scrollbar width)*.

### Step 3: Update `MappingInspector` (Self-Sizing)
Similar logic for the Inspector.

**Updates in `Source/MappingInspector.cpp`:**
1.  **Refactor `resized()`:** Calculate total height based on visible sliders.
2.  **Method:** `int getRequiredHeight()` returns a fixed minimum (e.g., 400px) or the calculated dynamic height.

### Step 4: Update `MainComponent` (The Layout)
Currently, `MainComponent` manages the split between Table and Inspector.

**Updates in `Source/MainComponent.h/cpp`:**
1.  **Member:** Add `juce::Viewport inspectorViewport;`.
2.  **Constructor:**
    *   `addAndMakeVisible(inspectorViewport);`
    *   `inspectorViewport.setViewedComponent(&inspector, false);`
3.  **Resized:**
    *   Instead of `inspector.setBounds(...)`, set `inspectorViewport.setBounds(...)`.
    *   Then set inner bounds: `inspector.setBounds(0, 0, inspectorViewport.getWidth() - 15, inspector.getRequiredHeight());`

---

**Generate code for: Updated `ZonePropertiesPanel`, `ZoneEditorComponent`, `MappingInspector`, and `MainComponent`.**