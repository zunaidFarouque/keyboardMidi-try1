#include "TouchpadVisualizerPanel.h"
#include "MappingTypes.h"
#include "MidiNoteUtilities.h"
#include "PitchPadUtilities.h"
#include "TouchpadMixerTypes.h"
#include <algorithm>
#include <optional>

TouchpadVisualizerPanel::TouchpadVisualizerPanel(InputProcessor *inputProc,
                                                 SettingsManager *settingsMgr)
    : inputProcessor(inputProc), settingsManager(settingsMgr) {}

TouchpadVisualizerPanel::~TouchpadVisualizerPanel() { stopTimer(); }

void TouchpadVisualizerPanel::setContacts(
    const std::vector<TouchpadContact> &contacts, uintptr_t deviceHandle) {
  juce::ScopedLock lock(contactsLock_);
  contacts_ = contacts;
  lastDeviceHandle_.store(deviceHandle, std::memory_order_release);
  repaint();
}

void TouchpadVisualizerPanel::setVisualizedLayer(int layerId) {
  if (layerId >= 0)
    currentVisualizedLayer = layerId;
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
  if (isVisible())
    startTimer(intervalMs);
}

void TouchpadVisualizerPanel::visibilityChanged() {
  if (isVisible()) {
    int interval = settingsManager
                       ? settingsManager->getWindowRefreshIntervalMs()
                       : kDefaultRefreshIntervalMs;
    startTimer(interval);
  } else {
    stopTimer();
  }
}

void TouchpadVisualizerPanel::timerCallback() { repaint(); }

void TouchpadVisualizerPanel::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  float panelLeft = 8.0f;
  float panelWidth = bounds.getWidth() - 16.0f;
  float panelTop = 8.0f;
  float panelBottom = bounds.getHeight() - 8.0f;

  if (panelWidth <= 40.0f || panelBottom <= panelTop)
    return;

  std::vector<TouchpadContact> contactsSnapshot;
  {
    juce::ScopedLock lock(contactsLock_);
    contactsSnapshot = contacts_;
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

  // When no layout is explicitly selected (-1), use first layout for current
  // layer so the touchpad shows the active layout when following the active layer.
  int displayLayoutIndex = selectedLayoutIndex_;
  if (displayLayoutIndex < 0 && inputProcessor) {
    auto ctx = inputProcessor->getContext();
    if (ctx) {
      for (size_t i = 0; i < ctx->touchpadLayoutOrder.size(); ++i) {
        const auto &ref = ctx->touchpadLayoutOrder[i];
        int layoutLayer = -1;
        if (ref.type == TouchpadType::Mixer &&
            ref.index < ctx->touchpadMixerStrips.size())
          layoutLayer = ctx->touchpadMixerStrips[ref.index].layerId;
        else if (ref.type == TouchpadType::DrumPad &&
                 ref.index < ctx->touchpadDrumPadStrips.size())
          layoutLayer = ctx->touchpadDrumPadStrips[ref.index].layerId;
        if (layoutLayer == currentVisualizedLayer) {
          displayLayoutIndex = static_cast<int>(i);
          break;
        }
      }
    }
  }

  if (inputProcessor && displayLayoutIndex >= 0) {
    auto ctx = inputProcessor->getContext();
    if (ctx) {
      size_t listIdx = static_cast<size_t>(displayLayoutIndex);
      if (listIdx < ctx->touchpadLayoutOrder.size()) {
        const auto &ref = ctx->touchpadLayoutOrder[listIdx];
        if (ref.type == TouchpadType::Mixer &&
            ref.index < ctx->touchpadMixerStrips.size()) {
          const auto &strip = ctx->touchpadMixerStrips[ref.index];
          if (strip.layerId == currentVisualizedLayer && strip.numFaders > 0) {
            uintptr_t dev =
                lastDeviceHandle_.load(std::memory_order_acquire);
            auto state = inputProcessor->getTouchpadMixerStripState(
                dev, static_cast<int>(ref.index), strip.numFaders);
            const auto &displayValues = state.displayValues;
            const auto &muted = state.muted;
            const int N = strip.numFaders;
            const float fw = touchpadRect.getWidth() / static_cast<float>(N);
            const float h = touchpadRect.getHeight();
            const float muteRegionH =
                (strip.modeFlags & kMixerModeMuteButtons) ? (h * 0.15f) : 0.0f;
            const float faderH = h - muteRegionH;
            const float faderTop = touchpadRect.getY();
            const float faderBottom = faderTop + faderH;
            const float inMin =
                juce::jlimit(0.0f, 1.0f, strip.inputMin);
            const float inMax =
                juce::jlimit(0.0f, 1.0f, strip.inputMax);
            const bool hasDeadZones = (inMin > 0.0f || inMax < 1.0f);

            for (int i = 0; i < N; ++i) {
              float stripX =
                  touchpadRect.getX() + static_cast<float>(i) * fw;
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
              g.setColour(juce::Colours::white);
              g.setFont(juce::jmin(9.0f, fw * 0.5f));
              g.drawText(juce::String(strip.ccStart + i), stripX,
                         faderBottom - 14.0f, fw, 12.0f,
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
        } else if (ref.type == TouchpadType::DrumPad &&
                   ref.index < ctx->touchpadDrumPadStrips.size()) {
          const auto &strip = ctx->touchpadDrumPadStrips[ref.index];
          if (strip.layerId == currentVisualizedLayer &&
              strip.numPads > 0) {
            const int R = strip.rows;
            const int C = strip.columns;
            const float inL = strip.deadZoneLeft;
            const float inR = strip.deadZoneRight;
            const float inT = strip.deadZoneTop;
            const float inB = strip.deadZoneBottom;
            const float activeW = 1.0f - inL - inR;
            const float activeH = 1.0f - inT - inB;
            const float padW = activeW / static_cast<float>(C);
            const float padH = activeH / static_cast<float>(R);
            if (inL > 0.0f || inR > 0.0f || inT > 0.0f || inB > 0.0f) {
              g.setColour(juce::Colour(0xff383838).withAlpha(0.75f));
              if (inL > 0.0f)
                g.fillRect(touchpadRect.getX(), touchpadRect.getY(),
                           inL * touchpadRect.getWidth(),
                           touchpadRect.getHeight());
              if (inR > 0.0f)
                g.fillRect(
                    touchpadRect.getRight() - inR * touchpadRect.getWidth(),
                    touchpadRect.getY(), inR * touchpadRect.getWidth(),
                    touchpadRect.getHeight());
              if (inT > 0.0f)
                g.fillRect(touchpadRect.getX(), touchpadRect.getY(),
                           touchpadRect.getWidth(),
                           inT * touchpadRect.getHeight());
              if (inB > 0.0f)
                g.fillRect(
                    touchpadRect.getX(),
                    touchpadRect.getBottom() -
                        inB * touchpadRect.getHeight(),
                    touchpadRect.getWidth(), inB * touchpadRect.getHeight());
            }
            for (int row = 0; row < R; ++row) {
              for (int col = 0; col < C; ++col) {
                float x = touchpadRect.getX() + inL * touchpadRect.getWidth() +
                          static_cast<float>(col) * padW *
                              touchpadRect.getWidth();
                float y = touchpadRect.getY() + inT * touchpadRect.getHeight() +
                          static_cast<float>(row) * padH *
                              touchpadRect.getHeight();
                float cw = padW * touchpadRect.getWidth();
                float ch = padH * touchpadRect.getHeight();
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
                       touchpadRect.getX(), touchpadRect.getBottom() + 2.0f,
                       touchpadRect.getWidth(), 10,
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
}
