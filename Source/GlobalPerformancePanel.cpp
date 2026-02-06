#include "GlobalPerformancePanel.h"
#include "MidiNoteUtilities.h"
#include "ScaleLibrary.h"

GlobalPerformancePanel::GlobalPerformancePanel(ZoneManager *zoneMgr,
                                               ScaleLibrary *scaleLib)
    : zoneManager(zoneMgr), scaleLibrary(scaleLib) {
  addAndMakeVisible(rootLabel);
  rootLabel.setText("Root:", juce::dontSendNotification);
  rootLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

  addAndMakeVisible(rootNoteCombo);
  for (int n = 0; n <= 127; ++n)
    rootNoteCombo.addItem(MidiNoteUtilities::getMidiNoteName(n), n + 1);
  rootNoteCombo.onChange = [this] {
    if (zoneManager) {
      int note = rootNoteCombo.getSelectedId() - 1;
      if (note >= 0 && note <= 127)
        zoneManager->setGlobalRoot(note);
    }
  };

  addAndMakeVisible(scaleLabel);
  scaleLabel.setText("Scale:", juce::dontSendNotification);
  scaleLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

  addAndMakeVisible(transposeLabel);
  transposeLabel.setText("Transpose:", juce::dontSendNotification);
  transposeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

  addAndMakeVisible(scaleCombo);
  if (scaleLibrary) {
    juce::StringArray names = scaleLibrary->getScaleNames();
    for (int i = 0; i < names.size(); ++i)
      scaleCombo.addItem(names[i], i + 1);
    scaleCombo.onChange = [this] {
      if (zoneManager && scaleLibrary) {
        int idx = scaleCombo.getSelectedId() - 1;
        juce::StringArray names = scaleLibrary->getScaleNames();
        if (idx >= 0 && idx < names.size())
          zoneManager->setGlobalScale(names[idx]);
      }
    };
  }

  addAndMakeVisible(chromaticDownButton);
  chromaticDownButton.setButtonText("-1");
  chromaticDownButton.onClick = [this] {
    if (zoneManager) {
      int current = zoneManager->getGlobalChromaticTranspose();
      zoneManager->setGlobalTranspose(current - 1, zoneManager->getGlobalDegreeTranspose());
    }
  };

  addAndMakeVisible(chromaticUpButton);
  chromaticUpButton.setButtonText("+1");
  chromaticUpButton.onClick = [this] {
    if (zoneManager) {
      int current = zoneManager->getGlobalChromaticTranspose();
      zoneManager->setGlobalTranspose(current + 1, zoneManager->getGlobalDegreeTranspose());
    }
  };

  addAndMakeVisible(chromaticSlider);
  chromaticSlider.setRange(-48, 48, 1);
  chromaticSlider.setNumDecimalPlacesToDisplay(0);
  chromaticSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
  chromaticSlider.addListener(this);

  addAndMakeVisible(statusLabel);
  statusLabel.setText("Transpose: 0st", juce::dontSendNotification);
  statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

  if (zoneManager) {
    zoneManager->addChangeListener(this);
    refreshGlobalRootAndScaleFromManager();
    updateStatusLabel();
  }
}

GlobalPerformancePanel::~GlobalPerformancePanel() {
  chromaticSlider.removeListener(this);
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
}

void GlobalPerformancePanel::sliderValueChanged(juce::Slider *slider) {
  if (!zoneManager || slider != &chromaticSlider)
    return;
  zoneManager->setGlobalTranspose(static_cast<int>(chromaticSlider.getValue()),
                                  zoneManager->getGlobalDegreeTranspose());
}

void GlobalPerformancePanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));
}

void GlobalPerformancePanel::resized() {
  auto area = getLocalBounds().reduced(6);
  const int rowH = 22;
  const int labelW = 72;   // Enough for "Root:", "Scale:", "Transpose:"
  const int buttonW = 32;
  const int gap = 6;

  // Row 1: Root (label then combo on next line, full width)
  rootLabel.setBounds(area.removeFromTop(rowH).reduced(0, 2));
  area.removeFromTop(2);
  rootNoteCombo.setBounds(area.removeFromTop(rowH).reduced(0, 2));
  area.removeFromTop(gap);

  // Row 2: Scale (label then combo on next line, full width)
  scaleLabel.setBounds(area.removeFromTop(rowH).reduced(0, 2));
  area.removeFromTop(2);
  scaleCombo.setBounds(area.removeFromTop(rowH).reduced(0, 2));
  area.removeFromTop(gap);

  // Row 3: Transpose (label, -1/+1 buttons, slider fills rest)
  auto chromRow = area.removeFromTop(rowH);
  transposeLabel.setBounds(chromRow.removeFromLeft(labelW).reduced(0, 2));
  chromaticDownButton.setBounds(chromRow.removeFromLeft(buttonW).reduced(1, 2));
  chromRow.removeFromLeft(2);
  chromaticUpButton.setBounds(chromRow.removeFromLeft(buttonW).reduced(1, 2));
  chromRow.removeFromLeft(4);
  chromaticSlider.setBounds(chromRow.reduced(0, 2));
  area.removeFromTop(gap);

  // Row 4: Status (full width)
  statusLabel.setBounds(area.removeFromTop(rowH).reduced(0, 2));
}

void GlobalPerformancePanel::refreshGlobalRootAndScaleFromManager() {
  if (!zoneManager)
    return;
  int root = zoneManager->getGlobalRootNote();
  rootNoteCombo.setSelectedId(root + 1, juce::dontSendNotification);
  juce::String scaleName = zoneManager->getGlobalScaleName();
  for (int i = 1; i <= scaleCombo.getNumItems(); ++i) {
    if (scaleCombo.getItemText(i - 1) == scaleName) {
      scaleCombo.setSelectedId(i, juce::dontSendNotification);
      break;
    }
  }
  chromaticSlider.setValue(zoneManager->getGlobalChromaticTranspose(),
                           juce::dontSendNotification);
}

void GlobalPerformancePanel::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == zoneManager) {
    juce::MessageManager::callAsync([this] {
      refreshGlobalRootAndScaleFromManager();
      updateStatusLabel();
    });
  }
}

void GlobalPerformancePanel::updateStatusLabel() {
  if (!zoneManager) {
    statusLabel.setText("Transpose: 0st", juce::dontSendNotification);
    return;
  }

  int chromatic = zoneManager->getGlobalChromaticTranspose();
  juce::String chromaticStr = (chromatic == 0) ? "0st" : ((chromatic > 0) ? "+" + juce::String(chromatic) + "st" : juce::String(chromatic) + "st");
  statusLabel.setText("Transpose: " + chromaticStr, juce::dontSendNotification);
}
