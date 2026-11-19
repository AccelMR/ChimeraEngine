/************************************************************************/
/**
 * @file chStreamTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the data streams.
 */
/************************************************************************/
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chPath.h"

#include "catch.hpp"

using namespace chEngineSDK;

TEST_CASE("chUtilities - DataStream getAsString", "[FileSystem]") {
  auto readText = [](Vector<uint8> bytes) {
    MemoryDataStream stream(bytes.data(), bytes.size(), false);
    return stream.getAsString();
  };
  const String expected = "A\xC3\xA9";

  REQUIRE(readText({'A', 0xC3, 0xA9}) == expected);
  REQUIRE(readText({0xEF, 0xBB, 0xBF, 'A', 0xC3, 0xA9}) == expected);
  REQUIRE(readText({0xFF, 0xFE, 'A', 0x00, 0xE9, 0x00}) == expected);
  REQUIRE(readText({0xFE, 0xFF, 0x00, 'A', 0x00, 0xE9}) == expected);
  REQUIRE(readText({0xFF, 0xFE, 0x00, 0x00, 'A', 0, 0, 0, 0xE9, 0, 0, 0}) == expected);
  REQUIRE(readText({0x00, 0x00, 0xFE, 0xFF, 0, 0, 0, 'A', 0, 0, 0, 0xE9}) == expected);
  REQUIRE(readText({0xFF, 0xFE, 'A', 0x00, 0xE9}) == "A");
  REQUIRE(readText({}).empty());
}

TEST_CASE("chUtilities - MemoryDataStream", "[FileSystem]") {
  SECTION("read and write stop at the end") {
    MemoryDataStream stream(4);
    const uint8 source[6] = {1, 2, 3, 4, 5, 6};
    REQUIRE(stream.write(source, sizeof(source)) == 4);
    REQUIRE(stream.write(source, 1) == 0);

    stream.seek(2);
    uint8 target[6] = {};
    REQUIRE(stream.read(target, sizeof(target)) == 2);
    REQUIRE(target[0] == 3);
    REQUIRE(target[1] == 4);
    REQUIRE(target[2] == 0);
    REQUIRE(stream.read(target, sizeof(target)) == 0);
    REQUIRE(stream.isAtEnd());
  }

  SECTION("a stream made without a size grows") {
    MemoryDataStream stream;
    REQUIRE(stream.size() == 0);
    uint32 values[300];
    for (uint32 i = 0; i < 300; ++i) {
      values[i] = i * 3;
    }
    for (const uint32 value : values) {
      REQUIRE(stream.write(&value, sizeof(value)) == sizeof(value));
    }
    REQUIRE(stream.size() == sizeof(values));

    // Writing over the start does not change the size.
    const uint32 first = 77;
    stream.seek(0);
    stream.write(&first, sizeof(first));
    REQUIRE(stream.size() == sizeof(values));
    REQUIRE(stream.tell() == sizeof(first));

    stream.seek(0);
    uint32 readBack[300] = {};
    REQUIRE(stream.read(readBack, sizeof(readBack)) == sizeof(readBack));
    REQUIRE(readBack[0] == 77);
    REQUIRE(readBack[299] == 299 * 3);
    REQUIRE(stream.read(readBack, 1) == 0);
  }

  SECTION("copies keep their data") {
    auto source = chMakeShared<MemoryDataStream>(3);
    const uint8 bytes[3] = {7, 8, 9};
    source->write(bytes, sizeof(bytes));

    SPtr<DataStream> clone = source->clone();
    source->seek(0);
    MemoryDataStream copy{SPtr<DataStream>(source)};
    source->close();

    uint8 fromClone[3] = {};
    REQUIRE(clone->read(fromClone, sizeof(fromClone)) == 3);
    REQUIRE(fromClone[2] == 9);

    uint8 fromCopy[3] = {};
    REQUIRE(copy.size() == 3);
    REQUIRE(copy.read(fromCopy, sizeof(fromCopy)) == 3);
    REQUIRE(fromCopy[0] == 7);
  }

  SECTION("a file stream copies any stream") {
    const Path memoryFile("chStreamTest_memory.bin");
    const Path fileFile("chStreamTest_file.bin");

    auto memory = chMakeShared<MemoryDataStream>(3);
    const uint8 bytes[3] = {'a', 'b', 'c'};
    memory->write(bytes, sizeof(bytes));
    REQUIRE(FileSystem::dumpMemStreamIntoFile(memory, memoryFile));

    SPtr<DataStream> fromMemory = FileSystem::openFile(memoryFile);
    REQUIRE(fromMemory);
    REQUIRE(fromMemory->getAsString() == "abc");

    REQUIRE(FileSystem::dumpMemStreamIntoFile(fromMemory, fileFile));
    fromMemory->close();
    SPtr<DataStream> fromFile = FileSystem::openFile(fileFile);
    REQUIRE(fromFile);
    REQUIRE(fromFile->getAsString() == "abc");
    fromFile->close();

    REQUIRE(FileSystem::remove(memoryFile));
    REQUIRE(FileSystem::remove(fileFile));
  }

  SECTION("a missing file does not throw") {
    REQUIRE_NOTHROW(FileDataStream(Path("chStreamTest_missing.bin")));
    REQUIRE_FALSE(FileDataStream(Path("chStreamTest_missing.bin")).isOpen());
    REQUIRE(FileSystem::openFile(Path("chStreamTest_missing.bin")) == nullptr);
  }

  SECTION("a moved stream keeps the memory and leaves the source empty") {
    MemoryDataStream source(3);
    const uint8 bytes[3] = {4, 5, 6};
    source.write(bytes, sizeof(bytes));

    MemoryDataStream moved(std::move(source));
    REQUIRE(source.size() == 0);
    REQUIRE(source.getStartPtr() == nullptr);
    REQUIRE(moved.size() == 3);

    MemoryDataStream assigned;
    assigned = std::move(moved);
    assigned.seek(0);
    uint8 target[3] = {};
    REQUIRE(assigned.read(target, sizeof(target)) == 3);
    REQUIRE(target[2] == 6);
  }

  SECTION("seek stops at the end") {
    MemoryDataStream stream(4);
    stream.seek(4);
    REQUIRE(stream.tell() == 4);
    REQUIRE(stream.isAtEnd());
    uint8 target = 0;
    REQUIRE(stream.read(&target, 1) == 0);
  }

  SECTION("a closed stream reads and writes nothing") {
    MemoryDataStream stream(4);
    stream.close();
    uint8 value = 1;
    REQUIRE(stream.size() == 0);
    REQUIRE(stream.read(&value, 1) == 0);
    REQUIRE(stream.write(&value, 1) == 0);
  }
}

TEST_CASE("chUtilities - FileDataStream", "[FileSystem]") {
  const Path file("chStreamTest_fileStream.bin");
  {
    SPtr<DataStream> output = FileSystem::createAndOpenFile(file);
    REQUIRE(output);
    const uint8 bytes[4] = {1, 2, 3, 4};
    REQUIRE(output->write(bytes, sizeof(bytes)) == sizeof(bytes));
  }

  SECTION("isAtEnd is true once the last byte is read, before any read fails") {
    SPtr<DataStream> input = FileSystem::openFile(file);
    REQUIRE(input);
    REQUIRE(input->size() == 4);
    REQUIRE_FALSE(input->isAtEnd());
    uint8 target[4] = {};
    REQUIRE(input->read(target, sizeof(target)) == 4);
    REQUIRE(input->isAtEnd());
    REQUIRE(target[3] == 4);
  }

  SECTION("a read only stream writes nothing") {
    SPtr<DataStream> input = FileSystem::openFile(file);
    REQUIRE(input);
    const uint8 value = 9;
    REQUIRE(input->write(&value, 1) == 0);
  }

  SECTION("a read and write stream writes after a read reached the end") {
    SPtr<DataStream> stream = FileSystem::openFile(file, false);
    REQUIRE(stream);
    uint8 target[8] = {};
    REQUIRE(stream->read(target, sizeof(target)) == 4);
    stream->seek(0);
    const uint8 value = 7;
    REQUIRE(stream->write(&value, 1) == 1);
    stream->close();

    SPtr<DataStream> input = FileSystem::openFile(file);
    REQUIRE(input);
    REQUIRE(input->size() == 4);
    REQUIRE(input->read(target, 4) == 4);
    REQUIRE(target[0] == 7);
    REQUIRE(target[1] == 2);
  }

  SECTION("cloning a write stream does not empty the file") {
    SPtr<DataStream> output = FileSystem::openFile(file, false);
    REQUIRE(output);
    SPtr<DataStream> clone = output->clone();
    REQUIRE(clone->size() == 4);
    REQUIRE_FALSE(clone->isWriteable());
  }

  SECTION("a closed stream reads nothing") {
    SPtr<DataStream> input = FileSystem::openFile(file);
    REQUIRE(input);
    input->close();
    uint8 target = 0;
    REQUIRE(input->read(&target, 1) == 0);
    REQUIRE(input->tell() == 0);
    REQUIRE(input->isAtEnd());
  }

  REQUIRE(FileSystem::remove(file));
}
