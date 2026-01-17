#include "GlobalPerformancePanel.h"

GlobalPerformancePanel::GlobalPerformancePanel(ZoneManager *zoneMgr)
    : zoneManager(zoneMgr) {
  
  addAndMakeVisible(degreeDownButton);
  degreeDownButton.setButtonText("-1");
  degreeDownButton.onClick = [this] {
    if (zoneManager) {
      int current = zoneManager->getGlobalDegreeTranspose();
      zoneManager->setGlobalTranspose(zoneManager->getGlobalChromaticTranspose(), current - 1);
    }
  };

  addAndMakeVisible(degreeUpButton);
  degreeUpButton.setButtonText("+1");
  degreeUpButton.onClick = [this] {
    if (zoneManager) {
      int current = zoneManager->getGlobalDegreeTranspose();
      zoneManager->setGlobalTranspose(zoneManager->getGlobalChromaticTranspose(), current + 1);
    }
  };

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

  addAndMakeVisible(statusLabel);
  statusLabel.setText("Scale: 0 | Pitch: 0st", juce::dontSendNotification);
  statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

  if (zoneManager) {
    zoneManager->addChangeListener(this);
    updateStatusLabel();
  }
}

GlobalPerformancePanel::~GlobalPerformancePanel() {
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
}

void GlobalPerformancePanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));
  
  // Draw labels
  g.setColour(juce::Colours::lightgrey);
  g.setFont(12.0f);
  g.drawText("Degree Shift:", 8, 4, 100, 20, juce::Justification::centredLeft);
  g.drawText("Chromatic Shift:", 120, 4, 100, 20, juce::Justification::centredLeft);
}

void GlobalPerformancePanel::resized() {
  auto area = getLocalBounds().reduced(4);
  
  // Degree buttons
  auto degreeArea = area.removeFromLeft(110);
  degreeDownButton.setBounds(degreeArea.removeFromTop(24).removeFromLeft(40));
  degreeArea.removeFromLeft(4);
  degreeUpButton.setBounds(degreeArea.removeFromTop(24).removeFromLeft(40));
  
  area.removeFromLeft(8);
  
  // Chromatic buttons
  auto chromaticArea = area.removeFromLeft(110);
  chromaticDownButton.setBounds(chromaticArea.removeFromTop(24).removeFromLeft(40));
  chromaticArea.removeFromLeft(4);
  chromaticUpButton.setBounds(chromaticArea.removeFromTop(24).removeFromLeft(40));
  
  area.removeFromLeft(8);
  
  // Status label
  statusLabel.setBounds(area);
}

void GlobalPerformancePanel::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == zoneManager) {
    juce::MessageManager::callAsync([this] {
      updateStatusLabel();
    });
  }
}

void GlobalPerformancePanel::updateStatusLabel() {
  if (!zoneManager) {
    statusLabel.setText("Scale: 0 | Pitch: 0st", juce::dontSendNotification);
    return;
  }

  int degree = zoneManager->getGlobalDegreeTranspose();
  int chromatic = zoneManager->getGlobalChromaticTranspose();

  juce::String degreeStr = (degree == 0) ? "0" : ((degree > 0) ? "+" + juce::String(degree) : juce::String(degree));
  juce::String chromaticStr = (chromatic == 0) ? "0st" : ((chromatic > 0) ? "+" + juce::String(chromatic) + "st" : juce::String(chromatic) + "st");

  statusLabel.setText("Scale: " + degreeStr + " | Pitch: " + chromaticStr, juce::dontSendNotification);
}
