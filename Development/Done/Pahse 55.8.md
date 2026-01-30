### 📋 Cursor Prompt: Phase 55.8 - Fix Checkbox Label Layout

**Target File:** `Source/MappingInspector.cpp`

**Task:**
Change the rendering of `Toggle` controls in `createControl`.
Instead of creating a `ToggleButton` with text (which puts the label on the right), create a separate `juce::Label` on the left and a text-less `juce::ToggleButton` on the right.

**Specific Instructions:**

1.  **Locate `createControl` implementation.**

2.  **Define a Helper Class (locally or at top of file):**
    We need a simple container to hold "Label + Component" side-by-side within a specific weight.
    ```cpp
    class LabeledControl : public juce::Component {
    public:
        LabeledControl(std::unique_ptr<juce::Label> l, std::unique_ptr<juce::Component> c) 
            : label(std::move(l)), editor(std::move(c)) {
            if (label) addAndMakeVisible(*label);
            addAndMakeVisible(*editor);
        }
        void resized() override {
            auto area = getLocalBounds();
            if (label) {
                // Label takes fit width or max 60%? 
                // Let's rely on the text width
                int textWidth = label->getFont().getStringWidth(label->getText()) + 10;
                label->setBounds(area.removeFromLeft(textWidth));
            }
            // Editor takes the rest
            editor->setBounds(area);
        }
    private:
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Component> editor;
    };
    ```

3.  **Refactor `createControl` Logic:**
    *   **Current Logic:** Likely differentiates Sliders (Group) vs Toggles (Raw Button).
    *   **New Logic:** Unify them.
    *   **Step 1:** Create the Editor Widget based on `def.controlType`.
        *   If `Toggle`: `auto* btn = new juce::ToggleButton();` (No text!). Bind `onClick`.
        *   If `Slider`: `auto* sld = new juce::Slider();` ...
        *   If `ComboBox`: `auto* box = new juce::ComboBox();` ...
    *   **Step 2:** Create the Label (if `def.label` is not empty).
    *   **Step 3:** Wrap in Container.
        *   If `def.label` is empty: `uiItem.component.reset(editorWidget);`
        *   If `def.label` exists:
            ```cpp
            auto* lbl = new juce::Label("", def.label);
            lbl->setJustificationType(juce::Justification::centredLeft);
            uiItem.component.reset(new LabeledControl(
                std::unique_ptr<juce::Label>(lbl), 
                std::unique_ptr<juce::Component>(editorWidget)
            ));
            ```

4.  **Refactor `MappingDefinition.cpp` (Optional Tweak):**
    *   Ensure "Send Value on Release" has a slightly larger `widthWeight` (e.g., 0.55f) to fit the text, and shrink "Release Value" slider slightly (e.g., 0.45f).

**Outcome:**
The "Send Value on Release" row will now render as:
`[Label "Send..."] [TickBox]`  ... `[Slider]`
This matches your orange sketch.