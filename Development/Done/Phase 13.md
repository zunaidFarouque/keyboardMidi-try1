# 🤖 Cursor Prompt: Phase 13 - Docking, Resizing & Window Management

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 12 Complete. We have `Visualizer`, `ZoneEditor`, `MappingEditor`, and `Inspector`. Currently, they are hardcoded in `MainComponent::resized()`.
*   **Phase Goal:** Implement a flexible IDE-style layout with resizable split-bars and the ability to "Pop Out" (detach) panels into separate windows.

**Strict Constraints:**
1.  **Ownership:** `MainComponent` MUST retain ownership of the logic components (Visualizer, etc.). The `DetachableContainer` only holds a pointer to the component it wraps.
2.  **Safety:** When a floating window is closed, the component must be immediately re-docked into the main UI to prevent dangling pointers.
3.  **CMake:** Provide the snippet to add the new file.

---

### Step 1: `DetachableContainer` (`Source/DetachableContainer.h/cpp`)
A generic wrapper that holds a component and adds a header bar.

**Requirements:**
1.  **Inner Class:** `class FloatingWindow : public juce::DocumentWindow`.
    *   On `closeButtonPressed()`, it calls a callback in the Container to re-dock.
2.  **Members:**
    *   `juce::Component* content;` (Reference to the wrapped view).
    *   `std::unique_ptr<FloatingWindow> window;`
    *   `juce::TextButton popOutButton;`
    *   `juce::Label titleLabel;`
3.  **API:**
    *   `DetachableContainer(juce::String title, juce::Component& contentToWrap);`
    *   `void setContent(juce::Component& newContent);`
4.  **Logic (`togglePopOut`):**
    *   **If Docked:** `removeChildComponent(content)`. Create `window`. `window->setContentNonOwned(content, true)`. Show window. Show placeholder text in container ("Popped Out").
    *   **If Floating:** `window` is reset/deleted. `addAndMakeVisible(content)`. `resized()`.

### Step 2: `MainComponent` (Layout Refactor)
Replace the static layout with a resizable split system.

**Requirements:**
1.  **Wrappers:** Wrap existing members in `DetachableContainer`.
    *   `DetachableContainer visualizerContainer { "Visualizer", visualizer };`
    *   `DetachableContainer editorContainer { "Mapping / Zones", tabbedComponent };` (You might need to group Zone/Mapping editors into a `juce::TabbedComponent` first).
    *   `DetachableContainer inspectorContainer { "Inspector", inspector };`
2.  **Layout Managers:**
    *   `juce::StretchableLayoutManager verticalLayout;` (Splits Visualizer / Bottom Area).
    *   `juce::StretchableLayoutManager horizontalLayout;` (Splits Editors / Inspector).
3.  **Resizers:**
    *   `juce::StretchableLayoutResizerBar vertBar;`
    *   `juce::StretchableLayoutResizerBar horzBar;`
4.  **Constructor:**
    *   Initialize `StretchableLayoutManager` items (min/max sizes).
    *   Add `ResizerBar`s to the component.
5.  **Resized():**
    *   Use `verticalLayout.layOutComponents(...)` to position the Top (Visualizer) and Bottom (Group).
    *   Inside the Bottom Group rect, use `horizontalLayout.layOutComponents(...)` to position Left (Editors) and Right (Inspector).

### Step 3: Persistence (Bonus)
Save the layout positions to `DeviceManager`'s config (or a local helper).

1.  **Destructor:** Read `verticalLayout.getItemCurrentPosition(1)` (the bar) and save to `globalConfig`.
2.  **Constructor:** Load that value and apply it to the layout.

### Step 4: Update `CMakeLists.txt`
Add `Source/DetachableContainer.cpp`.

---

**Generate code for: `DetachableContainer`, updated `MainComponent` (complete refactor of layout logic), and the `CMakeLists.txt` snippet.**