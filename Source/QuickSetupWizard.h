#pragma once
#include "DeviceManager.h"
#include "RawInputManager.h"
#include <JuceHeader.h>

class QuickSetupWizard : public juce::Component,
                          public RawInputManager::Listener {
public:
  QuickSetupWizard(DeviceManager& deviceMgr, RawInputManager& rawInputMgr);
  ~QuickSetupWizard() override;

  void paint(juce::Graphics& g) override;
  void resized() override;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) override;

  // Start the wizard sequence
  void startSequence(const juce::StringArray& aliasesToMap);

  // Callback when wizard finishes
  std::function<void()> onFinish;

private:
  DeviceManager& deviceManager;
  RawInputManager& rawInputManager;
  
  juce::StringArray aliasesToMap;
  int currentIndex = 0;
  
  juce::Label titleLabel;
  juce::Label instructionLabel;
  juce::TextButton skipButton;
  juce::TextButton cancelButton;
  
  void updateInstruction();
  void onSkip();
  void onCancel();
  void finishWizard();
};
