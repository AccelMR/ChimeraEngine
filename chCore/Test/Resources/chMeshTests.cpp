/************************************************************************/
/**
 * @file chMeshTests.cpp
 * @author AccelMR
 * @date 2026/11/17
 * @brief Tests of meshes: bounds, part split, skin weights, generated shapes and texture mips.
 */
/************************************************************************/
#include <cstring>
#include <limits>

#include "chAssetMeta.h"
#include "chBox.h"
#include "chCapsule.h"
#include "chFloat16.h"
#include "chGraphicsTypes.h"
#include "chMaterial.h"
#include "chMath.h"
#include "chMesh.h"
#include "chMeshGenerator.h"
#include "chMeshSplit.h"
#include "chModel.h"
#include "chModelAsset.h"
#include "chSphere.h"
#include "chSphereBoxBounds.h"
#include "chStringUtils.h"
#include "chTextureMips.h"
#include "chUUID.h"
#include "chVector2.h"
#include "chVector3.h"
#include "chVector4.h"
#include "chVertexLayout.h"

#include "catch.hpp"
#include "chModelTestHelpers.h"

using namespace chEngineSDK;

TEST_CASE("chCore - Mesh bounds", "[Resources]")
{
  const SPtr<Model> model = createTriangleModel();
  const SphereBoxBounds& bounds = model->getMeshes()[0]->getBounds();
  REQUIRE(bounds.center.nearEqual(Vector3(0.0f, 1.0f, 0.0f)));
  REQUIRE(bounds.boxExtent.nearEqual(Vector3(1.0f, 1.0f, 0.0f)));
  // The farthest vertices from the center are (1, 0, 0) and (-1, 0, 0).
  REQUIRE(bounds.sphereRadius == Approx(Math::sqrt(2.0f)));

  const Mesh empty;
  REQUIRE(empty.getBounds().sphereRadius == 0.0f);
}

namespace {

// A square made of two triangles at x, with its corners repeated for each triangle (no
// shared index), like the faces of a mesh cut along UV seams.
void
appendSquare(Vector<VertexStatic>& vertices, Vector<uint16>& indices, float x, float size)
{
  const Vector3 corners[4] = {Vector3(x, 0.0f, 0.0f), Vector3(x + size, 0.0f, 0.0f),
                              Vector3(x + size, size, 0.0f), Vector3(x, size, 0.0f)};
  const uint32 order[6] = {0, 1, 2, 0, 2, 3};
  for (const uint32 corner : order) {
    indices.push_back(static_cast<uint16>(vertices.size()));
    vertices.push_back({corners[corner], Vector3::UP, Vector2(0.0f, 0.0f),
                        Vector4(1.0f, 0.0f, 0.0f, 1.0f)});
  }
}

SPtr<Mesh>
createSquaresMesh(Span<const float> starts, float size)
{
  Vector<VertexStatic> vertices;
  Vector<uint16> indices;
  for (const float x : starts) {
    appendSquare(vertices, indices, x, size);
  }
  SPtr<Mesh> mesh = chMakeShared<Mesh>();
  mesh->setVertexData(vertices);
  mesh->setIndexData(indices);
  mesh->setMaterialSlot(3);
  return mesh;
}

} // namespace

TEST_CASE("chCore - Mesh split", "[Resources]")
{
  SECTION("Triangles that only share positions stay one piece")
  {
    const float starts[] = {0.0f};
    const SPtr<Mesh> mesh = createSquaresMesh(starts, 1.0f);
    const Vector<SPtr<Mesh>> parts = MeshSplit::splitParts(mesh);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0] == mesh);
  }

  SECTION("Pieces apart become meshes of their own")
  {
    const float starts[] = {0.0f, 5.0f, 10.0f};
    const Vector<SPtr<Mesh>> parts = MeshSplit::splitParts(createSquaresMesh(starts, 1.0f));
    REQUIRE(parts.size() == 3);
    for (SIZE_T i = 0; i < parts.size(); ++i) {
      const Mesh& part = *parts[i];
      REQUIRE(part.getMaterialSlot() == 3);
      REQUIRE(part.getIndexCount() == 6);
      REQUIRE(part.getVertexCount() == 6);
      REQUIRE(part.getVertexLayout().getVertexSize() == sizeof(VertexStatic));
      // In the order the triangles meet them.
      REQUIRE(part.getBounds().center.nearEqual(Vector3(starts[i] + 0.5f, 0.5f, 0.0f)));
    }
  }

  SECTION("Pieces whose boxes overlap or touch are joined")
  {
    // The second square starts where the first ends; the third overlaps the second.
    const float starts[] = {0.0f, 1.0f, 1.5f, 20.0f};
    const Vector<SPtr<Mesh>> parts = MeshSplit::splitParts(createSquaresMesh(starts, 1.0f));
    REQUIRE(parts.size() == 2);
    REQUIRE(parts[0]->getIndexCount() == 18);
    REQUIRE(parts[1]->getIndexCount() == 6);
  }

  SECTION("A mesh without positions is kept")
  {
    const SPtr<Mesh> empty = chMakeShared<Mesh>();
    const Vector<SPtr<Mesh>> parts = MeshSplit::splitParts(empty);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0] == empty);
  }
}

TEST_CASE("chCore - Mesh skin weights", "[Resources]")
{
  SECTION("Weights are scaled to 255 and sorted, strongest first")
  {
    const VertexSkin vertex =
        MeshSkin::packVertex(Vector<BoneInfluence>{{3, 0.25f}, {7, 0.75f}});
    REQUIRE(vertex.bones[0] == 7);
    REQUIRE(vertex.bones[1] == 3);
    REQUIRE(vertex.weights[0] == 191);
    REQUIRE(vertex.weights[1] == 64);
    REQUIRE(vertex.weights[2] == 0);
    REQUIRE(vertex.weights[3] == 0);
  }

  SECTION("Only the four strongest stay, and the sum is still 255")
  {
    const VertexSkin vertex = MeshSkin::packVertex(Vector<BoneInfluence>{
        {0, 0.1f}, {1, 0.3f}, {2, 0.05f}, {3, 0.2f}, {4, 0.25f}, {5, 0.1f}});
    REQUIRE(vertex.bones[0] == 1);
    REQUIRE(vertex.bones[1] == 4);
    REQUIRE(vertex.bones[2] == 3);
    REQUIRE((vertex.bones[3] == 0 || vertex.bones[3] == 5));
    uint32 sum = 0;
    for (const uint8 weight : vertex.weights) {
      sum += weight;
    }
    REQUIRE(sum == 255);
  }

  SECTION("Three equal thirds round to 255")
  {
    const VertexSkin vertex =
        MeshSkin::packVertex(Vector<BoneInfluence>{{0, 1.0f}, {1, 1.0f}, {2, 1.0f}});
    REQUIRE(vertex.weights[0] + vertex.weights[1] + vertex.weights[2] == 255);
  }

  SECTION("No weight, zero or NaN gives the first bone everything")
  {
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    for (const VertexSkin& vertex : {MeshSkin::packVertex({}),
                                     MeshSkin::packVertex(Vector<BoneInfluence>{{4, 0.0f}}),
                                     MeshSkin::packVertex(
                                         Vector<BoneInfluence>{{4, notANumber}})}) {
      REQUIRE(vertex.bones[0] == 0);
      REQUIRE(vertex.weights[0] == 255);
      REQUIRE(vertex.weights[1] == 0);
    }
  }
}

TEST_CASE("chCore - Texture mips", "[Resources]")
{
  REQUIRE(TextureMips::getMipCount(1, 1) == 1);
  REQUIRE(TextureMips::getMipCount(256, 256) == 9);
  REQUIRE(TextureMips::getMipCount(300, 17) == 9);

  SECTION("1x1 has no levels to add")
  {
    Vector<uint8> pixels = {10, 20, 30, 40};
    REQUIRE(TextureMips::appendChainRGBA8(pixels, 1, 1, true) == 1);
    REQUIRE(pixels.size() == 4);
  }

  SECTION("Sizes of a non square chain")
  {
    Vector<uint8> pixels(4 * 2 * 4, 255);
    REQUIRE(TextureMips::appendChainRGBA8(pixels, 4, 2, false) == 3);
    // 4x2, 2x1, 1x1.
    REQUIRE(pixels.size() == (8 + 2 + 1) * 4);
    for (uint8 value : pixels) {
      REQUIRE(value == 255);
    }
  }

  // A black and a white pixel, alpha 0 and 255.
  const Vector<uint8> blackAndWhite = {0, 0, 0, 0, 255, 255, 255, 255};

  SECTION("Linear data is averaged as is")
  {
    Vector<uint8> pixels = blackAndWhite;
    REQUIRE(TextureMips::appendChainRGBA8(pixels, 2, 1, false) == 2);
    REQUIRE(pixels.size() == 12);
    for (SIZE_T i = 8; i < 12; ++i) {
      REQUIRE(pixels[i] == 128);
    }
  }

  SECTION("sRGB colors are averaged as linear light, alpha as is")
  {
    Vector<uint8> pixels = blackAndWhite;
    REQUIRE(TextureMips::appendChainRGBA8(pixels, 2, 1, true) == 2);
    // Half the light is 0.735 in sRGB, not 0.5.
    REQUIRE(pixels[8] == 188);
    REQUIRE(pixels[9] == 188);
    REQUIRE(pixels[10] == 188);
    REQUIRE(pixels[11] == 128);
  }

  SECTION("HDR chains keep values above 1 as half floats")
  {
    // 2x1: a bright pixel (light above 1) and a black one.
    const Vector<float> hdr = {16.0f, 4.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    const auto readHalf = [](const Vector<uint8>& data, SIZE_T index) {
      uint16 half = 0;
      std::memcpy(&half, data.data() + index * sizeof(uint16), sizeof(half));
      return Float16::toFloat(half);
    };

    Vector<uint8> data;
    REQUIRE(TextureMips::buildChainRGBA16F(hdr, 2, 1, false, data) == 1);
    REQUIRE(data.size() == 2 * 4 * sizeof(uint16));
    REQUIRE(readHalf(data, 0) == 16.0f);
    REQUIRE(readHalf(data, 2) == 0.5f);

    REQUIRE(TextureMips::buildChainRGBA16F(hdr, 2, 1, true, data) == 2);
    REQUIRE(data.size() == 3 * 4 * sizeof(uint16));
    // Light is averaged as is, no sRGB.
    REQUIRE(readHalf(data, 8) == 8.0f);
    REQUIRE(readHalf(data, 9) == 2.0f);
    REQUIRE(readHalf(data, 10) == 0.25f);
    REQUIRE(readHalf(data, 11) == 1.0f);
  }
}

namespace {

/*
 * Checks what every generated shape must hold: valid indices, triangles that show their
 * front to the outside (clockwise, away from center, or up for the plane), unit normals,
 * tangents along U and bitangents up the image, and a texture that is not mirrored.
 */
void
checkGeneratedMesh(const Mesh& mesh, const ShapeDesc& desc)
{
  const Vector<VertexStatic> vertices = mesh.getVertexData<VertexStatic>();
  Vector<uint32> indices;
  REQUIRE(mesh.readIndices(indices));
  REQUIRE_FALSE(vertices.empty());
  REQUIRE(indices.size() % 3 == 0);
  REQUIRE(indices.size() >= 3);
  REQUIRE(mesh.getIndexType() ==
          (vertices.size() <= 65535 ? IndexType::UInt16 : IndexType::UInt32));

  for (const VertexStatic& vertex : vertices) {
    REQUIRE(Math::abs(vertex.normal.magnitude() - 1.0f) < 1e-4f);
    const Vector3 tangent(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z);
    REQUIRE(Math::abs(tangent.magnitude() - 1.0f) < 1e-4f);
    REQUIRE(Math::abs(tangent.dot(vertex.normal)) < 1e-4f);
    // Seen from outside, right is cross(normal, up), so an unmirrored map has w = -1.
    REQUIRE(vertex.tangent.w == -1.0f);
  }

  for (SIZE_T i = 0; i < indices.size(); i += 3) {
    REQUIRE(indices[i] < vertices.size());
    REQUIRE(indices[i + 1] < vertices.size());
    REQUIRE(indices[i + 2] < vertices.size());
    const VertexStatic& a = vertices[indices[i]];
    const VertexStatic& b = vertices[indices[i + 1]];
    const VertexStatic& c = vertices[indices[i + 2]];
    const Vector3 edge1 = b.position - a.position;
    const Vector3 edge2 = c.position - a.position;
    const Vector3 faceNormal = edge1.cross(edge2);
    REQUIRE(faceNormal.sqrMagnitude() > 0.0f);

    const Vector3 centroid = (a.position + b.position + c.position) / 3.0f;
    if (desc.type == ShapeType::Plane) {
      REQUIRE(faceNormal.z > 0.0f);
    }
    else {
      // Every other shape is convex around the origin.
      REQUIRE(faceNormal.dot(centroid) > 0.0f);
    }

    // Where U and V grow on the triangle must match the tangent frame of its vertices.
    const float du1 = b.texCoord.x - a.texCoord.x;
    const float dv1 = b.texCoord.y - a.texCoord.y;
    const float du2 = c.texCoord.x - a.texCoord.x;
    const float dv2 = c.texCoord.y - a.texCoord.y;
    const float determinant = du1 * dv2 - du2 * dv1;
    if (Math::abs(determinant) < 1e-8f) {
      continue;
    }
    const Vector3 alongU = (edge1 * dv2 - edge2 * dv1) / determinant;
    const Vector3 alongV = (edge2 * du1 - edge1 * du2) / determinant;
    for (const VertexStatic* vertex : {&a, &b, &c}) {
      const Vector3 tangent(vertex->tangent.x, vertex->tangent.y, vertex->tangent.z);
      const Vector3 imageUp = vertex->normal.cross(tangent) * vertex->tangent.w;
      REQUIRE(alongU.dot(tangent) > 0.0f);
      REQUIRE((-alongV).dot(imageUp) > 0.0f);
    }
  }
}

} // namespace

TEST_CASE("chCore - Mesh generator", "[Resources]")
{
  SECTION("Every shape faces out with a correct tangent frame")
  {
    const ShapeDesc shapes[] = {
        {.type = ShapeType::Plane},
        {.type = ShapeType::Plane, .size = Vector3(4.0f, 2.0f, 1.0f), .subdivisions = 3},
        {.type = ShapeType::Box, .size = Vector3(2.0f, 3.0f, 4.0f)},
        {.type = ShapeType::Sphere},
        {.type = ShapeType::Sphere, .segments = 3, .rings = 2},
        {.type = ShapeType::Capsule, .radius = 0.5f, .height = 3.0f},
        // No straight part: the two halves meet at the equator.
        {.type = ShapeType::Capsule, .radius = 0.5f, .height = 1.0f, .rings = 5},
        {.type = ShapeType::Cylinder, .radius = 1.0f, .height = 2.0f, .segments = 5},
        {.type = ShapeType::Cone},
        {.type = ShapeType::Cone, .radius = 2.0f, .height = 0.5f, .segments = 3}};
    for (const ShapeDesc& desc : shapes) {
      INFO(MeshGenerator::getName(desc.type));
      Mesh mesh;
      MeshGenerator::generate(desc, mesh);
      checkGeneratedMesh(mesh, desc);
    }
  }

  SECTION("Bounds follow the sizes, centered on the origin")
  {
    const auto boundsOf = [](const ShapeDesc& desc) {
      Mesh mesh;
      MeshGenerator::generate(desc, mesh);
      return mesh.getBounds().getBox();
    };
    const AABox box = boundsOf({.type = ShapeType::Box, .size = Vector3(2.0f, 3.0f, 4.0f)});
    REQUIRE(box.minPoint.nearEqual(Vector3(-1.0f, -1.5f, -2.0f)));
    REQUIRE(box.maxPoint.nearEqual(Vector3(1.0f, 1.5f, 2.0f)));

    const AABox plane =
        boundsOf({.type = ShapeType::Plane, .size = Vector3(4.0f, 2.0f, 9.0f)});
    REQUIRE(plane.minPoint.nearEqual(Vector3(-2.0f, -1.0f, 0.0f)));
    REQUIRE(plane.maxPoint.nearEqual(Vector3(2.0f, 1.0f, 0.0f)));

    const AABox sphere =
        boundsOf({.type = ShapeType::Sphere, .radius = 2.0f, .segments = 4});
    REQUIRE(sphere.maxPoint.nearEqual(Vector3(2.0f, 2.0f, 2.0f), 1e-4f));
    REQUIRE(sphere.minPoint.nearEqual(Vector3(-2.0f, -2.0f, -2.0f), 1e-4f));

    const AABox capsule = boundsOf(
        {.type = ShapeType::Capsule, .radius = 0.5f, .height = 3.0f, .segments = 4});
    REQUIRE(capsule.maxPoint.nearEqual(Vector3(0.5f, 0.5f, 1.5f), 1e-4f));

    const AABox cone = boundsOf(
        {.type = ShapeType::Cone, .radius = 1.0f, .height = 2.0f, .segments = 4});
    REQUIRE(Math::abs(cone.maxPoint.z - 1.0f) < 1e-5f);
    REQUIRE(Math::abs(cone.minPoint.z + 1.0f) < 1e-5f);
  }

  SECTION("Big meshes use 32-bit indices")
  {
    Mesh mesh;
    MeshGenerator::generate({.type = ShapeType::Plane, .subdivisions = 256}, mesh);
    REQUIRE(mesh.getVertexCount() == 257 * 257);
    REQUIRE(mesh.getIndexType() == IndexType::UInt32);
    REQUIRE(mesh.getIndexCount() == 256 * 256 * 6);
  }

  SECTION("Clamped and canonical descriptions")
  {
    const ShapeDesc clamped = MeshGenerator::getClamped({.type = ShapeType::Capsule,
                                                         .radius = 1.0f,
                                                         .height = 0.5f,
                                                         .segments = 1,
                                                         .rings = 1000});
    REQUIRE(clamped.height == 2.0f);
    REQUIRE(clamped.segments == MeshGenerator::MIN_SEGMENTS);
    REQUIRE(clamped.rings == MeshGenerator::MAX_RINGS);

    // Fields a type does not read do not tell two meshes apart.
    const ShapeDesc boxA{.type = ShapeType::Box, .radius = 3.0f, .segments = 7};
    const ShapeDesc boxB{.type = ShapeType::Box};
    REQUIRE_FALSE(boxA == boxB);
    REQUIRE(MeshGenerator::getCanonical(boxA) == MeshGenerator::getCanonical(boxB));
    REQUIRE(MeshGenerator::getCanonical(boxA).getHash() ==
            MeshGenerator::getCanonical(boxB).getHash());
    const ShapeDesc sphere{.type = ShapeType::Sphere, .radius = 3.0f};
    REQUIRE_FALSE(MeshGenerator::getCanonical(sphere) == MeshGenerator::getCanonical(boxB));
  }
}
