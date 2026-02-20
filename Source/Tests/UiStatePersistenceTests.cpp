#include "../SettingsManager.h"
#include "../ZoneManager.h"
#include "../Zone.h"
#include "../ScaleLibrary.h"
#include "../TouchpadMixerManager.h"
#include "../TouchpadMixerTypes.h"
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

TEST(UiStatePersistenceTest, MainWindowTabAndSelectionsRoundTrip) {
  juce::File file = makeTempSettingsFile();

  SettingsManager mgr;
  mgr.setRememberUiState(true);
  mgr.setMainWindowState("154 84 1228 667");
  mgr.setMainTabIndex(2);
  mgr.setVisualizerVisible(false);
  mgr.setEditorVisible(true);
  mgr.setLogVisible(false);
  mgr.setMappingsSelectedLayerId(3);
  mgr.setMappingsSelectedRow(5);
  mgr.setZonesSelectedIndex(4);
  mgr.setTouchpadSelectedRow(0);

  mgr.saveToXml(file);

  SettingsManager loaded;
  loaded.loadFromXml(file);

  EXPECT_TRUE(loaded.getRememberUiState());
  EXPECT_EQ(loaded.getMainTabIndex(), 2);
  EXPECT_EQ(loaded.getMainWindowState(), "154 84 1228 667");
  EXPECT_FALSE(loaded.getVisualizerVisible());
  EXPECT_TRUE(loaded.getEditorVisible());
  EXPECT_FALSE(loaded.getLogVisible());
  EXPECT_EQ(loaded.getMappingsSelectedLayerId(), 3);
  EXPECT_EQ(loaded.getMappingsSelectedRow(), 5);
  EXPECT_EQ(loaded.getZonesSelectedIndex(), 4);
  EXPECT_EQ(loaded.getTouchpadSelectedRow(), 0);
}

TEST(UiStatePersistenceTest, InvalidIndicesAreSanitizedOnLoad) {
  juce::File file = makeTempSettingsFile();

  juce::ValueTree root("MIDIQySettings");
  root.setProperty("rememberUiState", true, nullptr);

  juce::ValueTree ui("UIState");
  ui.setProperty("mainTabIndex", -1, nullptr);
  ui.setProperty("mappingsSelectedLayerId", 99, nullptr);
  ui.setProperty("mappingsSelectedRow", -5, nullptr);
  ui.setProperty("zonesSelectedIndex", -3, nullptr);
  ui.setProperty("touchpadSelectedRow", -7, nullptr);
  root.addChild(ui, -1, nullptr);

  if (auto xml = root.createXml())
    xml->writeTo(file);

  SettingsManager mgr;
  mgr.loadFromXml(file);

  EXPECT_EQ(mgr.getMainTabIndex(), 0);
  EXPECT_EQ(mgr.getMappingsSelectedLayerId(), 0);
  EXPECT_EQ(mgr.getMappingsSelectedRow(), -1);
  EXPECT_EQ(mgr.getZonesSelectedIndex(), -1);
  EXPECT_EQ(mgr.getTouchpadSelectedRow(), -1);
}

TEST(UiStatePersistenceTest, TouchpadSelectedRowRoundTrip) {
  juce::File file = makeTempSettingsFile();

  SettingsManager mgr;
  mgr.setRememberUiState(true);
  mgr.setTouchpadSelectedRow(0);
  mgr.saveToXml(file);

  SettingsManager loaded;
  loaded.loadFromXml(file);

  EXPECT_EQ(loaded.getTouchpadSelectedRow(), 0);
}

TEST(UiStatePersistenceTest, TouchpadSelectedRowLegacyMinusOneDefaultsToNone) {
  juce::File file = makeTempSettingsFile();

  juce::ValueTree root("MIDIQySettings");
  root.setProperty("rememberUiState", true, nullptr);

  juce::ValueTree ui("UIState");
  ui.setProperty("touchpadSelectedRow", -1, nullptr);
  root.addChild(ui, -1, nullptr);

  if (auto xml = root.createXml())
    xml->writeTo(file);

  SettingsManager mgr;
  mgr.loadFromXml(file);

  EXPECT_EQ(mgr.getTouchpadSelectedRow(), -1);
}

// Test that Zones selection persist-on-change works
TEST(UiStatePersistenceTest, ZonesSelectionPersistsOnChange) {
  ScaleLibrary scaleLib;
  ZoneManager zoneMgr(scaleLib);
  SettingsManager settingsMgr;
  settingsMgr.setRememberUiState(true);
  
  // Create a test zone
  auto zone1 = zoneMgr.createDefaultZone();
  zone1->name = "Test Zone 1";
  zoneMgr.addZone(zone1);
  
  auto zone2 = zoneMgr.createDefaultZone();
  zone2->name = "Test Zone 2";
  zoneMgr.addZone(zone2);
  
  // Simulate selection change by directly setting in SettingsManager
  // (This tests that SettingsManager persistence works)
  settingsMgr.setZonesSelectedIndex(0);
  EXPECT_EQ(settingsMgr.getZonesSelectedIndex(), 0);
  
  settingsMgr.setZonesSelectedIndex(1);
  EXPECT_EQ(settingsMgr.getZonesSelectedIndex(), 1);
  
  // Verify it persists through save/load
  juce::File file = makeTempSettingsFile();
  settingsMgr.saveToXml(file);
  
  SettingsManager loaded;
  loaded.loadFromXml(file);
  EXPECT_EQ(loaded.getZonesSelectedIndex(), 1);
}

// Test that Touchpad selection persist-on-change works
TEST(UiStatePersistenceTest, TouchpadSelectionPersistsOnChange) {
  SettingsManager settingsMgr;
  settingsMgr.setRememberUiState(true);
  
  // Simulate selection change
  settingsMgr.setTouchpadSelectedRow(0);
  EXPECT_EQ(settingsMgr.getTouchpadSelectedRow(), 0);
  
  settingsMgr.setTouchpadSelectedRow(2);
  EXPECT_EQ(settingsMgr.getTouchpadSelectedRow(), 2);
  
  // Verify it persists through save/load
  juce::File file = makeTempSettingsFile();
  settingsMgr.saveToXml(file);
  
  SettingsManager loaded;
  loaded.loadFromXml(file);
  EXPECT_EQ(loaded.getTouchpadSelectedRow(), 2);
}

// Test that negative selection indices are handled correctly
TEST(UiStatePersistenceTest, NegativeSelectionIndicesAreNormalized) {
  SettingsManager mgr;
  mgr.setRememberUiState(true);
  
  // Set valid indices
  mgr.setZonesSelectedIndex(0);
  mgr.setTouchpadSelectedRow(1);
  
  // Try to set negative (should be normalized to -1)
  mgr.setZonesSelectedIndex(-5);
  EXPECT_EQ(mgr.getZonesSelectedIndex(), -1);
  
  mgr.setTouchpadSelectedRow(-3);
  EXPECT_EQ(mgr.getTouchpadSelectedRow(), -1);
}

