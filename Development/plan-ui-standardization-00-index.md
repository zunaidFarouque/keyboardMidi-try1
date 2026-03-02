# UI Standardization Plan – Index

**Goal:** Standardize separation between Core logic and UI so that (1) features and logic are not broken during refactors, and (2) future visual overhauls (themes, layout changes, beautification) can be done without touching main logic.

**Principle:** Core owns all business logic and schema definitions; UI only renders, captures input, and delegates to Core/Logic with a stable contract.

---

## Related Documents

| Document | Focus |
|----------|--------|
| [plan-naming-standardization.md](plan-naming-standardization.md) | Renames (Touchpad Mixer → Layout, etc.). **Do not duplicate** – coordinate with that plan where it touches the same files. |
| [display-strings-audit.md](display-strings-audit.md) | User-facing strings; already applied. |
| [AI-build-and-test-guide.md](AI-build-and-test-guide.md) | Build and test commands. |

---

## This Standardization (UI/Core Separation)

| # | Document | Contents |
|---|----------|----------|
| **01** | [plan-ui-standardization-01-core-ui-separation.md](plan-ui-standardization-01-core-ui-separation.md) | Core vs UI inventory; logic that lives in UI today; **logic extraction** (Zone panel → use ZonePropertiesLogic; Touchpad panel → new TouchpadEditorLogic; helpers like aliasNameToHash → Core); where SettingsDefinition/StartupManager live. |
| **02** | [plan-ui-standardization-02-ui-organization-and-contracts.md](plan-ui-standardization-02-ui-organization-and-contracts.md) | UI folder structure (optional); **contract stability** (propertyKey, schema); what stays fixed when doing themes/layout/beautification; checklist so visual overhaul doesn’t break logic. |
| **03** | [plan-ui-standardization-03-testing-and-regression.md](plan-ui-standardization-03-testing-and-regression.md) | How to ensure nothing breaks: test run order, what to add (unit tests for new Logic, optional manual smoke list), step-by-step safety. |
| **04** | [plan-ui-standardization-04-per-feature-summary.md](plan-ui-standardization-04-per-feature-summary.md) | Per-feature summary: Keyboard Mappings, Zones, Touchpad, Settings – what is in Core vs UI, status, and action items. Quick reference when working on one area. |

---

## Recommended Order of Execution

1. **Read all three plans** (01, 02, 03).
2. **Run full test suite** and record baseline (see 03).
3. **Execute 01** in small steps: one extraction at a time, run tests after each step.
4. **Optional:** Apply 02 (folder structure) if desired; 02’s contract rules apply regardless.
5. **Before any visual overhaul:** Use 02’s contract checklist and 03’s regression steps.

---

## Summary of Current State (from analysis)

- **Core** (`MIDIQy_Core`): MappingCompiler, InputProcessor, ZoneManager, Zone, ZoneDefinition, **ZonePropertiesLogic**, **KeyboardMappingInspectorLogic**, MappingDefinition, TouchpadLayoutManager, TouchpadLayoutDefinition, SettingsManager, etc. No `juce_gui_basics`/`juce_gui_extra`.
- **UI** (`MIDIQy`): All `*Panel`, `*Component`, `*Editor*`, MainComponent, SettingsDefinition, StartupManager. Uses schemas from Core (ZoneDefinition, MappingDefinition, TouchpadLayoutDefinition) but **ZonePropertiesPanel does not call ZonePropertiesLogic** – it duplicates logic in lambdas. **TouchpadEditorPanel** has no Logic layer; all property apply logic is inline.
- **Tests:** Core logic (ZonePropertiesLogic, KeyboardMappingInspectorLogic, MappingCompiler, etc.) is well covered. UI does not call Logic in one place (Zone panel), so behavior is duplicated and could drift.

Standardization fixes this so that UI always delegates to Core/Logic, enabling safe refactors and safe visual overhauls.
