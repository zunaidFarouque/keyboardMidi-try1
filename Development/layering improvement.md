Layer Inheritance Through Layer Commands (Revised)

Requirement Change

Previous: Fall through to the next lower layer that has a non-layer-command.

New: Fall through to the next ACTIVE lower layer that has a non-layer-command.

Compile Time: Active = Only Layer 0 (Base)

At compile time we do not know runtime state. We assume only Layer 0 is active for fall-through purposes.





When we hit a layer command, replace it with Layer 0's mapping for that key (if any, and not a layer command), else Empty.



No need to walk L-2 down to 0; we always inherit from Layer 0 only.



Result: any key with a layer command in the previous layer directly inherits from Layer 0.

Runtime: Two Options

Option A – Runtime Skip (Recommended)

Idea: Keep current 9 grids (one per layer view). At lookup time, iterate active layers from highest to lowest. Skip any mapping that is a layer command. Use the first non-layer-command.





Complexity: O(k) where k = number of active layers (typically 1–3).



Memory: No change; 9 grids per alias.



Pros: Simple, minimal memory, very fast in practice.



Cons: Up to ~9 comparisons per key lookup.

Implementation: In Source/InputProcessor.cpp, in the layer iteration loop (around line 409), after if (!slot.isActive) continue;, add:

if (midiAction.type == ActionType::Command && isLayerCommand(midiAction))
  continue;  // Skip layer commands, use next active layer

isLayerCommand must be accessible (move to shared header or duplicate logic). Same skip in hasManualMappingForKey, getMappingType, getMappingForInput, etc., where we iterate layers.

Option B – Pre-compile All Combinations

Idea: Compile grids for every combination of active layers. At runtime, use a bitmask of active layers to index into the pre-compiled map. O(1) lookup.





Combinations: 2^8 = 256 (layers 1..8; layer 0 always active).



Memory: ~256 grids * 256 keys * ~200 B ≈ 13 MB per alias.



Pros: O(1) lookup.



Cons: Large memory use, more complex compilation, marginal realtime gain over Option A.

Assessment: Option A is sufficient for realtime. Option B is only worth it if profiling shows lookup cost is a bottleneck, which is unlikely.

Recommended Approach: Option A + Compile-Time Fall-through to Layer 0





Compile time (GridCompiler): When we filter a layer command, replace it with globalGrids[0][key] if that slot is active and not a layer command, else Empty. This gives correct visuals and base-case behavior.



Runtime (InputProcessor): When iterating active layers for a key, skip layer commands and continue to the next active layer. This respects actual runtime activation.



Device Pass 2: Add targetLayerForView and skip layer commands when layerId < targetLayerForView so device-specific layer commands do not override the correct inheritance.

Data Flow

flowchart TD
  subgraph Compile [Compile Time]
    A[Copy from L-1]
    B[For each key with layer command]
    C[Replace with Layer 0 slot]
    D[If Layer 0 has non-layer-cmd use it else Empty]
    A --> B --> C --> D
  end

  subgraph Runtime [Runtime - Option A]
    R1[Iterate active layers 8 to 0]
    R2[Found mapping?]
    R3[Is layer command?]
    R4[Skip continue]
    R5[Use it]
    R1 --> R2 --> R3
    R3 -->|Yes| R4 --> R1
    R3 -->|No| R5
  end

Files to Modify







File



Changes





Source/GridCompiler.cpp



Fall-through to Layer 0 only when filtering layer commands; Device Pass 2: skip layer commands when layerId &lt; targetLayerForView





Source/InputProcessor.cpp



Skip layer commands when iterating layers (processEvent, hasManualMappingForKey, getMappingType, getMappingForInput, etc.)





Source/MappingTypes.h or shared header



Expose isLayerCommand for InputProcessor (or inline equivalent)





Source/Tests/GridCompilerTests.cpp



Add LayerInheritsThroughLayerCommand test





Source/Tests/InputProcessorTests.cpp



Add runtime test: D on L0=G5, L1=Momentary, L2=empty; on Layer 2, D triggers G5

Alternative: Lazy Compile (Option B Light)

If Option B is desired but with lower memory:





Track which layers have any mapping for each key.



On first access for a given (keyCode, activeLayerMask), compile and cache.



Memory grows with observed combinations (often far fewer than 256).



Adds cache logic and invalidation on preset change. More complex; only pursue if Option A proves insufficient.

