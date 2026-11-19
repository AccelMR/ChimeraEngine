/************************************************************************/
/**
 * @file chRenderTestHelpers.h
 * @author AccelMR
 * @date 2026/11/17
 * @brief Textures that only exist on the CPU, so render graph tests need no GPU.
 */
/************************************************************************/
#pragma once

#include "chITexture.h"
#include "chITextureView.h"
#include "chRenderGraph.h"

#include "catch.hpp"

namespace chEngineSDK {

// Stands in for a swap chain image: the graph only reads its description.
class FakeTexture : public ITexture
{
 public:
  FakeTexture(Format format, uint32 width, uint32 height)
    : m_format(format),
      m_width(width),
      m_height(height)
  {}

  NODISCARD TextureType
  getType() const override { return TextureType::Texture2D; }

  NODISCARD Format
  getFormat() const override { return m_format; }

  NODISCARD uint32
  getWidth() const override { return m_width; }

  NODISCARD uint32
  getHeight() const override { return m_height; }

  NODISCARD uint32
  getDepth() const override { return 1; }

  NODISCARD uint32
  getMipLevels() const override { return 1; }

  NODISCARD uint32
  getArrayLayers() const override { return 1; }

  NODISCARD uint32
  getBindlessIndex() const override { return GraphicsLimits::INVALID_BINDLESS_INDEX; }

  NODISCARD SPtr<ITextureView>
  createView(const TextureViewCreateInfo& createInfo) override
  {
    CH_PARAMETER_UNUSED(createInfo);
    return nullptr;
  }

  void
  uploadData(const void* data, SIZE_T size) override
  {
    CH_PARAMETER_UNUSED(data);
    CH_PARAMETER_UNUSED(size);
  }

 private:
  Format m_format;
  uint32 m_width;
  uint32 m_height;
};

class FakeTextureView : public ITextureView
{
 public:
  NODISCARD Format
  getFormat() const override { return Format::Unknown; }

  NODISCARD TextureViewType
  getViewType() const override { return TextureViewType::View2D; }

  NODISCARD uint32
  getBaseMipLevel() const override { return 0; }

  NODISCARD uint32
  getMipLevelCount() const override { return 1; }

  NODISCARD uint32
  getBaseArrayLayer() const override { return 0; }

  NODISCARD uint32
  getArrayLayerCount() const override { return 1; }

  NODISCARD uint32
  getBindlessIndex() const override { return GraphicsLimits::INVALID_BINDLESS_INDEX; }

  NODISCARD uint32
  getStorageBindlessIndex() const override { return GraphicsLimits::INVALID_BINDLESS_INDEX; }
};

inline bool
hasBarrier(Span<const RGBarrier> barriers,
           RGTextureHandle texture,
           ResourceState before,
           ResourceState after)
{
  for (const RGBarrier& barrier : barriers) {
    if (barrier.texture == texture && barrier.before == before && barrier.after == after) {
      return true;
    }
  }
  return false;
}

} // namespace chEngineSDK
