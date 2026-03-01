#include "VisualizerComponent.h"
#include "ColourContrast.h"
#include "InputProcessor.h"
#include "KeyboardLayoutUtils.h"
#include "MappingTypes.h"
#include "MidiNoteUtilities.h"
#include "PitchPadUtilities.h"
#include "PresetManager.h"
#include "RawInputManager.h"
#include "ScaleLibrary.h"
#include "SettingsManager.h"
#include "TouchpadMixerTypes.h"
#include "VoiceManager.h"
#include "juce_graphics/juce_graphics.h"
#include <JuceHeader.h>
#include <algorithm>
#include <optional>

// Main window refresh cap: 30 FPS (must match
// MainComponent::kMainWindowRefreshIntervalMs)
static constexpr int kMainWindowRefreshIntervalMs = 34;

// Reserved width for touchpad panel on the LEFT of the keyboard (avoids
// overlap with other UI). 5:4 touchpad rectangle fits inside.
static constexpr float kTouchpadPanelLeftWidth = 180.0f;
static constexpr float kTouchpadPanelMargin = 16.0f;
// Touchpad rectangle aspect ratio: width:height = 3:2
static constexpr float kTouchpadAspectW = 3.0f;
static constexpr float kTouchpadAspectH = 2.0f;

// Helper to convert alias name to hash (same as in InputProcessor)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" ||
      aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

// Draggable bar to resize the global panel; collapse when dragged too far left
namespace {
class GlobalPanelResizerBar : public juce::Component {
public:
  std::function<void(float)> onWidthChange;

  void mouseDrag(const juce::MouseEvent &e) override {
    if (auto *p = getParentComponent(); p && onWidthChange) {
      int x = e.getEventRelativeTo(p).getPosition().getX();
      float w = (float)(p->getWidth() - x - getWidth());
      onWidthChange(w);
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff2a2a2a));
    g.setColour(juce::Colours::darkgrey);
    int cx = getWidth() / 2;
    for (int i = -1; i <= 1; ++i)
      g.fillRect(cx - 1 + i * 3, 8, 2, getHeight() - 16);
  }
};
} // namespace

juce::Colour
VisualizerComponent::getTextColorForKeyFill(juce::Colour keyFillColor) {
  return ColourContrast::getTextColorForKeyFill(keyFillColor);
}

VisualizerComponent::VisualizerComponent(
    ZoneManager *zoneMgr, DeviceManager *deviceMgr,
    const VoiceManager &voiceMgr, SettingsManager *settingsMgr,
    PresetManager *presetMgr, InputProcessor *inputProc, ScaleLibrary *scaleLib)
    : zoneManager(zoneMgr), deviceManager(deviceMgr), voiceManager(voiceMgr),
      settingsManager(settingsMgr), presetManager(presetMgr),
      inputProcessor(inputProc), scaleLibrary(scaleLib),
      globalPanel(zoneMgr, scaleLib) {
  // Phase 42: Listeners moved to initialize() – no addListener here

  addAndMakeVisible(globalPanel);

  globalPanelResizerBar = std::make_unique<GlobalPanelResizerBar>();
  auto *resizer =
      static_cast<GlobalPanelResizerBar *>(globalPanelResizerBar.get());
  resizer->setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
  resizer->onWidthChange = [this](float w) {
    setGlobalPanelWidthFromResizer(w);
  };
  addAndMakeVisible(*globalPanelResizerBar);

  addAndMakeVisible(expandPanelButton);
  expandPanelButton.setButtonText("<");
  expandPanelButton.setTooltip("Show global controls (Root, Scale, Transpose)");
  expandPanelButton.onClick = [this] {
    globalPanelCollapsed_ = false;
    globalPanelWidth_ = kGlobalPanelDefaultWidth;
    resized();
  };

  // Setup View Selector (Phase 39)
  addAndMakeVisible(viewSelector);
  viewSelector.onChange = [this] { onViewSelectorChanged(); };
  updateViewSelector();
  // Ensure selector is on top to receive mouse events
  viewSelector.toFront(false);

  // Phase 50.9.1: Follow Input toggle (always visible; layer system works
  // regardless of Studio Mode)
  addAndMakeVisible(followButton);
  followButton.setClickingTogglesState(true);
  followButton.setTooltip("When on, the visualizer follows the layer currently "
                          "being triggered by input.");
  followButton.onClick = [this] {
    followInputEnabled.store(followButton.getToggleState(),
                             std::memory_order_release);
    updateFollowButtonAppearance();
  };
  updateFollowButtonAppearance();

  // Show selected layer: when on, visualizer shows the layer selected in the
  // current tab (Mappings = selected layer, Zones = selected zone's layer).
  addAndMakeVisible(showSelectedLayerButton);
  showSelectedLayerButton.setClickingTogglesState(true);
  showSelectedLayerButton.setTooltip(
      "When on, the visualizer shows the layer selected in the current tab "
      "(Mappings tab = selected layer, Zones tab = selected zone's layer).");
  showSelectedLayerButton.onClick = [this] {
    showSelectedLayerEnabled_ = showSelectedLayerButton.getToggleState();
    if (settingsManager)
      settingsManager->setVisualizerShowSelectedLayer(showSelectedLayerEnabled_);
    if (showSelectedLayerEnabled_ && onShowSelectedLayerToggledOn)
      onShowSelectedLayerToggledOn();
    updateShowSelectedLayerButtonAppearance();
  };
  updateShowSelectedLayerButtonAppearance();

  touchpadPanel_ =
      std::make_unique<TouchpadVisualizerPanel>(inputProc, settingsMgr);
  addAndMakeVisible(*touchpadPanel_);

  // Hide view selector if Studio Mode is OFF (Phase 9.5)
  if (settingsManager) {
    viewSelector.setVisible(settingsManager->isStudioMode());
    if (!settingsManager->isStudioMode()) {
      // Lock to Global view
      currentViewHash = 0;
      viewSelector.setSelectedId(1, juce::dontSendNotification);
    }
  }

  // Initial positioning (will be updated in resized(), but set initial bounds)
  viewSelector.setBounds(0, 0, 200, 25);
  startTimer(settingsManager ? settingsManager->getWindowRefreshIntervalMs()
                             : kMainWindowRefreshIntervalMs);
}

void VisualizerComponent::setVisualizedLayer(int layerId) {
  if (layerId < 0)
    layerId = 0;
  currentVisualizedLayer = layerId;
  if (touchpadPanel_) {
    touchpadPanel_->setVisualizedLayer(layerId);
    touchpadPanel_->repaint();
  }
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::setSelectedTouchpadLayout(int layoutIndex,
                                                    int layerId) {
  selectedTouchpadLayoutIndex_ = layoutIndex;
  selectedTouchpadLayoutLayerId_ = layoutIndex >= 0 ? layerId : 0;
  if (touchpadPanel_) {
    touchpadPanel_->setSelectedLayout(layoutIndex,
                                      layoutIndex >= 0 ? layerId : 0);
    if (layoutIndex < 0)
      touchpadPanel_->setSoloLayoutGroupForEditing(-1);
  }
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::setSoloLayoutGroupForEditing(int groupId) {
  if (touchpadPanel_)
    touchpadPanel_->setSoloLayoutGroupForEditing(groupId);
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::initialize() {
  if (zoneManager)
    zoneManager->addChangeListener(this);
  if (settingsManager) {
    settingsManager->addChangeListener(this);
    showSelectedLayerEnabled_ =
        settingsManager->getVisualizerShowSelectedLayer();
    updateShowSelectedLayerButtonAppearance();
  }
  if (inputProcessor)
    inputProcessor->addChangeListener(
        this); // Phase 48: repaint on layer changes
  if (presetManager) {
    auto mappingsNode = presetManager->getMappingsNode();
    if (mappingsNode.isValid())
      mappingsNode.addListener(this);
    presetManager->getRootNode().addListener(this);
  }
  if (deviceManager)
    deviceManager->addChangeListener(this);
}

VisualizerComponent::~VisualizerComponent() {
  // Stop timer callbacks immediately
  stopTimer();

  // Unregister listeners
  // Note: rawInputManager listener is removed by MainComponent destructor
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
  if (settingsManager) {
    settingsManager->removeChangeListener(this);
  }
  if (inputProcessor) {
    inputProcessor->removeChangeListener(this);
  }
  if (presetManager) {
    auto mappingsNode = presetManager->getMappingsNode();
    if (mappingsNode.isValid()) {
      mappingsNode.removeListener(this);
    }
    presetManager->getRootNode().removeListener(this);
  }
  if (deviceManager) {
    deviceManager->removeChangeListener(this);
  }
  // VoiceManager doesn't have a listener interface, it's polled.
}

void VisualizerComponent::refreshCache() {
  // Phase 52.2: Render pre-compiled VisualGrid from InputProcessor only.
  if (getWidth() <= 0 || getHeight() <= 0) {
    cacheValid = false;
    backgroundCache = juce::Image();
    return;
  }
  if (!inputProcessor) {
    cacheValid = false;
    return;
  }
  if (!zoneManager) {
    cacheValid = false;
    return;
  }

  int width = getWidth();
  int height = getHeight();
  // Content area: left (touchpad) + center (keyboard); right is global panel
  int contentWidth = width - static_cast<int>(getEffectiveRightPanelWidth());
  if (contentWidth < 0)
    contentWidth = 0;

  if (width <= 0 || height <= 0) {
    cacheValid = false;
    backgroundCache = juce::Image(); // Clear invalid cache
    return;
  }

  // Create cache image matching component size
  // Use a temporary image first, then assign to ensure proper cleanup
  juce::Image newCache(juce::Image::ARGB, width, height, true);

  {
    juce::Graphics g(newCache);

    g.fillAll(juce::Colour(0xff111111)); // Background

    // --- 0. Header Bar (Transpose left, Sustain right) - only over content
    // area ---
    auto bounds = juce::Rectangle<int>(0, 0, contentWidth, height);
    auto headerRect = bounds.removeFromTop(30);
    g.setColour(juce::Colour(0xff222222));
    g.fillRect(headerRect);

    g.setColour(juce::Colours::white);
    g.setFont(12.0f);

    // TRANSPOSE (left) – pitch only
    if (zoneManager) {
      int chrom = zoneManager->getGlobalChromaticTranspose();
      juce::String chromStr =
          (chrom >= 0) ? ("+" + juce::String(chrom)) : juce::String(chrom);
      juce::String transposeText = "Transpose: " + chromStr + " st";
      g.drawText(transposeText, 8, 0, 200, headerRect.getHeight(),
                 juce::Justification::centredLeft, false);
    }

    // Sustain Indicator (right) - Use last known state for cache
    juce::Colour sustainColor =
        lastSustainState ? juce::Colours::lime : juce::Colours::grey;
    int indicatorSize = 12;
    int indicatorX = headerRect.getRight() - 100;
    int indicatorY = headerRect.getCentreY() - indicatorSize / 2;
    g.setColour(sustainColor);
    g.fillEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);
    g.setColour(juce::Colours::white);
    g.drawText("SUSTAIN", indicatorX + indicatorSize + 5, indicatorY, 60,
               indicatorSize, juce::Justification::centredLeft, false);

    // --- 1. Calculate Dynamic Scale ---
    float unitsWide = 23.0f;
    float unitsTall = 7.3f;
    float headerHeight = 30.0f;
    float availableHeight = static_cast<float>(height) - headerHeight;
    // Reserve LEFT for touchpad, RIGHT for global panel; center = keyboard
    float availableForKeyboard = static_cast<float>(contentWidth) -
                                 kTouchpadPanelLeftWidth -
                                 (2.0f * kTouchpadPanelMargin);

    float scaleX = (availableForKeyboard > 0.0f)
                       ? (availableForKeyboard / unitsWide)
                       : (static_cast<float>(contentWidth) / unitsWide);
    float scaleY =
        (availableHeight > 0.0f) ? (availableHeight / unitsTall) : scaleX;
    float keySize = std::min(scaleX, scaleY) * 0.9f;

    float row4Bottom = 5.8f * keySize;
    float minStartY = headerHeight + 1.44f * keySize;
    float maxStartY = static_cast<float>(height) - row4Bottom;

    float startY = (minStartY + maxStartY) / 2.0f;
    startY = juce::jlimit(minStartY, maxStartY, startY);

    float totalWidth = unitsWide * keySize;
    // Keyboard starts after the left touchpad panel
    float startX = kTouchpadPanelLeftWidth + kTouchpadPanelMargin +
                   (availableForKeyboard > totalWidth
                        ? (availableForKeyboard - totalWidth) * 0.5f
                        : 0.0f);

    // --- 2. Iterate Keys (Draw Static State Only) ---
    // Acquire baked visual grid from InputProcessor (Phase 50.6).
    std::shared_ptr<const CompiledMapContext> contextPtr;
    std::shared_ptr<const VisualGrid> targetGrid;

    if (inputProcessor) {
      contextPtr = inputProcessor->getContext();
    }
    if (contextPtr) {
      const auto &visualLookup = contextPtr->visualLookup;
      const std::vector<std::shared_ptr<const VisualGrid>> *layerVec = nullptr;

      auto it = visualLookup.find(currentViewHash);
      if (it != visualLookup.end() && currentViewHash != 0) {
        layerVec = &it->second;
      } else {
        auto itGlobal = visualLookup.find(0);
        if (itGlobal != visualLookup.end()) {
          layerVec = &itGlobal->second;
        }
      }

      if (layerVec && currentVisualizedLayer >= 0 &&
          currentVisualizedLayer < (int)layerVec->size()) {
        targetGrid = (*layerVec)[(size_t)currentVisualizedLayer];
      }
    }

    const auto &layout = KeyboardLayoutUtils::getLayout();
    for (const auto &pair : layout) {
      int keyCode = pair.first;
      const auto &geometry = pair.second;

      float rowOffset =
          (geometry.row == -1) ? -1.2f : static_cast<float>(geometry.row);
      float x = startX + (geometry.col * keySize);
      float y = startY + (rowOffset * keySize * 1.2f);
      float w = geometry.width * keySize;
      float h = geometry.height * keySize;

      juce::Rectangle<float> fullBounds(x, y, w, h);

      float padding = keySize * 0.1f;
      auto keyBounds = fullBounds.reduced(padding);

      // --- 3. Get Data from pre-baked VisualGrid (Phase 50.4/50.6) ---
      juce::Colour underlayColor = juce::Colours::transparentBlack;
      juce::Colour borderColor = juce::Colours::grey;
      float borderWidth = 1.0f;
      float alpha = 1.0f;

      VisualState state = VisualState::Empty;
      juce::String labelText = geometry.label;
      bool isGhost = false;

      if (targetGrid && keyCode >= 0 && keyCode < (int)targetGrid->size()) {
        const auto &slot = (*targetGrid)[(size_t)keyCode];
        int keyboardSolo = inputProcessor ? inputProcessor->getEffectiveKeyboardSoloGroupForLayer(currentVisualizedLayer) : 0;
        bool slotFilteredBySolo = (keyboardSolo == 0 && slot.keyboardGroupId != 0) ||
                                 (keyboardSolo > 0 && slot.keyboardGroupId != keyboardSolo);
        if (!slotFilteredBySolo) {
          state = slot.state;
          isGhost = slot.isGhost;
          if (!slot.displayColor.isTransparent())
            underlayColor = slot.displayColor;
          if (slot.label.isNotEmpty())
            labelText = slot.label;
        }
      }

      // Phase 52.2 / 54.1: Drawing rules from pre-compiled VisualGrid
      juce::Colour textColor = juce::Colours::white;

      if (state == VisualState::Inherited) {
        alpha = 0.3f;
        borderColor = juce::Colours::grey;
        borderWidth = 1.0f;
      } else if (state == VisualState::Override) {
        alpha = 1.0f;
        borderColor = juce::Colours::orange;
        borderWidth = 2.5f;
      } else if (state == VisualState::Conflict) {
        alpha = 1.0f;
        underlayColor = juce::Colours::darkred;
        borderColor = juce::Colours::yellow;
        borderWidth = 2.5f;
        textColor = juce::Colours::white;
      } else if (state == VisualState::Active) {
        alpha = 1.0f;
        borderColor = juce::Colours::grey;
        borderWidth = 1.0f;
      } else {
        alpha = 1.0f;
        borderColor = juce::Colours::grey;
        borderWidth = 1.0f;
      }

      // Phase 54.1: Ghost keys (e.g. passing tones) – dimmer
      if (isGhost &&
          (state == VisualState::Active || state == VisualState::Inherited))
        alpha *= 0.5f;

      // Phase 54.1/54.4: Smart contrast – text color from key fill (not
      // backdrop)
      // 1. Conflict: Always White on Red
      if (state == VisualState::Conflict) {
        textColor = juce::Colours::white;
      }
      // 2. Inherited (Dim): Always White (dim key is dark)
      else if (state == VisualState::Inherited) {
        textColor = juce::Colours::white;
      }
      // 3. Active/Override/Empty: Use key fill color (Layer 2 key body we draw)
      else {
        const juce::Colour keyFillColor(0xff333333); // Key body fill
        textColor = ColourContrast::getTextColorForKeyFill(keyFillColor);
      }

      // --- 4. Render Static Layers (Off State) ---

      // Layer 1: Underlay (Zone Color) - Phase 39.5: Alpha already applied
      if (!underlayColor.isTransparent()) {
        auto finalUnderlay = underlayColor.withAlpha(alpha);
        g.setColour(finalUnderlay);
        g.fillRect(fullBounds);
      }

      // Layer 2: Key Body (Dark Grey - Off State)
      g.setColour(juce::Colour(0xff333333));
      g.fillRoundedRectangle(keyBounds, 6.0f);

      // Layer 3: Border (Phase 39.1: Use VisualState)
      g.setColour(borderColor);
      g.drawRoundedRectangle(keyBounds, 6.0f, borderWidth);

      // Layer 4: Text (Phase 50.8: red for conflicts, white otherwise)
      g.setColour(textColor);
      g.setFont(keySize * 0.4f);
      g.drawText(labelText, keyBounds, juce::Justification::centred, false);
    }
  } // Graphics object destroyed here

  // Assign new cache only after Graphics is fully destroyed
  backgroundCache = std::move(newCache);
  cacheValid = true;
}

void VisualizerComponent::paint(juce::Graphics &g) {
  // SAFETY: Don't draw or touch cache when bounds are empty
  if (getWidth() <= 0 || getHeight() <= 0)
    return;
  if (!zoneManager)
    return;

  // Check Cache Validity
  if (!cacheValid || backgroundCache.isNull() ||
      backgroundCache.getWidth() != getWidth() ||
      backgroundCache.getHeight() != getHeight()) {
    refreshCache();
  }

  // Safety check: ensure cache is valid before drawing
  if (backgroundCache.isNull() || !cacheValid) {
    g.fillAll(juce::Colour(0xff111111)); // Fallback background
    return;
  }

  // Draw Background Cache
  g.drawImageAt(backgroundCache, 0, 0);

  // Update Sustain Indicator (dynamic - always redraw since it changes
  // frequently). Must use content area width so it's not hidden under the
  // global panel on the right.
  int contentW = getWidth() - static_cast<int>(getEffectiveRightPanelWidth());
  if (contentW < 0)
    contentW = 0;
  auto headerRect = juce::Rectangle<int>(0, 0, contentW, 30);
  bool sustainActive = voiceManager.isSustainActive();
  juce::Colour sustainColor =
      sustainActive ? juce::Colours::lime : juce::Colours::grey;
  int indicatorSize = 12;
  int indicatorX = headerRect.getRight() - 100;
  int indicatorY = headerRect.getCentreY() - indicatorSize / 2;
  g.setColour(juce::Colour(0xff222222));
  g.fillRect(indicatorX - 5, indicatorY - 2, 80,
             indicatorSize + 4); // Clear area
  g.setColour(sustainColor);
  g.fillEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);
  g.setColour(juce::Colours::white);
  g.setFont(12.0f);
  g.drawText("SUSTAIN", indicatorX + indicatorSize + 5, indicatorY, 60,
             indicatorSize, juce::Justification::centredLeft, false);

  // Phase 45: Active Layers HUD (uses InputProcessor state via ReadLock)
  if (inputProcessor) {
    juce::StringArray activeLayers = inputProcessor->getActiveLayerNames();
    if (activeLayers.size() > 0) {
      juce::String joined = activeLayers.joinIntoString(" | ");
      g.setColour(juce::Colours::cyan);
      g.setFont(12.0f);
      auto layersBounds =
          headerRect.withLeft(300).reduced(4, 4); // to the right of TRANSPOSE
      g.drawFittedText("LAYERS: " + joined, layersBounds,
                       juce::Justification::centredLeft, 1);
    }
  }

  // Check if MIDI mode is disabled and draw overlay
  bool midiModeDisabled =
      settingsManager && !settingsManager->isMidiModeActive();

  // --- Calculate Dynamic Scale (Same as refreshCache) ---
  // contentW already computed above for sustain indicator
  float unitsWide = 23.0f;
  float unitsTall = 7.3f;
  float headerHeight = 30.0f;
  float availableHeight = static_cast<float>(getHeight()) - headerHeight;
  float availableForKeyboard = static_cast<float>(contentW) -
                               kTouchpadPanelLeftWidth -
                               (2.0f * kTouchpadPanelMargin);

  float scaleX = (availableForKeyboard > 0.0f)
                     ? (availableForKeyboard / unitsWide)
                     : (static_cast<float>(contentW) / unitsWide);
  float scaleY =
      (availableHeight > 0.0f) ? (availableHeight / unitsTall) : scaleX;
  float keySize = std::min(scaleX, scaleY) * 0.9f;

  float row4Bottom = 5.8f * keySize;
  float minStartY = headerHeight + 1.44f * keySize;
  float maxStartY = static_cast<float>(getHeight()) - row4Bottom;

  float startY = (minStartY + maxStartY) / 2.0f;
  startY = juce::jlimit(minStartY, maxStartY, startY);

  float totalWidth = unitsWide * keySize;
  float startX = kTouchpadPanelLeftWidth + kTouchpadPanelMargin +
                 (availableForKeyboard > totalWidth
                      ? (availableForKeyboard - totalWidth) * 0.5f
                      : 0.0f);

  // Snapshot active keys under lock (RawInput may come from OS thread)
  std::set<int> activeKeysSnapshot;
  {
    juce::ScopedLock lock(keyStateLock);
    activeKeysSnapshot = activeKeys;
  }

  // Collect all keys that need dynamic rendering (active or latched)
  std::set<int> keysToRender;
  keysToRender.insert(activeKeysSnapshot.begin(), activeKeysSnapshot.end());

  // Also check for latched keys
  const auto &layout = KeyboardLayoutUtils::getLayout();
  for (const auto &pair : layout) {
    int keyCode = pair.first;
    if (voiceManager.isKeyLatched(keyCode)) {
      keysToRender.insert(keyCode);
    }
  }

  // Phase 52.2: Live input overlays only (no simulateInput). Yellow = pressed,
  // Cyan = latched.
  for (int keyCode : keysToRender) {
    auto it = layout.find(keyCode);
    if (it == layout.end())
      continue;

    const auto &geometry = it->second;

    // Calculate Position (same as refreshCache)
    float rowOffset =
        (geometry.row == -1) ? -1.2f : static_cast<float>(geometry.row);
    float x = startX + (geometry.col * keySize);
    float y = startY + (rowOffset * keySize * 1.2f);
    float w = geometry.width * keySize;
    float h = geometry.height * keySize;

    juce::Rectangle<float> fullBounds(x, y, w, h);
    float padding = keySize * 0.1f;
    auto keyBounds = fullBounds.reduced(padding);

    // Get key state (input overlays only – no simulation in paint)
    bool isPressed =
        (activeKeysSnapshot.find(keyCode) != activeKeysSnapshot.end());
    bool isLatched = voiceManager.isKeyLatched(keyCode);

    juce::String labelText = geometry.label;

    // Draw Latched State (Cyan)
    if (isLatched) {
      g.setColour(juce::Colours::cyan.withAlpha(0.8f));
      g.fillRoundedRectangle(keyBounds, 6.0f);
    }

    // Draw Active State (Yellow)
    if (isPressed) {
      g.setColour(juce::Colours::yellow);
      g.fillRoundedRectangle(keyBounds, 6.0f);
    }

    // Redraw Border (simple overlay border)
    juce::Colour dynamicBorderColor = juce::Colours::grey;
    float dynamicBorderWidth = 1.0f;
    g.setColour(dynamicBorderColor);
    g.drawRoundedRectangle(keyBounds, 6.0f, dynamicBorderWidth);

    // Redraw Text: active key always has brighter fill → black; else white
    juce::Colour textColor =
        (isPressed || isLatched) ? juce::Colours::black : juce::Colours::white;
    g.setColour(textColor);
    g.setFont(keySize * 0.4f);
    g.drawText(labelText, keyBounds, juce::Justification::centred, false);
  }

  // Touchpad panel drawn by TouchpadVisualizerPanel child component

  // Draw overlay if MIDI mode is disabled
  if (midiModeDisabled) {
    // Semi-transparent black overlay
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillAll();

    // Draw text message
    g.setColour(juce::Colours::burlywood);
    g.setFont(20.0f);
    juce::String toggleKeyName =
        settingsManager
            ? RawInputManager::getKeyName(settingsManager->getToggleKey())
            : "F12";
    juce::String message =
        "MIDI MODE DISABLED\n(Press " + toggleKeyName + " to enable)";
    g.drawText(message, getLocalBounds(), juce::Justification::bottomLeft,
               false);
  }
}

float VisualizerComponent::getEffectiveRightPanelWidth() const {
  if (globalPanelCollapsed_)
    return kExpandTabWidth;
  return static_cast<float>(kResizerBarWidth) + globalPanelWidth_;
}

void VisualizerComponent::setGlobalPanelWidthFromResizer(float newWidth) {
  if (newWidth < kGlobalPanelMinWidth) {
    globalPanelCollapsed_ = true;
    globalPanelWidth_ =
        kGlobalPanelDefaultWidth; // restore default when re-expanded
  } else {
    globalPanelCollapsed_ = false;
    globalPanelWidth_ = juce::jlimit(60.0f, 500.0f, newWidth);
  }
  resized();
}

void VisualizerComponent::updateGlobalPanelLayout(int w, int h) {
  float effectiveRight = getEffectiveRightPanelWidth();
  int effRight = static_cast<int>(effectiveRight);

  if (globalPanelCollapsed_) {
    expandPanelButton.setBounds(w - static_cast<int>(kExpandTabWidth), 0,
                                static_cast<int>(kExpandTabWidth), h);
    expandPanelButton.setVisible(true);
    if (globalPanelResizerBar) {
      globalPanelResizerBar->setBounds(0, 0, 0, 0);
      globalPanelResizerBar->setVisible(false);
    }
    globalPanel.setBounds(0, 0, 0, 0);
    globalPanel.setVisible(false);
    expandPanelButton.toFront(false);
  } else {
    expandPanelButton.setBounds(0, 0, 0, 0);
    expandPanelButton.setVisible(false);
    int panelW = static_cast<int>(globalPanelWidth_);
    if (globalPanelResizerBar) {
      globalPanelResizerBar->setBounds(w - panelW - kResizerBarWidth, 0,
                                       kResizerBarWidth, h);
      globalPanelResizerBar->setVisible(true);
    }
    globalPanel.setBounds(w - panelW, 0, panelW, h);
    globalPanel.setVisible(true);
    globalPanelResizerBar->toFront(false);
    globalPanel.toFront(false);
  }
}

void VisualizerComponent::resized() {
  int w = getWidth();
  int h = getHeight();
  float effectiveRight = getEffectiveRightPanelWidth();
  int effRight = static_cast<int>(effectiveRight);

  updateGlobalPanelLayout(w, h);

  // Position Follow Input button (always visible) and View Selector (when
  // Studio Mode)
  int headerHeight = 30; // Status bar height
  int selectorHeight = 25;
  int margin = 10;
  int selectorY = headerHeight + margin;
  int buttonWidth = 110;
  int buttonHeight = selectorHeight;

  if (viewSelector.isVisible()) {
    int selectorWidth = 200;
    int selectorX = w - effRight - selectorWidth - margin;
    int buttonX = selectorX - buttonWidth - 8;
    followButton.setBounds(buttonX, selectorY, buttonWidth, buttonHeight);
    int showSelX = buttonX - buttonWidth - 8;
    showSelectedLayerButton.setBounds(showSelX, selectorY, buttonWidth,
                                     buttonHeight);
    viewSelector.setBounds(selectorX, selectorY, selectorWidth, selectorHeight);
  } else {
    // Follow Input and Show selected layer: place left of right panel
    int buttonX = w - effRight - buttonWidth - margin;
    followButton.setBounds(buttonX, selectorY, buttonWidth, buttonHeight);
    int showSelX = buttonX - buttonWidth - 8;
    showSelectedLayerButton.setBounds(showSelX, selectorY, buttonWidth,
                                      buttonHeight);
  }

  if (touchpadPanel_) {
    touchpadPanel_->setBounds(0, headerHeight,
                              static_cast<int>(kTouchpadPanelLeftWidth),
                              h - headerHeight);
  }

  cacheValid = false;
  needsRepaint = true;
  repaint();
}

void VisualizerComponent::updateFollowButtonAppearance() {
  const bool enabled = followInputEnabled.load(std::memory_order_relaxed);
  followButton.setToggleState(enabled, juce::dontSendNotification);

  // Simple visual cue
  followButton.setColour(juce::TextButton::buttonColourId,
                         enabled ? juce::Colours::darkgreen
                                 : juce::Colours::darkgrey);
  followButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colours::white);
  followButton.setColour(juce::TextButton::textColourOnId,
                         juce::Colours::white);
}

void VisualizerComponent::updateShowSelectedLayerButtonAppearance() {
  showSelectedLayerButton.setToggleState(showSelectedLayerEnabled_,
                                         juce::dontSendNotification);
  showSelectedLayerButton.setColour(juce::TextButton::buttonColourId,
                                    showSelectedLayerEnabled_
                                        ? juce::Colours::darkgreen
                                        : juce::Colours::darkgrey);
  showSelectedLayerButton.setColour(juce::TextButton::textColourOffId,
                                    juce::Colours::white);
  showSelectedLayerButton.setColour(juce::TextButton::textColourOnId,
                                    juce::Colours::white);
}

void VisualizerComponent::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                            bool isDown) {
  // Phase 50.9: Hot path – keep this extremely lightweight.
  // 1) Update mailbox with last input device (for Dynamic View), on key-down.
  if (isDown) {
    lastInputDeviceHandle.store(deviceHandle, std::memory_order_relaxed);
  }

  // 2) Track active keys (kept for visual overlays; guarded by lock).
  {
    juce::ScopedLock lock(keyStateLock);
    if (isDown)
      activeKeys.insert(keyCode);
    else
      activeKeys.erase(keyCode);
  }

  // 3) Mark dirty, but DO NOT repaint here – timerCallback() owns repaint.
  needsRepaint.store(true, std::memory_order_release);
}

void VisualizerComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                          float value) {
  // Ignore axis events
}

void VisualizerComponent::handleTouchpadContacts(
    uintptr_t deviceHandle, const std::vector<TouchpadContact> &contacts) {
  lastTouchpadDeviceHandle.store(deviceHandle, std::memory_order_release);
  {
    juce::ScopedLock lock(contactsLock);
    lastTouchpadContacts = contacts;
  }
  if (touchpadPanel_) {
    int throttleMs = settingsManager ? settingsManager->getWindowRefreshIntervalMs()
                                    : 34;
    int64_t now = juce::Time::getMillisecondCounter();
    bool throttleOk = (now - lastTouchpadPanelUpdateMs >= throttleMs);
    bool liftDetected = false;
    {
      juce::ScopedLock lock(contactsLock);
      liftDetected = touchpadContactsHaveLift(lastSentToPanelContacts_, contacts);
    }
    if (throttleOk || liftDetected) {
      lastTouchpadPanelUpdateMs = now;
      lastSentToPanelContacts_ = contacts;
      touchpadPanel_->setContacts(contacts, deviceHandle);
    }
  }
  needsRepaint.store(true, std::memory_order_release);
}

void VisualizerComponent::changeListenerCallback(
    juce::ChangeBroadcaster *source) {
  if (source == inputProcessor) {
    // Phase 48: layer state changed -> update HUD next frame
    needsRepaint = true;
    return;
  }
  if (source == &voiceManager) {
    // Sustain state changed -> update indicator immediately
    lastSustainState = voiceManager.isSustainActive();
    needsRepaint.store(true, std::memory_order_release);
    repaint();
    return;
  }
  if (source == zoneManager || source == settingsManager) {
    cacheValid = false;
    needsRepaint = true;
    // Update view selector visibility based on Studio Mode (Phase 9.5)
    if (source == settingsManager && settingsManager) {
      bool studioMode = settingsManager->isStudioMode();
      viewSelector.setVisible(studioMode);
      updateViewSelector();
      resized();
      viewSelector.toFront(false);
    }
  } else if (source == deviceManager) {
    // Device alias configuration changed, refresh view selector
    updateViewSelector();
  }
}

void VisualizerComponent::updateViewSelector() {
  // Phase 9.5: Studio Mode Check
  if (settingsManager && !settingsManager->isStudioMode()) {
    // Studio Mode OFF: Only show Global, disable selector
    viewSelector.clear(juce::dontSendNotification);
    viewHashes.clear();
    viewSelector.addItem("Global (All Devices)", 1);
    viewHashes.push_back(0);
    viewSelector.setSelectedItemIndex(0, juce::dontSendNotification);
    viewSelector.setEnabled(false);
    currentViewHash = 0;
    return;
  }

  // Studio Mode ON: Enable selector and populate normally
  viewSelector.setEnabled(true);

  // Phase 39.2: Save current selection using index
  uintptr_t currentHash = 0;
  int currentIndex = viewSelector.getSelectedItemIndex();
  if (currentIndex >= 0 && currentIndex < static_cast<int>(viewHashes.size())) {
    currentHash = viewHashes[currentIndex];
  }

  // Clear
  viewSelector.clear(juce::dontSendNotification);
  viewHashes.clear();

  if (deviceManager == nullptr) {
    // Add only Global option
    viewSelector.addItem("Global (All Devices)", 1);
    viewHashes.push_back(0);
    viewSelector.setSelectedItemIndex(0, juce::dontSendNotification);
    currentViewHash = 0;
    return;
  }

  // Add "Global (All Devices)" option (Index 0)
  viewSelector.addItem("Global (All Devices)", 1);
  viewHashes.push_back(0);

  // Add all aliases (Phase 39.2: Store full 64-bit hashes)
  auto aliases = deviceManager->getAllAliasNames();
  for (int i = 0; i < aliases.size(); ++i) {
    uintptr_t aliasHash = aliasNameToHash(aliases[i]);
    viewSelector.addItem(aliases[i], i + 2); // Start IDs from 2
    viewHashes.push_back(aliasHash);
  }

  // Restore selection if possible (Phase 39.2: Use index-based lookup)
  int restoreIndex = 0;
  for (size_t i = 0; i < viewHashes.size(); ++i) {
    if (viewHashes[i] == currentHash) {
      restoreIndex = static_cast<int>(i);
      break;
    }
  }
  viewSelector.setSelectedItemIndex(restoreIndex, juce::dontSendNotification);
  currentViewHash = viewHashes[restoreIndex];
}

void VisualizerComponent::onViewSelectorChanged() {
  // Phase 39.2: Use index-based lookup to get full 64-bit hash
  int index = viewSelector.getSelectedItemIndex();
  if (index >= 0 && index < static_cast<int>(viewHashes.size())) {
    currentViewHash = viewHashes[index];
  } else {
    currentViewHash = 0;
  }

  // Invalidate cache and repaint
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::restartTimerWithInterval(int intervalMs) {
  stopTimer();
  startTimer(intervalMs);
  if (touchpadPanel_)
    touchpadPanel_->restartTimerWithInterval(intervalMs);
}

void VisualizerComponent::setTouchpadTabActive(bool active) {
  touchpadTabActive_ = active;
  if (active && touchpadPanel_)
    touchpadPanel_->repaint();
}

void VisualizerComponent::timerCallback() {
  // Safeguard: timer is normally stopped when minimized (MainComponent); skip
  // work if ever called while minimized
  if (auto *topLevel = getTopLevelComponent()) {
    if (auto *peer = topLevel->getPeer()) {
      if (peer->isMinimised())
        return;
    }
  }

  // Phase 50.9: Async Dynamic View and 30Hz UI polling. Runs on the message
  // thread only.

  // Step 1: Dynamic View – adjust view + layer based on last input.
  // When NOT in touchpad tab and NOT "show selected layer": touchpad view
  // follows active layer. When show selected layer is on, MainComponent owns
  // the layer via setVisualizedLayer; do not overwrite here.
  if (!touchpadTabActive_ && inputProcessor && !showSelectedLayerEnabled_) {
    int activeLayer = inputProcessor->getHighestActiveLayerIndex();
    bool changed = false;
    if (currentVisualizedLayer != activeLayer) {
      setVisualizedLayer(activeLayer);
      changed = true;
      needsRepaint.store(true, std::memory_order_release);
    }
    if (selectedTouchpadLayoutIndex_ >= 0) {
      setSelectedTouchpadLayout(-1, 0);
      changed = true;
      needsRepaint.store(true, std::memory_order_release);
    }
    if (changed && onTouchpadViewChanged)
      onTouchpadViewChanged(activeLayer, -1);
  } else if (followInputEnabled.load(std::memory_order_acquire)) {
    // A. Device Switching
    uintptr_t handle = lastInputDeviceHandle.load(std::memory_order_relaxed);
    if (handle != 0 && deviceManager != nullptr && viewSelector.isVisible()) {
      juce::String aliasName = deviceManager->getAliasForHardware(handle);
      uintptr_t aliasHash =
          (aliasName != "Unassigned" && aliasName.isNotEmpty())
              ? aliasNameToHash(aliasName)
              : 0;

      if (aliasHash != currentViewHash) {
        currentViewHash = aliasHash;

        // Update selector UI to match the new hash (no notifications)
        int idxToSelect = 0;
        for (size_t i = 0; i < viewHashes.size(); ++i) {
          if (viewHashes[i] == currentViewHash) {
            idxToSelect = (int)i;
            break;
          }
        }
        viewSelector.setSelectedItemIndex(idxToSelect,
                                          juce::dontSendNotification);

        cacheValid = false;
        needsRepaint.store(true, std::memory_order_release);
      }
    }

    // B. Layer Switching – follow the highest active layer from InputProcessor.
    // When Touchpad tab is active or "show selected layer" is on, do not
    // override; selection owns the layer.
    if (!touchpadTabActive_ && inputProcessor && !showSelectedLayerEnabled_) {
      int activeLayer = inputProcessor->getHighestActiveLayerIndex();
      if (currentVisualizedLayer != activeLayer) {
        setVisualizedLayer(activeLayer); // invalidates cache + marks dirty
        needsRepaint.store(true, std::memory_order_release);
      }
    }
  }

  // Step 2: Poll external state + rebuild cache / repaint on demand.

  // Check external Sustain state
  bool sus = voiceManager.isSustainActive();
  if (lastSustainState != sus) {
    lastSustainState = sus;
    needsRepaint.store(true, std::memory_order_release);
  }

  // Rebuild Cache if invalid
  if (!cacheValid) {
    refreshCache();
  }

  // Repaint if needed
  if (needsRepaint.exchange(false, std::memory_order_acq_rel)) {
    repaint();
  }
}

void VisualizerComponent::valueTreeChildAdded(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
  if (!presetManager || presetManager->getIsLoading())
    return;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::valueTreeChildRemoved(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved,
    int indexFromWhichChildWasRemoved) {
  if (!presetManager || presetManager->getIsLoading())
    return;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
  if (!presetManager || presetManager->getIsLoading())
    return;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::valueTreeParentChanged(
    juce::ValueTree &treeWhoseParentHasChanged) {
  if (!presetManager || presetManager->getIsLoading())
    return;
  cacheValid = false;
  needsRepaint = true;
}
