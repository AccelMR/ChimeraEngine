/************************************************************************/
/**
 * @file chAssetFileTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the asset file start and the editor data inside it.
 */
/************************************************************************/
#include "chAssetFile.h"
#include "chAssetMeta.h"
#include "chConfigFile.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chGraphicsTypes.h"
#include "chMaterial.h"
#include "chMaterialAsset.h"
#include "chMesh.h"
#include "chModel.h"
#include "chModelExpand.h"
#include "chSkeleton.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chCore - Asset file start", "[Assets]")
{
  const chEngineSDK::UUID uuid = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID assetType = AssetTypeTraits<MaterialAsset>::getTypeId();
  const chEngineSDK::UUID references[] = {chEngineSDK::UUID::createRandom(),
                                          chEngineSDK::UUID::createRandom()};
  const String editorText = "[Asset]\nCreated=1\n";

  MemoryDataStream stream(4096);
  AssetFile::writeStart(stream, uuid, assetType, references, editorText);
  // Only the identity follows the header; the name and folder come from the file's place.
  const SIZE_T referenceCountOffset = sizeof(AssetFileHeader) + 2 * sizeof(chEngineSDK::UUID);
  const SIZE_T editorDataOffset = referenceCountOffset + sizeof(uint32) + sizeof(references);
  REQUIRE(stream.tell() == editorDataOffset + sizeof(uint32) + editorText.size());
  AssetFile::writeString(stream, "Slot");
  stream.seek(0);

  AssetFileInfo info;
  Vector<chEngineSDK::UUID> readReferences;
  REQUIRE(AssetFile::readInfo(stream, info));
  REQUIRE(info.formatVersion == AssetFileHeader::FORMAT_VERSION);
  REQUIRE(info.uuid == uuid);
  REQUIRE(info.assetType == assetType);
  REQUIRE(AssetFile::readReferences(stream, readReferences));
  REQUIRE(readReferences.size() == 2);
  REQUIRE(readReferences[0] == references[0]);
  REQUIRE(readReferences[1] == references[1]);
  String editorData;
  REQUIRE(AssetFile::readEditorData(stream, &editorData));
  REQUIRE(editorData == editorText);
  String text;
  REQUIRE(AssetFile::readString(stream, text, 64));
  REQUIRE(text == "Slot");

  SECTION("The game moves past the editor data without reading it")
  {
    stream.seek(editorDataOffset);
    REQUIRE(AssetFile::readEditorData(stream, nullptr));
    REQUIRE(AssetFile::readString(stream, text, 64));
    REQUIRE(text == "Slot");
  }

  SECTION("A file without the magic number is rejected")
  {
    MemoryDataStream other(64);
    const uint32 junk[2] = {0x12345678, AssetFileHeader::FORMAT_VERSION};
    other.write(junk, sizeof(junk));
    other.seek(0);
    REQUIRE_FALSE(AssetFile::readInfo(other, info));
  }

  SECTION("Other format versions are rejected")
  {
    for (const uint32 version : {AssetFileHeader::FORMAT_VERSION - 1,
                                 AssetFileHeader::FORMAT_VERSION + 1}) {
      MemoryDataStream other(64);
      const uint32 header[2] = {AssetFileHeader::MAGIC, version};
      other.write(header, sizeof(header));
      other.seek(0);
      REQUIRE_FALSE(AssetFile::readInfo(other, info));
    }
  }

  SECTION("Broken lengths are rejected")
  {
    stream.seek(referenceCountOffset);
    const uint32 tooMany = AssetFile::MAX_REFERENCES + 1;
    stream.write(&tooMany, sizeof(tooMany));
    stream.seek(referenceCountOffset);
    REQUIRE_FALSE(AssetFile::readReferences(stream, readReferences));
    REQUIRE(readReferences.empty());

    stream.seek(editorDataOffset);
    const uint32 tooLong = AssetFile::MAX_EDITOR_DATA_SIZE + 1;
    stream.write(&tooLong, sizeof(tooLong));
    stream.seek(editorDataOffset);
    REQUIRE_FALSE(AssetFile::readEditorData(stream, &editorData));
  }

  SECTION("Rewriting replaces only the editor data")
  {
    const Path root = FileSystem::absolutePath(Path("AssetFileTestDir"));
    FileSystem::removeAll(root);
    const Path source = root.join(Path("Rock.chAss"));
    const Path stripped = root.join(Path("Out/Rock.chAss"));
    SPtr<DataStream> file = FileSystem::createAndOpenFile(source);
    REQUIRE(file);
    stream.seek(0);
    Vector<uint8> bytes(stream.size());
    REQUIRE(stream.read(bytes.data(), bytes.size()) == bytes.size());
    file->write(bytes.data(), bytes.size());
    file->close();

    REQUIRE(AssetFile::rewriteWithEditorData(source, stripped, StringView()));
    SPtr<DataStream> result = FileSystem::openFile(stripped);
    REQUIRE(result);
    REQUIRE(AssetFile::readInfo(*result, info));
    REQUIRE(info.uuid == uuid);
    REQUIRE(AssetFile::readReferences(*result, readReferences));
    REQUIRE(readReferences.size() == 2);
    REQUIRE(AssetFile::readEditorData(*result, &editorData));
    REQUIRE(editorData.empty());
    REQUIRE(AssetFile::readString(*result, text, 64));
    REQUIRE(text == "Slot");
    result->close();

    // In place, as the editor would.
    REQUIRE(AssetFile::rewriteWithEditorData(stripped, stripped, "[Asset]\nCreated=2\n"));
    result = FileSystem::openFile(stripped);
    REQUIRE(AssetFile::readInfo(*result, info));
    REQUIRE(AssetFile::readReferences(*result, readReferences));
    REQUIRE(AssetFile::readEditorData(*result, &editorData));
    REQUIRE(editorData == "[Asset]\nCreated=2\n");
    REQUIRE(AssetFile::readString(*result, text, 64));
    REQUIRE(text == "Slot");
    result->close();
    FileSystem::removeAll(root);
  }
}

#if USING(CH_EDITOR)
TEST_CASE("chCore - Asset editor data", "[Assets]")
{
  const Path project = FileSystem::absolutePath(Path("MetaTestProject"));
  ImportSettings settings;
  settings.srgb = false;
  settings.expandMode = ModelExpandMode::PerMesh;
  settings.splitParts = true;
  settings.autoAlphaMask = false;
  settings.importModel = false;
  const AssetEditorData data{.creationTime = 1760000000,
                             .engineVersion = "1.2.3",
                             .sourceFile = project.join(Path("../Art/Sponza.gltf")),
                             .sourcePart = "Material #1",
                             .importSettings = settings};

  ConfigFile file;
  AssetMeta::write(data, project, file);
  // Relative to the project, so the project can move together with its sources.
  REQUIRE(file.getValue("Import", "Source"));
  REQUIRE(*file.getValue("Import", "Source") == "../Art/Sponza.gltf");
  REQUIRE(*file.getValue("Import", "ExpandMode") == "PerMesh");

  ConfigFile parsed;
  parsed.parse(file.toText());
  AssetEditorData read;
  AssetMeta::read(parsed, project, read);
  REQUIRE(read.creationTime == data.creationTime);
  REQUIRE(read.engineVersion == "1.2.3");
  REQUIRE(read.sourceFile == FileSystem::absolutePath(data.sourceFile));
  REQUIRE(read.sourcePart == "Material #1");
  REQUIRE_FALSE(read.importSettings.srgb);
  REQUIRE(read.importSettings.generateMips);
  REQUIRE(read.importSettings.expandMode == ModelExpandMode::PerMesh);
  REQUIRE(read.importSettings.splitParts);
  REQUIRE_FALSE(read.importSettings.autoAlphaMask);
  REQUIRE_FALSE(read.importSettings.importModel);

  SECTION("Skeleton modes read back")
  {
    for (const ImportSkeletonMode mode :
         {ImportSkeletonMode::Auto, ImportSkeletonMode::CreateNew, ImportSkeletonMode::None}) {
      AssetEditorData withMode = data;
      withMode.importSettings.skeletonMode = mode;
      ConfigFile written;
      AssetMeta::write(withMode, project, written);
      ConfigFile reparsed;
      reparsed.parse(written.toText());
      AssetEditorData modeRead;
      AssetMeta::read(reparsed, project, modeRead);
      REQUIRE(modeRead.importSettings.skeletonMode == mode);
    }
  }

  SECTION("Missing and broken values keep the defaults")
  {
    ConfigFile broken;
    broken.parse("[Import]\nExpandMode=Sideways\nSplitParts=maybe\n");
    AssetEditorData defaults;
    AssetMeta::read(broken, project, defaults);
    REQUIRE(defaults.creationTime == 0);
    REQUIRE(defaults.sourceFile.empty());
    REQUIRE(defaults.importSettings.expandMode == ModelExpandMode::Single);
    REQUIRE_FALSE(defaults.importSettings.splitParts);
  }
}

#endif
