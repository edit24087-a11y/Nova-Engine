// Nova Engine — render device abstraction.
// The core never talks to GL directly; it issues abstract draw commands to a
// RenderDevice. Two backends exist:
//   - GLRenderer (engine-android, OpenGL ES 3.0) — production
//   - NullRenderer (core, headless) — tests / offscreen rendering
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "nova/math.hpp"

namespace nova {

enum class Primitive { Triangles, Quad };

struct RenderStats {
    int drawCalls = 0;
    int triangles = 0;
    float lastFrameMs = 0.0f;
    float fps = 0.0f;
};

struct MeshData {
    std::vector<float> vertices; // x,y,z per vertex
    std::vector<unsigned int> indices;
};

struct MaterialData {
    std::string name = "default";
    Vec4 color = Vec4(0.7f, 0.7f, 0.8f, 1.0f);
    bool useLighting = true;
};

class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    virtual bool initialize(int width, int height, std::string* error) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void beginFrame(const Vec4& clearColor) = 0;
    virtual void endFrame() = 0;

    // 2D sprites.
    virtual void drawSprite(const std::string& texture, Vec2 pos, Vec2 size,
                            float rotationDeg, Vec4 tint) = 0;

    // 3D meshes.
    virtual void drawMesh(const MeshData& mesh, const MaterialData& material,
                          const Mat4& model, const Mat4& viewProj) = 0;

    // Lighting (3D).
    virtual void setLight(const std::string& name, Vec3 dir, Vec4 color) = 0;
    virtual void setFog(float nearPlane, float farPlane, Vec4 fogColor) = 0;

    // Grid helper (3D editor).
    virtual void drawGrid(const Mat4& viewProj, Vec3 origin, float size, float step) = 0;

    virtual const RenderStats& stats() const = 0;
    virtual std::string backendName() const = 0;
};

std::unique_ptr<RenderDevice> createNullRenderer();

} // namespace nova
