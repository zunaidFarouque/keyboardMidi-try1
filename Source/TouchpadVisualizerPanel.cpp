#include "TouchpadVisualizerPanel.h"
#include "MappingTypes.h"
#include "MidiNoteUtilities.h"
#include "PitchPadUtilities.h"
#include "TouchpadMixerTypes.h"
#include <algorithm>
#include <optional>
#include <set>

namespace {
struct TouchpadMappingVisual {
  const TouchpadMappingEntry *entry = nullptr;
  juce::Rectangle<float> regionRect;
};
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
      for (const auto &entry : ctx->touchpadMappings) {
        if (entry.layerId != currentVisualizedLayer)
          continue;
        if ((soloGroupForLayer == 0 && entry.layoutGroupId != 0) ||
            (soloGroupForLayer > 0 &&
             entry.layoutGroupId != soloGroupForLayer))
          continue;
        TouchpadMappingVisual vis;
        vis.entry = &entry;
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
    }
  }

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

  g.setColour(juce::Colours::lightgrey);
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
        juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, cx,
                                        cy));
    g.drawText(yLabel, static_cast<int>(cx - 40.0f),
               static_cast<int>(cy - 6.0f), 80, 12,
               juce::Justification::centred, false);
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

      juce::Colour baseCol = juce::Colours::lightgrey.withAlpha(0.4f);
      if (settingsManager) {
        try {
          baseCol =
              settingsManager->getTypeColor(entry.action.type).withAlpha(0.4f);
        } catch (...) {
          // Fallback to default color on any error.
        }
      }

      // Conversion-kind hints via border thickness.
      float borderThickness = 1.0f;
      if (entry.conversionKind ==
              TouchpadConversionKind::ContinuousToRange &&
          (entry.action.adsrSettings.target == AdsrTarget::PitchBend ||
           entry.action.adsrSettings.target == AdsrTarget::SmartScaleBend)) {
        borderThickness = 1.5f; // Pitch pad-style mappings
      } else if (entry.conversionKind == TouchpadConversionKind::SlideToCC ||
                 entry.conversionKind == TouchpadConversionKind::EncoderCC) {
        borderThickness = 1.2f; // Slider/encoder-style mappings
      }

      g.setColour(baseCol.withAlpha(0.18f));
      g.fillRect(vis.regionRect);
      g.setColour(baseCol.brighter(0.4f));
      g.drawRect(vis.regionRect, borderThickness);
    }
  }
}
