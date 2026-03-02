#include "../DeviceManager.h"
#include <gtest/gtest.h>

TEST(DeviceManagerAliasHash, EmptyAndSpecialNamesReturnZero) {
  EXPECT_EQ(DeviceManager::getAliasHash(""), 0u);
  EXPECT_EQ(DeviceManager::getAliasHash("   "), 0u);
  EXPECT_EQ(DeviceManager::getAliasHash("Any / Master"), 0u);
  EXPECT_EQ(DeviceManager::getAliasHash("any / master"), 0u);
  EXPECT_EQ(DeviceManager::getAliasHash("Global (All Devices)"), 0u);
  EXPECT_EQ(DeviceManager::getAliasHash("global (all devices)"), 0u);
  EXPECT_EQ(DeviceManager::getAliasHash("Global"), 0u);
  EXPECT_EQ(DeviceManager::getAliasHash("Unassigned"), 0u);
}

TEST(DeviceManagerAliasHash, NormalNamesReturnNonZeroAndStable) {
  auto h1 = DeviceManager::getAliasHash("My Keyboard");
  auto h2 = DeviceManager::getAliasHash("My Keyboard");
  auto h3 = DeviceManager::getAliasHash("Other Device");

  EXPECT_NE(h1, 0u);
  EXPECT_EQ(h1, h2);
  EXPECT_NE(h1, h3);
}

