// Nova Engine — 3D physics (Phase 4 core): rigid bodies with AABB collision
// shapes, gravity, and simple impulse resolution. Full Jolt-style integration
// happens on the Android side; this core model keeps the API stable.
#pragma once
#include "nova/math.hpp"

namespace nova {

enum class Body3DType { Static, RigidBody, Character };

struct AABB3D {
    Vec3 min, max;
    AABB3D() : min(Vec3::zero()), max(Vec3::zero()) {}
    AABB3D(Vec3 center, Vec3 halfExtents)
        : min(center - halfExtents), max(center + halfExtents) {}
    bool intersects(const AABB3D& o) const {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }
};

class RigidBody3D {
public:
    std::string name;
    Body3DType type = Body3DType::RigidBody;
    Vec3 position;
    Vec3 velocity = Vec3::zero();
    Vec3 acceleration = Vec3::zero();
    Vec3 rotationDeg = Vec3::zero();
    Vec3 scale = Vec3(1, 1, 1);
    float mass = 1.0f;
    Vec3 halfExtents = Vec3(0.5f, 0.5f, 0.5f);
    bool useGravity = true;
    bool enabled = true;

    AABB3D aabb() const { return AABB3D(position, halfExtents * scale); }

    void applyForce(Vec3 f) { acceleration += f / std::max(mass, 0.0001f); }
    void applyImpulse(Vec3 j) { velocity += j / std::max(mass, 0.0001f); }

    void integrate(float dt, Vec3 gravity) {
        if (type == Body3DType::Static || !enabled) return;
        if (useGravity) velocity += gravity * dt;
        velocity += acceleration * dt;
        acceleration = Vec3::zero();
        position += velocity * dt;
    }
};

class World3D {
public:
    Vec3 gravity = Vec3(0, -9.81f, 0);
    std::vector<RigidBody3D*> bodies;

    RigidBody3D* addBody(RigidBody3D* b) {
        if (b) bodies.push_back(b);
        return b;
    }
    void removeBody(RigidBody3D* b) {
        auto it = std::find(bodies.begin(), bodies.end(), b);
        if (it != bodies.end()) bodies.erase(it);
    }

    void step(float dt) {
        for (RigidBody3D* b : bodies) b->integrate(dt, gravity);
        // Resolve overlapping pairs with positional correction.
        for (size_t i = 0; i < bodies.size(); ++i) {
            for (size_t j = i + 1; j < bodies.size(); ++j) {
                RigidBody3D* a = bodies[i];
                RigidBody3D* b = bodies[j];
                if (!a->enabled || !b->enabled) continue;
                if (a->type == Body3DType::Static && b->type == Body3DType::Static) continue;
                AABB3D ba = a->aabb();
                AABB3D bb = b->aabb();
                // Overlap along each axis.
                float ox = std::min(ba.max.x, bb.max.x) - std::max(ba.min.x, bb.min.x);
                float oy = std::min(ba.max.y, bb.max.y) - std::max(ba.min.y, bb.min.y);
                float oz = std::min(ba.max.z, bb.max.z) - std::max(ba.min.z, bb.min.z);
                if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) continue;
                Vec3 push;
                float depth = ox;
                push = Vec3(a->position.x > b->position.x ? 1.0f : -1.0f, 0.0f, 0.0f);
                if (oy < depth) {
                    depth = oy;
                    push = Vec3(0.0f, a->position.y > b->position.y ? 1.0f : -1.0f, 0.0f);
                }
                if (oz < depth) {
                    depth = oz;
                    push = Vec3(0.0f, 0.0f, a->position.z > b->position.z ? 1.0f : -1.0f);
                }
                float invA = a->type == Body3DType::Static ? 0.0f : 1.0f / std::max(a->mass, 0.0001f);
                float invB = b->type == Body3DType::Static ? 0.0f : 1.0f / std::max(b->mass, 0.0001f);
                float invSum = invA + invB;
                if (invSum <= 0.0f) continue;
                float correction = (depth * 0.5f + 0.01f) / invSum;
                a->position += push * (correction * invA);
                b->position -= push * (correction * invB);
                // Zero relative velocity along push.
                float rel = (a->velocity - b->velocity).dot(push);
                if (rel > 0.0f) continue;
                float imp = -rel / invSum;
                if (a->type != Body3DType::Static) a->velocity += push * (imp * invA);
                if (b->type != Body3DType::Static) b->velocity -= push * (imp * invB);
            }
        }
    }
};

} // namespace nova
