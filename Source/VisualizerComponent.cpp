#include "VisualizerComponent.h"
#include "ColourContrast.h"
#include "InputProcessor.h"
#include "KeyboardLayoutUtils.h"
#include "MappingTypes.h"
#include "PitchPadUtilities.h"
#include "TouchpadMixerTypes.h"
#include "PresetManager.h"
#include "RawInputManager.h"
#include "ScaleLibrary.h"
#include "SettingsManager.h"
#include "VoiceManager.h"
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

VisualizerComponent::VisualizerComponent(ZoneManager *zoneMgr,
                                         DeviceManager *deviceMgr,
                                         const VoiceManager &voiceMgr,
                                         SettingsManager *settingsMgr,
                                         PresetManager *presetMgr,
                                         InputProcessor *inputProc,
                                         ScaleLibrary *scaleLib)
    : zoneManager(zoneMgr), deviceManager(deviceMgr), voiceManager(voiceMgr),
      settingsManager(settingsMgr), presetManager(presetMgr),
      inputProcessor(inputProc), scaleLibrary(scaleLib),
      globalPanel(zoneMgr, scaleLib) {
  // Phase 42: Listeners moved to initialize() – no addListener here

  addAndMakeVisible(globalPanel);

  globalPanelResizerBar = std::make_unique<GlobalPanelResizerBar>();
  auto *resizer = static_cast<GlobalPanelResizerBar *>(globalPanelResizerBar.get());
  resizer->setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
  resizer->onWidthChange = [this](float w) { setGlobalPanelWidthFromResizer(w); };
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

  // Phase 50.9.1: Follow Input toggle (always visible; layer system works regardless of Studio Mode)
  addAndMakeVisible(followButton);
  followButton.setClickingTogglesState(true);
  followButton.setTooltip("When on, the visualizer follows the layer currently being triggered by input.");
  followButton.onClick = [this] {
    followInputEnabled.store(followButton.getToggleState(),
                             std::memory_order_release);
    updateFollowButtonAppearance();
  };
  updateFollowButtonAppearance();

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
  // Clamp to non-negative; InputProcessor further clamps to valid range
  if (layerId < 0)
    layerId = 0;
  currentVisualizedLayer = layerId;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::setSelectedTouchpadMixerStrip(int stripIndex,
                                                        int layerId) {
  selectedTouchpadMixerStripIndex_ = stripIndex;
  selectedTouchpadMixerLayerId_ = stripIndex >= 0 ? layerId : 0;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::initialize() {
  if (zoneManager)
    zoneManager->addChangeListener(this);
  if (settingsManager)
    settingsManager->addChangeListener(this);
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

    // --- 0. Header Bar (Transpose left, Sustain right) - only over content area ---
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
        state = slot.state;
        isGhost = slot.isGhost;
        if (!slot.displayColor.isTransparent())
          underlayColor = slot.displayColor;
        if (slot.label.isNotEmpty())
          labelText = slot.label;
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
  // frequently)
  bool sustainActive = voiceManager.isSustainActive();
  auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), 30);
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
  int contentW = getWidth() - static_cast<int>(getEffectiveRightPanelWidth());
  if (contentW < 0)
    contentW = 0;
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

  // Touchpad panel on the LEFT (no overlap with keyboard). 5:4 rectangle,
  // X and Y bands from same buildPitchPadLayout() as runtime (resting space
  // slider drives layout).
  float panelLeft = kTouchpadPanelMargin;
  float panelWidth = kTouchpadPanelLeftWidth - (2.0f * kTouchpadPanelMargin);
  float panelTop = headerHeight + 8.0f;
  float panelBottom = static_cast<float>(getHeight()) - 8.0f;
  if (panelWidth > 40.0f && panelBottom > panelTop) {
    std::vector<TouchpadContact> contactsSnapshot;
    {
      juce::ScopedLock lock(contactsLock);
      contactsSnapshot = lastTouchpadContacts;
    }

    // Find pitch-pad configs and Y CC range per axis (same compiled context as
    // MIDI/runtime). Also capture control labels (e.g. "PitchBend", "CC1").
    std::optional<PitchPadConfig> configX;
    std::optional<PitchPadConfig> configY;
    std::optional<std::pair<float, float>> yCcInputRange; // inputMin, inputMax
    juce::String xControlLabel;
    juce::String yControlLabel;
    if (inputProcessor) {
      std::shared_ptr<const CompiledMapContext> ctx =
          inputProcessor->getContext();
      if (ctx) {
        for (const auto &entry : ctx->touchpadMappings) {
          if (entry.layerId != currentVisualizedLayer)
            continue;
          if (entry.eventId == TouchpadEvent::Finger1X &&
              entry.conversionParams.pitchPadConfig.has_value()) {
            configX = entry.conversionParams.pitchPadConfig;
            auto target = entry.action.adsrSettings.target;
            if (target == AdsrTarget::PitchBend ||
                target == AdsrTarget::SmartScaleBend) {
              xControlLabel = "PitchBend";
            } else if (target == AdsrTarget::CC) {
              xControlLabel =
                  "CC" + juce::String(entry.action.adsrSettings.ccNumber);
            }
          } else if (entry.eventId == TouchpadEvent::Finger1Y) {
            auto target = entry.action.adsrSettings.target;
            if (entry.conversionParams.pitchPadConfig.has_value()) {
              configY = entry.conversionParams.pitchPadConfig;
              if (target == AdsrTarget::PitchBend ||
                  target == AdsrTarget::SmartScaleBend) {
                yControlLabel = "PitchBend";
              } else if (target == AdsrTarget::CC) {
                yControlLabel =
                    "CC" + juce::String(entry.action.adsrSettings.ccNumber);
              }
            } else if (entry.conversionKind ==
                           TouchpadConversionKind::ContinuousToRange &&
                       target == AdsrTarget::CC) {
              yCcInputRange = {entry.conversionParams.inputMin,
                               entry.conversionParams.inputMax};
              yControlLabel =
                  "CC" + juce::String(entry.action.adsrSettings.ccNumber);
            }
          }
        }
      }
    }

    // Relative X: anchor as center when config is Relative and anchor is set
    std::optional<float> anchorNormX;
    if (configX && configX->mode == PitchPadMode::Relative && inputProcessor)
      anchorNormX = inputProcessor->getPitchPadRelativeAnchorNormX(
          lastTouchpadDeviceHandle.load(std::memory_order_acquire),
          currentVisualizedLayer, TouchpadEvent::Finger1X);

    // 3:2 touchpad rectangle
    float rectW = panelWidth;
    float rectH = rectW * (kTouchpadAspectH / kTouchpadAspectW);
    if (rectH > panelBottom - panelTop - 30.0f) {
      rectH = juce::jmax(20.0f, panelBottom - panelTop - 30.0f);
      rectW = rectH * (kTouchpadAspectW / kTouchpadAspectH);
    }
    float rectX = panelLeft + (panelWidth - rectW) * 0.5f;
    float rectY = panelTop;
    juce::Rectangle<float> touchpadRect(rectX, rectY, rectW, rectH);

    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(touchpadRect, 4.0f);
    g.setColour(juce::Colours::darkgrey);
    g.drawRoundedRectangle(touchpadRect, 4.0f, 1.0f);

    // Axis colours: X = blue-ish greys, Y = green/amber greys. Opacity is
    // user-adjustable via Settings -> Visualizer.
    float xOpacity =
        settingsManager ? settingsManager->getVisualizerXOpacity() : 0.45f;
    float yOpacity =
        settingsManager ? settingsManager->getVisualizerYOpacity() : 0.45f;
    xOpacity = juce::jlimit(0.0f, 1.0f, xOpacity);
    yOpacity = juce::jlimit(0.0f, 1.0f, yOpacity);

    const juce::Colour xRestCol =
        juce::Colour(0xff404055).withAlpha(xOpacity); // resting bands
    const juce::Colour xTransCol =
        juce::Colour(0xff353550).withAlpha(xOpacity); // transition bands
    const juce::Colour yRestCol =
        juce::Colour(0xff455040).withAlpha(yOpacity); // resting bands
    const juce::Colour yTransCol =
        juce::Colour(0xff354035).withAlpha(yOpacity); // transition bands
    const juce::Colour yCcInactiveCol =
        juce::Colour(0xff353535).withAlpha(yOpacity); // outside CC range
    const juce::Colour yCcActiveCol =
        juce::Colour(0xff405538)
            .withAlpha(
                juce::jlimit(0.0f, 1.0f, yOpacity + 0.1f)); // active CC range

    // X-axis bands (horizontal): absolute or shifted by relative anchor
    if (configX) {
      PitchPadLayout layoutX = buildPitchPadLayout(*configX);
      float offset = 0.0f;
      if (anchorNormX && configX->mode == PitchPadMode::Relative) {
        float zeroX = 0.5f;
        for (const auto &b : layoutX.bands) {
          if (b.step == static_cast<int>(configX->zeroStep)) {
            zeroX = (b.xStart + b.xEnd) * 0.5f;
            break;
          }
        }
        offset = *anchorNormX - zeroX;
      }
      for (const auto &band : layoutX.bands) {
        float xStart = juce::jlimit(0.0f, 1.0f, band.xStart + offset);
        float xEnd = juce::jlimit(0.0f, 1.0f, band.xEnd + offset);
        if (xEnd <= xStart)
          continue;
        float bx = touchpadRect.getX() + xStart * touchpadRect.getWidth();
        float bw = (xEnd - xStart) * touchpadRect.getWidth();
        if (bw > 0.5f) {
          g.setColour(band.isRest ? xRestCol : xTransCol);
          g.fillRect(bx, touchpadRect.getY(), bw, touchpadRect.getHeight());
        }
      }
    }
    // Y-axis: pitch-pad bands or CC input range (inputMin–inputMax = active)
    if (configY) {
      PitchPadLayout layoutY = buildPitchPadLayout(*configY);
      for (const auto &band : layoutY.bands) {
        float by = touchpadRect.getY() + band.xStart * touchpadRect.getHeight();
        float bh = (band.xEnd - band.xStart) * touchpadRect.getHeight();
        if (bh > 0.5f) {
          g.setColour(band.isRest ? yRestCol : yTransCol);
          g.fillRect(touchpadRect.getX(), by, touchpadRect.getWidth(), bh);
        }
      }
    } else if (yCcInputRange) {
      float imin = juce::jlimit(0.0f, 1.0f, yCcInputRange->first);
      float imax = juce::jlimit(0.0f, 1.0f, yCcInputRange->second);
      float baseY = touchpadRect.getY();
      float h = touchpadRect.getHeight();
      // Below inputMin (top in screen coords: Y increases down)
      if (imin > 0.0f) {
        g.setColour(yCcInactiveCol);
        g.fillRect(touchpadRect.getX(), baseY, touchpadRect.getWidth(),
                   imin * h);
      }
      // Active strip inputMin..inputMax
      if (imax > imin) {
        g.setColour(yCcActiveCol);
        g.fillRect(touchpadRect.getX(), baseY + imin * h,
                   touchpadRect.getWidth(), (imax - imin) * h);
      }
      // Above inputMax
      if (imax < 1.0f) {
        g.setColour(yCcInactiveCol);
        g.fillRect(touchpadRect.getX(), baseY + imax * h,
                   touchpadRect.getWidth(), (1.0f - imax) * h);
      }
    }

    // Axis labels:
    //  - X on the RIGHT: "[control]   X" (e.g. "PitchBend   X")
    //  - Y along the vertical axis, rotated 90°: "[control]   Y" (e.g. "CC1 Y")
    g.setColour(juce::Colours::lightgrey);
    g.setFont(10.0f);
    juce::String xLabel =
        xControlLabel.isNotEmpty() ? (xControlLabel + "   X") : "X";
    juce::String yLabel =
        yControlLabel.isNotEmpty() ? (yControlLabel + "   Y") : "Y";

    // X label: right side of the rectangle
    g.drawText(xLabel, touchpadRect.getX(), touchpadRect.getBottom() - 14.0f,
               touchpadRect.getWidth(), 12, juce::Justification::centredRight,
               false);

    // Y label: rotated 90 degrees, centered along left edge
    {
      juce::Graphics::ScopedSaveState save(g);
      float cx = touchpadRect.getX() + 6.0f;
      float cy = touchpadRect.getCentreY();
      g.addTransform(juce::AffineTransform::rotation(
          -juce::MathConstants<float>::halfPi, cx, cy));
      // After rotation, draw a small horizontal label centered at (cx, cy).
      g.drawText(yLabel, static_cast<int>(cx - 40.0f),
                 static_cast<int>(cy - 6.0f), 80, 12,
                 juce::Justification::centred, false);
    }
    // Touch points (X, Y) in 2D – one per finger, each a different colour
    static const juce::Colour fingerColours[] = {
        juce::Colours::lime,   // finger 1
        juce::Colours::cyan,   // finger 2
        juce::Colours::orange, // finger 3
        juce::Colours::magenta // finger 4+
    };
    const int numColours =
        static_cast<int>(sizeof(fingerColours) / sizeof(fingerColours[0]));
    for (size_t i = 0; i < contactsSnapshot.size(); ++i) {
      const auto &c = contactsSnapshot[i];
      if (!c.tipDown)
        continue;
      float nx = juce::jlimit(0.0f, 1.0f, c.normX);
      float ny = juce::jlimit(0.0f, 1.0f, c.normY);
      float px = touchpadRect.getX() + nx * touchpadRect.getWidth();
      float py = touchpadRect.getY() + ny * touchpadRect.getHeight();
      juce::Colour col = fingerColours[static_cast<size_t>(i) %
                                       static_cast<size_t>(numColours)];
      g.setColour(col);
      g.fillEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f);
      g.setColour(col.contrasting(0.5f));
      g.drawEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f, 1.0f);
    }

    // Touchpad mixer overlay: when we have a selected strip and we're
    // viewing its layer, show the strip (fader columns + values) always.
    if (inputProcessor && selectedTouchpadMixerStripIndex_ >= 0 &&
        currentVisualizedLayer == selectedTouchpadMixerLayerId_) {
      std::shared_ptr<const CompiledMapContext> ctx = inputProcessor->getContext();
      if (ctx) {
        uintptr_t dev =
            lastTouchpadDeviceHandle.load(std::memory_order_acquire);
        size_t stripIdx =
            static_cast<size_t>(selectedTouchpadMixerStripIndex_);
        if (stripIdx < ctx->touchpadMixerStrips.size()) {
          const auto &strip = ctx->touchpadMixerStrips[stripIdx];
          if (strip.layerId == currentVisualizedLayer && strip.numFaders > 0) {
            std::vector<int> displayValues =
                inputProcessor->getTouchpadMixerStripDisplayValues(
                    dev, static_cast<int>(stripIdx), strip.numFaders);
            std::vector<bool> muted =
                inputProcessor->getTouchpadMixerStripMuteState(
                    dev, static_cast<int>(stripIdx), strip.numFaders);
            const int N = strip.numFaders;
            const float fw = touchpadRect.getWidth() / static_cast<float>(N);
            const float h = touchpadRect.getHeight();
            const float muteRegionH = strip.muteButtonsEnabled ? (h * 0.15f) : 0.0f;
            const float faderH = h - muteRegionH;
            for (int i = 0; i < N; ++i) {
              int displayVal = (i < static_cast<int>(displayValues.size()))
                                  ? displayValues[(size_t)i]
                                  : 0;
              bool isMuted = (i < static_cast<int>(muted.size())) && muted[(size_t)i];
              float fill = static_cast<float>(juce::jlimit(0, 127, displayVal)) / 127.0f;
              float stripX = touchpadRect.getX() + static_cast<float>(i) * fw;
              float fillH = fill * faderH;
              float faderBottom = touchpadRect.getY() + faderH;
              g.setColour(isMuted ? juce::Colour(0xff505070).withAlpha(0.85f)
                                 : juce::Colour(0xff406080).withAlpha(0.6f));
              g.fillRect(stripX + 1.0f,
                        faderBottom - fillH,
                        fw - 2.0f,
                        fillH);
              g.setColour(isMuted ? juce::Colour(0xff8080a0).withAlpha(0.6f)
                                 : juce::Colours::lightgrey.withAlpha(0.5f));
              g.drawRect(stripX, touchpadRect.getY(), fw, faderH, 0.5f);
              // Muted: "M" at top of strip for visibility
              if (isMuted) {
                g.setColour(juce::Colours::white);
                g.setFont(juce::jmin(10.0f, fw * 0.6f));
                g.drawText("M", stripX, touchpadRect.getY(), fw, 14.0f,
                           juce::Justification::centred, false);
                g.setFont(juce::jmin(9.0f, fw * 0.5f));
                g.drawText(juce::String(displayVal), stripX,
                           touchpadRect.getY() + 14.0f, fw, 12.0f,
                           juce::Justification::centred, false);
              } else {
                g.setColour(juce::Colours::white);
                g.setFont(juce::jmin(10.0f, fw * 0.6f));
                g.drawText(juce::String(displayVal), stripX, touchpadRect.getY(),
                           fw, 14.0f, juce::Justification::centred, false);
              }
              // CC number at bottom of fader area (above mute row)
              g.setColour(juce::Colours::white);
              g.setFont(juce::jmin(9.0f, fw * 0.5f));
              g.drawText(juce::String(strip.ccStart + i),
                         stripX, faderBottom - 14.0f, fw, 12.0f,
                         juce::Justification::centred, false);
            }
            if (strip.muteButtonsEnabled && muteRegionH > 0) {
              float muteTop = touchpadRect.getY() + faderH;
              g.setColour(juce::Colour(0xff303050).withAlpha(0.8f));
              g.fillRect(touchpadRect.getX(), muteTop, touchpadRect.getWidth(),
                         muteRegionH);
              g.setColour(juce::Colours::lightgrey.withAlpha(0.6f));
              g.setFont(8.0f);
              for (int i = 0; i < N; ++i) {
                float mx = touchpadRect.getX() + static_cast<float>(i) * fw;
                g.drawText("M", mx, muteTop, fw, muteRegionH,
                           juce::Justification::centred, false);
              }
            }
            g.setColour(juce::Colours::white);
            g.setFont(9.0f);
            g.drawText("Mixer: CC" + juce::String(strip.ccStart) + "-" +
                       juce::String(strip.ccStart + N - 1),
                       touchpadRect.getX(), touchpadRect.getBottom() + 2.0f,
                       touchpadRect.getWidth(), 10,
                       juce::Justification::centredLeft, false);
          }
        }
      }
    }

    float y = touchpadRect.getBottom() + 6.0f;
    float lineHeight = 16.0f;
    g.setColour(juce::Colours::white);
    g.setFont(10.0f);
    juce::String title =
        contactsSnapshot.empty() ? "Touchpad: (no contacts)" : "Touchpad:";
    g.drawText(title, panelLeft, y, panelWidth, lineHeight,
               juce::Justification::centredLeft, false);
    y += lineHeight;
    for (size_t i = 0;
         i < contactsSnapshot.size() && y + lineHeight <= panelBottom; ++i) {
      const auto &c = contactsSnapshot[i];
      juce::String line = "Pt" + juce::String(static_cast<int>(i) + 1) + ": ";
      if (c.tipDown)
        line +=
            "X=" + juce::String(c.normX, 2) + " Y=" + juce::String(c.normY, 2);
      else
        line += "X=- Y=-";
      g.drawText(line, panelLeft, y, panelWidth, lineHeight,
                 juce::Justification::centredLeft, false);
      y += lineHeight;
    }
  }

  // Draw overlay if MIDI mode is disabled
  if (midiModeDisabled) {
    // Semi-transparent black overlay
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillAll();

    // Draw text message
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    juce::String toggleKeyName =
        settingsManager
            ? RawInputManager::getKeyName(settingsManager->getToggleKey())
            : "F12";
    juce::String message =
        "MIDI MODE DISABLED\n(Press " + toggleKeyName + " to enable)";
    g.drawText(message, getLocalBounds(), juce::Justification::centred, false);
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
    globalPanelWidth_ = kGlobalPanelDefaultWidth; // restore default when re-expanded
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

  // Position Follow Input button (always visible) and View Selector (when Studio Mode)
  int headerHeight = 30;   // Status bar height
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
    viewSelector.setBounds(selectorX, selectorY, selectorWidth, selectorHeight);
  } else {
    // Follow Input only: place button left of right panel
    int buttonX = w - effRight - buttonWidth - margin;
    followButton.setBounds(buttonX, selectorY, buttonWidth, buttonHeight);
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
  juce::ScopedLock lock(contactsLock);
  lastTouchpadContacts = contacts;
  needsRepaint.store(true, std::memory_order_release);
}

void VisualizerComponent::changeListenerCallback(
    juce::ChangeBroadcaster *source) {
  if (source == inputProcessor) {
    // Phase 48: layer state changed -> update HUD next frame
    needsRepaint = true;
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
  if (followInputEnabled.load(std::memory_order_acquire)) {
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
    if (inputProcessor) {
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
