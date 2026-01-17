#include "MappingEditorComponent.h"
#include "KeyNameUtilities.h"

MappingEditorComponent::MappingEditorComponent(PresetManager &pm,
                                               RawInputManager &rawInputMgr)
    : presetManager(pm), rawInputManager(rawInputMgr) {
  // Setup Headers
  table.getHeader().addColumn("Key", 1, 50);
  table.getHeader().addColumn("Device", 2, 70);
  table.getHeader().addColumn("Type", 3, 60);
  table.getHeader().addColumn("Data1", 4, 50); // Note
  table.getHeader().addColumn("Data2", 5, 50); // Vel
  table.getHeader().addColumn("Ch", 6, 30);

  table.setModel(this);
  addAndMakeVisible(table);

  // Setup Add Button
  addButton.setButtonText("+");
  addButton.onClick = [this] {
    // Create new mapping
    juce::ValueTree newMapping("Mapping");
    newMapping.setProperty("inputKey", 81, nullptr); // Default Q
    newMapping.setProperty("deviceHash", "0", nullptr);
    newMapping.setProperty("type", "Note", nullptr);
    newMapping.setProperty("channel", 1, nullptr);
    newMapping.setProperty("data1", 60, nullptr); // Middle C
    newMapping.setProperty("data2", 127, nullptr);

    // Add to the Mappings node
    auto mappingsNode = presetManager.getMappingsNode();
    if (mappingsNode.isValid()) {
      mappingsNode.addChild(newMapping, -1, nullptr);
      // Force update immediately
      table.updateContent();
      table.repaint();
    } else {
      DBG("Error: Mappings node is invalid!");
    }
  };
  addAndMakeVisible(addButton);

  // Setup Learn Button
  learnButton.setButtonText("Learn");
  learnButton.setClickingTogglesState(true);
  addAndMakeVisible(learnButton);

  // Attach listeners
  presetManager.getRootNode().addListener(this);
  rawInputManager.addListener(this);

  // Initial update
  table.updateContent();
}

MappingEditorComponent::~MappingEditorComponent() {
  presetManager.getRootNode().removeListener(this);
  rawInputManager.removeListener(this);
}

void MappingEditorComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff333333));

  // If empty, draw a hint
  if (getNumRows() == 0) {
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText("No Mappings. Click '+' to add.", getLocalBounds(),
               juce::Justification::centred, true);
  }
}

void MappingEditorComponent::resized() {
  auto area = getLocalBounds();
  auto header = area.removeFromTop(24);
  addButton.setBounds(header.removeFromRight(30));
  header.removeFromRight(4);
  learnButton.setBounds(header.removeFromRight(60));
  table.setBounds(area);
}

int MappingEditorComponent::getNumRows() {
  return presetManager.getMappingsNode().getNumChildren();
}

void MappingEditorComponent::paintRowBackground(juce::Graphics &g,
                                                int rowNumber, int width,
                                                int height,
                                                bool rowIsSelected) {
  if (rowIsSelected)
    g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));
  else if (rowNumber % 2)
    g.fillAll(juce::Colours::white.withAlpha(0.05f));
}

void MappingEditorComponent::paintCell(juce::Graphics &g, int rowNumber,
                                       int columnId, int width, int height,
                                       bool rowIsSelected) {
  auto node = presetManager.getMappingsNode().getChild(rowNumber);
  if (!node.isValid())
    return;

  juce::String text;

  auto getHex = [](const juce::var &v) {
    if (v.isString())
      return v.toString();
    return juce::String::toHexString((juce::int64)v).toUpperCase();
  };

  switch (columnId) {
  case 1:
    // Check for displayName first, otherwise show inputKey
    if (node.hasProperty("displayName")) {
      text = node.getProperty("displayName").toString();
    } else {
      text = node.getProperty("inputKey").toString();
    }
    break;
  case 2:
    text = getHex(node.getProperty("deviceHash"));
    break;
  case 3:
    text = node.getProperty("type").toString();
    break;
  case 4:
    text = node.getProperty("data1").toString();
    break;
  case 5:
    text = node.getProperty("data2").toString();
    break;
  case 6:
    text = node.getProperty("channel").toString();
    break;
  }

  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(text, 2, 0, width - 4, height, juce::Justification::centredLeft,
             true);
}

// --- VALUE TREE LISTENER CALLBACKS (THE FIX) ---
// These ensure the table refreshes when you Load a Preset

void MappingEditorComponent::valueTreeChildAdded(juce::ValueTree &parent,
                                                 juce::ValueTree &child) {
  // Update if a row is added OR if the whole "Mappings" folder is replaced
  // (Load Preset)
  if (parent.hasType("Mappings") || child.hasType("Mappings"))
    table.updateContent();
}

void MappingEditorComponent::valueTreeChildRemoved(juce::ValueTree &parent,
                                                   juce::ValueTree &child,
                                                   int) {
  if (parent.hasType("Mappings") || child.hasType("Mappings"))
    table.updateContent();
}

void MappingEditorComponent::valueTreePropertyChanged(
    juce::ValueTree &, const juce::Identifier &) {
  table.repaint();
}

void MappingEditorComponent::valueTreeParentChanged(juce::ValueTree &child) {
  if (child.hasType("Mappings"))
    table.updateContent();
}

void MappingEditorComponent::handleRawKeyEvent(uintptr_t deviceHandle,
                                                int keyCode, bool isDown) {
  // Check if learn mode is active
  if (!learnButton.getToggleState())
    return;

  // Only learn on key down
  if (!isDown)
    return;

  // Thread safety: wrap UI updates in callAsync
  juce::MessageManager::callAsync([this, deviceHandle, keyCode] {
    // Get selected row
    int selectedRow = table.getSelectedRow();
    if (selectedRow < 0)
      return;

    // Get the ValueTree for the selected row
    auto mappingsNode = presetManager.getMappingsNode();
    if (!mappingsNode.isValid())
      return;

    auto mappingNode = mappingsNode.getChild(selectedRow);
    if (!mappingNode.isValid())
      return;

    // Update the mapping properties
    mappingNode.setProperty("inputKey", keyCode, nullptr);
    mappingNode.setProperty("deviceHash",
                            juce::String::toHexString((juce::int64)deviceHandle)
                                .toUpperCase(),
                            nullptr);

    // Create display name using KeyNameUtilities
    juce::String deviceName =
        KeyNameUtilities::getFriendlyDeviceName(deviceHandle);
    juce::String keyName = KeyNameUtilities::getKeyName(keyCode);
    juce::String displayName = deviceName + " - " + keyName;
    mappingNode.setProperty("displayName", displayName, nullptr);

    // Turn off learn mode
    learnButton.setToggleState(false, juce::dontSendNotification);

    // Refresh the table
    table.repaint();
  });
}