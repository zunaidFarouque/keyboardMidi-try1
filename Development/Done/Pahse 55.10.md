### 📋 Cursor Prompt: Phase 55.10 - Smart Layouts & Labelled Separators

**Target Files:**
1.  `Source/MappingDefinition.h`
2.  `Source/MappingDefinition.cpp`
3.  `Source/MappingInspector.h`
4.  `Source/MappingInspector.cpp`

**Task:**
1.  Implement "Auto Width" layout logic to pack controls tightly (e.g., Checkbox takes minimum space, Slider takes the rest).
2.  Add a Labelled Separator to the Note schema.

**Specific Instructions:**

1.  **Update `Source/MappingDefinition.h`:**
    *   Add to `InspectorControl` struct:
        ```cpp
        bool autoWidth = false; // If true, ignore widthWeight and fit to content
        ```

2.  **Update `Source/MappingDefinition.cpp`:**
    *   **In `getSchema` (Note Type):**
        *   Replace the "Transpose" separator with a Labelled one:
            `result.push_back(createSeparator("Note Settings", juce::Justification::centredLeft));`
    *   **In `getSchema` (CC Type):**
        *   **"Send Value on Release":** Set `autoWidth = true`. Set `widthWeight = 0.0f` (ignored).
        *   **"Release Value":** Set `autoWidth = false`. Set `widthWeight = 1.0f` (Take all remaining space).

3.  **Update `Source/MappingInspector.h`:**
    *   Inside the private `LabeledControl` class (if defined in header) or forward declare it:
        *   Add `int getIdealWidth() const;`
    *   Update `UiItem` struct:
        *   Add `bool isAutoWidth;`

4.  **Update `Source/MappingInspector.cpp`:**

    *   **Update `LabeledControl::getIdealWidth` implementation:**
        ```cpp
        int getIdealWidth() const {
            int w = 0;
            if (label) w += label->getFont().getStringWidth(label->getText()) + 10; // Text + Padding
            // If editor is a ToggleButton (no text), it needs ~24px. 
            // If it's something else, we might need a default, but for now this feature is for Toggles.
            w += 30; 
            return w;
        }
        ```

    *   **Update `createControl`:**
        *   When creating `UiItem`, copy `def.autoWidth` to `item.isAutoWidth`.

    *   **Rewrite `resized` (The Flex Logic):**
        ```cpp
        auto bounds = getLocalBounds().reduced(4);
        int y = bounds.getY();
        const int rowHeight = 25;
        const int spacing = 4;

        for (auto& row : uiRows) {
            if (row.items.empty()) continue;

            // 1. Calculate Available Width
            int totalAvailable = bounds.getWidth();
            int usedWidth = 0;
            float totalWeight = 0.0f;

            // 2. Measure Auto-Width Items
            for (auto& item : row.items) {
                if (item.isAutoWidth) {
                    // We need to cast to LabeledControl to ask for width, 
                    // or implement a virtual method if UiItem held a wrapper.
                    // For safety, assuming LabeledControl usage:
                    if (auto* lc = dynamic_cast<LabeledControl*>(item.component.get())) {
                        int w = lc->getIdealWidth();
                        item.component->setBounds(0, 0, w, rowHeight); // Temp bounds to store width
                        usedWidth += w;
                    } else {
                        // Fallback for raw components (e.g. raw Toggle)
                        usedWidth += 100; 
                    }
                } else {
                    totalWeight += item.weight;
                }
            }

            // 3. Layout
            int remainingWidth = std::max(0, totalAvailable - usedWidth);
            int x = bounds.getX();

            for (auto& item : row.items) {
                int w = 0;
                if (item.isAutoWidth) {
                    // Retrieve stored width (hacky but works if we just set bounds.width above)
                    // Better: recalculate or store in a temp vector
                    if (auto* lc = dynamic_cast<LabeledControl*>(item.component.get())) {
                        w = lc->getIdealWidth();
                    } else { w = 100; }
                } else {
                    // Proportional
                    if (totalWeight > 0.0f)
                        w = static_cast<int>((item.weight / totalWeight) * remainingWidth);
                    else
                        w = remainingWidth; // Fallback
                }

                item.component->setBounds(x, y, w, rowHeight);
                x += w;
            }
            
            y += rowHeight + spacing;
        }
        ```

**Outcome:**
The Checkbox will shrink to fit its text exactly. The Slider will expand to fill the rest of the row. The visual gap will disappear. You will also see a nice header for "Note Settings".