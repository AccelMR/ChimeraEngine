/************************************************************************/
/**
 * @file chPakArchiveTests.cpp
 * @author AccelMR
 * @date 2026/11/20
 * @brief Tests of pak archives, the mapped file under them and mounting them.
 */
/************************************************************************/
#include "chAlgorithm.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chMappedFile.h"
#include "chPakArchive.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
Span<const uint8>
asBytes(StringView text)
{
  return Span<const uint8>(reinterpret_cast<const uint8*>(text.data()), text.size());
}

String
asText(Span<const uint8> bytes)
{
  return String(reinterpret_cast<const ANSICHAR*>(bytes.data()), bytes.size());
}

void
writeText(const Path& file, StringView text)
{
  SPtr<DataStream> stream = FileSystem::createAndOpenFile(file);
  REQUIRE(stream);
  stream->write(text.data(), text.size());
  stream->close();
}

void
writeBytes(const Path& file, Span<const uint8> bytes)
{
  SPtr<DataStream> stream = FileSystem::createAndOpenFile(file);
  REQUIRE(stream);
  stream->write(bytes.data(), bytes.size());
  stream->close();
}

String
readWhole(const Path& path)
{
  const Vector<uint8> bytes = FileSystem::fastRead(path);
  return asText(bytes);
}
} // namespace

TEST_CASE("chUtilities - Mapped file", "[FileSystem]")
{
  const Path root = FileSystem::absolutePath(Path("MappedFileTestDir"));
  FileSystem::removeAll(root);
  writeText(root.join(Path("Data.txt")), "mapped bytes");
  writeText(root.join(Path("Empty.txt")), "");

  MappedFile file;
  REQUIRE(file.open(root.join(Path("Data.txt"))));
  REQUIRE(file.isOpen());
  REQUIRE(asText(Span<const uint8>(file.getData(), file.getSize())) == "mapped bytes");

  MappedFile moved = std::move(file);
  REQUIRE_FALSE(file.isOpen());
  REQUIRE(moved.getSize() == 12);

  REQUIRE(moved.open(root.join(Path("Empty.txt"))));
  REQUIRE(moved.getSize() == 0);
  REQUIRE_FALSE(moved.open(root.join(Path("Missing.txt"))));
  REQUIRE_FALSE(moved.isOpen());
  moved.close();
  FileSystem::removeAll(root);
}

TEST_CASE("chUtilities - Pak archive", "[FileSystem]")
{
  const Path root = FileSystem::absolutePath(Path("PakArchiveTestDir"));
  FileSystem::removeAll(root);
  const Path pakFile = root.join(Path("Content.chpak"));

  {
    PakWriter writer;
    REQUIRE(writer.begin(pakFile));
    REQUIRE(writer.addEntry("Readme.txt", asBytes("hello")));
    REQUIRE(writer.addEntry("Maps/Main.chAss", asBytes("main scene")));
    REQUIRE(writer.addEntry("Maps/Levels/Ruins.chAss", asBytes("ruins")));
    REQUIRE(writer.addEntry("Empty.dat", {}));
    // Repeats, absolute paths and paths that climb out are refused.
    REQUIRE_FALSE(writer.addEntry("Readme.txt", asBytes("again")));
    REQUIRE_FALSE(writer.addEntry("/Absolute.txt", asBytes("x")));
    REQUIRE_FALSE(writer.addEntry("Maps/../Out.txt", asBytes("x")));
    REQUIRE_FALSE(writer.addEntry("Maps//Double.txt", asBytes("x")));
    REQUIRE_FALSE(writer.addEntry("Maps\\Back.txt", asBytes("x")));
    REQUIRE(writer.finish());
  }

  SECTION("Files and folders read back from the mapping")
  {
    String error;
    const SPtr<PakArchive> archive = PakArchive::open(pakFile, error);
    REQUIRE(archive);
    REQUIRE(archive->getEntryCount() == 4);
    REQUIRE(archive->isFile("Maps/Main.chAss"));
    REQUIRE_FALSE(archive->isFile("Maps"));
    REQUIRE(archive->isDirectory("Maps"));
    REQUIRE(archive->isDirectory("Maps/Levels"));
    REQUIRE(archive->isDirectory(""));
    REQUIRE_FALSE(archive->isDirectory("Readme.txt"));
    REQUIRE(asText(archive->getData("Maps/Levels/Ruins.chAss")) == "ruins");
    REQUIRE(archive->getData("Empty.dat").empty());
    REQUIRE(archive->isFile("Empty.dat"));

    // Every file starts aligned in the archive.
    const PakEntry* entry = archive->findEntry("Maps/Main.chAss");
    REQUIRE(entry);
    REQUIRE(entry->offset % PakArchive::DATA_ALIGNMENT == 0);

    // A stream keeps the archive alive and cannot write into the mapping.
    SPtr<DataStream> stream = archive->openEntry("Readme.txt");
    REQUIRE(stream);
    REQUIRE(stream->size() == 5);
    REQUIRE(stream->write("xx", 2) == 0);
    ANSICHAR text[5] = {};
    REQUIRE(stream->read(text, 5) == 5);
    REQUIRE(StringView(text, 5) == "hello");
    REQUIRE_FALSE(archive->openEntry("Missing.txt"));

    Vector<String> direct;
    archive->forEachEntry("", false, [&](StringView path, bool bDirectory) {
      direct.push_back(String(path) + (bDirectory ? "/" : ""));
    });
    Algorithm::sort(direct);
    REQUIRE(direct == Vector<String>{"Empty.dat", "Maps/", "Readme.txt"});

    Vector<String> underMaps;
    archive->forEachEntry("Maps", true, [&](StringView path, bool bDirectory) {
      underMaps.push_back(String(path) + (bDirectory ? "/" : ""));
    });
    Algorithm::sort(underMaps);
    REQUIRE(underMaps == Vector<String>{"Levels/", "Levels/Ruins.chAss", "Main.chAss"});
  }

  SECTION("Cut, foreign and broken archives are refused")
  {
    const Vector<uint8> bytes = FileSystem::fastRead(pakFile);
    const Path broken = root.join(Path("Broken.chpak"));
    String error;
    for (SIZE_T size = 0; size < bytes.size(); size += 13) {
      writeBytes(broken, Span<const uint8>(bytes.data(), size));
      INFO("cut at " << size);
      REQUIRE_FALSE(PakArchive::open(broken, error));
    }
    Vector<uint8> foreign = bytes;
    foreign[0] = 'X';
    writeBytes(broken, foreign);
    REQUIRE_FALSE(PakArchive::open(broken, error));
    REQUIRE_FALSE(PakArchive::open(root.join(Path("Missing.chpak")), error));
  }

  SECTION("A mounted archive is a layer like a folder")
  {
    REQUIRE(FileSystem::mountArchive("PakTest", pakFile));
    REQUIRE(FileSystem::isFile(Path("/PakTest/Maps/Main.chAss")));
    REQUIRE(FileSystem::isDirectory(Path("/PakTest/Maps")));
    REQUIRE(FileSystem::isDirectory(Path("/PakTest")));
    REQUIRE(FileSystem::exists(Path("/PakTest/Readme.txt")));
    REQUIRE_FALSE(FileSystem::exists(Path("/PakTest/Missing.txt")));
    REQUIRE(readWhole(Path("/PakTest/Maps/Main.chAss")) == "main scene");
    REQUIRE(FileSystem::getLastWriteTime(Path("/PakTest/Readme.txt")) != 0);

    Vector<MountLayer> layers;
    FileSystem::getMountLayers("PakTest", layers);
    REQUIRE(layers.size() == 1);
    REQUIRE(layers[0].archive);

    // A folder mounted over the archive replaces its files and adds its own.
    const Path folder = root.join(Path("Override"));
    writeText(folder.join(Path("Maps/Main.chAss")), "modded scene");
    writeText(folder.join(Path("Maps/Extra.chAss")), "extra");
    REQUIRE(FileSystem::mount("PakTest", folder, 10));
    REQUIRE(readWhole(Path("/PakTest/Maps/Main.chAss")) == "modded scene");
    REQUIRE(readWhole(Path("/PakTest/Maps/Levels/Ruins.chAss")) == "ruins");

    Vector<Path> files;
    Vector<Path> directories;
    FileSystem::getChildren(Path("/PakTest/Maps"), files, directories);
    Vector<String> names;
    for (const Path& file : files) {
      names.push_back(file.toString());
    }
    Algorithm::sort(names);
    REQUIRE(names == Vector<String>{"/PakTest/Maps/Extra.chAss", "/PakTest/Maps/Main.chAss"});
    REQUIRE(directories.size() == 1);
    REQUIRE(directories[0].toString() == "/PakTest/Maps/Levels");

    Vector<String> recursive;
    FileSystem::forEachFileChildRecursive(Path("/PakTest"), [&](const Path& path) {
      if (FileSystem::isFile(path)) {
        recursive.push_back(path.toString());
      }
    });
    REQUIRE(recursive.size() == 5);

    REQUIRE(FileSystem::unmount("PakTest", folder));
    REQUIRE(readWhole(Path("/PakTest/Maps/Main.chAss")) == "main scene");
    REQUIRE(FileSystem::unmount("PakTest", pakFile));
    REQUIRE_FALSE(FileSystem::isVirtual(Path("/PakTest/Readme.txt")));
  }

  FileSystem::removeAll(root);
}
