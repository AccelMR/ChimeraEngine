/************************************************************************/
/**
 * @file chAnimationGraphTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of animation graphs: data, blend spaces, runtime, animator and editing.
 */
/************************************************************************/
#include <cstring>

#include "chAnimationClip.h"
#include "chAnimationClipAsset.h"
#include "chAnimationGraph.h"
#include "chAnimationGraphAsset.h"
#include "chAnimationGraphInstance.h"
#include "chAnimatorComponent.h"
#include "chAssetMeta.h"
#include "chBlendSpace.h"
#include "chConfigFile.h"
#include "chFileStream.h"
#include "chGraphicsTypes.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chModelComponent.h"
#include "chQuaternion.h"
#include "chScene.h"
#include "chStringUtils.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"

#include "catch.hpp"
#include "chAnimationTestHelpers.h"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

namespace {
/*
 * Parameters Speed, Grounded, Weapon, Jump, DirX, DirY; four clips; a Base layer with Idle,
 * Locomotion (1D on Speed), Strafe (2D on DirX, DirY) and Jump, and an UpperBody layer
 * masked at Spine.
 */
struct TestGraphParts
{
  Vector<AnimationParameter> parameters;
  Vector<chEngineSDK::UUID> clips;
  Vector<AnimationLayer> layers;
};

TestGraphParts
createTestGraphParts()
{
  TestGraphParts parts;
  parts.parameters = {
      {.name = "Speed", .type = AnimationParameterType::Float},
      {.name = "Grounded",
       .type = AnimationParameterType::Bool,
       .defaultValue = {.intValue = 1}},
      {.name = "Weapon", .type = AnimationParameterType::Int, .defaultValue = {.intValue = 2}},
      {.name = "Jump", .type = AnimationParameterType::Trigger},
      {.name = "DirX", .type = AnimationParameterType::Float},
      {.name = "DirY",
       .type = AnimationParameterType::Float,
       .defaultValue = {.floatValue = 0.5f}},
  };
  for (uint32 i = 0; i < 4; ++i) {
    parts.clips.push_back(chEngineSDK::UUID::createRandom());
  }

  AnimationLayer base{.name = "Base"};
  base.states.push_back({.name = "Idle", .clip = 0});
  base.states.push_back(
      {.name = "Locomotion",
       .kind = AnimationStateKind::BlendSpace1D,
       .parameterX = 0,
       .samples = {{.clip = 0, .x = 0.0f}, {.clip = 1, .x = 1.0f}, {.clip = 2, .x = 3.0f}}});
  base.states.push_back({.name = "Strafe",
                         .kind = AnimationStateKind::BlendSpace2D,
                         .parameterX = 4,
                         .parameterY = 5,
                         .samples = {{.clip = 0, .x = 0.0f, .y = 0.0f},
                                     {.clip = 1, .x = 1.0f, .y = 0.0f},
                                     {.clip = 2, .x = 0.0f, .y = 1.0f}},
                         .speed = 1.5f,
                         .speedParameter = 0});
  base.states.push_back({.name = "Jump", .clip = 3, .bLooping = false});
  base.transitions.push_back({.source = 0,
                              .target = 1,
                              .conditions = {{.parameter = 0,
                                              .op = AnimationConditionOp::Greater,
                                              .threshold = {.floatValue = 0.1f}}}});
  base.transitions.push_back(
      {.source = 1,
       .target = 0,
       .duration = 0.4f,
       .bInterruptible = true,
       .priority = 2,
       .conditions = {{.parameter = 0,
                       .op = AnimationConditionOp::LessOrEqual,
                       .threshold = {.floatValue = 0.1f}},
                      {.parameter = 1, .op = AnimationConditionOp::IsSet}}});
  base.transitions.push_back(
      {.source = AnimationGraph::ANY_STATE,
       .target = 3,
       .bCanTransitionToSelf = true,
       .conditions = {{.parameter = 3, .op = AnimationConditionOp::IsSet}}});
  base.transitions.push_back(
      {.source = 3, .target = 0, .bHasExitTime = true, .exitTime = 0.9f});
  parts.layers.push_back(std::move(base));

  AnimationLayer upper{.name = "UpperBody", .weight = 0.5f, .maskBones = {"Spine"}};
  upper.states.push_back({.name = "Aim", .clip = 3});
  upper.states.push_back({.name = "Holster", .clip = 2});
  upper.transitions.push_back({.source = 0,
                               .target = 1,
                               .conditions = {{.parameter = 2,
                                               .op = AnimationConditionOp::Equal,
                                               .threshold = {.intValue = 0}}}});
  parts.layers.push_back(std::move(upper));
  return parts;
}

AnimationGraph
buildTestGraph(const TestGraphParts& parts)
{
  AnimationGraph graph;
  for (const AnimationParameter& parameter : parts.parameters) {
    static_cast<void>(graph.addParameter(parameter));
  }
  for (const chEngineSDK::UUID& clip : parts.clips) {
    static_cast<void>(graph.addClip(clip));
  }
  for (const AnimationLayer& layer : parts.layers) {
    static_cast<void>(graph.addLayer(layer));
  }
  return graph;
}
} // namespace

TEST_CASE("chCore - Animation graph", "[Animation]")
{
  const TestGraphParts parts = createTestGraphParts();
  const AnimationGraph graph = buildTestGraph(parts);
  String error;
  REQUIRE(graph.validate(&error));
  REQUIRE(error.empty());

  SECTION("Names are found ignoring case")
  {
    REQUIRE(graph.findParameter("jump") == 3);
    REQUIRE(graph.findParameter("Missing") == AnimationGraph::NOT_FOUND);
    REQUIRE(graph.findLayer("upperbody") == 1);
    REQUIRE(graph.findState(0, "STRAFE") == 2);
    REQUIRE(graph.findState(1, "Idle") == AnimationGraph::NOT_FOUND);
    REQUIRE(graph.findState(5, "Idle") == AnimationGraph::NOT_FOUND);
  }

  SECTION("Every clip is a reference")
  {
    Vector<chEngineSDK::UUID> references;
    graph.collectReferences(references);
    REQUIRE(references == parts.clips);
  }

  SECTION("Save and read")
  {
    MemoryDataStream stream;
    graph.serialize(stream);
    const Vector<uint8> bytes = readAllBytes(stream);
    const SPtr<AnimationGraph> read = AnimationGraph::deserialize(stream);
    REQUIRE(read);

    REQUIRE(read->getParameters().size() == 6);
    const AnimationParameter& grounded = read->getParameters()[1];
    REQUIRE(grounded.name == "Grounded");
    REQUIRE(grounded.type == AnimationParameterType::Bool);
    REQUIRE(grounded.defaultValue.intValue == 1);
    REQUIRE(read->getParameters()[5].defaultValue.floatValue == 0.5f);
    REQUIRE(read->getClips().size() == 4);
    REQUIRE(read->getClips()[2].clipId == parts.clips[2]);
    REQUIRE_FALSE(read->getClips()[2].clip);

    REQUIRE(read->getLayers().size() == 2);
    const AnimationLayer& base = read->getLayers()[0];
    REQUIRE(base.states.size() == 4);
    const AnimationState& strafe = base.states[2];
    REQUIRE(strafe.kind == AnimationStateKind::BlendSpace2D);
    REQUIRE(strafe.parameterX == 4);
    REQUIRE(strafe.parameterY == 5);
    REQUIRE(strafe.samples.size() == 3);
    REQUIRE(strafe.samples[2].clip == 2);
    REQUIRE(strafe.samples[2].y == 1.0f);
    REQUIRE(strafe.speed == 1.5f);
    REQUIRE(strafe.speedParameter == 0);
    REQUIRE(base.states[1].speedParameter == AnimationGraph::NO_PARAMETER);
    REQUIRE_FALSE(base.states[3].bLooping);

    REQUIRE(base.transitions.size() == 4);
    const AnimationTransition& toIdle = base.transitions[1];
    REQUIRE(toIdle.duration == 0.4f);
    REQUIRE(toIdle.bInterruptible);
    REQUIRE(toIdle.priority == 2);
    REQUIRE(toIdle.conditions.size() == 2);
    REQUIRE(toIdle.conditions[0].op == AnimationConditionOp::LessOrEqual);
    REQUIRE(toIdle.conditions[0].threshold.floatValue == 0.1f);
    REQUIRE(base.transitions[2].source == AnimationGraph::ANY_STATE);
    REQUIRE(base.transitions[2].bCanTransitionToSelf);
    REQUIRE(base.transitions[3].bHasExitTime);
    REQUIRE(base.transitions[3].exitTime == 0.9f);

    const AnimationLayer& upper = read->getLayers()[1];
    REQUIRE(upper.weight == 0.5f);
    REQUIRE(upper.maskBones == Vector<String>{"Spine"});
    REQUIRE(upper.transitions[0].conditions[0].op == AnimationConditionOp::Equal);

    // Saving what was read gives the same bytes.
    MemoryDataStream again;
    read->serialize(again);
    REQUIRE(readAllBytes(again) == bytes);

    for (const SIZE_T size : {SIZE_T(0), SIZE_T(10), bytes.size() / 2, bytes.size() - 1}) {
      MemoryDataStream cut;
      cut.write(bytes.data(), size);
      cut.seek(0);
      REQUIRE_FALSE(AnimationGraph::deserialize(cut));
    }
  }

  SECTION("A newer version is refused")
  {
    MemoryDataStream stream;
    graph.serialize(stream);
    Vector<uint8> bytes = readAllBytes(stream);
    const uint32 newer = AnimationGraph::SERIALIZATION_VERSION + 1;
    std::memcpy(bytes.data(), &newer, sizeof(newer));
    MemoryDataStream changed;
    changed.write(bytes.data(), bytes.size());
    changed.seek(0);
    REQUIRE_FALSE(AnimationGraph::deserialize(changed));
  }

  SECTION("Broken graphs are refused with a reason")
  {
    struct BrokenCase
    {
      const ANSICHAR* expected;
      Function<void(TestGraphParts&)> breakIt;
    };
    const Vector<BrokenCase> cases{
        {"Two parameters", [](TestGraphParts& p) { p.parameters[4].name = "speed"; }},
        {"no valid name", [](TestGraphParts& p) { p.parameters[0].name.clear(); }},
        {"must start off",
         [](TestGraphParts& p) { p.parameters[3].defaultValue.intValue = 1; }},
        {"not 0 or 1", [](TestGraphParts& p) { p.parameters[1].defaultValue.intValue = 2; }},
        {"unknown type",
         [](TestGraphParts& p) {
           p.parameters[0].type = static_cast<AnimationParameterType>(9);
         }},
        {"no layers", [](TestGraphParts& p) { p.layers.clear(); }},
        {"Two layers", [](TestGraphParts& p) { p.layers[1].name = "BASE"; }},
        {"weight outside", [](TestGraphParts& p) { p.layers[1].weight = 1.5f; }},
        {"cannot have a mask", [](TestGraphParts& p) { p.layers[0].maskBones = {"Hips"}; }},
        {"default state", [](TestGraphParts& p) { p.layers[1].defaultState = 2; }},
        {"two states named", [](TestGraphParts& p) { p.layers[0].states[3].name = "idle"; }},
        {"clip that does not exist",
         [](TestGraphParts& p) { p.layers[0].states[0].clip = 4; }},
        {"unknown kind",
         [](TestGraphParts& p) {
           p.layers[0].states[0].kind = static_cast<AnimationStateKind>(3);
         }},
        {"Float or Int parameters",
         [](TestGraphParts& p) { p.layers[0].states[1].parameterX = 1; }},
        {"Float or Int parameters",
         [](TestGraphParts& p) { p.layers[0].states[2].parameterY = 4; }},
        {"has no clips", [](TestGraphParts& p) { p.layers[0].states[1].samples.clear(); }},
        {"same position",
         [](TestGraphParts& p) { p.layers[0].states[2].samples[2].y = 0.0f; }},
        {"speed parameter",
         [](TestGraphParts& p) { p.layers[0].states[2].speedParameter = 1; }},
        {"do not exist", [](TestGraphParts& p) { p.layers[0].transitions[0].target = 9; }},
        {"to itself", [](TestGraphParts& p) { p.layers[0].transitions[0].target = 0; }},
        {"invalid time",
         [](TestGraphParts& p) { p.layers[0].transitions[0].duration = -1.0f; }},
        {"cannot have an exit time",
         [](TestGraphParts& p) { p.layers[0].transitions[2].bHasExitTime = true; }},
        {"needs a condition or an exit time",
         [](TestGraphParts& p) { p.layers[0].transitions[3].bHasExitTime = false; }},
        {"needs a condition or an exit time",
         [](TestGraphParts& p) { p.layers[0].transitions[2].conditions.clear(); }},
        {"parameter that does not exist",
         [](TestGraphParts& p) { p.layers[0].transitions[0].conditions[0].parameter = 6; }},
        // Floats are never compared for equality, triggers are only tested as set.
        {"does not allow",
         [](TestGraphParts& p) {
           p.layers[0].transitions[0].conditions[0].op = AnimationConditionOp::Equal;
         }},
        {"does not allow",
         [](TestGraphParts& p) {
           p.layers[0].transitions[2].conditions[0].op = AnimationConditionOp::IsNotSet;
         }},
    };

    for (const BrokenCase& brokenCase : cases) {
      INFO(brokenCase.expected);
      TestGraphParts broken = createTestGraphParts();
      brokenCase.breakIt(broken);
      const AnimationGraph brokenGraph = buildTestGraph(broken);
      String reason;
      REQUIRE_FALSE(brokenGraph.validate(&reason));
      REQUIRE(StringUtils::containsIgnoreCase(reason, brokenCase.expected));

      // A file holding it reads as broken and says why.
      MemoryDataStream stream;
      brokenGraph.serialize(stream);
      stream.seek(0);
      String readReason;
      REQUIRE_FALSE(AnimationGraph::deserialize(stream, &readReason));
      REQUIRE(readReason == reason);

      // The editor reads it anyway, so it can be fixed.
      stream.seek(0);
      REQUIRE(AnimationGraph::deserialize(stream, nullptr, false));
    }
  }
}

namespace {
/*
 * Sum of the sample positions times their weights: inside a triangle it gives back the
 * point asked for.
 */
Vector2
blendedPosition(const AnimationState& state, const BlendSpaceWeights& weights)
{
  float sum = 0.0f;
  Vector2 position(0.0f, 0.0f);
  for (uint32 i = 0; i < weights.count; ++i) {
    const BlendSpaceSample& sample = state.samples[weights.items[i].sample];
    position.x += sample.x * weights.items[i].weight;
    position.y += sample.y * weights.items[i].weight;
    sum += weights.items[i].weight;
  }
  REQUIRE(sum == Approx(1.0f).margin(1e-5f));
  return position;
}

float
weightOf(const BlendSpaceWeights& weights, uint32 sample)
{
  float weight = 0.0f;
  for (uint32 i = 0; i < weights.count; ++i) {
    weight += weights.items[i].sample == sample ? weights.items[i].weight : 0.0f;
  }
  return weight;
}

AnimationState
createBlendState(AnimationStateKind kind, const Vector<Pair<float, float>>& positions)
{
  AnimationState state{.name = "Blend", .kind = kind, .parameterX = 0, .parameterY = 1};
  for (const Pair<float, float>& position : positions) {
    state.samples.push_back({.clip = static_cast<uint32>(state.samples.size()),
                             .x = position.first,
                             .y = position.second});
  }
  return state;
}
} // namespace

TEST_CASE("chCore - Blend space", "[Animation]")
{
  SECTION("1D blends the two neighbors and holds the ends")
  {
    // Out of order on purpose: build sorts them.
    const AnimationState state = createBlendState(AnimationStateKind::BlendSpace1D,
                                                  {{3.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}});
    BlendSpace space;
    space.build(state);

    BlendSpaceWeights weights = space.computeWeights(0.5f, 0.0f);
    REQUIRE(weights.count == 2);
    REQUIRE(weightOf(weights, 1) == Approx(0.5f));
    REQUIRE(weightOf(weights, 2) == Approx(0.5f));
    weights = space.computeWeights(2.5f, 0.0f);
    REQUIRE(weightOf(weights, 2) == Approx(0.25f));
    REQUIRE(weightOf(weights, 0) == Approx(0.75f));
    weights = space.computeWeights(1.0f, 0.0f);
    REQUIRE(weights.count == 1);
    REQUIRE(weights.items[0].sample == 2);
    REQUIRE(space.computeWeights(-4.0f, 0.0f).items[0].sample == 1);
    REQUIRE(space.computeWeights(9.0f, 0.0f).items[0].sample == 0);
  }

  SECTION("A single clip always has all the weight")
  {
    BlendSpace space;
    space.build(createBlendState(AnimationStateKind::BlendSpace2D, {{2.0f, 3.0f}}));
    const BlendSpaceWeights weights = space.computeWeights(-1.0f, 7.0f);
    REQUIRE(weights.count == 1);
    REQUIRE(weights.items[0].weight == 1.0f);
  }

  SECTION("2D gives back the point inside the triangles")
  {
    // Axes with different ranges, as speed against direction.
    const AnimationState state = createBlendState(
        AnimationStateKind::BlendSpace2D,
        {{0.0f, 0.0f}, {600.0f, 0.0f}, {0.0f, 1.0f}, {600.0f, 1.0f}, {300.0f, 0.5f}});
    BlendSpace space;
    space.build(state);
    REQUIRE(space.getTriangles().size() == 12);

    for (const Pair<float, float>& point : Vector<Pair<float, float>>{
             {150.0f, 0.25f}, {450.0f, 0.9f}, {300.0f, 0.5f}, {0.0f, 0.0f}, {600.0f, 0.5f}}) {
      const Vector2 position =
          blendedPosition(state, space.computeWeights(point.first, point.second));
      REQUIRE(position.x == Approx(point.first).margin(1e-2f));
      REQUIRE(position.y == Approx(point.second).margin(1e-4f));
    }
    // A clip's own position gives it all the weight.
    REQUIRE(weightOf(space.computeWeights(300.0f, 0.5f), 4) == Approx(1.0f));
  }

  SECTION("2D outside the outline takes its closest point")
  {
    const AnimationState state =
        createBlendState(AnimationStateKind::BlendSpace2D,
                         {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}});
    BlendSpace space;
    space.build(state);
    REQUIRE(space.getTriangles().size() == 6);

    const BlendSpaceWeights weights = space.computeWeights(3.0f, 0.25f);
    REQUIRE(weights.count == 2);
    REQUIRE(weightOf(weights, 1) == Approx(0.75f));
    REQUIRE(weightOf(weights, 3) == Approx(0.25f));
    const Vector2 corner = blendedPosition(state, space.computeWeights(-5.0f, -5.0f));
    REQUIRE(corner.x == Approx(0.0f).margin(1e-5f));
    REQUIRE(corner.y == Approx(0.0f).margin(1e-5f));
  }

  SECTION("2D samples on one line blend along it")
  {
    const AnimationState state = createBlendState(AnimationStateKind::BlendSpace2D,
                                                  {{2.0f, 2.0f}, {0.0f, 0.0f}, {1.0f, 1.0f}});
    BlendSpace space;
    space.build(state);
    REQUIRE(space.getTriangles().empty());

    const BlendSpaceWeights weights = space.computeWeights(0.5f, 0.5f);
    REQUIRE(weightOf(weights, 1) == Approx(0.5f));
    REQUIRE(weightOf(weights, 2) == Approx(0.5f));
    REQUIRE(weightOf(space.computeWeights(5.0f, 4.0f), 0) == Approx(1.0f));
  }
}

namespace {
constexpr uint32 kArm = 2;
constexpr uint32 kLeg = 3;

float
poseX(const AnimationGraphInstance& instance, uint32 node)
{
  return instance.getLocalPose()[node].position.x;
}

/*
 * Starts in A (still at x = 0); B is still at x = 1 and C at x = 10, all looping and one
 * second long. Parameters: Go (Bool), Hit (Trigger). Transitions are added by each test.
 */
struct SmallGraph
{
  SPtr<AnimationGraph> graph = chMakeShared<AnimationGraph>();
  AnimationLayer layer{.name = "Base"};
  uint32 go = 0;
  uint32 hit = 0;

  SmallGraph()
  {
    go = graph->addParameter({.name = "Go", .type = AnimationParameterType::Bool});
    hit = graph->addParameter({.name = "Hit", .type = AnimationParameterType::Trigger});
    layer.states.push_back({.name = "A", .clip = addGraphTestClip(*graph, 0.0f, 0.0f, 1.0f)});
    layer.states.push_back({.name = "B", .clip = addGraphTestClip(*graph, 1.0f, 1.0f, 1.0f)});
    layer.states.push_back(
        {.name = "C", .clip = addGraphTestClip(*graph, 10.0f, 10.0f, 1.0f)});
  }

  AnimationCondition
  goIsSet() const
  {
    return {.parameter = go, .op = AnimationConditionOp::IsSet};
  }

  AnimationCondition
  hitIsSet() const
  {
    return {.parameter = hit, .op = AnimationConditionOp::IsSet};
  }

  void
  bind(AnimationGraphInstance& instance)
  {
    static_cast<void>(graph->addLayer(layer));
    REQUIRE(graph->validate());
    instance.setModel(createGraphTestModel());
    instance.setGraph(graph);
    REQUIRE(instance.isBound());
  }
};
} // namespace

TEST_CASE("chCore - Animation graph runtime", "[Animation]")
{
  AnimationGraphInstance instance;

  SECTION("A graph with errors is kept but never bound")
  {
    TestGraphParts parts = createTestGraphParts();
    parts.layers[0].transitions[0].target = 9;
    const SPtr<AnimationGraph> broken = chMakeShared<AnimationGraph>(buildTestGraph(parts));
    instance.setModel(createGraphTestModel());
    instance.setGraph(broken);
    REQUIRE(instance.getGraph() == broken);
    REQUIRE_FALSE(instance.isBound());
    REQUIRE_FALSE(instance.advance(0.1f));
    REQUIRE(instance.getCurrentState(0) == AnimationGraphInstance::NOT_FOUND);
  }

  SECTION("Nothing runs until a model and a graph are set")
  {
    REQUIRE_FALSE(instance.advance(0.1f));
    REQUIRE(instance.getPoseMatrices().empty());
    instance.setModel(createGraphTestModel());
    REQUIRE_FALSE(instance.isBound());
    REQUIRE(instance.getCurrentState(0) == AnimationGraphInstance::NOT_FOUND);
  }

  SECTION("Locomotion, a jump and an upper body layer")
  {
    const SPtr<AnimationGraph> graph = chMakeShared<AnimationGraph>();
    const uint32 speed = graph->addParameter({.name = "Speed"});
    const uint32 jump =
        graph->addParameter({.name = "Jump", .type = AnimationParameterType::Trigger});
    const uint32 idleClip = addGraphTestClip(*graph, 0.0f, 0.0f, 1.0f);
    const uint32 walkClip = addGraphTestClip(*graph, 1.0f, 1.0f, 1.0f);
    const uint32 runClip = addGraphTestClip(*graph, 3.0f, 3.0f, 0.5f);
    const uint32 jumpClip = addGraphTestClip(*graph, 10.0f, 10.0f, 1.0f);
    const uint32 aimClip = addGraphTestClip(*graph, 5.0f, 5.0f, 1.0f);

    AnimationLayer base{.name = "Base"};
    base.states.push_back({.name = "Idle", .clip = idleClip});
    base.states.push_back(
        {.name = "Move",
         .kind = AnimationStateKind::BlendSpace1D,
         .parameterX = speed,
         .samples = {{.clip = walkClip, .x = 1.0f}, {.clip = runClip, .x = 3.0f}}});
    base.states.push_back({.name = "Jump", .clip = jumpClip, .bLooping = false});
    base.transitions.push_back({.source = 0,
                                .target = 1,
                                .duration = 0.0f,
                                .conditions = {{.parameter = speed,
                                                .op = AnimationConditionOp::Greater,
                                                .threshold = {.floatValue = 0.1f}}}});
    base.transitions.push_back({.source = 1,
                                .target = 0,
                                .duration = 0.5f,
                                .conditions = {{.parameter = speed,
                                                .op = AnimationConditionOp::LessOrEqual,
                                                .threshold = {.floatValue = 0.1f}}}});
    base.transitions.push_back({.source = AnimationGraph::ANY_STATE,
                                .target = 2,
                                .duration = 0.0f,
                                .conditions = {{.parameter = jump}}});
    base.transitions.push_back(
        {.source = 2, .target = 0, .duration = 0.0f, .bHasExitTime = true, .exitTime = 1.0f});
    static_cast<void>(graph->addLayer(base));

    AnimationLayer upper{.name = "UpperBody", .maskBones = {"Spine"}};
    upper.states.push_back({.name = "Aim", .clip = aimClip});
    static_cast<void>(graph->addLayer(upper));
    REQUIRE(graph->validate());

    instance.setModel(createGraphTestModel());
    instance.setGraph(graph);
    REQUIRE(instance.isBound());

    // The pose is there before the first advance; the mask covers Spine and Arm.
    REQUIRE(instance.getCurrentState(0) == 0);
    REQUIRE(poseX(instance, kHips) == 0.0f);
    REQUIRE(poseX(instance, kLeg) == 0.0f);
    REQUIRE(poseX(instance, kSpine) == Approx(5.0f));
    REQUIRE(poseX(instance, kArm) == Approx(5.0f));
    REQUIRE(instance.getPoseMatrices()[kArm].transformPosition(Vector3::ZERO).x
            == Approx(10.0f));

    // Parameters only take their own type.
    REQUIRE_FALSE(instance.setTrigger(speed));
    REQUIRE_FALSE(instance.setFloat(jump, 1.0f));
    REQUIRE_FALSE(instance.setFloat(99, 1.0f));

    // Halfway between walk and run; the blend lasts 0.5 * 1 + 0.5 * 0.5 seconds.
    REQUIRE(instance.setFloat(speed, 2.0f));
    REQUIRE(instance.advance(0.1f));
    REQUIRE(instance.getCurrentState(0) == 1);
    REQUIRE_FALSE(instance.isInTransition(0));
    REQUIRE(poseX(instance, kHips) == Approx(2.0f));
    instance.advance(0.075f);
    REQUIRE(instance.getStateTime(0) == Approx(0.1f));

    // Back to Idle over half a second, fading from Move as it plays (now at walk).
    instance.setFloat(speed, 0.0f);
    instance.advance(0.0f);
    REQUIRE(instance.getCurrentState(0) == 0);
    REQUIRE(instance.isInTransition(0));
    REQUIRE(poseX(instance, kHips) == Approx(1.0f));
    instance.advance(0.25f);
    REQUIRE(poseX(instance, kHips) == Approx(0.5f));

    // The fade cannot be interrupted, so the jump waits for it and the trigger stays set.
    REQUIRE(instance.setTrigger(jump));
    instance.advance(0.1f);
    REQUIRE(instance.getCurrentState(0) == 0);
    REQUIRE(instance.getBool(jump));
    instance.advance(0.2f);
    REQUIRE(instance.getCurrentState(0) == 2);
    REQUIRE_FALSE(instance.getBool(jump));
    REQUIRE(poseX(instance, kHips) == Approx(10.0f));
    REQUIRE(poseX(instance, kSpine) == Approx(5.0f));

    // The jump does not loop; at its end the exit time takes it back to Idle.
    instance.advance(0.5f);
    REQUIRE(instance.getCurrentState(0) == 2);
    REQUIRE(instance.getStateTime(0) == Approx(0.5f));
    instance.advance(0.6f);
    REQUIRE(instance.getCurrentState(0) == 0);
    REQUIRE(poseX(instance, kHips) == 0.0f);

    // A half weight layer moves its nodes halfway.
    instance.setLayerWeight(1, 0.5f);
    instance.advance(0.0f);
    REQUIRE(poseX(instance, kSpine) == Approx(2.5f));
    REQUIRE(poseX(instance, kLeg) == 0.0f);
    instance.setLayerWeight(1, 7.0f);
    REQUIRE(instance.getLayerWeight(1) == 1.0f);

    // reset goes back to the defaults.
    instance.setFloat(speed, 2.0f);
    instance.advance(0.1f);
    instance.reset();
    REQUIRE(instance.getFloat(speed) == 0.0f);
    REQUIRE(instance.getCurrentState(0) == 0);
    REQUIRE(instance.getLayerWeight(1) == 1.0f);
  }

  SECTION("Higher priority wins, ties keep the order of the list")
  {
    SmallGraph setup;
    setup.layer.transitions.push_back(
        {.source = 0, .target = 1, .duration = 0.0f, .conditions = {setup.goIsSet()}});
    setup.layer.transitions.push_back({.source = 0,
                                       .target = 2,
                                       .duration = 0.0f,
                                       .priority = 5,
                                       .conditions = {setup.goIsSet()}});
    setup.layer.transitions.push_back({.source = 0,
                                       .target = 1,
                                       .duration = 0.0f,
                                       .priority = 5,
                                       .conditions = {setup.goIsSet()}});
    setup.bind(instance);
    instance.setBool(setup.go, true);
    instance.advance(0.0f);
    REQUIRE(instance.getCurrentState(0) == 2);
  }

  SECTION("An interrupted fade starts the next one from the pose shown")
  {
    SmallGraph setup;
    setup.layer.transitions.push_back({.source = 0,
                                       .target = 1,
                                       .duration = 1.0f,
                                       .bInterruptible = true,
                                       .conditions = {setup.goIsSet()}});
    setup.layer.transitions.push_back(
        {.source = 1, .target = 2, .duration = 1.0f, .conditions = {setup.hitIsSet()}});
    setup.bind(instance);
    instance.setBool(setup.go, true);
    instance.advance(0.0f);
    instance.advance(0.5f);
    REQUIRE(poseX(instance, kHips) == Approx(0.5f));

    instance.setTrigger(setup.hit);
    instance.advance(0.0f);
    REQUIRE(instance.getCurrentState(0) == 2);
    REQUIRE(poseX(instance, kHips) == Approx(0.5f));
    instance.advance(0.5f);
    REQUIRE(poseX(instance, kHips) == Approx(5.25f));
    instance.advance(0.5f);
    REQUIRE_FALSE(instance.isInTransition(0));
    REQUIRE(poseX(instance, kHips) == Approx(10.0f));
  }

  SECTION("Any State restarts its target only when allowed")
  {
    SmallGraph setup;
    setup.layer.transitions.push_back({.source = AnimationGraph::ANY_STATE,
                                       .target = 1,
                                       .duration = 0.0f,
                                       .conditions = {setup.goIsSet()}});
    setup.bind(instance);
    instance.setBool(setup.go, true);
    instance.advance(0.0f);
    REQUIRE(instance.getCurrentState(0) == 1);
    instance.advance(0.3f);
    instance.advance(0.3f);
    REQUIRE(instance.getStateTime(0) == Approx(0.6f));
  }

  SECTION("Any State to itself restarts every time its conditions hold")
  {
    SmallGraph setup;
    setup.layer.transitions.push_back({.source = AnimationGraph::ANY_STATE,
                                       .target = 1,
                                       .duration = 0.0f,
                                       .bCanTransitionToSelf = true,
                                       .conditions = {setup.goIsSet()}});
    setup.bind(instance);
    instance.setBool(setup.go, true);
    instance.advance(0.0f);
    instance.advance(0.3f);
    REQUIRE(instance.getCurrentState(0) == 1);
    REQUIRE(instance.getStateTime(0) == 0.0f);
  }

  SECTION("A looping state reaches its exit time in every loop")
  {
    SmallGraph setup;
    setup.layer.transitions.push_back(
        {.source = 0, .target = 1, .duration = 0.0f, .bHasExitTime = true, .exitTime = 0.5f});
    setup.bind(instance);
    instance.advance(1.2f);
    REQUIRE(instance.getCurrentState(0) == 0);
    instance.advance(0.4f);
    REQUIRE(instance.getCurrentState(0) == 1);
  }

  SECTION("Blend space clips stay in step and a parameter scales the speed")
  {
    const SPtr<AnimationGraph> graph = chMakeShared<AnimationGraph>();
    const uint32 speed =
        graph->addParameter({.name = "Speed", .defaultValue = {.floatValue = 2.0f}});
    const uint32 rate =
        graph->addParameter({.name = "Rate", .defaultValue = {.floatValue = 1.0f}});
    // Both go from 0 to 1 over their own length, so in step they match at every moment.
    const uint32 walkClip = addGraphTestClip(*graph, 0.0f, 1.0f, 1.0f);
    const uint32 runClip = addGraphTestClip(*graph, 0.0f, 1.0f, 0.5f);
    AnimationLayer layer{.name = "Base"};
    layer.states.push_back(
        {.name = "Move",
         .kind = AnimationStateKind::BlendSpace1D,
         .parameterX = speed,
         .samples = {{.clip = walkClip, .x = 1.0f}, {.clip = runClip, .x = 3.0f}},
         .speedParameter = rate});
    static_cast<void>(graph->addLayer(layer));
    REQUIRE(graph->validate());
    instance.setModel(createGraphTestModel());
    instance.setGraph(graph);

    instance.advance(0.1875f);
    REQUIRE(instance.getStateTime(0) == Approx(0.25f));
    REQUIRE(poseX(instance, kHips) == Approx(0.25f));

    instance.setFloat(rate, 2.0f);
    instance.advance(0.1875f);
    REQUIRE(instance.getStateTime(0) == Approx(0.75f));
    REQUIRE(poseX(instance, kHips) == Approx(0.75f));
  }

  SECTION("A missing clip shows the rest pose")
  {
    const SPtr<AnimationGraph> graph = chMakeShared<AnimationGraph>();
    const uint32 clip = graph->addClip(chEngineSDK::UUID::createRandom());
    AnimationLayer layer{.name = "Base"};
    layer.states.push_back({.name = "Lost", .clip = clip});
    static_cast<void>(graph->addLayer(layer));
    instance.setModel(createGraphTestModel());
    instance.setGraph(graph);
    REQUIRE(instance.advance(0.5f));
    REQUIRE(poseX(instance, kHips) == 0.0f);
    REQUIRE(instance.getStateTime(0) > 1.0f);
  }
}
namespace {

#if USING(CH_EDITOR)
/*
 * Opens the editor data calls the asset manager makes when it saves and scans files.
 */
class LayoutTestGraphAsset : public AnimationGraphAsset
{
 public:
  using AnimationGraphAsset::AnimationGraphAsset;
  using AnimationGraphAsset::readEditorValues;
  using AnimationGraphAsset::writeEditorValues;
};
#endif

/*
 * Idle (x = 0) goes to Walk (x = 1) at once when Speed is above 0.5.
 */
SPtr<AnimationGraph>
createWalkGraph()
{
  SPtr<AnimationGraph> graph = chMakeShared<AnimationGraph>();
  const uint32 speed = graph->addParameter({.name = "Speed"});
  AnimationLayer layer{.name = "Base"};
  layer.states.push_back({.name = "Idle", .clip = addGraphTestClip(*graph, 0.0f, 0.0f, 1.0f)});
  layer.states.push_back({.name = "Walk", .clip = addGraphTestClip(*graph, 1.0f, 1.0f, 1.0f)});
  layer.transitions.push_back({.source = 0,
                               .target = 1,
                               .duration = 0.0f,
                               .conditions = {{.parameter = speed,
                                               .op = AnimationConditionOp::Greater,
                                               .threshold = {.floatValue = 0.5f}}}});
  static_cast<void>(graph->addLayer(layer));
  return graph;
}
} // namespace

TEST_CASE("chCore - Animator graph", "[Animation]")
{
  Scene scene("Test", chEngineSDK::UUID::createRandom());
  const SPtr<GameObject> walker = scene.createGameObject("Walker");
  ModelComponent& model =
      walker->addComponent<ModelComponent>(createTestModelAsset(createGraphTestModel()));
  AnimatorComponent& animator = walker->addComponent<AnimatorComponent>();
  const SPtr<AnimationGraphAsset> graphAsset = createTestGraphAsset(createWalkGraph());
  const auto hipsX = [&model]() {
    return model.getNodeMatrix(kHips).transformPosition(Vector3::ZERO).x;
  };

  animator.setGraph(graphAsset);
  REQUIRE(animator.hasGraph());
  REQUIRE(animator.getGraphInstance().isBound());
  REQUIRE_FALSE(model.getPose().empty());
  REQUIRE(hipsX() == 0.0f);

  // The graph runs in the scene's update, also while the scene is not playing.
  AnimationGraphInstance& instance = animator.getGraphInstance();
  instance.setFloat(0, 1.0f);
  scene.update(0.1f);
  REQUIRE(instance.getCurrentState(0) == 1);
  REQUIRE(hipsX() == Approx(1.0f));

  SECTION("A clip that does not loop shows over the graph until it ends")
  {
    animator.setLooping(false);
    animator.play(createGraphTestClip(20.0f, 20.0f, 1.0f), 0.4f);
    REQUIRE(animator.isClipOverGraph());
    REQUIRE(hipsX() == Approx(1.0f));
    scene.update(0.2f);
    REQUIRE(hipsX() == Approx(10.5f));
    scene.update(0.2f);
    REQUIRE(hipsX() == Approx(20.0f));
    scene.update(0.5f);
    REQUIRE(hipsX() == Approx(20.0f));
    // It ends at 1 second and fades back out.
    scene.update(0.2f);
    REQUIRE(animator.isClipOverGraph());
    REQUIRE(hipsX() == Approx(10.5f));
    scene.update(0.2f);
    REQUIRE_FALSE(animator.isClipOverGraph());
    REQUIRE_FALSE(animator.getClipAsset());
    REQUIRE(hipsX() == Approx(1.0f));
  }

  SECTION("A looping clip stays until stop")
  {
    animator.play(createGraphTestClip(20.0f, 20.0f, 1.0f));
    REQUIRE(hipsX() == Approx(20.0f));
    scene.update(5.0f);
    REQUIRE(hipsX() == Approx(20.0f));
    animator.stop();
    REQUIRE_FALSE(animator.isClipOverGraph());
    REQUIRE(hipsX() == Approx(1.0f));
  }

  SECTION("Pausing stops the graph")
  {
    const float time = instance.getStateTime(0);
    animator.setPlaying(false);
    REQUIRE_FALSE(animator.isPlaying());
    scene.update(0.5f);
    REQUIRE(instance.getStateTime(0) == time);
    animator.setPlaying(true);
    scene.update(0.5f);
    REQUIRE(instance.getStateTime(0) == Approx(time + 0.5f));
  }

  SECTION("The graph is saved, a clip over it is not")
  {
    animator.play(createGraphTestClip(20.0f, 20.0f, 1.0f));
    MemoryDataStream stream;
    animator.serialize(stream);
    stream.seek(0);
    AnimatorComponent loaded;
    REQUIRE(loaded.deserialize(stream));
    // The tests have no asset manager, so the graph is missing but its id is kept.
    REQUIRE_FALSE(loaded.hasGraph());
    Vector<chEngineSDK::UUID> references;
    loaded.collectReferences(references);
    REQUIRE(references == Vector<chEngineSDK::UUID>{graphAsset->getUUID()});

    const Vector<uint8> bytes = readAllBytes(stream);
    for (const SIZE_T size : {SIZE_T(0), SIZE_T(4), bytes.size() - 1}) {
      MemoryDataStream cut;
      cut.write(bytes.data(), size);
      cut.seek(0);
      AnimatorComponent fromCut;
      REQUIRE_FALSE(fromCut.deserialize(cut));
    }
  }

  SECTION("Without the graph it plays clips again")
  {
    animator.setGraph(nullptr);
    REQUIRE_FALSE(animator.hasGraph());
    REQUIRE(hipsX() == 0.0f);
    animator.play(createGraphTestClip(20.0f, 20.0f, 1.0f));
    REQUIRE(hipsX() == Approx(20.0f));

    // A clip that does not loop keeps playing after the update play runs at once.
    animator.setLooping(false);
    animator.play(createGraphTestClip(0.0f, 4.0f, 1.0f));
    REQUIRE(animator.isPlaying());
    scene.update(0.5f);
    REQUIRE(hipsX() == Approx(2.0f));
  }
}
#if USING(CH_EDITOR)
TEST_CASE("chCore - Animation graph layout", "[Animation]")
{
  AssetMetadata metadata;
  metadata.uuid = chEngineSDK::UUID::createRandom();
  metadata.assetType = AssetTypeTraits<AnimationGraphAsset>::getTypeId();
  LayoutTestGraphAsset asset(metadata);

  AnimationGraphLayout layout;
  layout.layers.push_back({.statePositions = {Vector2(0.0f, 0.0f), Vector2(260.5f, -140.0f)},
                           .anyStatePosition = Vector2(-260.0f, 12.25f)});
  layout.layers.push_back({.anyStatePosition = Vector2(1.0f, 2.0f)});
  layout.previewModel = chEngineSDK::UUID::createRandom();
  asset.setLayout(layout);

  ConfigFile file;
  asset.writeEditorValues(file);
  ConfigFile parsed;
  parsed.parse(file.toText());
  LayoutTestGraphAsset read(metadata);
  read.readEditorValues(parsed);

  const AnimationGraphLayout& readLayout = read.getLayout();
  REQUIRE(readLayout.layers.size() == 2);
  REQUIRE(readLayout.layers[0].statePositions.size() == 2);
  REQUIRE(readLayout.layers[0].statePositions[1] == Vector2(260.5f, -140.0f));
  REQUIRE(readLayout.layers[0].anyStatePosition == Vector2(-260.0f, 12.25f));
  REQUIRE(readLayout.layers[1].statePositions.empty());
  REQUIRE(readLayout.layers[1].anyStatePosition == Vector2(1.0f, 2.0f));
  REQUIRE(readLayout.previewModel == layout.previewModel);

  SECTION("Broken text keeps what it could read")
  {
    ConfigFile broken;
    broken.parse("[GraphLayout]\nLayer0=1 2 3 x\nLayer0Any=nope\n");
    LayoutTestGraphAsset partial(metadata);
    partial.readEditorValues(broken);
    REQUIRE(partial.getLayout().layers.size() == 1);
    REQUIRE(partial.getLayout().layers[0].statePositions.size() == 1);
    REQUIRE(partial.getLayout().layers[0].anyStatePosition == Vector2::ZERO);
  }
}
#endif // USING(CH_EDITOR)

#if USING(CH_EDITOR)
TEST_CASE("chCore - Animation graph editing", "[Animation]")
{
  AnimationGraph graph = buildTestGraph(createTestGraphParts());
  String error;

  SECTION("Removing a parameter keeps the others in place")
  {
    REQUIRE(graph.countParameterUses(0) == 4);
    REQUIRE(graph.countParameterUses(1) == 1);
    graph.removeParameter(1);
    REQUIRE(graph.getParameters().size() == 5);
    const AnimationLayer& base = graph.getLayers()[0];
    REQUIRE(base.transitions[1].conditions.size() == 1);
    REQUIRE(base.transitions[1].conditions[0].parameter == 0);
    REQUIRE(base.states[2].parameterX == 3);
    REQUIRE(base.states[2].parameterY == 4);
    REQUIRE(base.transitions[2].conditions[0].parameter == 2);
    REQUIRE(graph.getLayers()[1].transitions[0].conditions[0].parameter == 1);
    REQUIRE(graph.validate(&error));
  }

  SECTION("A removed blend parameter leaves the blend space broken")
  {
    graph.removeParameter(0);
    const AnimationLayer& base = graph.getLayers()[0];
    REQUIRE(base.states[1].parameterX == AnimationGraph::NO_PARAMETER);
    REQUIRE(base.states[2].speedParameter == AnimationGraph::NO_PARAMETER);
    REQUIRE(base.transitions[0].conditions.empty());
    REQUIRE_FALSE(graph.validate(&error));
  }

  SECTION("Removing a clip")
  {
    REQUIRE(graph.countClipUses(2) == 3);
    graph.removeClip(2);
    REQUIRE(graph.getClips().size() == 3);
    const AnimationLayer& base = graph.getLayers()[0];
    REQUIRE(base.states[1].samples.size() == 2);
    REQUIRE(base.states[2].samples.size() == 2);
    REQUIRE(base.states[3].clip == 2);
    REQUIRE(graph.getLayers()[1].states[0].clip == 2);
    REQUIRE(graph.getLayers()[1].states[1].clip == AnimationGraph::NOT_FOUND);
    REQUIRE_FALSE(graph.validate(&error));
    REQUIRE(StringUtils::containsIgnoreCase(error, "clip that does not exist"));
  }

  SECTION("Clips are found or added, and unused ones go")
  {
    const chEngineSDK::UUID existing = graph.getClips()[1].clipId;
    REQUIRE(graph.findOrAddClip(existing, nullptr) == 1);
    const chEngineSDK::UUID extra = chEngineSDK::UUID::createRandom();
    REQUIRE(graph.findOrAddClip(extra, nullptr) == 4);
    REQUIRE(graph.countClipUses(4) == 0);
    graph.removeUnusedClips();
    REQUIRE(graph.getClips().size() == 4);
    REQUIRE(graph.validate(&error));
  }

  SECTION("Removing a state drops its transitions")
  {
    graph.removeState(0, 0);
    const AnimationLayer& base = graph.getLayers()[0];
    REQUIRE(base.states.size() == 3);
    REQUIRE(base.transitions.size() == 1);
    REQUIRE(base.transitions[0].source == AnimationGraph::ANY_STATE);
    REQUIRE(base.transitions[0].target == 2);
    REQUIRE(base.defaultState == 0);
    REQUIRE(graph.validate(&error));

    AnimationGraph other = buildTestGraph(createTestGraphParts());
    other.getLayersForEdit()[0].defaultState = 2;
    other.removeState(0, 1);
    REQUIRE(other.getLayers()[0].defaultState == 1);
  }

  SECTION("Removing a layer")
  {
    graph.removeLayer(1);
    REQUIRE(graph.getLayers().size() == 1);
    graph.removeLayer(5);
    REQUIRE(graph.getLayers().size() == 1);
  }
}
#endif // USING(CH_EDITOR)
