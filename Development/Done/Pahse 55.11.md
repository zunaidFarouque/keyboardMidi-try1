### 📋 Cursor Prompt: Phase 55.11 - UI Padding & Polish

**Target File:** `Source/MappingInspector.cpp`

**Task:**
Increase the vertical spacing around Labelled Separators to improve readability and reduce density.

**Specific Instructions:**

1.  **Update `SeparatorComponent`:**
    *   Currently, it likely fills its bounds.
    *   We can't change the drawing much without affecting the lines, but we can change **how much height** the separator requests in the layout logic.

2.  **Update `resized` (Layout Logic):**
    *   Locate the loop where `rowHeight` and `spacing` are used.
    *   **Logic Change:**
        *   If a row contains *only* a `Separator` (which is our standard usage for labelled separators), give it more height or add extra padding before it.
        *   Standard `rowHeight` is 25.
        *   Let's check if the row has a separator: `if (row.items.size() == 1 && ... check type or cast ...)`.
        *   *Alternative:* Just increase the `spacing` variable generally? No, that affects everything.
        *   **Better Approach:** When iterating `uiRows`, check if the *current* row is a Separator. If so, add extra `spacing` to `y` *before* laying it out.

3.  **Refactor `resized` implementation:**
    *   Iterate `uiRows`.
    *   Check if the row is a separator row (we added `bool isSeparator` to `UiRow` struct in Phase 55.6, right? If not, we should have).
    *   If `row.isSeparator`:
        *   Add extra padding before: `y += 10;`
        *   Set height for separator: `25` (standard).
        *   Add extra padding after? Maybe standard spacing is fine.
    *   If normal row:
        *   `y` increment logic remains standard.

**Note:** If `UiRow` doesn't have an `isSeparator` flag, we can infer it by checking if `items[0].component` is dynamic_castable to `SeparatorComponent`.

**Action:**
Update `MappingInspector::resized` to add `12px` of extra top margin whenever it encounters a Separator row.