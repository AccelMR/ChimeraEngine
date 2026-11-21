/************************************************************************/
/**
 * @file chBcTextureCompressor.h
 * @author AccelMR
 * @date 2026/11/20
 * @brief
 *  BC1, BC4, BC5 and BC7 encoding with bc7enc_rdo.
 */
/************************************************************************/
#pragma once

#include "chTextureCooker.h"

namespace chEngineSDK {

/**
 * Gives Package Project the BC formats through bc7enc_rdo, which lives only in this plugin.
 */
class BcTextureCompressor : public ITextureCompressor
{
 public:
  BcTextureCompressor();

  NODISCARD bool
  supportsFormat(Format format) const override;

  void
  compressBlocks(Format format,
                 const uint8* blocks,
                 SIZE_T blockCount,
                 TextureCompressionQuality quality,
                 uint8* output) const override;
};

} // namespace chEngineSDK
