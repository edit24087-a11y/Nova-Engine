// Nova Engine — 2D physics (spec §5): bodies, collision shapes, forces.
// A compact sweep-based 2D collision system with circle/box shapes, static
// bodies, rigid bodies, character bodies, area detection, and query APIs.
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "nova/math.hpp"

namespace nova {

enum class BodyType {
    Static,    // immovable (walls, platforms)
    RigidBody, // dynamic, affected by forces/gravity
    Character  // kinematic, player-controlled, ignores gravity unless asked
};

enum class CollisionShapeKind { Circle, Box };

struct CollisionShape {
    CollisionShapeKind kind = CollisionShapeKind::Circle;
    float radius = 16.0f;   // circle
    Vec2 size = Vec2(32, 32); // box (half-extents are size/2)
    bool isTrigger = false; // area detection only
};

class Body2D {
public:
    std::string name;
    BodyType type = BodyType::Static;
    Vec2 position;
    Vec2 velocity = Vec2::zero();
    Vec2 acceleration = Vec2::zero();
    float rotationDeg = 0.0f;
    Vec2 scale = Vec2(1, 1);
    float mass = 1.0f;
    float friction = 0.2f;
    float restitution = 0.2f;
    float linearDamping = 0.0f;
    bool useGravity = true;
    CollisionShape shape;
    bool enabled = true;
    int layer = 1;

    // Callbacks: collide(other), areaEnter(body), areaExit(body).
    std::function<void(Body2D*)> onCollide;
    std::function<void(Body2D*)> onAreaEnter;
    std::function<void(Body2D*)> onAreaExit;
};

class World2D {
public:
    Vec2 gravity = Vec2(0.0f, 980.0f); // pixels/s^2 (2D-friendly units)
    float fixedTimeStep = 1.0f / 60.0f;
    int maxSubSteps = 5;

    std::vector<Body2D*> bodies;
    std::vector<Body2D*> triggers;

    Body2D* addBody(Body2D* body);
    void removeBody(Body2D* body);

    // Advance simulation by dt seconds (with internal sub-stepping).
    void step(float dt);

    // Direct queries.
    std::vector<Body2D*> queryCircle(Vec2 center, float radius, int maskLayer = -1);
    std::vector<Body2D*> queryAABB(Vec2 min, Vec2 max, int maskLayer = -1);
    Body2D* rayCast(Vec2 origin, Vec2 dirNormalized, float maxDistance, float* hitDistance = nullptr);

    // Intersection tests.
    static bool circleVsCircle(const Vec2& a, float ra, const Vec2& b, float rb);
    static bool boxVsBox(const Vec2& a, const Vec2& as, const Vec2& b, const Vec2& bs);
    static bool circleVsBox(const Vec2& c, float r, const Vec2& b, const Vec2& bs);

    void clear();
    int bodyCount() const { return (int)bodies.size(); }
};

} // namespace nova
