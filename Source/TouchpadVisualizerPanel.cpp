#include "TouchpadVisualizerPanel.h"
#include "MappingTypes.h"
#include "MidiNoteUtilities.h"
#include "PitchPadUtilities.h"
#include "TouchpadMixerTypes.h"
#include <algorithm>
#include <optional>
#include <set>

namespace {

enum class TouchpadMappingVisualKind {
  Note,
  ExpressionCC,
  Pitch,
  Slide,
  Encoder,
  Command,
  Macro,
  Other
};

enum class TouchpadVisualAxis { None, Horizontal, Vertical, Both };

struct TouchpadMappingVisual {
  const TouchpadMappingEntry *entry = nullptr;
  juce::Rectangle<float> regionRect;
  TouchpadMappingVisualKind kind = TouchpadMappingVisualKind::Other;
  TouchpadVisualAxis axis = TouchpadVisualAxis::None;
  bool isPositionDependent = false;
  bool hasRememberedValue = false;
  bool isLatched = false;
  bool isRegionLocked = false;
  std::optional<float> currentValue01;
};

static bool isPitchTarget(const MidiAction &act) {
  return act.adsrSettings.target == AdsrTarget::PitchBend ||
         act.adsrSettings.target == AdsrTarget::SmartScaleBend;
}

static TouchpadMappingVisualKind classifyVisualKind(
    const TouchpadMappingEntry &entry) {
  if (entry.conversionKind == TouchpadConversionKind::SlideToCC)
    return TouchpadMappingVisualKind::Slide;
  if (entry.conversionKind == TouchpadConversionKind::EncoderCC)
    return TouchpadMappingVisualKind::Encoder;

  switch (entry.action.type) {
  case ActionType::Note:
    return TouchpadMappingVisualKind::Note;
  case ActionType::Expression:
    return isPitchTarget(entry.action) ? TouchpadMappingVisualKind::Pitch
                                       : TouchpadMappingVisualKind::ExpressionCC;
  case ActionType::Command:
    return TouchpadMappingVisualKind::Command;
  case ActionType::Macro:
    return TouchpadMappingVisualKind::Macro;
  default:
    break;
  }
  return TouchpadMappingVisualKind::Other;
}

static TouchpadVisualAxis getVisualAxis(const TouchpadMappingEntry &entry) {
  if (entry.conversionKind == TouchpadConversionKind::SlideToCC) {
    switch (entry.conversionParams.slideAxis) {
    case 0:
      return TouchpadVisualAxis::Vertical;
    case 1:
      return TouchpadVisualAxis::Horizontal;
    case 2:
      return TouchpadVisualAxis::Both;
    default:
      break;
    }
  } else if (entry.conversionKind == TouchpadConversionKind::EncoderCC) {
    switch (entry.conversionParams.encoderAxis) {
    case 0:
      return TouchpadVisualAxis::Vertical;
    case 1:
      return TouchpadVisualAxis::Horizontal;
    case 2:
      return TouchpadVisualAxis::Both;
    default:
      break;
    }
  }

  switch (entry.eventId) {
  case TouchpadEvent::Finger1X:
  case TouchpadEvent::Finger2X:
  case TouchpadEvent::Finger1And2AvgX:
    return TouchpadVisualAxis::Horizontal;
  case TouchpadEvent::Finger1Y:
  case TouchpadEvent::Finger2Y:
  case TouchpadEvent::Finger1And2AvgY:
    return TouchpadVisualAxis::Vertical;
  case TouchpadEvent::Finger1And2Dist:
    return TouchpadVisualAxis::Both;
  default:
    break;
  }
  return TouchpadVisualAxis::None;
}

static bool isPositionDependentMapping(const TouchpadMappingEntry &entry) {
  switch (entry.eventId) {
  case TouchpadEvent::Finger1X:
  case TouchpadEvent::Finger1Y:
  case TouchpadEvent::Finger2X:
  case TouchpadEvent::Finger2Y:
  case TouchpadEvent::Finger1And2Dist:
  case TouchpadEvent::Finger1And2AvgX:
  case TouchpadEvent::Finger1And2AvgY:
    return true;
  default:
    break;
  }
  return entry.conversionKind == TouchpadConversionKind::SlideToCC ||
         entry.conversionKind == TouchpadConversionKind::EncoderCC;
}

static bool isLatchedMapping(const TouchpadMappingEntry &entry) {
  if (entry.action.type == ActionType::Note &&
      entry.action.releaseBehavior == NoteReleaseBehavior::AlwaysLatch)
    return true;

  if (entry.conversionKind == TouchpadConversionKind::BoolToCC &&
      entry.conversionParams.ccReleaseBehavior ==
          CcReleaseBehavior::AlwaysLatch)
    return true;

  return false;
}

static bool hasRememberedValueMapping(const TouchpadMappingEntry &entry) {
  if (entry.conversionKind == TouchpadConversionKind::SlideToCC ||
      entry.conversionKind == TouchpadConversionKind::EncoderCC)
    return true;

  if (entry.conversionKind == TouchpadConversionKind::ContinuousToRange &&
      entry.action.type == ActionType::Expression)
    return true;

  if (entry.conversionKind == TouchpadConversionKind::BoolToCC &&
      entry.conversionParams.ccReleaseBehavior ==
          CcReleaseBehavior::AlwaysLatch)
    return true;

  return false;
}

static juce::String touchpadEventToLabel(int eventId) {
  switch (eventId) {
  case TouchpadEvent::Finger1Down:
    return "F1Down";
  case TouchpadEvent::Finger1Up:
    return "F1Up";
  case TouchpadEvent::Finger1X:
    return "F1X";
  case TouchpadEvent::Finger1Y:
    return "F1Y";
  case TouchpadEvent::Finger2Down:
    return "F2Down";
  case TouchpadEvent::Finger2Up:
    return "F2Up";
  case TouchpadEvent::Finger2X:
    return "F2X";
  case TouchpadEvent::Finger2Y:
    return "F2Y";
  case TouchpadEvent::Finger1And2Dist:
    return "Dist";
  case TouchpadEvent::Finger1And2AvgX:
    return "AvgX";
  case TouchpadEvent::Finger1And2AvgY:
    return "AvgY";
  default:
    break;
  }
  return {};
}

} // namespace

TouchpadVisualizerPanel::TouchpadVisualizerPanel(InputProcessor *inputProc,
                                                 SettingsManager *settingsMgr)
    : inputProcessor(inputProc), settingsManager(settingsMgr) {}

TouchpadVisualizerPanel::~TouchpadVisualizerPanel() { stopTimer(); }

void TouchpadVisualizerPanel::setContacts(
    const std::vector<TouchpadContact> &contacts, uintptr_t deviceHandle) {
  juce::ScopedLock lock(contactsLock_);
  int64_t now = juce::Time::getMillisecondCounter();
  
  // Update timestamps for all contacts in the new list
  std::set<int> currentContactIds;
  for (const auto &contact : contacts) {
    contactLastUpdateTime_[contact.contactId] = now;
    currentContactIds.insert(contact.contactId);
  }
  
  // Remove entries for contacts that are no longer present
  // (clean up old contactIds that are no longer active)
  for (auto it = contactLastUpdateTime_.begin(); it != contactLastUpdateTime_.end();) {
    if (currentContactIds.find(it->first) == currentContactIds.end()) {
      it = contactLastUpdateTime_.erase(it);
    } else {
      ++it;
    }
  }
  
  contacts_ = contacts;
  lastDeviceHandle_.store(deviceHandle, std::memory_order_release);

  // Track last time we had at least one finger down (for timer efficiency)
  bool hasTipDown = false;
  for (const auto &c : contacts)
    if (c.tipDown) { hasTipDown = true; break; }
  if (hasTipDown)
    lastTimeHadContactsMs_ = now;

  repaint();

  // Only run timer when there's something to update: contacts active or in timeout window
  if (isVisible()) {
    if (hasTipDown || (now - lastTimeHadContactsMs_ <= kContactTimeoutMs)) {
      int interval = settingsManager ? settingsManager->getWindowRefreshIntervalMs()
                                     : kDefaultRefreshIntervalMs;
      if (!isTimerRunning())
        startTimer(interval);
    }
  }
}

void TouchpadVisualizerPanel::setVisualizedLayer(int layerId) {
  if (layerId >= 0)
    currentVisualizedLayer = layerId;
  lastPaintedContactCount_ = -1;
  lastPaintedStateHash_ = 0;
}

void TouchpadVisualizerPanel::setSelectedLayout(int layoutIndex, int layerId) {
  selectedLayoutIndex_ = layoutIndex;
  selectedLayoutLayerId_ = layoutIndex >= 0 ? layerId : 0;
}

void TouchpadVisualizerPanel::setShowContactCoordinates(bool show) {
  showContactCoordinates_ = show;
}

void TouchpadVisualizerPanel::restartTimerWithInterval(int intervalMs) {
  stopTimer();
  if (!isVisible()) return;
  int64_t now = juce::Time::getMillisecondCounter();
  juce::ScopedLock lock(contactsLock_);
  bool hasTipDown = false;
  for (const auto &c : contacts_)
    if (c.tipDown) { hasTipDown = true; break; }
  if (hasTipDown || (now - lastTimeHadContactsMs_ <= kContactTimeoutMs))
    startTimer(intervalMs);
}

void TouchpadVisualizerPanel::visibilityChanged() {
  if (isVisible()) {
    int64_t now = juce::Time::getMillisecondCounter();
    juce::ScopedLock lock(contactsLock_);
    bool hasTipDown = false;
    for (const auto &c : contacts_)
      if (c.tipDown) { hasTipDown = true; break; }
    bool inTimeoutWindow = (now - lastTimeHadContactsMs_ <= kContactTimeoutMs);
    if (hasTipDown || inTimeoutWindow) {
      int interval = settingsManager
                         ? settingsManager->getWindowRefreshIntervalMs()
                         : kDefaultRefreshIntervalMs;
      startTimer(interval);
    }
  } else {
    stopTimer();
  }
}

void TouchpadVisualizerPanel::timerCallback() {
  int64_t now = juce::Time::getMillisecondCounter();
  std::vector<TouchpadContact> contactsSnapshot;
  std::unordered_map<int, int64_t> timestampSnapshot;
  {
    juce::ScopedLock lock(contactsLock_);
    contactsSnapshot = contacts_;
    timestampSnapshot = contactLastUpdateTime_;
  }

  // If no contacts and past timeout window, stop timer and do one final repaint to clear
  bool anyTipDown = false;
  for (const auto &c : contactsSnapshot)
    if (c.tipDown) { anyTipDown = true; break; }
  if (!anyTipDown && (now - lastTimeHadContactsMs_ > kContactTimeoutMs)) {
    stopTimer();
    repaint();
    return;
  }

  // Build effective (filtered) contacts for change detection
  std::vector<TouchpadContact> filtered;
  filtered.reserve(contactsSnapshot.size());
  for (const auto &contact : contactsSnapshot) {
    if (!contact.tipDown) continue;
    auto it = timestampSnapshot.find(contact.contactId);
    if (it == timestampSnapshot.end()) continue;
    if (now - it->second > kContactTimeoutMs) continue;
    filtered.push_back(contact);
  }

  // Skip repaint if nothing changed
  int count = static_cast<int>(filtered.size());
  uint32_t hash = 0;
  for (const auto &c : filtered)
    hash = (hash * 31u) + static_cast<uint32_t>(c.contactId)
         + static_cast<uint32_t>(c.normX * 1000.f)
         + static_cast<uint32_t>(c.normY * 1000.f) * 7u;
  if (count == lastPaintedContactCount_ && hash == lastPaintedStateHash_)
    return;
  lastPaintedContactCount_ = count;
  lastPaintedStateHash_ = hash;

  repaint();
}

void TouchpadVisualizerPanel::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  float panelLeft = 8.0f;
  float panelWidth = bounds.getWidth() - 16.0f;
  float panelTop = 8.0f;
  float panelBottom = bounds.getHeight() - 8.0f;

  if (panelWidth <= 40.0f || panelBottom <= panelTop)
    return;

  std::vector<TouchpadContact> contactsSnapshot;
  std::unordered_map<int, int64_t> timestampSnapshot;
  int64_t now = juce::Time::getMillisecondCounter();
  
  {
    juce::ScopedLock lock(contactsLock_);
    contactsSnapshot = contacts_;
    timestampSnapshot = contactLastUpdateTime_;
  }
  
  // Filter out stale contacts: remove tipDown==false and contacts older than timeout
  contactsSnapshot.erase(
      std::remove_if(contactsSnapshot.begin(), contactsSnapshot.end(),
                     [&timestampSnapshot, now](const TouchpadContact &contact) {
                       // Remove if tipDown is false (finger lifted)
                       if (!contact.tipDown)
                         return true;
                       
                       // Remove if contact hasn't been updated in the last timeout period
                       auto it = timestampSnapshot.find(contact.contactId);
                       if (it != timestampSnapshot.end()) {
                         int64_t age = now - it->second;
                         if (age > TouchpadVisualizerPanel::kContactTimeoutMs) {
                           return true; // Stale contact, remove it
                         }
                       } else {
                         // Contact not in timestamp map, treat as stale
                         return true;
                       }
                       
                       return false; // Keep this contact
                     }),
      contactsSnapshot.end());

  int soloGroupForLayer = 0;
  if (inputProcessor) {
    soloGroupForLayer =
        inputProcessor->getEffectiveSoloLayoutGroupForLayer(
            currentVisualizedLayer);
  }

  std::optional<PitchPadConfig> configX;
  std::optional<PitchPadConfig> configY;
  std::optional<std::pair<float, float>> yCcInputRange;
  juce::String xControlLabel;
  juce::String yControlLabel;
  if (inputProcessor) {
    auto ctx = inputProcessor->getContext();
    if (ctx) {
      for (const auto &entry : ctx->touchpadMappings) {
        if (entry.layerId != currentVisualizedLayer)
          continue;
        if ((soloGroupForLayer == 0 && entry.layoutGroupId != 0) ||
            (soloGroupForLayer > 0 &&
             entry.layoutGroupId != soloGroupForLayer))
          continue;
        if (entry.eventId == TouchpadEvent::Finger1X &&
            entry.conversionParams.pitchPadConfig.has_value()) {
          configX = entry.conversionParams.pitchPadConfig;
          auto target = entry.action.adsrSettings.target;
          if (target == AdsrTarget::PitchBend ||
              target == AdsrTarget::SmartScaleBend)
            xControlLabel = "PitchBend";
          else if (target == AdsrTarget::CC)
            xControlLabel =
                "CC" + juce::String(entry.action.adsrSettings.ccNumber);
        } else if (entry.eventId == TouchpadEvent::Finger1Y) {
          auto target = entry.action.adsrSettings.target;
          if (entry.conversionParams.pitchPadConfig.has_value()) {
            configY = entry.conversionParams.pitchPadConfig;
            if (target == AdsrTarget::PitchBend ||
                target == AdsrTarget::SmartScaleBend)
              yControlLabel = "PitchBend";
            else if (target == AdsrTarget::CC)
              yControlLabel =
                "CC" + juce::String(entry.action.adsrSettings.ccNumber);
          } else if (entry.conversionKind ==
                         TouchpadConversionKind::ContinuousToRange &&
                     target == AdsrTarget::CC) {
            yCcInputRange = {entry.conversionParams.inputMin,
                             entry.conversionParams.inputMax};
            yControlLabel =
                "CC" + juce::String(entry.action.adsrSettings.ccNumber);
          } else if (entry.conversionKind ==
                     TouchpadConversionKind::EncoderCC) {
            yControlLabel =
                "Encoder CC" + juce::String(entry.action.adsrSettings.ccNumber);
          }
        }
      }
      // Fallback: if no pitch-pad config found for current layer (e.g. Touchpad
      // tab mapping selected but layer mismatch), use first PitchBend/SmartScaleBend
      // entry in context so bands always show when such a mapping exists.
      if (!configX) {
        for (const auto &entry : ctx->touchpadMappings) {
          if ((soloGroupForLayer == 0 && entry.layoutGroupId != 0) ||
              (soloGroupForLayer > 0 &&
               entry.layoutGroupId != soloGroupForLayer))
            continue;
          if (entry.eventId == TouchpadEvent::Finger1X &&
              entry.conversionParams.pitchPadConfig.has_value()) {
            auto target = entry.action.adsrSettings.target;
            if (target == AdsrTarget::PitchBend ||
                target == AdsrTarget::SmartScaleBend) {
              configX = entry.conversionParams.pitchPadConfig;
              xControlLabel = "PitchBend";
              break;
            }
          }
        }
      }
      if (!configY) {
        for (const auto &entry : ctx->touchpadMappings) {
          if ((soloGroupForLayer == 0 && entry.layoutGroupId != 0) ||
              (soloGroupForLayer > 0 &&
               entry.layoutGroupId != soloGroupForLayer))
            continue;
          if (entry.eventId == TouchpadEvent::Finger1Y &&
              entry.conversionParams.pitchPadConfig.has_value()) {
            auto target = entry.action.adsrSettings.target;
            if (target == AdsrTarget::PitchBend ||
                target == AdsrTarget::SmartScaleBend) {
              configY = entry.conversionParams.pitchPadConfig;
              yControlLabel = "PitchBend";
              break;
            }
          }
        }
      }
    }
  }

  std::optional<float> anchorNormX;
  if (configX && configX->mode == PitchPadMode::Relative && inputProcessor) {
    anchorNormX = inputProcessor->getPitchPadRelativeAnchorNormX(
        lastDeviceHandle_.load(std::memory_order_acquire), currentVisualizedLayer,
        TouchpadEvent::Finger1X);
  }

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

  float xOpacity =
      settingsManager ? settingsManager->getVisualizerXOpacity() : 0.45f;
  float yOpacity =
      settingsManager ? settingsManager->getVisualizerYOpacity() : 0.45f;
  xOpacity = juce::jlimit(0.0f, 1.0f, xOpacity);
  yOpacity = juce::jlimit(0.0f, 1.0f, yOpacity);

  const juce::Colour xRestCol =
      juce::Colour(0xff404055).withAlpha(xOpacity);
  const juce::Colour xTransCol =
      juce::Colour(0xff353550).withAlpha(xOpacity);
  const juce::Colour yRestCol =
      juce::Colour(0xff455040).withAlpha(yOpacity);
  const juce::Colour yTransCol =
      juce::Colour(0xff354035).withAlpha(yOpacity);
  const juce::Colour yCcInactiveCol =
      juce::Colour(0xff353535).withAlpha(yOpacity);
  const juce::Colour yCcActiveCol =
      juce::Colour(0xff405538)
          .withAlpha(juce::jlimit(0.0f, 1.0f, yOpacity + 0.1f));

  // Build per-mapping visuals for current layer/group based on compiled
  // regions. These are derived from CompiledContext each paint so the
  // visualizer stays in sync with runtime behaviour.
  std::vector<TouchpadMappingVisual> mappingVisuals;
  if (inputProcessor) {
    auto ctx = inputProcessor->getContext();
    if (ctx) {
      mappingVisuals.reserve(ctx->touchpadMappings.size());
      uintptr_t dev =
          lastDeviceHandle_.load(std::memory_order_acquire);
      for (const auto &entry : ctx->touchpadMappings) {
        if (entry.layerId != currentVisualizedLayer)
          continue;
        if ((soloGroupForLayer == 0 && entry.layoutGroupId != 0) ||
            (soloGroupForLayer > 0 &&
             entry.layoutGroupId != soloGroupForLayer))
          continue;
        TouchpadMappingVisual vis;
        vis.entry = &entry;
        vis.kind = classifyVisualKind(entry);
        vis.axis = getVisualAxis(entry);
        vis.isRegionLocked = entry.regionLock;
        vis.isPositionDependent = isPositionDependentMapping(entry);
        vis.isLatched = isLatchedMapping(entry);
        vis.hasRememberedValue = hasRememberedValueMapping(entry);
        if (vis.hasRememberedValue) {
          if (auto v = inputProcessor->getTouchpadMappingValue01(dev, entry))
            vis.currentValue01 = *v;
        }
        vis.regionRect = juce::Rectangle<float>(
            touchpadRect.getX() +
                entry.regionLeft * touchpadRect.getWidth(),
            touchpadRect.getY() +
                entry.regionTop * touchpadRect.getHeight(),
            (entry.regionRight - entry.regionLeft) * touchpadRect.getWidth(),
            (entry.regionBottom - entry.regionTop) * touchpadRect.getHeight());
        if (vis.regionRect.getWidth() <= 0.5f ||
            vis.regionRect.getHeight() <= 0.5f)
          continue;
        mappingVisuals.push_back(std::move(vis));
      }

      std::sort(mappingVisuals.begin(), mappingVisuals.end(),
                [](const TouchpadMappingVisual &a,
                   const TouchpadMappingVisual &b) {
                  int za = a.entry ? a.entry->zIndex : 0;
                  int zb = b.entry ? b.entry->zIndex : 0;
                  return za < zb; // lower first, higher drawn on top
                });
    }
  }

  if (yCcInputRange) {
    float imin = juce::jlimit(0.0f, 1.0f, yCcInputRange->first);
    float imax = juce::jlimit(0.0f, 1.0f, yCcInputRange->second);
    float baseY = touchpadRect.getY();
    float h = touchpadRect.getHeight();
    if (imin > 0.0f) {
      g.setColour(yCcInactiveCol);
      g.fillRect(touchpadRect.getX(), baseY, touchpadRect.getWidth(), imin * h);
    }
    if (imax > imin) {
      g.setColour(yCcActiveCol);
      g.fillRect(touchpadRect.getX(), baseY + imin * h, touchpadRect.getWidth(),
                 (imax - imin) * h);
    }
    if (imax < 1.0f) {
      g.setColour(yCcInactiveCol);
      g.fillRect(touchpadRect.getX(), baseY + imax * h, touchpadRect.getWidth(),
                 (1.0f - imax) * h);
    }
  }

  // Axis labels are helpful for pitch-pad and CC-position views, but when we
  // have explicit per-mapping overlays they can visually clash. Hide them
  // whenever mapping visuals are present to keep the UI clean.
  if (mappingVisuals.empty()) {
    g.setColour(juce::Colours::lightgrey.withAlpha(0.85f));
    g.setFont(10.0f);
    juce::String xLabel =
        xControlLabel.isNotEmpty() ? (xControlLabel + "   X") : "X";
    juce::String yLabel =
        yControlLabel.isNotEmpty() ? (yControlLabel + "   Y") : "Y";
    g.drawText(xLabel, touchpadRect.getX(), touchpadRect.getBottom() - 14.0f,
               touchpadRect.getWidth(), 12, juce::Justification::centredRight,
               false);
    {
      juce::Graphics::ScopedSaveState save(g);
      float cx = touchpadRect.getX() + 6.0f;
      float cy = touchpadRect.getCentreY();
      g.addTransform(
          juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                          cx, cy));
      g.drawText(yLabel, static_cast<int>(cx - 40.0f),
                 static_cast<int>(cy - 6.0f), 80, 12,
                 juce::Justification::centred, false);
    }
  }

  static const juce::Colour fingerColours[] = {
      juce::Colours::lime, juce::Colours::cyan, juce::Colours::orange,
      juce::Colours::magenta};
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
    juce::Colour col =
        fingerColours[static_cast<size_t>(i) %
                      static_cast<size_t>(numColours)];
    g.setColour(col);
    g.fillEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f);
    g.setColour(col.contrasting(0.5f));
    g.drawEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f, 1.0f);
  }

  // Region lock: draw ghost at effective position when finger is outside region
  if (inputProcessor) {
    auto ghosts =
        inputProcessor->getEffectiveContactPositions(
            lastDeviceHandle_.load(std::memory_order_acquire), contactsSnapshot);
    juce::Colour ghostCol = juce::Colours::white.withAlpha(0.5f);
    for (const auto &gh : ghosts) {
      float gx = juce::jlimit(0.0f, 1.0f, gh.normX);
      float gy = juce::jlimit(0.0f, 1.0f, gh.normY);
      float gpx = touchpadRect.getX() + gx * touchpadRect.getWidth();
      float gpy = touchpadRect.getY() + gy * touchpadRect.getHeight();
      g.setColour(ghostCol);
      g.fillEllipse(gpx - 5.0f, gpy - 5.0f, 10.0f, 10.0f);
      g.setColour(juce::Colours::white.withAlpha(0.6f));
      g.drawEllipse(gpx - 5.0f, gpy - 5.0f, 10.0f, 10.0f, 1.0f);
    }
  }

  // Draw all layouts for the current layer (ordered by z-index from touchpadLayoutOrder).
  if (inputProcessor) {
    auto ctx = inputProcessor->getContext();
    if (ctx) {
      for (size_t listIdx = 0; listIdx < ctx->touchpadLayoutOrder.size();
           ++listIdx) {
        const auto &ref = ctx->touchpadLayoutOrder[listIdx];
        int soloGroupId =
            inputProcessor->getEffectiveSoloLayoutGroupForLayer(currentVisualizedLayer);
        if (ref.type == TouchpadType::Mixer &&
            ref.index < ctx->touchpadMixerStrips.size()) {
          const auto &strip = ctx->touchpadMixerStrips[ref.index];
          if (strip.layerId == currentVisualizedLayer &&
              strip.numFaders > 0 &&
              ((soloGroupId == 0 && strip.layoutGroupId == 0) || (soloGroupId > 0 && strip.layoutGroupId == soloGroupId))) {
            juce::Rectangle<float> layoutRect(
                touchpadRect.getX() +
                    strip.regionLeft * touchpadRect.getWidth(),
                touchpadRect.getY() +
                    strip.regionTop * touchpadRect.getHeight(),
                (strip.regionRight - strip.regionLeft) * touchpadRect.getWidth(),
                (strip.regionBottom - strip.regionTop) *
                    touchpadRect.getHeight());
            uintptr_t dev =
                lastDeviceHandle_.load(std::memory_order_acquire);
            auto state = inputProcessor->getTouchpadMixerStripState(
                dev, static_cast<int>(ref.index), strip.numFaders);
            const auto &displayValues = state.displayValues;
            const auto &muted = state.muted;
            const int N = strip.numFaders;
            const float fw = layoutRect.getWidth() / static_cast<float>(N);
            const float h = layoutRect.getHeight();
            // Use same constant as processor so fader fill aligns with finger when mute on
            const float muteRegionH =
                (strip.modeFlags & kMixerModeMuteButtons)
                    ? (h * (1.0f - kMuteButtonRegionTop))
                    : 0.0f;
            const float faderH = h - muteRegionH;
            const float faderTop = layoutRect.getY();
            const float faderBottom = faderTop + faderH;
            const float inMin =
                juce::jlimit(0.0f, 1.0f, strip.inputMin);
            const float inMax =
                juce::jlimit(0.0f, 1.0f, strip.inputMax);
            const bool hasDeadZones = (inMin > 0.0f || inMax < 1.0f);

            for (int i = 0; i < N; ++i) {
              float stripX =
                  layoutRect.getX() + static_cast<float>(i) * fw;
              if (hasDeadZones) {
                if (inMin > 0.0f) {
                  float topDeadH = inMin * faderH;
                  g.setColour(juce::Colour(0xff383838).withAlpha(0.75f));
                  g.fillRect(stripX + 1.0f, faderTop, fw - 2.0f, topDeadH);
                }
                if (inMax < 1.0f) {
                  float bottomDeadH = (1.0f - inMax) * faderH;
                  g.setColour(juce::Colour(0xff383838).withAlpha(0.75f));
                  g.fillRect(stripX + 1.0f, faderBottom - bottomDeadH, fw - 2.0f,
                             bottomDeadH);
                }
                if (inMin > 0.0f && inMin < 1.0f) {
                  float yLine = faderTop + inMin * faderH;
                  g.setColour(juce::Colours::orange.withAlpha(0.7f));
                  g.drawHorizontalLine(static_cast<int>(yLine), stripX,
                                       stripX + fw);
                }
                if (inMax > 0.0f && inMax < 1.0f) {
                  float yLine = faderTop + inMax * faderH;
                  g.setColour(juce::Colours::orange.withAlpha(0.7f));
                  g.drawHorizontalLine(static_cast<int>(yLine), stripX,
                                       stripX + fw);
                }
              }
              int displayVal =
                  (i < static_cast<int>(displayValues.size()))
                      ? displayValues[(size_t)i]
                      : 0;
              bool isMuted =
                  (i < static_cast<int>(muted.size())) && muted[(size_t)i];
              float fill =
                  static_cast<float>(juce::jlimit(0, 127, displayVal)) /
                  127.0f;
              // Use faderH only (never full h) so fill aligns with finger when mute on
              float fillH = fill * faderH;
              g.setColour(isMuted
                              ? juce::Colour(0xff505070).withAlpha(0.85f)
                              : juce::Colour(0xff406080).withAlpha(0.6f));
              g.fillRect(stripX + 1.0f, faderBottom - fillH, fw - 2.0f, fillH);
              g.setColour(isMuted
                              ? juce::Colour(0xff8080a0).withAlpha(0.6f)
                              : juce::Colours::lightgrey.withAlpha(0.5f));
              g.drawRect(stripX, faderTop, fw, faderH, 0.5f);
              if (isMuted) {
                g.setColour(juce::Colours::white);
                g.setFont(juce::jmin(10.0f, fw * 0.6f));
                g.drawText("M", stripX, layoutRect.getY(), fw, 14.0f,
                           juce::Justification::centred, false);
                g.setFont(juce::jmin(9.0f, fw * 0.5f));
                g.drawText(juce::String(displayVal), stripX,
                           layoutRect.getY() + 14.0f, fw, 12.0f,
                           juce::Justification::centred, false);
              } else {
                g.setColour(juce::Colours::white);
                g.setFont(juce::jmin(10.0f, fw * 0.6f));
                g.drawText(juce::String(displayVal), stripX, layoutRect.getY(),
                           fw, 14.0f, juce::Justification::centred, false);
              }
              g.setColour(juce::Colours::white);
              g.setFont(juce::jmin(9.0f, fw * 0.5f));
              g.drawText(juce::String(strip.ccStart + i), stripX,
                         faderBottom - 14.0f, fw, 12.0f,
                         juce::Justification::centred, false);
            }
            if (strip.muteButtonsEnabled && muteRegionH > 0) {
              float muteTop = layoutRect.getY() + faderH;
              g.setColour(juce::Colour(0xff303050).withAlpha(0.8f));
              g.fillRect(layoutRect.getX(), muteTop, layoutRect.getWidth(),
                         muteRegionH);
              g.setColour(juce::Colours::lightgrey.withAlpha(0.6f));
              g.setFont(8.0f);
              for (int i = 0; i < N; ++i) {
                float mx = layoutRect.getX() + static_cast<float>(i) * fw;
                g.drawText("M", mx, muteTop, fw, muteRegionH,
                           juce::Justification::centred, false);
              }
            }
            g.setColour(juce::Colours::white);
            g.setFont(9.0f);
            g.drawText("Mixer: CC" + juce::String(strip.ccStart) + "-" +
                           juce::String(strip.ccStart + N - 1),
                       layoutRect.getX(), layoutRect.getBottom() + 2.0f,
                       layoutRect.getWidth(), 10,
                       juce::Justification::centredLeft, false);
          }
        } else if (ref.type == TouchpadType::DrumPad &&
                   ref.index < ctx->touchpadDrumPadStrips.size()) {
          const auto &strip = ctx->touchpadDrumPadStrips[ref.index];
          if (strip.layerId == currentVisualizedLayer && strip.rows > 0 &&
              strip.columns > 0 &&
              ((soloGroupId == 0 && strip.layoutGroupId == 0) || (soloGroupId > 0 && strip.layoutGroupId == soloGroupId))) {
            juce::Rectangle<float> layoutRect(
                touchpadRect.getX() +
                    strip.regionLeft * touchpadRect.getWidth(),
                touchpadRect.getY() +
                    strip.regionTop * touchpadRect.getHeight(),
                (strip.regionRight - strip.regionLeft) * touchpadRect.getWidth(),
                (strip.regionBottom - strip.regionTop) *
                    touchpadRect.getHeight());
            const int R = strip.rows;
            const int C = strip.columns;
            const float padW = 1.0f / static_cast<float>(C);
            const float padH = 1.0f / static_cast<float>(R);

            if (strip.layoutMode == DrumPadLayoutMode::Classic) {
              for (int row = 0; row < R; ++row) {
                for (int col = 0; col < C; ++col) {
                  float x =
                      layoutRect.getX() +
                      static_cast<float>(col) * padW * layoutRect.getWidth();
                  float y =
                      layoutRect.getY() +
                      static_cast<float>(row) * padH * layoutRect.getHeight();
                  float cw = padW * layoutRect.getWidth();
                  float ch = padH * layoutRect.getHeight();
                  g.setColour(juce::Colour(0xff405060).withAlpha(0.6f));
                  g.fillRect(x + 1.0f, y + 1.0f, cw - 2.0f, ch - 2.0f);
                  g.setColour(juce::Colours::lightgrey.withAlpha(0.5f));
                  g.drawRect(x, y, cw, ch, 0.5f);
                  int note =
                      strip.midiNoteStart + row * C + col;
                  note = juce::jlimit(0, 127, note);
                  g.setColour(juce::Colours::white);
                  g.setFont(juce::jmin(9.0f, cw * 0.4f));
                  g.drawText(MidiNoteUtilities::getMidiNoteName(note), x, y, cw,
                             ch, juce::Justification::centred, false);
                }
              }
              int lastNote = juce::jlimit(
                  0, 127, strip.midiNoteStart + strip.numPads - 1);
              g.setColour(juce::Colours::white);
              g.setFont(9.0f);
              g.drawText("Drum Pad: " +
                             MidiNoteUtilities::getMidiNoteName(
                                 strip.midiNoteStart) +
                             "-" +
                             MidiNoteUtilities::getMidiNoteName(lastNote),
                         layoutRect.getX(), layoutRect.getBottom() + 2.0f,
                         layoutRect.getWidth(), 10,
                         juce::Justification::centredLeft, false);
            } else if (strip.layoutMode == DrumPadLayoutMode::HarmonicGrid) {
              for (int row = 0; row < R; ++row) {
                for (int col = 0; col < C; ++col) {
                  float x =
                      layoutRect.getX() +
                      static_cast<float>(col) * padW * layoutRect.getWidth();
                  float y =
                      layoutRect.getY() +
                      static_cast<float>(row) * padH * layoutRect.getHeight();
                  float cw = padW * layoutRect.getWidth();
                  float ch = padH * layoutRect.getHeight();
                  // Slightly different color to hint "Harmonic" mode.
                  g.setColour(juce::Colour(0xff405045).withAlpha(0.6f));
                  g.fillRect(x + 1.0f, y + 1.0f, cw - 2.0f, ch - 2.0f);
                  g.setColour(juce::Colours::lightgrey.withAlpha(0.5f));
                  g.drawRect(x, y, cw, ch, 0.5f);
                  int note = strip.midiNoteStart +
                             col + row * strip.harmonicRowInterval;
                  note = juce::jlimit(0, 127, note);
                  g.setColour(juce::Colours::white);
                  g.setFont(juce::jmin(9.0f, cw * 0.4f));
                  g.drawText(MidiNoteUtilities::getMidiNoteName(note), x, y, cw,
                             ch, juce::Justification::centred, false);
                }
              }
              g.setColour(juce::Colours::white);
              g.setFont(9.0f);
              g.drawText("Harmonic Grid", layoutRect.getX(),
                         layoutRect.getBottom() + 2.0f, layoutRect.getWidth(),
                         10, juce::Justification::centredLeft, false);
            }
          }
        } else if (ref.type == TouchpadType::ChordPad &&
                   ref.index < ctx->touchpadChordPads.size()) {
          const auto &strip = ctx->touchpadChordPads[ref.index];
          if (strip.layerId == currentVisualizedLayer && strip.rows > 0 &&
              strip.columns > 0 &&
              ((soloGroupId == 0 && strip.layoutGroupId == 0) || (soloGroupId > 0 && strip.layoutGroupId == soloGroupId))) {
            juce::Rectangle<float> layoutRect(
                touchpadRect.getX() +
                    strip.regionLeft * touchpadRect.getWidth(),
                touchpadRect.getY() +
                    strip.regionTop * touchpadRect.getHeight(),
                (strip.regionRight - strip.regionLeft) * touchpadRect.getWidth(),
                (strip.regionBottom - strip.regionTop) *
                    touchpadRect.getHeight());
            const int R = strip.rows;
            const int C = strip.columns;
            const float padW = 1.0f / static_cast<float>(C);
            const float padH = 1.0f / static_cast<float>(R);
            for (int row = 0; row < R; ++row) {
              for (int col = 0; col < C; ++col) {
                float x = layoutRect.getX() +
                          static_cast<float>(col) * padW * layoutRect.getWidth();
                float y = layoutRect.getY() +
                          static_cast<float>(row) * padH *
                              layoutRect.getHeight();
                float cw = padW * layoutRect.getWidth();
                float ch = padH * layoutRect.getHeight();
                g.setColour(juce::Colour(0xff504050).withAlpha(0.6f));
                g.fillRect(x + 1.0f, y + 1.0f, cw - 2.0f, ch - 2.0f);
                g.setColour(juce::Colours::lightgrey.withAlpha(0.5f));
                g.drawRect(x, y, cw, ch, 0.5f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::jmin(9.0f, cw * 0.4f));
                g.drawText("Chord", x, y, cw, ch,
                           juce::Justification::centred, false);
              }
            }
            g.setColour(juce::Colours::white);
            g.setFont(9.0f);
            g.drawText("Chord Pad", layoutRect.getX(),
                       layoutRect.getBottom() + 2.0f, layoutRect.getWidth(), 10,
                       juce::Justification::centredLeft, false);
          }
        }
      }
    }
  }

  if (showContactCoordinates_) {
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
        line += "X=" + juce::String(c.normX, 2) + " Y=" +
                juce::String(c.normY, 2);
      else
        line += "X=- Y=-";
      g.drawText(line, panelLeft, y, panelWidth, lineHeight,
                 juce::Justification::centredLeft, false);
      y += lineHeight;
    }
  }

  // Per-mapping region overlays: visualize every compiled touchpad mapping as a
  // bounding box, filtered by the same layer + layout-group solo rules used at
  // runtime.
  if (!mappingVisuals.empty()) {
    for (const auto &vis : mappingVisuals) {
      if (!vis.entry)
        continue;
      const auto &entry = *vis.entry;

      juce::Colour baseCol;
      switch (vis.kind) {
      case TouchpadMappingVisualKind::Note:
        baseCol = juce::Colour(0xff3a5f9f); // soft blue
        break;
      case TouchpadMappingVisualKind::ExpressionCC:
        baseCol = juce::Colour(0xff2f7f4f); // soft green
        break;
      case TouchpadMappingVisualKind::Pitch:
        baseCol = juce::Colour(0xff7a4fb8); // soft purple
        break;
      case TouchpadMappingVisualKind::Slide:
        baseCol = juce::Colour(0xff3f8f6f); // slider-style green
        break;
      case TouchpadMappingVisualKind::Encoder:
        baseCol = juce::Colour(0xff9f7f3a); // amber
        break;
      case TouchpadMappingVisualKind::Command:
      case TouchpadMappingVisualKind::Macro:
        baseCol = juce::Colour(0xffc28b2f); // command amber
        break;
      default:
        baseCol = juce::Colour(0xff555555); // neutral
        break;
      }

      float cornerRadius = 3.0f;
      float borderThickness = 1.0f;
      if (vis.kind == TouchpadMappingVisualKind::Pitch)
        borderThickness = 1.5f;
      else if (vis.kind == TouchpadMappingVisualKind::Slide ||
               vis.kind == TouchpadMappingVisualKind::Encoder)
        borderThickness = 1.2f;
      if (vis.isRegionLocked)
        borderThickness += 0.4f;

      // Pitch/SmartScaleBend: draw bands inside this mapping's region only.
      if (vis.kind == TouchpadMappingVisualKind::Pitch &&
          entry.conversionParams.pitchPadConfig.has_value()) {
        const auto &config = *entry.conversionParams.pitchPadConfig;
        PitchPadLayout layout = buildPitchPadLayout(config);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(vis.regionRect.toNearestInt());
        const auto &r = vis.regionRect;
        if (entry.eventId == TouchpadEvent::Finger1X) {
          float offset = 0.0f;
          if (anchorNormX && config.mode == PitchPadMode::Relative) {
            float zeroX = 0.5f;
            for (const auto &b : layout.bands) {
              if (b.step == static_cast<int>(config.zeroStep)) {
                zeroX = (b.xStart + b.xEnd) * 0.5f;
                break;
              }
            }
            offset = *anchorNormX - zeroX;
          }
          for (const auto &band : layout.bands) {
            float xStart = juce::jlimit(0.0f, 1.0f, band.xStart + offset);
            float xEnd = juce::jlimit(0.0f, 1.0f, band.xEnd + offset);
            if (xEnd <= xStart)
              continue;
            float bx = r.getX() + xStart * r.getWidth();
            float bw = (xEnd - xStart) * r.getWidth();
            if (bw > 0.5f) {
              g.setColour(band.isRest ? xRestCol : xTransCol);
              g.fillRect(bx, r.getY(), bw, r.getHeight());
            }
          }
        } else if (entry.eventId == TouchpadEvent::Finger1Y) {
          for (const auto &band : layout.bands) {
            float by = r.getY() + band.xStart * r.getHeight();
            float bh = (band.xEnd - band.xStart) * r.getHeight();
            if (bh > 0.5f) {
              g.setColour(band.isRest ? yRestCol : yTransCol);
              g.fillRect(r.getX(), by, r.getWidth(), bh);
            }
          }
        }
      }

      // Fill: solid or simple gradient along axis for position-dependent
      // mappings. For pitch-pad mappings, keep the fill subtle so the
      // underlying band-based visualization remains clearly visible.
      if (vis.isPositionDependent &&
          vis.kind != TouchpadMappingVisualKind::Pitch) {
        juce::Colour cLow = baseCol.darker(0.4f).withAlpha(0.45f);
        juce::Colour cHigh = baseCol.brighter(0.35f).withAlpha(0.85f);
        juce::ColourGradient grad(cLow, 0.0f, 0.0f, cHigh, 1.0f, 1.0f, false);
        auto r = vis.regionRect;
        if (vis.axis == TouchpadVisualAxis::Horizontal) {
          grad.point1 = {r.getX(), r.getCentreY()};
          grad.point2 = {r.getRight(), r.getCentreY()};
        } else if (vis.axis == TouchpadVisualAxis::Vertical) {
          grad.point1 = {r.getCentreX(), r.getY()};
          grad.point2 = {r.getCentreX(), r.getBottom()};
        } else { // Both / None -> subtle diagonal
          grad.point1 = {r.getX(), r.getY()};
          grad.point2 = {r.getRight(), r.getBottom()};
        }
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, cornerRadius);
      } else if (vis.kind != TouchpadMappingVisualKind::Pitch) {
        g.setColour(baseCol.withAlpha(0.24f));
        g.fillRoundedRectangle(vis.regionRect, cornerRadius);
      }

      g.setColour(baseCol.brighter(0.35f).withAlpha(0.9f));
      g.drawRoundedRectangle(vis.regionRect, cornerRadius, borderThickness);

      // Value bar for mappings with remembered values.
      if (vis.hasRememberedValue && vis.currentValue01.has_value()) {
        float v = juce::jlimit(0.0f, 1.0f, *vis.currentValue01);
        juce::Colour barCol =
            baseCol.brighter(0.6f).withAlpha(0.95f);
        g.setColour(barCol);
        auto r = vis.regionRect.reduced(1.0f, 1.0f);
        if (vis.axis == TouchpadVisualAxis::Horizontal) {
          float w = r.getWidth() * v;
          g.fillRect(r.getX(), r.getBottom() - 3.0f, w, 3.0f);
        } else {
          float h = r.getHeight() * v;
          float y = r.getBottom() - h;
          g.fillRect(r.getRight() - 3.0f, y, 3.0f, h);
        }
      }

      // Header + centre text labels.
      juce::String typeLabel;
      switch (vis.kind) {
      case TouchpadMappingVisualKind::Note:
        typeLabel = "Note";
        break;
      case TouchpadMappingVisualKind::ExpressionCC:
        typeLabel = "Expr";
        break;
      case TouchpadMappingVisualKind::Pitch:
        typeLabel = "Pitch";
        break;
      case TouchpadMappingVisualKind::Slide:
        typeLabel = "Slide";
        break;
      case TouchpadMappingVisualKind::Encoder:
        typeLabel = "Enc";
        break;
      case TouchpadMappingVisualKind::Command:
        typeLabel = "Cmd";
        break;
      case TouchpadMappingVisualKind::Macro:
        typeLabel = "Macro";
        break;
      default:
        typeLabel = "Map";
        break;
      }

      juce::String targetLabel;
      if (entry.action.type == ActionType::Note) {
        int note = juce::jlimit(0, 127, entry.action.data1);
        targetLabel = MidiNoteUtilities::getMidiNoteName(note);
      } else if (entry.action.type == ActionType::Expression) {
        if (entry.action.adsrSettings.target == AdsrTarget::CC)
          targetLabel =
              "CC" + juce::String(entry.action.adsrSettings.ccNumber);
        else if (isPitchTarget(entry.action))
          targetLabel = "PB";
      }

      juce::String eventLabel = touchpadEventToLabel(entry.eventId);
      // Compact header: focus on type + target to avoid cramped text and keep
      // the most important information visible at small sizes.
      juce::String header = typeLabel;
      if (targetLabel.isNotEmpty())
        header += "  " + targetLabel;

      g.setColour(juce::Colours::white.withAlpha(0.85f));
      float headerFontSize =
          juce::jmin(10.0f, vis.regionRect.getHeight() * 0.26f);
      g.setFont(headerFontSize);
      juce::Rectangle<float> headerArea =
          vis.regionRect.reduced(5.0f, 4.0f);
      headerArea.setHeight(headerFontSize + 2.0f);
      g.drawText(header, headerArea, juce::Justification::centredLeft,
                 false);

      juce::String mainLabel;
      if (vis.kind == TouchpadMappingVisualKind::Note) {
        mainLabel = targetLabel;
      } else if (vis.kind == TouchpadMappingVisualKind::Pitch) {
        mainLabel = "PB";
      } else if (vis.kind == TouchpadMappingVisualKind::Encoder) {
        if (targetLabel.isNotEmpty())
          mainLabel = "Enc " + targetLabel;
        else
          mainLabel = "Enc";
      } else if (targetLabel.isNotEmpty()) {
        mainLabel = targetLabel;
      } else {
        mainLabel = typeLabel;
      }

      float mainFontSize =
          juce::jmin(12.0f, vis.regionRect.getHeight() * 0.45f);
      g.setFont(mainFontSize);
      g.drawText(mainLabel, vis.regionRect,
                 juce::Justification::centred, false);

      // Axis arrows.
      g.setColour(baseCol.brighter(0.7f).withAlpha(0.9f));
      float arrowFontSize =
          juce::jmin(9.0f, vis.regionRect.getHeight() * 0.35f);
      g.setFont(arrowFontSize);
      if (vis.axis == TouchpadVisualAxis::Horizontal ||
          vis.axis == TouchpadVisualAxis::Both) {
        juce::Rectangle<float> r = vis.regionRect;
        r = r.withHeight(arrowFontSize + 2.0f);
        r.setY(vis.regionRect.getBottom() - r.getHeight());
        g.drawText(">", r.reduced(4.0f, 0.0f),
                   juce::Justification::centredRight, false);
      }
      if (vis.axis == TouchpadVisualAxis::Vertical ||
          vis.axis == TouchpadVisualAxis::Both) {
        juce::Rectangle<float> r = vis.regionRect;
        // Place vertical axis arrow around the vertical centre on the left edge
        // so it does not clash with the header text at the top.
        r = r.withWidth(arrowFontSize + 4.0f);
        r.setHeight(arrowFontSize + 2.0f);
        r.setY(vis.regionRect.getCentreY() - r.getHeight() * 0.5f);
        g.drawText("^", r, juce::Justification::centred, false);
      }

      // Region lock glyph (top-right) – draw a tiny lock outline instead of
      // text so the icon is clear but unobtrusive.
      if (vis.isRegionLocked) {
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        juce::Rectangle<float> r = vis.regionRect.reduced(3.0f, 3.0f);
        float bodyW = juce::jmin(8.0f, r.getWidth() * 0.35f);
        float bodyH = juce::jmin(6.0f, r.getHeight() * 0.3f);
        float bodyX = r.getRight() - bodyW;
        float bodyY = r.getY() + (bodyH * 0.8f);
        g.drawRoundedRectangle(bodyX, bodyY, bodyW, bodyH, 1.5f, 1.0f);
        float shackleW = bodyW * 0.6f;
        float shackleX = bodyX + (bodyW - shackleW) * 0.5f;
        float shackleY = bodyY - 3.0f;
        g.drawLine(shackleX, shackleY + 1.0f, shackleX, bodyY, 1.0f);
        g.drawLine(shackleX + shackleW, shackleY + 1.0f,
                   shackleX + shackleW, bodyY, 1.0f);
        g.drawLine(shackleX, shackleY + 1.0f,
                   shackleX + shackleW, shackleY + 1.0f, 1.0f);
      }

      // Latched indicator (bottom-left small dot).
      if (vis.isLatched) {
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        float radius = 2.0f;
        float cx = vis.regionRect.getX() + 4.0f;
        float cy = vis.regionRect.getBottom() - 4.0f;
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f,
                      radius * 2.0f);
      }
    }
  }
}
