# Touchpad Layout Flow – Real-Time Performance Analysis

## Summary

| Component | Current | O(1)? | Notes |
|-----------|---------|-------|-------|
| **Compiled structs** | Yes | ✓ | `invRegionWidth`, `invRegionHeight` precomputed; no branching in hot path |
| **findLayoutForPoint** | O(L) | ✗ | Iterates all layouts; L = layout count |
| **Per-frame calls** | O((4 + M + C×D) × L) | ✗ | Redundant calls + strip/contact loops |
| **Visualizer paint** | O(L) | ✓ (acceptable) | UI thread; L typically small |

**L** = layouts, **M** = mixer strips, **D** = drum strips, **C** = contacts (typically 1–4)

---

## What Is Already O(1)

- **Coord remapping**: `(nx - regionLeft) * invRegionWidth` – no loops
- **Fader index**: `static_cast<int>(localX * N)` – O(1)
- **Pad index**: `row * columns + col` – O(1)
- **Compiled lookup**: `touchpadMixerStrips[index]`, `touchpadDrumPadStrips[index]` – O(1)

---

## What Is NOT O(1)

### 1. `findLayoutForPoint(nx, ny)`

**Location**: `InputProcessor.cpp` ~1569

```cpp
for (size_t i = 0; i < ctx->touchpadLayoutOrder.size(); ++i) {
  const auto &ref = ctx->touchpadLayoutOrder[i];
  // ... check region contains (nx,ny)
  if (nx >= s.regionLeft && nx < s.regionRight && ...)
    return {{type, ref.index}};
}
```

**Complexity**: O(L) per call. It scans layouts in z-order.

### 2. Redundant Calls (Bug)

**Location**: `InputProcessor.cpp` ~1596–1601

```cpp
bool drumPadConsumesFinger1Down =
    tip1 && findLayoutForPoint(x1, y1).has_value() &&
    findLayoutForPoint(x1, y1)->first == TouchpadType::DrumPad;  // duplicate!
bool drumPadConsumesFinger2Down =
    tip2 && findLayoutForPoint(x2, y2).has_value() &&
    findLayoutForPoint(x2, y2)->first == TouchpadType::DrumPad;  // duplicate!
```

`findLayoutForPoint` is called twice for each finger. Fix: cache result.

### 3. Overuse in Loops

- **Mixer loop**: `findLayoutForPoint(applierX, applierY)` called **M** times (once per strip). Only 2 distinct points (finger1, finger2). Should call at most 2×, cache by point.
- **Drum Pad loop**: `findLayoutForPoint(c.normX, c.normY)` called **C × D** times (each contact × each strip). Should call **C** times, cache by contact.

---

## Path to O(1) Point→Layout Lookup

To make point→layout lookup O(1), use a **spatial grid** at compile time:

1. **GridCompiler**: Build a 2D grid (e.g. 32×32 or 64×64) over normalized (0–1) touchpad.
2. For each cell `(gx, gy)`, compute which layout (by z-order) contains the cell center.
3. Store `layoutRef[gx][gy]` → `{TouchpadType, index}`.
4. **Runtime**: `gx = (int)(nx * 31)`, `gy = (int)(ny * 31)` → O(1) array index.

**Trade-off**: Slightly more memory (~2–8 KB for 32×32), compile-time cost once per preset change.

---

## Quick Wins (Without O(1) Grid)

1. **Cache `findLayoutForPoint` for appliers**  
   Call once for (x1,y1) and (x2,y2); reuse for drumPadConsumes and mixer loop.

2. **Cache per contact in drum loop**  
   Before looping strips, for each contact compute `layoutForContact[c]`. Use that when iterating strips.

3. **Remove redundant calls**  
   Fix the duplicate `findLayoutForPoint(x1,y1)` and `findLayoutForPoint(x2,y2)`.

These keep per-frame cost at O(L) but cut the number of calls from O((4 + M + C×D) × L) to O((2 + C) × L).

---

## Typical Numbers

- L = 2–10 layouts
- M, D = 1–5 each
- C = 1–4 contacts

**Worst case today**: 4 + 5 + (4×5) = 29 calls × 10 layouts = 290 iterations per frame.  
**After quick wins**: (2 + 4) × 10 = 60 iterations per frame.

For L ≤ 10, this is still effectively “constant” in practice, but not O(1) by definition.
