# 🤖 Cursor Prompt: Phase 9.3 - Device Setup UI Fixes & Renaming

**Role:** Expert C++ Audio Developer (JUCE UI).

**Context:**
We are building "MIDIQy".
*   **Current State:** `DeviceSetupComponent` exists but has a refresh bug.
*   **The Bug:** Clicking a different Alias in the left list **does not** update the Hardware list on the right. It keeps showing the previous selection until the window is reopened.
*   **UX Issues:** No column headers/labels. No way to Rename an alias.

**Phase Goal:**
1.  Fix the ListBox update logic so the Hardware list refreshes instantly on selection change.
2.  Add Labels ("Aliases", "Associated Devices") above the lists.
3.  Add a "Rename Alias" button.

**Strict Constraints:**
1.  **Refactoring:** Ensure `DeviceSetupComponent` creates a custom `ListBoxModel` for the Hardware list that holds a reference to the **currently selected alias name**. When selection changes, update this name and call `updateContent()`.
2.  **Renaming:** Implement `renameAlias` in `DeviceManager` (manipulating the ValueTree). The UI should prompt using `juce::AlertWindow::showAsync` (text entry).
3.  **Layout:** Add `juce::Label` headers above the lists.

---

### Step 1: Update `DeviceManager` (`Source/DeviceManager.h/cpp`)
Add renaming capability.

**Method:** `void renameAlias(const juce::String& oldName, const juce::String& newName);`
*   **Logic:**
    *   Find the child node in `globalConfig` with property `name == oldName`.
    *   If found, set property `name` to `newName`.
    *   Call `sendChangeMessage()` (Critical: this updates InputProcessor and MappingEditor).

### Step 2: Update `DeviceSetupComponent` (`Source/DeviceSetupComponent.h/cpp`)
Fix the UI logic.

**1. Inner Class `HardwareListModel`:**
*   Add member: `juce::String currentAliasName;`
*   Add method: `void setAlias(const juce::String& name) { currentAliasName = name; }`
*   **Update `getNumRows`:** Call `deviceManager.getHardwareForAlias(currentAliasName).size()`.
*   **Update `paintListBoxItem`:** Use `currentAliasName` to fetch the specific list.

**2. Main Class (`DeviceSetupComponent`):**
*   **Members:**
    *   Add `juce::Label aliasHeaderLabel;` ("Defined Aliases").
    *   Add `juce::Label hardwareHeaderLabel;` ("Associated Hardware").
    *   Add `juce::TextButton renameButton;` ("Rename").
*   **Constructor:**
    *   Setup Labels (Justification::left).
    *   **Fix the Bug:** In `aliasListBox.onChange` (or `selectedRowsChanged` in model):
        *   Get selected Alias Name.
        *   `hardwareListModel->setAlias(selectedName);`
        *   `hardwareListBox.updateContent();`
        *   `hardwareListBox.repaint();`
    *   **Rename Button Logic:**
        *   Get selected alias.
        *   Launch `AlertWindow` with a TextEditor.
        *   On OK: Call `deviceManager.renameAlias(...)`, then `aliasListBox.updateContent()`.

**3. `resized()`:**
*   Add space at the top of the lists for the new Labels (~24px).

---

**Generate code for: Updated `DeviceManager.h/cpp` and Updated `DeviceSetupComponent.h/cpp`.**