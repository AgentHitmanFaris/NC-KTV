#include <gtest/gtest.h>
#include "plugins/plugin_manager.h"
#include <QDir>
#include <QFile>

using namespace ncktv;

class PluginManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create dummy plugin directory
        QDir().mkpath("plugins/installed/test_plugin");
        QFile file("plugins/installed/test_plugin/manifest.json");
        if (file.open(QIODevice::WriteOnly)) {
            file.write("{}");
            file.close();
        }
    }

    void TearDown() override {
        QDir("plugins").removeRecursively();
    }
};

TEST_F(PluginManagerTest, UninstallPluginRemovesDirectory) {
    PluginManager manager;

    // Check if directory exists
    ASSERT_TRUE(QDir("plugins/installed/test_plugin").exists());

    bool result = manager.uninstallPlugin("test_plugin");
    // Initially this will fail because uninstallPlugin is not implemented yet
    EXPECT_TRUE(result);

    // Check if directory is removed
    EXPECT_FALSE(QDir("plugins/installed/test_plugin").exists());
}

TEST_F(PluginManagerTest, UninstallNonExistentPlugin) {
    PluginManager manager;
    bool result = manager.uninstallPlugin("non_existent");
    EXPECT_FALSE(result);
}
