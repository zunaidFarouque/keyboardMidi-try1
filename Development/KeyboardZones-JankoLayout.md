### Keyboard Zones – Janko Layout

- **Layout Strategy**: `Janko` is available under **Keys & Layout → Layout Strategy** alongside `Linear`, `Grid`, and `Piano`.
- **Behavior**:
  - Uses a **chromatic isomorphic mapping**: moving roughly one key to the right on the captured keyboard shape is a whole step, and moving between adjacent rows/columns approximates a semitone.
  - Internally it uses a **chromatic scale** (12-TET) for cache generation, so chord shapes and fingerings remain consistent across keys.
- **Usage tips**:
  - Capture **at least two rows of keys** (e.g. a QWERTY-style block) to get useful Janko shapes.
  - Janko respects zone settings like **chord type**, **degree offset**, and **global transpose**, but ignores the musical scale choice for its geometric layout (it always lays out chromatically).
  - Use this when you want **uniform fingering patterns** for scales and chords, independent of key.

