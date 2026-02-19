#include "../SettingsManager.h"
#include <gtest/gtest.h>

namespace {

juce::File makeTempSettingsFile() {
  auto tempDir = juce::File::getSpecialLocation(
      juce::File::tempDirectory).getChildFile("MIDIQyTests");
  tempDir.createDirectory();
  auto file = tempDir.getChildFile("settings-ui-state.xml");
  if (file.existsAsFile())
    file.deleteFile();
  return file;
}

} // namespace

TEST(UiStatePersistenceTest, RememberUiStateDefaultsTrueAndPersists) {
  juce::File file = makeTempSettingsFile();

  // Initial manager: default values
  SettingsManager mgr;
  EXPECT_TRUE(mgr.getRememberUiState());
  mgr.setRememberUiState(false);
  mgr.setMainWindowState("100 100 640 480 0 0");
  mgr.setMainTabIndex(2);
  mgr.setVisualizerVisible(false);
  mgr.setEditorVisible(true);
  mgr.setLogVisible(false);
  mgr.saveToXml(file);

  // New manager: load from disk
  SettingsManager loaded;
  loaded.loadFromXml(file);

  EXPECT_FALSE(loaded.getRememberUiState());
  EXPECT_EQ(loaded.getMainTabIndex(), 2);
  EXPECT_FALSE(loaded.getVisualizerVisible());
  EXPECT_TRUE(loaded.getEditorVisible());
  EXPECT_FALSE(loaded.getLogVisible());
  EXPECT_TRUE(loaded.getMainWindowState().isNotEmpty());
}

