#include "VisualizerComponent.h"
#include "InputProcessor.h"
#include "PresetManager.h"
#include "VoiceManager.h"
#include "Zone.h"
#include <JuceHeader.h>
#include <algorithm>

// Helper to convert alias name to hash (same as in InputProcessor)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" || aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

VisualizerComponent::VisualizerComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, const VoiceManager &voiceMgr, PresetManager *presetMgr, InputProcessor *inputProc)
    : zoneManager(zoneMgr), deviceManager(deviceMgr), voiceManager(voiceMgr), presetManager(presetMgr), inputProcessor(inputProc) {
  if (zoneManager) {
    zoneManager->addChangeListener(this);
  }
  if (presetManager) {
    auto mappingsNode = presetManager->getMappingsNode();
    if (mappingsNode.isValid()) {
      mappingsNode.addListener(this);
    }
    // Also listen to root node changes (in case mappings node is recreated)
    presetManager->getRootNode().addListener(this);
  }
  // Start timer to update sustain/latch indicators (30 FPS)
  startTimer(33); // ~30 FPS
}

VisualizerComponent::~VisualizerComponent() {
  stopTimer();
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
  if (presetManager) {
    auto mappingsNode = presetManager->getMappingsNode();
    if (mappingsNode.isValid()) {
      mappingsNode.removeListener(this);
    }
    presetManager->getRootNode().removeListener(this);
  }
}

void VisualizerComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff111111)); // Background

  if (!zoneManager) {
    return; // Can't render without zone manager
  }

  // --- 0. Header Bar (Transpose left, Sustain right) ---
  auto bounds = getLocalBounds();
  auto headerRect = bounds.removeFromTop(30);
  g.setColour(juce::Colour(0xff222222));
  g.fillRect(headerRect);

  g.setColour(juce::Colours::white);
  g.setFont(12.0f);

  // TRANSPOSE (left)
  if (zoneManager) {
    int chrom = zoneManager->getGlobalChromaticTranspose();
    int deg = zoneManager->getGlobalDegreeTranspose();
    juce::String chromStr = (chrom >= 0) ? ("+" + juce::String(chrom)) : juce::String(chrom);
    juce::String degStr = (deg >= 0) ? ("+" + juce::String(deg)) : juce::String(deg);
    juce::String transposeText = "TRANSPOSE: [Pitch: " + chromStr + "] [Scale: " + degStr + "]";
    g.drawText(transposeText, 8, 0, 280, headerRect.getHeight(), juce::Justification::centredLeft, false);
  }

  // Sustain Indicator (right)
  bool sustainActive = voiceManager.isSustainActive();
  juce::Colour sustainColor = sustainActive ? juce::Colours::lime : juce::Colours::grey;
  int indicatorSize = 12;
  int indicatorX = headerRect.getRight() - 100;
  int indicatorY = headerRect.getCentreY() - indicatorSize / 2;
  g.setColour(sustainColor);
  g.fillEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);
  g.setColour(juce::Colours::white);
  g.drawText("SUSTAIN", indicatorX + indicatorSize + 5, indicatorY, 60, indicatorSize, juce::Justification::centredLeft, false);

  // --- 1. Calculate Dynamic Scale ---
  // Full 104-key layout is roughly 23.0 keys wide (including numpad) and 6.5 keys tall (including function row).
  // Row -1 (function row) is at -1.2f offset, rows 0-4 are at 0-4
  // With 1.2f vertical spacing multiplier: function row at -1.44f, rows 0-4 at 0.0f to 4.8f
  // Row 4 bottom (including key height): 4.8f + 1.0f = 5.8f
  // Total span from function row top to row 4 bottom: 5.8f - (-1.44f) = 7.24f units
  float unitsWide = 23.0f;
  float unitsTall = 7.3f; // Accounts for function row at -1.2 offset + key height
  float headerHeight = 30.0f;
  float availableHeight = static_cast<float>(getHeight()) - headerHeight;
  
  // Scale keySize to fit in AVAILABLE height (below header) and width
  float scaleX = static_cast<float>(getWidth()) / unitsWide;
  float scaleY = (availableHeight > 0.0f) ? (availableHeight / unitsTall) : scaleX;
  float keySize = std::min(scaleX, scaleY) * 0.9f; // 0.9f for margin
  
  // Keyboard vertical span: top at startY - 1.44*keySize, bottom at startY + 5.8*keySize
  // Constraint: top >= headerHeight, bottom <= getHeight() (keys use component coords)
  float row4Bottom = 5.8f * keySize; // Height from startY to row 4 bottom
  float minStartY = headerHeight + 1.44f * keySize;
  float maxStartY = static_cast<float>(getHeight()) - row4Bottom;
  
  // Center vertically within [minStartY, maxStartY]; clamp so keyboard is never clipped
  float startY = (minStartY + maxStartY) / 2.0f;
  startY = juce::jlimit(minStartY, maxStartY, startY);
  
  float totalWidth = unitsWide * keySize;
  float startX = (static_cast<float>(getWidth()) - totalWidth) / 2.0f;

  // --- 2. Iterate Keys ---
  const auto &layout = KeyboardLayoutUtils::getLayout();
  for (const auto &pair : layout) {
    int keyCode = pair.first;
    const auto &geometry = pair.second;
    
    // Calculate Position relative to start
    // Row -1 (function row) needs special handling - place it above row 0
    float rowOffset = (geometry.row == -1) ? -1.2f : static_cast<float>(geometry.row);
    float x = startX + (geometry.col * keySize);
    float y = startY + (rowOffset * keySize * 1.2f); // 1.2 multiplier for vertical spacing
    float w = geometry.width * keySize;
    float h = geometry.height * keySize;

    juce::Rectangle<float> fullBounds(x, y, w, h);
    
    // Define Inner Body (Padding)
    float padding = keySize * 0.1f; // 10% padding looks good at any size
    auto keyBounds = fullBounds.reduced(padding);

    // --- 3. Get Data from Engine ---
    // Conflict: key in multiple zones, or in a zone and also in a manual mapping
    int zoneCount = zoneManager->getZoneCountForKey(keyCode);
    bool hasManual = inputProcessor && inputProcessor->hasManualMappingForKey(keyCode);
    bool isConflict = (zoneCount >= 2) || (zoneCount >= 1 && hasManual);

    // A. Zone Underlay Color (or red if conflict)
    juce::Colour underlayColor = juce::Colours::transparentBlack;
    if (isConflict) {
      underlayColor = juce::Colours::red.withAlpha(0.7f);
    } else {
      // Check master alias (hash 0) first, then device aliases
      auto zoneColor = zoneManager->getZoneColorForKey(keyCode, 0);
      if (zoneColor.has_value()) {
        underlayColor = zoneColor.value();
      } else if (deviceManager) {
        auto aliases = deviceManager->getAllAliasNames();
        for (const auto &aliasName : aliases) {
          uintptr_t aliasHash = aliasNameToHash(aliasName);
          zoneColor = zoneManager->getZoneColorForKey(keyCode, aliasHash);
          if (zoneColor.has_value()) {
            underlayColor = zoneColor.value();
            break;
          }
        }
      }
    }

    // B. Key State
    bool isPressed = (activeKeys.find(keyCode) != activeKeys.end());
    
    // C. Text/Label
    juce::String labelText = geometry.label; // Default: "Q"
    
    // Find which zone this key belongs to and get the note name
    std::pair<std::shared_ptr<Zone>, bool> zoneInfo{nullptr, false};
    
    // Try master alias first
    zoneInfo = findZoneForKey(keyCode, 0);
    
    // If not found, try device aliases
    if (!zoneInfo.first && deviceManager) {
      auto aliases = deviceManager->getAllAliasNames();
      for (const auto &aliasName : aliases) {
        uintptr_t aliasHash = aliasNameToHash(aliasName);
        zoneInfo = findZoneForKey(keyCode, aliasHash);
        if (zoneInfo.first) {
          break;
        }
      }
    }
    
    if (zoneInfo.first) {
      // Use cached label from zone (supports both note names and Roman numerals)
      juce::String cachedLabel = zoneInfo.first->getKeyLabel(keyCode);
      if (cachedLabel.isNotEmpty()) {
        labelText = cachedLabel;
      } else {
        // Fallback: calculate note name manually if cache is empty
        uintptr_t aliasHash = zoneInfo.first->targetAliasHash;
        auto action = zoneManager->simulateInput(keyCode, aliasHash);
        
        if (action.has_value() && action->type == ActionType::Note) {
          labelText = MidiNoteUtilities::getMidiNoteName(action->data1);
        }
      }
    }

    // --- 4. Get Buffered Notes (for Strum mode visualization) ---
    // COMMENTED OUT: Strumming indicator disabled for now
    /*
    std::vector<int> bufferedNotes;
    bool isBuffered = false;
    
    if (inputProcessor && zoneInfo.first) {
      // Check if zone is in Strum mode
      if (zoneInfo.first->playMode == Zone::PlayMode::Strum) {
        bufferedNotes = inputProcessor->getBufferedNotes();
        
        if (!bufferedNotes.empty()) {
          // Check if any note in the chord matches this key's note
          // For chords, we need to check all notes in the chord
          uintptr_t aliasHash = zoneInfo.first->targetAliasHash;
          auto action = zoneManager->simulateInput(keyCode, aliasHash);
          if (action.has_value() && action->type == ActionType::Note) {
            int baseNote = action->data1;
            
            // Check if base note is in buffer (for single notes)
            isBuffered = std::find(bufferedNotes.begin(), bufferedNotes.end(), baseNote) != bufferedNotes.end();
            
            // Also check if this key would generate a chord that overlaps with buffer
            // (This handles the case where the key generates a chord)
            if (!isBuffered && zoneInfo.first->chordType != ChordUtilities::ChordType::None) {
              // The key generates a chord - check if any note in the buffer matches any note in the chord
              // For simplicity, we'll just check if the base note is in the buffer
              // In a full implementation, we'd generate the full chord and check overlap
            }
          }
        }
      }
    }
    */
    bool isBuffered = false; // Disabled - always false

    // --- 5. Render Layers ---
    
    // Layer 1: Underlay (Zone Color) - Sharp rectangle
    if (!underlayColor.isTransparent()) {
      g.setColour(underlayColor.withAlpha(0.6f));
      g.fillRect(fullBounds);
    }

    // Layer 1.5: Latched State (drawn after Zone Underlay, before Physical Press)
    bool isLatched = voiceManager.isKeyLatched(keyCode);
    if (isLatched) {
      g.setColour(juce::Colours::cyan.withAlpha(0.8f));
      g.fillRoundedRectangle(keyBounds, 6.0f);
    }

    // Layer 2: Key Body
    juce::Colour bodyColor;
    if (isPressed) {
      bodyColor = juce::Colours::yellow; // Playing
    } else if (isBuffered) {
      bodyColor = juce::Colours::lightblue; // Buffered (waiting)
    } else {
      bodyColor = juce::Colour(0xff333333); // Default
    }
    g.setColour(bodyColor);
    g.fillRoundedRectangle(keyBounds, 6.0f);

    // Layer 3: Border
    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(keyBounds, 6.0f, 2.0f);

    // Layer 4: Text (red if conflict, else black when pressed, white otherwise)
    juce::Colour textColor = isConflict ? juce::Colours::red
        : (isPressed ? juce::Colours::black : juce::Colours::white);
    g.setColour(textColor);
    g.setFont(keySize * 0.4f); // Dynamic font size
    g.drawText(labelText, keyBounds, juce::Justification::centred, false);
  }
}

void VisualizerComponent::resized() {
  repaint();
}

void VisualizerComponent::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) {
  if (isDown) {
    activeKeys.insert(keyCode);
  } else {
    activeKeys.erase(keyCode);
  }
  
  juce::MessageManager::callAsync([this] {
    repaint();
  });
}

void VisualizerComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) {
  // Ignore axis events
}

void VisualizerComponent::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == zoneManager) {
    juce::MessageManager::callAsync([this] {
      repaint();
    });
  }
}

void VisualizerComponent::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
  // Repaint when mappings are added
  auto mappingsNode = presetManager ? presetManager->getMappingsNode() : juce::ValueTree();
  if (parentTree.isEquivalentTo(mappingsNode) || parentTree.getParent().isEquivalentTo(mappingsNode)) {
    // Small delay to ensure InputProcessor has finished updating keyMapping
    juce::MessageManager::callAsync([this] {
      juce::MessageManager::callAsync([this] {
        repaint();
      });
    });
  }
}

void VisualizerComponent::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved) {
  // Repaint when mappings are removed
  auto mappingsNode = presetManager ? presetManager->getMappingsNode() : juce::ValueTree();
  if (parentTree.isEquivalentTo(mappingsNode) || parentTree.getParent().isEquivalentTo(mappingsNode)) {
    // Small delay to ensure InputProcessor has finished updating keyMapping
    juce::MessageManager::callAsync([this] {
      juce::MessageManager::callAsync([this] {
        repaint();
      });
    });
  }
}

void VisualizerComponent::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) {
  // Repaint when mapping properties change (e.g., keyCode, which affects conflict detection)
  auto mappingsNode = presetManager ? presetManager->getMappingsNode() : juce::ValueTree();
  if (treeWhosePropertyHasChanged.getParent().isEquivalentTo(mappingsNode)) {
    // Small delay to ensure InputProcessor has finished updating keyMapping
    juce::MessageManager::callAsync([this] {
      juce::MessageManager::callAsync([this] {
        repaint();
      });
    });
  }
}

bool VisualizerComponent::isKeyInAnyZone(int keyCode, uintptr_t aliasHash) {
  if (!zoneManager)
    return false;

  const auto zones = zoneManager->getZones();
  for (const auto &zone : zones) {
    if (zone->targetAliasHash != aliasHash)
      continue;

    // Check if key is in this zone's inputKeyCodes
    auto it = std::find(zone->inputKeyCodes.begin(), zone->inputKeyCodes.end(), keyCode);
    if (it != zone->inputKeyCodes.end()) {
      return true;
    }
  }

  return false;
}

std::pair<std::shared_ptr<Zone>, bool> VisualizerComponent::findZoneForKey(int keyCode, uintptr_t aliasHash) {
  if (!zoneManager)
    return {nullptr, false};

  const auto zones = zoneManager->getZones();
  for (const auto &zone : zones) {
    if (zone->targetAliasHash != aliasHash)
      continue;

    // Check if key is in this zone's inputKeyCodes
    auto it = std::find(zone->inputKeyCodes.begin(), zone->inputKeyCodes.end(), keyCode);
    if (it != zone->inputKeyCodes.end()) {
      // Calculate the degree (same logic as Zone::processKey)
      int degree = 0;
      
      if (zone->layoutStrategy == Zone::LayoutStrategy::Linear) {
        // Linear mode: Use index
        int index = static_cast<int>(std::distance(zone->inputKeyCodes.begin(), it));
        degree = index;
      } else {
        // Grid mode: Calculate based on keyboard geometry
        const auto& layout = KeyboardLayoutUtils::getLayout();
        
        auto currentKeyIt = layout.find(keyCode);
        if (currentKeyIt == layout.end()) {
          // Fallback to Linear if key not in layout
          int index = static_cast<int>(std::distance(zone->inputKeyCodes.begin(), it));
          degree = index;
        } else {
          // Get anchor key (first key in inputKeyCodes)
          if (zone->inputKeyCodes.empty()) {
            return {nullptr, false};
          }
          
          auto anchorKeyIt = layout.find(zone->inputKeyCodes[0]);
          if (anchorKeyIt == layout.end()) {
            // Fallback to Linear if anchor not in layout
            int index = static_cast<int>(std::distance(zone->inputKeyCodes.begin(), it));
            degree = index;
          } else {
            const auto& currentKey = currentKeyIt->second;
            const auto& anchorKey = anchorKeyIt->second;
            
            int deltaCol = static_cast<int>(currentKey.col) - static_cast<int>(anchorKey.col);
            int deltaRow = currentKey.row - anchorKey.row;
            
            // Degree = deltaCol + (deltaRow * gridInterval)
            degree = deltaCol + (deltaRow * zone->gridInterval);
          }
        }
      }
      
      // Calculate effective transposition
      int effDegTrans = zone->isTransposeLocked ? 0 : zoneManager->getGlobalDegreeTranspose();
      
      // Calculate final degree (same as Zone::processKey)
      int finalDegree = degree + zone->degreeOffset + effDegTrans;
      
      // Check if this is the root (finalDegree == 0)
      bool isRoot = (finalDegree == 0);
      
      return {zone, isRoot};
    }
  }

  return {nullptr, false};
}

void VisualizerComponent::timerCallback() {
  // Repaint to update sustain indicator and latched keys
  repaint();
}
