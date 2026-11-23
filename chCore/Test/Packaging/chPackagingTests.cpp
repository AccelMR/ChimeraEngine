/************************************************************************/
/**
 * @file chPackagingTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of the project descriptor and of packaging a project.
 */
/************************************************************************/
#include "chAlgorithm.h"
#include "chAssetFile.h"
#include "chAssetMeta.h"
#include "chConfigFile.h"
#include "chDynamicLibrary.h"
#include "chFileStream.h"
#include "chFileSystem.h"
#include "chGraphicsTypes.h"
#include "chModel.h"
#include "chPrefabAsset.h"
#include "chProjectDescriptor.h"
#include "chProjectPackager.h"
#include "chScene.h"
#include "chSceneAsset.h"
#include "chStringUtils.h"
#include "chUUID.h"

#include "catch.hpp"

using namespace chEngineSDK;

#if USING(CH_EDITOR)
namespace {
void
writeTestFile(const Path& path, StringView text)
{
  SPtr<DataStream> stream = FileSystem::createAndOpenFile(path);
  REQUIRE(stream);
  stream->write(text.data(), text.size());
  stream->close();
}

// An asset file start with editorData, then data as the asset's own bytes.
void
writeTestAsset(const Path& path, StringView editorData, StringView data)
{
  SPtr<DataStream> stream = FileSystem::createAndOpenFile(path);
  REQUIRE(stream);
  AssetFile::writeStart(*stream, chEngineSDK::UUID::createRandom(),
                        AssetTypeTraits<SceneAsset>::getTypeId(), {}, editorData);
  stream->write(data.data(), data.size());
  stream->close();
}
} // namespace

TEST_CASE("chCore - Project descriptor", "[Packaging]")
{
  const Path root = FileSystem::absolutePath(Path("ProjectDescriptorTestDir"));
  FileSystem::removeAll(root);

  SECTION("An empty file is a project named after the file")
  {
    const Path file = root.join(Path("Old/Old.chproject"));
    writeTestFile(file, "");
    ProjectDescriptor descriptor;
    REQUIRE(descriptor.load(file));
    REQUIRE(descriptor.name == "Old");
    REQUIRE(descriptor.engineVersion.empty());
    REQUIRE(descriptor.gameModules.empty());
    REQUIRE(descriptor.plugins.empty());
    REQUIRE(ProjectDescriptor::findProjectFile(root.join(Path("Old"))) == file);
    REQUIRE(ProjectDescriptor::findProjectFile(root).empty());
  }

  SECTION("Saved values read back")
  {
    ProjectDescriptor descriptor;
    descriptor.name = "My Game";
    descriptor.engineVersion = "1.2.3.4";
    descriptor.description = "A test";
    descriptor.gameModules = {"GameCode", "Tools"};
    descriptor.plugins = {"chProjectiles"};
    const Path file = root.join(Path("Saved/Saved.chproject"));
    REQUIRE(descriptor.save(file));

    ProjectDescriptor loaded;
    REQUIRE(loaded.load(file));
    REQUIRE(loaded.name == "My Game");
    REQUIRE(loaded.engineVersion == "1.2.3.4");
    REQUIRE(loaded.description == "A test");
    REQUIRE(loaded.gameModules == Vector<String>{"GameCode", "Tools"});
    REQUIRE(loaded.plugins == Vector<String>{"chProjectiles"});

    writeTestFile(file, "[Modules]\nGameModules= A , ,B\n[Plugins]\nEnabled=P1, P2\n");
    REQUIRE(loaded.load(file));
    REQUIRE(loaded.name == "Saved");
    REQUIRE(loaded.gameModules == Vector<String>{"A", "B"});
    REQUIRE(loaded.plugins == Vector<String>{"P1", "P2"});
  }

  SECTION("Create makes the folders and the file")
  {
    Path projectFile;
    String error;
    REQUIRE(ProjectDescriptor::create(root, "NewGame", projectFile, error));
    REQUIRE(projectFile == root.join(Path("NewGame/NewGame.chproject")));
    REQUIRE(FileSystem::isDirectory(root.join(Path("NewGame/Assets"))));
    REQUIRE(FileSystem::isDirectory(root.join(Path("NewGame/Config"))));
    ProjectDescriptor descriptor;
    REQUIRE(descriptor.load(projectFile));
    REQUIRE(descriptor.name == "NewGame");
    REQUIRE(descriptor.engineVersion == CH_ENGINE_VERSION_STRING);

    // The folder is no longer empty.
    REQUIRE_FALSE(ProjectDescriptor::create(root, "NewGame", projectFile, error));
    REQUIRE_FALSE(error.empty());
    REQUIRE_FALSE(ProjectDescriptor::create(root, "Bad/Name", projectFile, error));
    REQUIRE_FALSE(ProjectDescriptor::create(Path(), "Fine", projectFile, error));
  }

  SECTION("Names")
  {
    REQUIRE(ProjectDescriptor::isValidName("My Game_2"));
    REQUIRE_FALSE(ProjectDescriptor::isValidName(""));
    REQUIRE_FALSE(ProjectDescriptor::isValidName(".."));
    REQUIRE_FALSE(ProjectDescriptor::isValidName("Game."));
    REQUIRE_FALSE(ProjectDescriptor::isValidName("a:b"));
  }

  FileSystem::removeAll(root);
}

#endif
#if USING(CH_EDITOR)
TEST_CASE("chCore - Project packager", "[Packaging]")
{
  const Path root = FileSystem::absolutePath(Path("PackagerTestDir"));
  FileSystem::removeAll(root);

  const Path binaries = root.join(Path("Build"));
  String executable(ProjectPackager::GAME_EXECUTABLE_NAME);
  executable += ProjectPackager::EXECUTABLE_EXTENSION;
  String library("chCore");
  library += DynamicLibrary::EXTENSION;
  writeTestFile(binaries.join(Path(executable)), "exe");
  writeTestFile(binaries.join(Path(library)), "lib");
  writeTestFile(binaries.join(Path("chGame.pdb")), "symbols");
  writeTestFile(binaries.join(Path("Codecs/chImageCodec.bin")), "codec");
  writeTestFile(binaries.join(Path("Shaders/SPIRV/forward.ps.spv")), "spirv");

  const Path engine = root.join(Path("EngineContent"));
  writeTestFile(engine.join(Path("Config/Engine.ini")), "[Window]\nWidth=640\n");
  writeTestFile(engine.join(Path("Shaders/forward.hlsl")), "source");
  writeTestFile(engine.join(Path("PostProcess/Vignette.hlsl")), "source");

  const Path project = root.join(Path("Project"));
  writeTestFile(project.join(Path("Project.chproject")), "[Modules]\nGameModules=GameCode\n");
  writeTestFile(project.join(Path("Config/Game.ini")), "[Game]\nTitle=Test\n");
  writeTestFile(project.join(Path("Config/Editor.ini")), "[Editor]\nGrid=false\n");
  writeTestFile(project.join(Path("Config/Package.ini")), "[Package]\nScenes=/Game/Maps/Main\n");
  const Path mainScene = project.join(Path("Assets/Maps/Main.chAss"));
  writeTestAsset(mainScene, "[Import]\nSource=C:/Users/Someone/Art/Main.fbx\n", "scene");
  writeTestFile(project.join(Path("Assets/Unused.chAss")), "unused");

  const Path output = root.join(Path("Out/MyGame"));
  PackageDesc desc{.gameName = "MyGame",
                   .outputDirectory = output,
                   .binaryDirectory = binaries,
                   .engineContentDirectory = engine,
                   .projectDirectory = project,
                   .assetFiles = {mainScene},
                   .startupScene = "/Game/Maps/Main"};

  SECTION("The package holds what a game reads and nothing else")
  {
    const PackageResult result = ProjectPackager::package(desc);
    REQUIRE(result.bSucceeded);
    REQUIRE(result.binaryFileCount == 3);
    REQUIRE(result.assetFileCount == 1);

    String packagedExecutable("MyGame");
    packagedExecutable += ProjectPackager::EXECUTABLE_EXTENSION;
    REQUIRE(FileSystem::isFile(output.join(Path(packagedExecutable))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path(executable))));
    REQUIRE(FileSystem::isFile(output.join(Path(library))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("chGame.pdb"))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("Codecs"))));
    REQUIRE(FileSystem::isFile(output.join(Path("Shaders/SPIRV/forward.ps.spv"))));
    REQUIRE(FileSystem::isFile(output.join(Path("Engine/Config/Engine.ini"))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("Engine/Shaders"))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("Engine/PostProcess"))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("Config/Editor.ini"))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("Config/Package.ini"))));
    REQUIRE(FileSystem::isFile(output.join(Path("Assets/Maps/Main.chAss"))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("Assets/Unused.chAss"))));
    // The editor data (paths of this machine) stays out of the package; the data is kept.
    SPtr<DataStream> packagedScene =
        FileSystem::openFile(output.join(Path("Assets/Maps/Main.chAss")));
    REQUIRE(packagedScene);
    AssetFileInfo sceneInfo;
    Vector<chEngineSDK::UUID> sceneReferences;
    String sceneEditorData;
    REQUIRE(AssetFile::readInfo(*packagedScene, sceneInfo));
    REQUIRE(AssetFile::readReferences(*packagedScene, sceneReferences));
    REQUIRE(AssetFile::readEditorData(*packagedScene, &sceneEditorData));
    REQUIRE(sceneEditorData.empty());
    ANSICHAR sceneBytes[5] = {};
    REQUIRE(packagedScene->read(sceneBytes, sizeof(sceneBytes)) == sizeof(sceneBytes));
    REQUIRE(StringView(sceneBytes, sizeof(sceneBytes)) == "scene");
    packagedScene->close();
    REQUIRE(FileSystem::isFile(output.join(Path("MyGame.chproject"))));
    ProjectDescriptor packagedProject;
    REQUIRE(packagedProject.load(output.join(Path("MyGame.chproject"))));
    REQUIRE(packagedProject.name == "MyGame");
    REQUIRE(packagedProject.engineVersion == CH_ENGINE_VERSION_STRING);
    REQUIRE(packagedProject.gameModules == Vector<String>{"GameCode"});

    ConfigFile gameConfig;
    REQUIRE(gameConfig.load(output.join(Path("Config/Game.ini"))));
    REQUIRE(gameConfig.getValue("Game", "StartupScene"));
    REQUIRE(*gameConfig.getValue("Game", "StartupScene") == "/Game/Maps/Main");
    REQUIRE(*gameConfig.getValue("Game", "Title") == "Test");
    REQUIRE(gameConfig.getValue("Window", "Title"));
    REQUIRE(*gameConfig.getValue("Window", "Title") == "MyGame");

    // A window title the project set itself is kept.
    writeTestFile(project.join(Path("Config/Engine.ini")), "[Window]\nTitle=Mine\n");
    REQUIRE(ProjectPackager::package(desc).bSucceeded);
    ConfigFile secondConfig;
    REQUIRE(secondConfig.load(output.join(Path("Config/Game.ini"))));
    REQUIRE_FALSE(secondConfig.getValue("Window", "Title"));

    // A second package replaces the first as a whole.
    writeTestFile(output.join(Path("Stale.txt")), "old");
    REQUIRE(ProjectPackager::package(desc).bSucceeded);
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("Stale.txt"))));
  }

  SECTION("Progress counts every file and a cancel removes the output")
  {
    PackageProgress progress;
    REQUIRE(ProjectPackager::package(desc, &progress).bSucceeded);
    // Executable, library, shader, asset, Engine.ini and Game.ini.
    REQUIRE(progress.totalFiles.load() == 6);
    REQUIRE(progress.copiedFiles.load() == 6);

    PackageProgress cancelled;
    cancelled.bCancelRequested.store(true);
    const PackageResult result = ProjectPackager::package(desc, &cancelled);
    REQUIRE_FALSE(result.bSucceeded);
    REQUIRE(result.bCancelled);
    REQUIRE_FALSE(FileSystem::exists(output));
  }

  SECTION("Bad requests are refused before anything is copied")
  {
    PackageDesc otherFolder = desc;
    otherFolder.outputDirectory = root.join(Path("NotAPackage"));
    writeTestFile(otherFolder.outputDirectory.join(Path("Keep.txt")), "mine");
    REQUIRE_FALSE(ProjectPackager::package(otherFolder).bSucceeded);
    REQUIRE(FileSystem::isFile(otherFolder.outputDirectory.join(Path("Keep.txt"))));

    PackageDesc noBuild = desc;
    noBuild.binaryDirectory = root.join(Path("Missing"));
    REQUIRE_FALSE(ProjectPackager::package(noBuild).bSucceeded);

    PackageDesc insideAssets = desc;
    insideAssets.outputDirectory = project.join(Path("Assets/Package"));
    REQUIRE_FALSE(ProjectPackager::package(insideAssets).bSucceeded);

    PackageDesc badName = desc;
    badName.gameName = "My:Game";
    REQUIRE_FALSE(ProjectPackager::package(badName).bSucceeded);
    REQUIRE_FALSE(FileSystem::exists(output));
  }

  SECTION("Only the plugins the project enables are copied, and a missing one stops it")
  {
    String plugin("chProjectiles");
    plugin += DynamicLibrary::EXTENSION;
    String otherPlugin("chOther");
    otherPlugin += DynamicLibrary::EXTENSION;
    writeTestFile(binaries.join(Path("Plugins")).join(Path(plugin)), "plugin");
    writeTestFile(binaries.join(Path("Plugins")).join(Path(otherPlugin)), "plugin");
    const Path shaderFile("Plugins/Shaders/chProjectiles/SPIRV/bullet.vs.spv");
    const Path otherShaderFile("Plugins/Shaders/chOther/SPIRV/other.vs.spv");
    writeTestFile(binaries.join(shaderFile), "shader");
    writeTestFile(binaries.join(otherShaderFile), "shader");
    writeTestFile(project.join(Path("Project.chproject")),
                  "[Modules]\nGameModules=GameCode\n[Plugins]\nEnabled=chProjectiles\n");
    const PackageResult result = ProjectPackager::package(desc);
    REQUIRE(result.bSucceeded);
    REQUIRE(FileSystem::isFile(output.join(Path("Plugins")).join(Path(plugin))));
    REQUIRE_FALSE(FileSystem::exists(output.join(Path("Plugins")).join(Path(otherPlugin))));
    REQUIRE(FileSystem::isFile(output.join(shaderFile)));
    REQUIRE_FALSE(FileSystem::exists(output.join(otherShaderFile)));
    ProjectDescriptor packagedProject;
    REQUIRE(packagedProject.load(output.join(Path("MyGame.chproject"))));
    REQUIRE(packagedProject.plugins == Vector<String>{"chProjectiles"});

    writeTestFile(project.join(Path("Project.chproject")), "[Plugins]\nEnabled=chMissing\n");
    const PackageResult missing = ProjectPackager::package(desc);
    REQUIRE_FALSE(missing.bSucceeded);
    REQUIRE(missing.error.find("chMissing") != String::npos);
  }

  SECTION("Debug builds are told apart by the engine library")
  {
    REQUIRE_FALSE(ProjectPackager::isDebugBuild(binaries));
    String debugLibrary("chCored");
    debugLibrary += DynamicLibrary::EXTENSION;
    writeTestFile(binaries.join(Path(debugLibrary)), "lib");
    REQUIRE(ProjectPackager::isDebugBuild(binaries));
  }

  SECTION("Game names")
  {
    REQUIRE(ProjectPackager::isValidGameName("My Game-2"));
    REQUIRE_FALSE(ProjectPackager::isValidGameName(""));
    REQUIRE_FALSE(ProjectPackager::isValidGameName("a/b"));
    REQUIRE_FALSE(ProjectPackager::isValidGameName("Game."));
    REQUIRE_FALSE(ProjectPackager::isValidGameName(".."));
  }

  FileSystem::removeAll(root);
}

#endif
#if USING(CH_EDITOR)
TEST_CASE("chCore - Packaged assets follow references", "[Packaging]")
{
  const chEngineSDK::UUID scene = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID model = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID material = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID texture = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID missing = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID unused = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID secondScene = chEngineSDK::UUID::createRandom();
  const chEngineSDK::UUID secondModel = chEngineSDK::UUID::createRandom();

  // The material is reached twice and the texture points back at the scene.
  UnorderedMap<chEngineSDK::UUID, Vector<chEngineSDK::UUID>> references;
  references[scene] = {model, material};
  references[model] = {material, missing};
  references[material] = {texture};
  references[texture] = {scene};
  references[unused] = {};
  // A second scene shares the texture and brings its own model.
  references[secondScene] = {texture, secondModel};
  references[secondModel] = {};

  Vector<chEngineSDK::UUID> assets;
  Vector<chEngineSDK::UUID> missingAssets;
  const chEngineSDK::UUID roots[] = {scene, secondScene};
  ProjectPackager::collectAssets(
      roots,
      [&references](const chEngineSDK::UUID& uuid) -> const Vector<chEngineSDK::UUID>* {
        const auto found = references.find(uuid);
        return found != references.end() ? &found->second : nullptr;
      },
      assets, missingAssets);

  REQUIRE(assets.size() == 6);
  REQUIRE(assets[0] == scene);
  REQUIRE(Algorithm::contains(assets, secondScene));
  REQUIRE(Algorithm::contains(assets, secondModel));
  REQUIRE(Algorithm::contains(assets, model));
  REQUIRE(Algorithm::contains(assets, material));
  REQUIRE(Algorithm::contains(assets, texture));
  REQUIRE_FALSE(Algorithm::contains(assets, unused));
  REQUIRE(missingAssets.size() == 1);
  REQUIRE(missingAssets[0] == missing);
}

#endif
#if USING(CH_EDITOR)
TEST_CASE("chCore - Package rules", "[Packaging]")
{
  SECTION("Entries name a folder with everything inside, or one asset")
  {
    REQUIRE(PackageRules::matches("/Game/Enemies", "/Game/Enemies/Orc"));
    REQUIRE(PackageRules::matches("/Game/Enemies/", "/Game/Enemies/Big/Troll"));
    REQUIRE(PackageRules::matches("/Game/Enemies/Orc", "/Game/Enemies/Orc"));
    REQUIRE_FALSE(PackageRules::matches("/Game/Enemies", "/Game/EnemiesOld/Orc"));
    REQUIRE_FALSE(PackageRules::matches("/Game/Enemies/Orc", "/Game/Enemies/OrcBoss"));
    REQUIRE_FALSE(PackageRules::matches("", "/Game/Enemies/Orc"));
  }

  SECTION("Always included assets are found once each")
  {
    const auto makePrefab = [](const ANSICHAR* folder, const ANSICHAR* name) {
      AssetMetadata metadata;
      metadata.uuid = chEngineSDK::UUID::createRandom();
      metadata.assetType = AssetTypeTraits<PrefabAsset>::getTypeId();
      StringUtils::copyToBuffer(metadata.name, name);
      StringUtils::copyToBuffer(metadata.assetPath, folder);
      return SPtr<IAsset>(chMakeShared<PrefabAsset>(metadata));
    };
    const Vector<SPtr<IAsset>> assets = {makePrefab("/Game/Enemies", "Orc"),
                                         makePrefab("/Game/Enemies/Big", "Troll"),
                                         makePrefab("/Game/Props", "Barrel"),
                                         makePrefab("/Game/Props", "Crate")};
    REQUIRE(PackageRules::getAssetPath(*assets[0]) == "/Game/Enemies/Orc");

    PackageRules rules;
    rules.alwaysInclude = {"/Game/Enemies", "/Game/Enemies/Orc", "/Game/Props/Crate",
                           "/Game/Gone"};
    Vector<chEngineSDK::UUID> roots = {assets[3]->getUUID()};
    Vector<String> unmatched;
    rules.collectAlwaysIncluded(assets, roots, &unmatched);
    REQUIRE(roots.size() == 3);
    REQUIRE(roots[0] == assets[3]->getUUID());
    REQUIRE(Algorithm::contains(roots, assets[0]->getUUID()));
    REQUIRE(Algorithm::contains(roots, assets[1]->getUUID()));
    REQUIRE(unmatched.size() == 1);
    REQUIRE(unmatched[0] == "/Game/Gone");
  }

  SECTION("Saving keeps other sections and skips an unchanged file")
  {
    const Path root = FileSystem::absolutePath(Path("PackageRulesTestDir"));
    FileSystem::removeAll(root);
    const Path file = root.join(Path(String(PackageRules::FILE_NAME)));
    writeTestFile(file, "[Package]\nScenes = /Game/Maps/A , /Game/Maps/B\n[Other]\nKey=1\n");

    PackageRules rules = PackageRules::load(file);
    REQUIRE(rules.scenes == Vector<String>{"/Game/Maps/A", "/Game/Maps/B"});
    REQUIRE(rules.alwaysInclude.empty());

    rules.alwaysInclude = {"/Game/Enemies"};
    REQUIRE(rules.save(file));
    ConfigFile saved;
    REQUIRE(saved.load(file));
    REQUIRE(*saved.getValue("Package", "AlwaysInclude") == "/Game/Enemies");
    REQUIRE(*saved.getValue("Other", "Key") == "1");
    const PackageRules loaded = PackageRules::load(file);
    REQUIRE(loaded.scenes == rules.scenes);
    REQUIRE(loaded.alwaysInclude == rules.alwaysInclude);

    // Unchanged rules leave the file as it is, comments too.
    writeTestFile(file, "; kept\n[Package]\nScenes=/Game/Maps/A\n");
    REQUIRE(PackageRules::load(file).save(file));
    {
      FileDataStream stream(file);
      REQUIRE(stream.getAsString().starts_with("; kept"));
    }
    REQUIRE(PackageRules::load(root.join(Path("Missing.ini"))).scenes.empty());
    FileSystem::removeAll(root);
  }
}
#endif // USING(CH_EDITOR)
