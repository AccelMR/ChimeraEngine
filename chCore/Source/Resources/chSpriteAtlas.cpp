/************************************************************************/
/**
 * @file chSpriteAtlas.cpp
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  The frames and animations of one sprite sheet.
 */
/************************************************************************/
#include "chSpriteAtlas.h"

#include "chAlgorithm.h"
#include "chAssetFile.h"
#include "chFileStream.h"
#include "chMath.h"
#include "chStringUtils.h"

namespace chEngineSDK {
namespace {
struct FrameFileData
{
  float position[2];
  float size[2];
  float pivot[2];
};
static_assert(sizeof(FrameFileData) == 24, "FrameFileData must have no padding");

struct StepFileData
{
  uint32 frame;
  float duration;
};
static_assert(sizeof(StepFileData) == 8, "StepFileData must have no padding");

NODISCARD uint32
findByName(const auto& entries, const StringView name)
{
  for (SIZE_T i = 0; i < entries.size(); ++i) {
    if (StringUtils::equalsIgnoreCase(entries[i].name, name)) {
      return static_cast<uint32>(i);
    }
  }
  return SpriteAtlas::NOT_FOUND;
}

/*
 * True when two entries share a name (ignoring case); outName gets it.
 */
NODISCARD bool
hasDuplicateName(const auto& entries, String& outName)
{
  for (SIZE_T i = 0; i < entries.size(); ++i) {
    for (SIZE_T j = i + 1; j < entries.size(); ++j) {
      if (StringUtils::equalsIgnoreCase(entries[i].name, entries[j].name)) {
        outName = entries[i].name;
        return true;
      }
    }
  }
  return false;
}
} // namespace

/*
 */
float
SpriteAnimation::getDuration() const
{
  float duration = 0.0f;
  for (const SpriteAnimationFrame& step : frames) {
    duration += step.duration;
  }
  return duration;
}

/*
 */
uint32
SpriteAnimation::getStepAt(float time) const
{
  if (frames.empty()) {
    return 0;
  }
  const float duration = getDuration();
  if (duration <= 0.0f) {
    return 0;
  }
  if (bLoop) {
    time = Math::fmod(time, duration);
    if (time < 0.0f) {
      time += duration;
    }
  }
  else if (time >= duration) {
    return static_cast<uint32>(frames.size() - 1);
  }
  for (SIZE_T step = 0; step < frames.size(); ++step) {
    time -= frames[step].duration;
    if (time < 0.0f) {
      return static_cast<uint32>(step);
    }
  }
  return static_cast<uint32>(frames.size() - 1);
}

/*
 */
uint32
SpriteAtlas::findFrame(StringView name) const
{
  return findByName(m_frames, name);
}

/*
 */
uint32
SpriteAtlas::findAnimation(StringView name) const
{
  return findByName(m_animations, name);
}

/*
 */
bool
SpriteAtlas::validate(String& outError) const
{
  String name;
  if (hasDuplicateName(m_frames, name)) {
    outError = StringUtils::format("two frames are named '{0}'", name);
    return false;
  }
  if (hasDuplicateName(m_animations, name)) {
    outError = StringUtils::format("two animations are named '{0}'", name);
    return false;
  }
  for (const SpriteAtlasFrame& frame : m_frames) {
    if (!(frame.size.x > 0.0f) || !(frame.size.y > 0.0f) || !Math::isFinite(frame.size.x) ||
        !Math::isFinite(frame.size.y) || !Math::isFinite(frame.position.x) ||
        !Math::isFinite(frame.position.y)) {
      outError = StringUtils::format("frame '{0}' has no size", frame.name);
      return false;
    }
  }
  for (const SpriteAnimation& animation : m_animations) {
    for (const SpriteAnimationFrame& step : animation.frames) {
      if (step.frame >= m_frames.size()) {
        outError = StringUtils::format("animation '{0}' shows a frame that does not exist",
                                       animation.name);
        return false;
      }
      if (!(step.duration > 0.0f) || !Math::isFinite(step.duration)) {
        outError = StringUtils::format("animation '{0}' has a step without duration",
                                       animation.name);
        return false;
      }
    }
  }
  return true;
}

/*
 */
void
SpriteAtlas::serialize(DataStream& stream) const
{
  const uint32 version = SERIALIZATION_VERSION;
  const uint32 frameCount = static_cast<uint32>(m_frames.size());
  const uint32 animationCount = static_cast<uint32>(m_animations.size());
  stream << version;
  stream << m_textureId;
  stream << frameCount;
  for (const SpriteAtlasFrame& frame : m_frames) {
    AssetFile::writeString(stream, frame.name);
    const FrameFileData data{.position = {frame.position.x, frame.position.y},
                             .size = {frame.size.x, frame.size.y},
                             .pivot = {frame.pivot.x, frame.pivot.y}};
    stream << data;
  }
  stream << animationCount;
  for (const SpriteAnimation& animation : m_animations) {
    AssetFile::writeString(stream, animation.name);
    const uint32 loop = animation.bLoop ? 1u : 0u;
    const uint32 stepCount = static_cast<uint32>(animation.frames.size());
    stream << loop;
    stream << stepCount;
    for (const SpriteAnimationFrame& step : animation.frames) {
      const StepFileData data{.frame = step.frame, .duration = step.duration};
      stream << data;
    }
  }
}

/*
 */
bool
SpriteAtlas::deserialize(DataStream& stream, String& outError)
{
  m_frames.clear();
  m_animations.clear();
  uint32 version = 0;
  uint32 frameCount = 0;
  if (!AssetFile::readValue(stream, version) || version != SERIALIZATION_VERSION) {
    outError = "unknown version";
    return false;
  }
  if (!AssetFile::readValue(stream, m_textureId) ||
      !AssetFile::readValue(stream, frameCount) || frameCount > MAX_FRAMES ||
      frameCount * sizeof(FrameFileData) > AssetFile::getBytesLeft(stream)) {
    outError = "the frames are cut";
    return false;
  }
  m_frames.resize(frameCount);
  for (SpriteAtlasFrame& frame : m_frames) {
    FrameFileData data;
    if (!AssetFile::readString(stream, frame.name, MAX_NAME_LENGTH) ||
        !AssetFile::readValue(stream, data)) {
      outError = "the frames are cut";
      return false;
    }
    frame.position = Vector2(data.position[0], data.position[1]);
    frame.size = Vector2(data.size[0], data.size[1]);
    frame.pivot = Vector2(data.pivot[0], data.pivot[1]);
  }

  uint32 animationCount = 0;
  if (!AssetFile::readValue(stream, animationCount) || animationCount > MAX_ANIMATIONS) {
    outError = "the animations are cut";
    return false;
  }
  m_animations.resize(animationCount);
  for (SpriteAnimation& animation : m_animations) {
    uint32 loop = 0;
    uint32 stepCount = 0;
    if (!AssetFile::readString(stream, animation.name, MAX_NAME_LENGTH) ||
        !AssetFile::readValue(stream, loop) || !AssetFile::readValue(stream, stepCount) ||
        stepCount > MAX_FRAMES ||
        stepCount * sizeof(StepFileData) > AssetFile::getBytesLeft(stream)) {
      outError = "the animations are cut";
      return false;
    }
    animation.bLoop = loop != 0;
    animation.frames.resize(stepCount);
    for (SpriteAnimationFrame& step : animation.frames) {
      StepFileData data;
      if (!AssetFile::readValue(stream, data)) {
        outError = "the animations are cut";
        return false;
      }
      step.frame = data.frame;
      step.duration = data.duration;
    }
  }
  return validate(outError);
}

#if USING(CH_EDITOR)
/*
 */
void
SpriteAtlas::makeGridFrames(uint32 textureWidth,
                            uint32 textureHeight,
                            uint32 columns,
                            uint32 rows,
                            uint32 margin,
                            uint32 spacing,
                            StringView prefix,
                            Vector<SpriteAtlasFrame>& outFrames)
{
  outFrames.clear();
  if (columns == 0 || rows == 0) {
    return;
  }
  const int64 usableWidth = static_cast<int64>(textureWidth) -
                            2 * static_cast<int64>(margin) -
                            static_cast<int64>(spacing) * (columns - 1);
  const int64 usableHeight = static_cast<int64>(textureHeight) -
                             2 * static_cast<int64>(margin) -
                             static_cast<int64>(spacing) * (rows - 1);
  if (usableWidth < columns || usableHeight < rows) {
    return;
  }
  makeCellFrames(textureWidth, textureHeight, static_cast<uint32>(usableWidth / columns),
                 static_cast<uint32>(usableHeight / rows), margin, spacing, prefix, outFrames);
}

/*
 */
void
SpriteAtlas::makeCellFrames(uint32 textureWidth,
                            uint32 textureHeight,
                            uint32 cellWidth,
                            uint32 cellHeight,
                            uint32 margin,
                            uint32 spacing,
                            StringView prefix,
                            Vector<SpriteAtlasFrame>& outFrames)
{
  outFrames.clear();
  if (cellWidth == 0 || cellHeight == 0) {
    return;
  }
  uint32 index = 0;
  const uint64 stepX = static_cast<uint64>(cellWidth) + spacing;
  const uint64 stepY = static_cast<uint64>(cellHeight) + spacing;
  for (uint64 y = margin; y + cellHeight + margin <= textureHeight; y += stepY) {
    for (uint64 x = margin; x + cellWidth + margin <= textureWidth; x += stepX) {
      outFrames.push_back(
          {.name = StringUtils::format("{0}_{1}", prefix, index++),
           .position = Vector2(static_cast<float>(x), static_cast<float>(y)),
           .size = Vector2(static_cast<float>(cellWidth), static_cast<float>(cellHeight)),
           .pivot = Vector2(0.5f, 0.5f)});
    }
  }
}

/*
 */
void
SpriteAtlas::detectFrames(Span<const uint8> rgba,
                          uint32 width,
                          uint32 height,
                          uint8 alphaThreshold,
                          uint32 padding,
                          StringView prefix,
                          Vector<SpriteAtlasFrame>& outFrames)
{
  outFrames.clear();
  const SIZE_T pixelCount = static_cast<SIZE_T>(width) * height;
  if (pixelCount == 0 || rgba.size() < pixelCount * 4) {
    return;
  }

  struct Region
  {
    uint32 minX;
    uint32 minY;
    uint32 maxX;
    uint32 maxY;
  };
  Vector<Region> regions;
  Vector<uint8> visited(pixelCount, 0);
  Vector<uint32> stack;
  const auto isOpaque = [&rgba, alphaThreshold](SIZE_T pixel) {
    return rgba[pixel * 4 + 3] > alphaThreshold;
  };

  for (SIZE_T start = 0; start < pixelCount; ++start) {
    if (visited[start] != 0 || !isOpaque(start)) {
      continue;
    }
    Region region{.minX = width, .minY = height, .maxX = 0, .maxY = 0};
    visited[start] = 1;
    stack.push_back(static_cast<uint32>(start));
    while (!stack.empty()) {
      const uint32 pixel = stack.back();
      stack.pop_back();
      const uint32 x = pixel % width;
      const uint32 y = pixel / width;
      region.minX = Math::min(region.minX, x);
      region.minY = Math::min(region.minY, y);
      region.maxX = Math::max(region.maxX, x);
      region.maxY = Math::max(region.maxY, y);
      const auto visit = [&](uint32 neighbor) {
        if (visited[neighbor] == 0 && isOpaque(neighbor)) {
          visited[neighbor] = 1;
          stack.push_back(neighbor);
        }
      };
      if (x > 0) {
        visit(pixel - 1);
      }
      if (x + 1 < width) {
        visit(pixel + 1);
      }
      if (y > 0) {
        visit(pixel - width);
      }
      if (y + 1 < height) {
        visit(pixel + width);
      }
    }
    regions.push_back(region);
  }

  Algorithm::sort(regions, [](const Region& a, const Region& b) {
    return a.minY != b.minY ? a.minY < b.minY : a.minX < b.minX;
  });
  outFrames.reserve(regions.size());
  for (SIZE_T i = 0; i < regions.size(); ++i) {
    const Region& region = regions[i];
    const uint32 left = region.minX > padding ? region.minX - padding : 0;
    const uint32 top = region.minY > padding ? region.minY - padding : 0;
    const uint32 right = Math::min(region.maxX + padding, width - 1);
    const uint32 bottom = Math::min(region.maxY + padding, height - 1);
    outFrames.push_back({.name = StringUtils::format("{0}_{1}", prefix, i),
                         .position =
                             Vector2(static_cast<float>(left), static_cast<float>(top)),
                         .size = Vector2(static_cast<float>(right - left + 1),
                                         static_cast<float>(bottom - top + 1)),
                         .pivot = Vector2(0.5f, 0.5f)});
  }
}
#endif // USING(CH_EDITOR)

} // namespace chEngineSDK
