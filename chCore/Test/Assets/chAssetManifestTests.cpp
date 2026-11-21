/************************************************************************/
/**
 * @file chAssetManifestTests.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief Tests of the asset manifest packaged games read instead of scanning.
 */
/************************************************************************/
#include "chPrerequisitesCore.h"

#if USING(CH_EDITOR)

#include "chAssetManifest.h"
#include "chFileStream.h"
#include "chFileSystem.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chCore - Asset manifest", "[Assets]")
{
  const Path root = FileSystem::absolutePath(Path("AssetManifestTestDir"));
  FileSystem::removeAll(root);
  const Path file = root.join(Path(String(AssetManifest::FILE_NAME)));

  AssetManifest manifest;
  manifest.gameName = "MyGame";
  manifest.engineVersion = "1.2.3";
  const auto scene = chEngineSDK::UUID::createRandom();
  const auto texture = chEngineSDK::UUID::createRandom();
  const auto script = chEngineSDK::UUID::createRandom();
  const auto sceneType = chEngineSDK::UUID::createRandom();
  manifest.entries.push_back({.path = "Maps/Main.chAss",
                              .uuid = scene,
                              .assetType = sceneType,
                              .references = {texture, script}});
  manifest.entries.push_back({.path = "Textures/T_Wall.chAss",
                              .uuid = texture,
                              .assetType = chEngineSDK::UUID::createRandom(),
                              .references = {}});
  manifest.entries.push_back({.path = "Scripts/Player.lua",
                              .uuid = script,
                              .assetType = chEngineSDK::UUID::createRandom(),
                              .references = {}});
  REQUIRE(manifest.write(file));

  SECTION("What is written reads back")
  {
    AssetManifest read;
    String error;
    REQUIRE(read.read(file, error));
    REQUIRE(read.gameName == "MyGame");
    REQUIRE(read.engineVersion == "1.2.3");
    REQUIRE(read.entries.size() == 3);
    REQUIRE(read.entries[0].path == "Maps/Main.chAss");
    REQUIRE(read.entries[0].uuid == scene);
    REQUIRE(read.entries[0].assetType == sceneType);
    REQUIRE(read.entries[0].references == Vector<chEngineSDK::UUID>{texture, script});
    REQUIRE(read.entries[1].references.empty());
    REQUIRE(read.entries[2].path == "Scripts/Player.lua");
  }

  SECTION("Missing, cut, foreign and newer files are refused")
  {
    AssetManifest read;
    String error;
    REQUIRE_FALSE(read.read(root.join(Path("Missing.chman")), error));
    REQUIRE_FALSE(error.empty());

    Vector<uint8> bytes = FileSystem::fastRead(file);
    REQUIRE(bytes.size() > 40);
    auto writeBytes = [&file](const Vector<uint8>& data) {
      SPtr<DataStream> stream = FileSystem::createAndOpenFile(file);
      REQUIRE(stream);
      stream->write(data.data(), data.size());
      stream->close();
    };

    // Every cut, whether it falls in the header, a string or a list, fails cleanly.
    for (SIZE_T size = 1; size < bytes.size(); size += 7) {
      writeBytes(Vector<uint8>(bytes.begin(), bytes.begin() + static_cast<int64>(size)));
      INFO("cut at " << size);
      REQUIRE_FALSE(read.read(file, error));
      REQUIRE(read.entries.empty());
    }

    Vector<uint8> newer = bytes;
    newer[4] = static_cast<uint8>(AssetManifest::VERSION + 1);
    writeBytes(newer);
    REQUIRE_FALSE(read.read(file, error));

    Vector<uint8> foreign = bytes;
    foreign[0] = 'X';
    writeBytes(foreign);
    REQUIRE_FALSE(read.read(file, error));

    // An entry count far past what the bytes can hold allocates nothing.
    Vector<uint8> huge = bytes;
    huge[8] = 0xFF;
    huge[9] = 0xFF;
    huge[10] = 0x0F;
    writeBytes(huge);
    REQUIRE_FALSE(read.read(file, error));
  }

  FileSystem::removeAll(root);
}

#endif // USING(CH_EDITOR)
