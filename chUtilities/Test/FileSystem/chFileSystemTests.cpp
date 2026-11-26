/************************************************************************/
/**
 * @file chFileSystemTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of Path, FileSystem and the directory watcher.
 */
/************************************************************************/
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>

#include "chAlgorithm.h"
#include "chDirectoryWatcher.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chPath.h"
#include "chSTDThreading.h"

#include "catch.hpp"

using namespace chEngineSDK;

namespace {
// Path keeps the text it was given, so the reference must keep it too. On Windows
// generic_string() only turns the native separators into '/'; libstdc++ also joins
// repeated separators there, so elsewhere the native text ('/' already) is used.
String
toReferenceText(const std::filesystem::path& path)
{
#if USING(CH_PLATFORM_WIN32)
  return path.generic_string();
#else
  return path.string();
#endif
}

void
writeWatchedFile(const Path& file, const ANSICHAR* text)
{
  SPtr<DataStream> output = FileSystem::createAndOpenFile(file);
  REQUIRE(output);
  REQUIRE(output->write(text, std::strlen(text)) == std::strlen(text));
}

/*
 * The system reports a change a moment after it happens, so this polls for up to a second.
 * changes keeps what every call took, since one poll can bring several changes.
 */
bool
waitForChange(DirectoryWatcher& watcher,
              Vector<FileChange>& changes,
              const ANSICHAR* path,
              const FileChangeType type)
{
  const Path expected(path);
  for (uint32 attempt = 0; attempt < 100; ++attempt) {
    if (!watcher.poll(changes)) {
      return false;
    }
    if (Algorithm::containsIf(changes, [&](const FileChange& change) {
          return change.path == expected && change.type == type;
        })) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}
} // namespace

TEST_CASE("chUtilities - Directory watcher", "[FileSystem]") {
  const Path root("chWatcherTest");
  FileSystem::removeAll(root);
  REQUIRE(FileSystem::createDirectories(root / Path("Sub")));

  DirectoryWatcher watcher;
  REQUIRE(watcher.start(root, true));
  REQUIRE(watcher.isWatching());
  Vector<FileChange> changes;
  REQUIRE(watcher.poll(changes));
  REQUIRE(changes.empty());
  // Takes what the last step still reports, so the next wait only sees its own changes.
  const auto settle = [&watcher, &changes]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(watcher.poll(changes));
    changes.clear();
  };

  writeWatchedFile(root / Path("Sub/a.txt"), "one");
  REQUIRE(waitForChange(watcher, changes, "Sub/a.txt", FileChangeType::Added));
  settle();
  writeWatchedFile(root / Path("Sub/a.txt"), "two");
  REQUIRE(waitForChange(watcher, changes, "Sub/a.txt", FileChangeType::Modified));

  // A rename is the old name removed and the new one added.
  settle();
  REQUIRE(FileSystem::renameFile(root / Path("Sub/a.txt"), root / Path("Sub/b.txt")));
  REQUIRE(waitForChange(watcher, changes, "Sub/a.txt", FileChangeType::Removed));
  REQUIRE(waitForChange(watcher, changes, "Sub/b.txt", FileChangeType::Added));

  // A folder made after the start is watched too, once its own change was taken.
  settle();
  REQUIRE(FileSystem::createDirectory(root / Path("New")));
  REQUIRE(waitForChange(watcher, changes, "New", FileChangeType::Added));
  writeWatchedFile(root / Path("New/c.txt"), "three");
  REQUIRE(waitForChange(watcher, changes, "New/c.txt", FileChangeType::Added));

  settle();
  REQUIRE(FileSystem::removeFile(root / Path("Sub/b.txt")));
  REQUIRE(waitForChange(watcher, changes, "Sub/b.txt", FileChangeType::Removed));

  watcher.stop();
  REQUIRE_FALSE(watcher.isWatching());
  REQUIRE(watcher.poll(changes));
  REQUIRE(FileSystem::removeAll(root));
}

TEST_CASE("chUtilities - Paths outside ASCII", "[FileSystem]") {
  // "Jos\xC3\xA9" is "Jose" with an acute e, written as UTF-8 bytes.
  const Path directory("chStreamTest_Jos\xC3\xA9");
  const Path file = directory / Path("\xC3\xB1" "and\xC3\xBA.txt");
  REQUIRE(FileSystem::createDirectories(directory));
  REQUIRE(FileSystem::isDirectory(directory));
  {
    SPtr<DataStream> output = FileSystem::createAndOpenFile(file);
    REQUIRE(output);
    REQUIRE(output->write("abc", 3) == 3);
  }
  REQUIRE(FileSystem::isFile(file));

  // Checked apart from FileSystem: text read in the wrong code page would still find its
  // own files, but under another name.
  const std::filesystem::path onDisk =
      std::filesystem::path(FileSystem::getBaseDirectory().getPlatformString()) /
      std::filesystem::path(u8"chStreamTest_Jos\u00E9");
  REQUIRE(std::filesystem::is_directory(onDisk));

  SPtr<DataStream> input = FileSystem::openFile(file);
  REQUIRE(input);
  REQUIRE(input->getAsString() == "abc");
  input->close();

  Vector<Path> files;
  Vector<Path> directories;
  FileSystem::getChildren(directory, files, directories);
  REQUIRE(files.size() == 1);
  REQUIRE(files[0].getFileName() == "\xC3\xB1" "and\xC3\xBA.txt");
  REQUIRE(FileSystem::absolutePath(file).toString().find("chStreamTest_Jos\xC3\xA9") !=
          String::npos);

  REQUIRE(FileSystem::removeAll(directory));
  REQUIRE_FALSE(FileSystem::exists(directory));
}

TEST_CASE("chUtilities - Path matches std::filesystem", "[FileSystem]") {
  namespace fs = std::filesystem;
  const Vector<String> cases = {
    "", "/", "//", "///a", "//a", "C:", "C:/", "C://a", "C:/a", "C:a", "c:/a/b.txt", "x:",
    "1:/a", "//server", "//server/", "//server/share/x.y", "//?/C:/x", "//./pipe",
    "a", "a/", "a/b", "a//b", "a/b/", "a/b//", "/a", "/a/", "/a/b.c", ".", "..", "a/..",
    "a/.", "./a", ".bashrc", "dir/.bashrc", ".a.b", "a.b.c", "a.", "/a.b/c", "../x.tar.gz",
    "Assets/Textures/wood.png",
  };

  for (const String& text : cases) {
    INFO("path: \"" << text << "\"");
    const Path path(text);
    const fs::path expected(text);
    CHECK(path.getFileName() == toReferenceText(expected.filename()));
    CHECK(path.getFileName(false) == toReferenceText(expected.stem()));
    CHECK(path.getExtension() == toReferenceText(expected.extension()));
    CHECK(path.getDirectory().toString() == toReferenceText(expected.parent_path()));
    CHECK(path.isRelative() == expected.is_relative());

    for (const String& other : cases) {
      INFO("joined with: \"" << other << "\"");
      const String joined = toReferenceText(expected / other);
      CHECK(path.join(Path(other)).toString() == joined);
      CHECK((path / other).toString() == joined);

      Path inPlace = path;
      inPlace /= Path(other);
      CHECK(inPlace.toString() == joined);
    }
  }

  SECTION("backslashes become separators") {
    REQUIRE(Path("a\\b\\c.txt").toString() == "a/b/c.txt");
    REQUIRE((Path("a") / String("b\\c")).toString() == "a/b/c");
  }

  SECTION("several paths join in order") {
    REQUIRE(Path(Path("a"), Path("b/"), Path("c")).toString() == "a/b/c");
    REQUIRE(Path(Vector<Path>{Path("a"), Path("/b"), Path("c")}).toString() == "/b/c");
    Path self("a");
    self /= self;
    REQUIRE(self.toString() == "a/a");
  }
}

TEST_CASE("chUtilities - FileSystem", "[FileSystem]") {
  SECTION("absolutePath removes dot segments") {
    REQUIRE(FileSystem::absolutePath(Path("a/../b")) == FileSystem::absolutePath(Path("b")));
    REQUIRE(FileSystem::absolutePath(Path("./a/")) == FileSystem::absolutePath(Path("a")));
    REQUIRE_FALSE(FileSystem::absolutePath(Path("a")).isRelative());
  }

  SECTION("isSubPath compares whole folder names") {
    REQUIRE(FileSystem::isSubPath(Path("Assets"), Path("Assets/Textures/a.chAss")));
    REQUIRE(FileSystem::isSubPath(Path("Assets"), Path("Assets")));
    REQUIRE(FileSystem::isSubPath(Path("Assets"), FileSystem::absolutePath(Path("Assets/x"))));
    REQUIRE(FileSystem::isSubPath(Path("Other/../Assets"), Path("Assets/x")));
    REQUIRE_FALSE(FileSystem::isSubPath(Path("Assets"), Path("Assets2/x")));
    REQUIRE_FALSE(FileSystem::isSubPath(Path("Assets"), Path("Assets/../x")));
    REQUIRE_FALSE(FileSystem::isSubPath(Path("Assets/x"), Path("Assets")));
  }

  SECTION("relative paths resolve against the base directory") {
    const Path workingDirectory = FileSystem::getBaseDirectory();
    const Path executableDirectory = FileSystem::getExecutableDirectory();
    REQUIRE_FALSE(executableDirectory.isRelative());
    REQUIRE(FileSystem::isDirectory(executableDirectory));

    FileSystem::setBaseDirectory(executableDirectory);
    REQUIRE(FileSystem::getBaseDirectory() == executableDirectory);
    REQUIRE(FileSystem::absolutePath(Path("Assets/a")) ==
            executableDirectory.join(Path("Assets/a")));
    REQUIRE(FileSystem::toRelativePath(executableDirectory.join(Path("Assets/a"))) ==
            Path("Assets/a"));
    REQUIRE(FileSystem::toRelativePath(executableDirectory.getDirectory()) == Path(".."));

    FileSystem::setBaseDirectory(workingDirectory);
    REQUIRE(FileSystem::getBaseDirectory() == workingDirectory);
  }

  SECTION("mounted directories are layered by priority") {
    const Path root("chMountTestDir");
    FileSystem::removeAll(root);

    auto writeFile = [](const Path& path, uint8 value) {
      SPtr<DataStream> file = FileSystem::createAndOpenFile(path);
      REQUIRE(file != nullptr);
      file->write(&value, 1);
      file->close();
    };
    writeFile(Path("chMountTestDir/Base/shared.bin"), 1);
    writeFile(Path("chMountTestDir/Base/baseOnly.bin"), 2);
    writeFile(Path("chMountTestDir/Mod/shared.bin"), 3);
    writeFile(Path("chMountTestDir/Engine/engine.bin"), 4);

    REQUIRE(FileSystem::mount("TestGame", Path("chMountTestDir/Base"), 0, true));
    REQUIRE(FileSystem::mount("TestGame", Path("chMountTestDir/Mod"), 10));
    REQUIRE(FileSystem::mount("TestEngine", Path("chMountTestDir/Engine")));

    REQUIRE(FileSystem::isVirtual(Path("/TestGame/shared.bin")));
    REQUIRE_FALSE(FileSystem::isVirtual(Path("/NotMounted/shared.bin")));
    REQUIRE_FALSE(FileSystem::isVirtual(Path("/TestGame/../shared.bin")));

    REQUIRE(FileSystem::fastRead(Path("/TestGame/shared.bin"))[0] == 3);
    REQUIRE(FileSystem::fastRead(Path("/TestGame/baseOnly.bin"))[0] == 2);
    REQUIRE(FileSystem::isFile(Path("/TestEngine/engine.bin")));

    writeFile(Path("/TestGame/new.bin"), 5);
    REQUIRE(FileSystem::isFile(Path("chMountTestDir/Base/new.bin")));
    REQUIRE(FileSystem::createAndOpenFile(Path("/TestEngine/x.bin")) == nullptr);

    Vector<Path> files, directories;
    FileSystem::getChildren(Path("/TestGame"), files, directories);
    REQUIRE(files.size() == 3);
    for (const Path& file : files) {
      REQUIRE(FileSystem::isSubPath(Path("/TestGame"), file));
    }

    REQUIRE(FileSystem::toVirtualPath(Path("chMountTestDir/Base/baseOnly.bin")) ==
            Path("/TestGame/baseOnly.bin"));
    REQUIRE(FileSystem::toVirtualPath(Path("chMountTestDir/Other.bin")).empty());
    REQUIRE(FileSystem::isSubPath(Path("/TestGame"), Path("chMountTestDir/Base/x")));
    REQUIRE_FALSE(FileSystem::isSubPath(Path("/TestGame"), Path("/TestEngine/engine.bin")));
    REQUIRE(FileSystem::isSubPath(Path("/TestGame"), Path("/TestGame")));
    REQUIRE_FALSE(FileSystem::isSubPath(Path("/TestGame"), Path("/TestGameX/a")));

    // Text that is not normalized takes the slow path and must give the same answers.
    REQUIRE(FileSystem::isVirtual(Path("/Other/../TestGame/shared.bin")));
    REQUIRE(FileSystem::toVirtualPath(Path("/TestGame/./sub//x.bin/")) ==
            Path("/TestGame/sub/x.bin"));
    REQUIRE(FileSystem::toVirtualPath(Path("/TestGame/sub/x.bin")) ==
            Path("/TestGame/sub/x.bin"));
    REQUIRE(FileSystem::toVirtualPath(Path("chMountTestDir/Mod/../Base/baseOnly.bin")) ==
            Path("/TestGame/baseOnly.bin"));
    REQUIRE(FileSystem::fastRead(Path("/TestGame/sub/../shared.bin"))[0] == 3);

    writeFile(Path("chMountTestDir/Mod/sub/deep.bin"), 6);
    writeFile(Path("chMountTestDir/Base/sub/deep.bin"), 7);
    Vector<Path> recursiveFiles;
    FileSystem::forEachFileChildRecursive(Path("/TestGame"), [&](const Path& file) {
      recursiveFiles.push_back(file);
    });
    // shared.bin and sub/deep.bin are in both layers but listed once.
    REQUIRE(recursiveFiles.size() == 5);
    REQUIRE(std::find(recursiveFiles.begin(), recursiveFiles.end(),
                      Path("/TestGame/sub/deep.bin")) != recursiveFiles.end());
    REQUIRE(FileSystem::fastRead(Path("/TestGame/sub/deep.bin"))[0] == 6);

    // With one layer the path is not checked on disk, so a missing file still resolves
    // to where it would be.
    REQUIRE(FileSystem::absolutePath(Path("/TestEngine/missing.bin")) ==
            FileSystem::absolutePath(Path("chMountTestDir/Engine/missing.bin")));
    Vector<Path> engineFiles;
    FileSystem::forEachFileChildRecursive(Path("/TestEngine"), [&](const Path& file) {
      engineFiles.push_back(file);
    });
    REQUIRE(engineFiles.size() == 1);
    REQUIRE(engineFiles[0] == Path("/TestEngine/engine.bin"));

    REQUIRE(FileSystem::unmount("TestGame", Path("chMountTestDir/Mod")));
    REQUIRE(FileSystem::fastRead(Path("/TestGame/shared.bin"))[0] == 1);
    REQUIRE(FileSystem::fastRead(Path("/TestGame/sub/deep.bin"))[0] == 7);

    REQUIRE(FileSystem::unmount("TestGame", Path("chMountTestDir/Base")));
    REQUIRE(FileSystem::unmount("TestEngine", Path("chMountTestDir/Engine")));
    REQUIRE_FALSE(FileSystem::isVirtual(Path("/TestGame/shared.bin")));
    REQUIRE(FileSystem::removeAll(root));
  }

  SECTION("disk operations report failures instead of throwing") {
    const Path root("chFileSystemTestDir");
    FileSystem::removeAll(root);

    REQUIRE_FALSE(FileSystem::exists(root));
    REQUIRE(FileSystem::fastRead(Path("chFileSystemTestDir/missing.bin")).empty());
    REQUIRE(FileSystem::openFile(Path("chFileSystemTestDir/missing.bin")) == nullptr);
    REQUIRE_FALSE(FileSystem::removeFile(Path("chFileSystemTestDir/missing.bin")));

    SPtr<DataStream> file = FileSystem::createAndOpenFile(Path("chFileSystemTestDir/a/b/f.bin"));
    REQUIRE(file != nullptr);
    const uint8 bytes[3] = {1, 2, 3};
    file->write(bytes, sizeof(bytes));
    file->close();

    const Vector<uint8> readBack = FileSystem::fastRead(Path("chFileSystemTestDir/a/b/f.bin"));
    REQUIRE(readBack.size() == 3);
    REQUIRE(readBack[2] == 3);

    REQUIRE_FALSE(FileSystem::remove(Path("chFileSystemTestDir/a")));

    Vector<Path> files, directories;
    FileSystem::getChildren(Path("chFileSystemTestDir/a"), files, directories);
    REQUIRE(files.empty());
    REQUIRE(directories.size() == 1);

    int32 visited = 0;
    FileSystem::forEachFileChildRecursive(root, [&](const Path&) { ++visited; });
    REQUIRE(visited == 3);

    REQUIRE(FileSystem::renameFile(Path("chFileSystemTestDir/a/b/f.bin"),
                                   Path("chFileSystemTestDir/a/b/g.bin")));
    REQUIRE(FileSystem::isFile(Path("chFileSystemTestDir/a/b/g.bin")));

    REQUIRE(FileSystem::copyFile(Path("chFileSystemTestDir/a/b/g.bin"),
                                 Path("chFileSystemTestDir/copy/deep/g.bin")));
    REQUIRE(FileSystem::fastRead(Path("chFileSystemTestDir/copy/deep/g.bin")) == readBack);
    // A second copy replaces the file instead of failing.
    REQUIRE(FileSystem::copyFile(Path("chFileSystemTestDir/a/b/g.bin"),
                                 Path("chFileSystemTestDir/copy/deep/g.bin")));
    REQUIRE_FALSE(FileSystem::copyFile(Path("chFileSystemTestDir/missing.bin"),
                                       Path("chFileSystemTestDir/copy/missing.bin")));
    REQUIRE_FALSE(FileSystem::copyFile(Path("chFileSystemTestDir/a"),
                                       Path("chFileSystemTestDir/copy/a")));

    // A directory moves with its contents; a taken name or a file is refused.
    REQUIRE(FileSystem::renameDirectory(Path("chFileSystemTestDir/a/b"),
                                        Path("chFileSystemTestDir/copy/b")));
    REQUIRE(FileSystem::isFile(Path("chFileSystemTestDir/copy/b/g.bin")));
    REQUIRE_FALSE(FileSystem::exists(Path("chFileSystemTestDir/a/b")));
    REQUIRE_FALSE(FileSystem::renameDirectory(Path("chFileSystemTestDir/copy/b"),
                                              Path("chFileSystemTestDir/copy/deep")));
    REQUIRE(FileSystem::isFile(Path("chFileSystemTestDir/copy/b/g.bin")));
    REQUIRE_FALSE(FileSystem::renameDirectory(Path("chFileSystemTestDir/copy/b/g.bin"),
                                              Path("chFileSystemTestDir/copy/h.bin")));

    REQUIRE(FileSystem::removeAll(root));
    REQUIRE_FALSE(FileSystem::exists(root));
  }
}
