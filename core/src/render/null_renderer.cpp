// Nova Engine — headless null renderer (tests, offscreen, fallback).
// Counts draw calls and triangles so rendering paths are validated without GL.
#include "nova/render/RenderDevice.hpp"
#include <cstring>

namespace nova {

class NullRenderer : public RenderDevice {
public:
    bool initialize(int width, int height, std::string* error) override {
        (void)error;
        width_ = width;
        height_ = height;
        return true;
    }
    void resize(int width, int height) override { width_ = width; height_ = height; }
    void beginFrame(const Vec4&) override {
        stats_.drawCalls = 0;
        stats_.triangles = 0;
    }
    void endFrame() override {}

    void drawSprite(const std::string&, Vec2, Vec2, float, Vec4) override {
        ++stats_.drawCalls;
        stats_.triangles += 2;
    }
    void drawMesh(const MeshData& mesh, const MaterialData&, const Mat4&, const Mat4&) override {
        ++stats_.drawCalls;
        stats_.triangles += (int)(mesh.indices.size() / 3);
    }
    void setLight(const std::string&, Vec3, Vec4) override {}
    void setFog(float, float, Vec4) override {}
    void drawGrid(const Mat4&, Vec3, float, float) override { ++stats_.drawCalls; }

    const RenderStats& stats() const override { return stats_; }
    std::string backendName() const override { return "null"; }

private:
    int width_ = 0, height_ = 0;
    RenderStats stats_;
};

std::unique_ptr<RenderDevice> createNullRenderer() {
    return std::make_unique<NullRenderer>();
}

} // namespace nova
