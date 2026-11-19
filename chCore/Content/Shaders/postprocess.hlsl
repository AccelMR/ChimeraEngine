// The vertex shader of every post-process pass whose shader has no VSMain of its own.

#include "chPostProcessCommon.hlsli"

PostProcessInput
VSMain(uint vertexId : SV_VertexID)
{
  return postProcessVertex(vertexId);
}
