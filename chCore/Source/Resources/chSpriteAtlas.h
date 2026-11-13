/************************************************************************/
/**
 * @file chSpriteAtlas.h
 * @author AccelMR
 * @date 2026/11/13
 * @brief
 *  The frames and animations of one sprite sheet.
 */
/************************************************************************/
#pragma once

#include "chPrerequisitesCore.h"

#include "chUUID.h"
#include "chVector2.h"

namespace chEngineSDK {
class DataStream;

/**
 * One image of a sprite sheet: where it is in the texture and which point of it stands at
 * the object.
 */
struct SpriteAtlasFrame
{
  String name;
  // Top left corner in pixels, from the top left of the texture.
  Vector2 position = Vector2::ZERO;
  // Width and height in pixels.
  Vector2 size = Vector2::ZERO;
  // (0, 0) is the bottom left corner of the frame, (1, 1) the top right one.
  Vector2 pivot{0.5f, 0.5f};
};

/**
 * One step of an animation: a frame of the atlas shown for some time.
 */
struct SpriteAnimationFrame
{
  uint32 frame = 0;
  // Seconds.
  float duration = 0.1f;
};

/**
 * Frames of the atlas shown one after the other.
 */
struct CH_CORE_EXPORT SpriteAnimation
{
  String name;
  Vector<SpriteAnimationFrame> frames;
  bool bLoop = true;

  /**
   * The sum of the frame durations.
   */
  NODISCARD float
  getDuration() const;

  /**
   * The step shown at time (seconds from the start): wrapped when it loops, the last one
   * after the end when it does not. 0 for an animation without steps.
   */
  NODISCARD uint32
  getStepAt(float time) const;
};

/**
 * Exists so a sprite sheet is described once and every sprite that uses it shares the
 * description: the texture, its frames by name and the animations made of them, like the
 * tags of a sheet in a drawing program. Sprites refer to frames by index, so the order of
 * the frames is kept when saved.
 */
class CH_CORE_EXPORT SpriteAtlas
{
 public:
  NODISCARD FORCEINLINE const UUID&
  getTextureId() const noexcept { return m_textureId; }

  FORCEINLINE void
  setTextureId(const UUID& textureId) noexcept { m_textureId = textureId; }

  NODISCARD FORCEINLINE const Vector<SpriteAtlasFrame>&
  getFrames() const noexcept { return m_frames; }

  NODISCARD FORCEINLINE Vector<SpriteAtlasFrame>&
  getFrames() noexcept { return m_frames; }

  NODISCARD FORCEINLINE const Vector<SpriteAnimation>&
  getAnimations() const noexcept { return m_animations; }

  NODISCARD FORCEINLINE Vector<SpriteAnimation>&
  getAnimations() noexcept { return m_animations; }

  /**
   * Index of the frame or animation with that name (ignoring case), or NOT_FOUND. Walks the
   * list, so it is meant for load time and scripts' onStart, not for every frame.
   */
  NODISCARD uint32
  findFrame(StringView name) const;

  NODISCARD uint32
  findAnimation(StringView name) const;

  /**
   * False with the reason when the atlas cannot be used: a frame without size, two frames or
   * two animations with one name, an animation step past the frames or with a duration
   * that is not positive.
   */
  NODISCARD bool
  validate(String& outError) const;

  void
  serialize(DataStream& stream) const;

  /**
   * False for broken data, another version or an atlas validate refuses.
   */
  NODISCARD bool
  deserialize(DataStream& stream, String& outError);

#if USING(CH_EDITOR)
  /**
   * Frames of a grid of columns by rows over the texture, after margin pixels at its edges
   * and spacing pixels between cells, named prefix_0, prefix_1... left to right, top to
   * bottom.
   */
  static void
  makeGridFrames(uint32 textureWidth,
                 uint32 textureHeight,
                 uint32 columns,
                 uint32 rows,
                 uint32 margin,
                 uint32 spacing,
                 StringView prefix,
                 Vector<SpriteAtlasFrame>& outFrames);

  /**
   * Frames of cellWidth by cellHeight pixels, as many as fit whole, laid out like
   * makeGridFrames.
   */
  static void
  makeCellFrames(uint32 textureWidth,
                 uint32 textureHeight,
                 uint32 cellWidth,
                 uint32 cellHeight,
                 uint32 margin,
                 uint32 spacing,
                 StringView prefix,
                 Vector<SpriteAtlasFrame>& outFrames);

  /**
   * A frame around each group of touching pixels (sides, not corners) whose alpha is above
   * alphaThreshold, grown by padding pixels and kept inside the image, in reading order (top
   * to bottom, then left to right). rgba holds width * height pixels of 4 bytes.
   */
  static void
  detectFrames(Span<const uint8> rgba,
               uint32 width,
               uint32 height,
               uint8 alphaThreshold,
               uint32 padding,
               StringView prefix,
               Vector<SpriteAtlasFrame>& outFrames);
#endif // USING(CH_EDITOR)

  static constexpr uint32 NOT_FOUND = 0xFFFFFFFFu;
  static constexpr uint32 SERIALIZATION_VERSION = 1;
  static constexpr uint32 MAX_FRAMES = 65536;
  static constexpr uint32 MAX_ANIMATIONS = 4096;
  static constexpr uint32 MAX_NAME_LENGTH = 256;

 private:
  UUID m_textureId = UUID::null();
  Vector<SpriteAtlasFrame> m_frames;
  Vector<SpriteAnimation> m_animations;
};

} // namespace chEngineSDK
