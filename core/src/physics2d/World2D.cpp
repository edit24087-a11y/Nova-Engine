// Nova Engine — 2D physics implementation.
#include "nova/physics2d/World2D.hpp"
#include <algorithm>

namespace nova {

Body2D* World2D::addBody(Body2D* body) {
    if (!body) return nullptr;
    bodies.push_back(body);
    if (body->shape.isTrigger) triggers.push_back(body);
    return body;
}

void World2D::removeBody(Body2D* body) {
    auto it = std::find(bodies.begin(), bodies.end(), body);
    if (it != bodies.end()) bodies.erase(it);
    auto it2 = std::find(triggers.begin(), triggers.end(), body);
    if (it2 != triggers.end()) triggers.erase(it2);
}

void World2D::clear() {
    bodies.clear();
    triggers.clear();
}

bool World2D::circleVsCircle(const Vec2& a, float ra, const Vec2& b, float rb) {
    return (a - b).lengthSq() <= (ra + rb) * (ra + rb);
}

bool World2D::boxVsBox(const Vec2& a, const Vec2& as, const Vec2& b, const Vec2& bs) {
    Vec2 h1(as.x / 2, as.y / 2);
    Vec2 h2(bs.x / 2, bs.y / 2);
    return std::abs(a.x - b.x) <= h1.x + h2.x && std::abs(a.y - b.y) <= h1.y + h2.y;
}

bool World2D::circleVsBox(const Vec2& c, float r, const Vec2& b, const Vec2& bs) {
    Vec2 h(bs.x / 2, bs.y / 2);
    Vec2 closest(std::clamp(c.x, b.x - h.x, b.x + h.x), std::clamp(c.y, b.y - h.y, b.y + h.y));
    return (c - closest).lengthSq() <= r * r;
}

static bool shapeOverlaps(const CollisionShape& sa, const Vec2& pa,
                          const CollisionShape& sb, const Vec2& pb) {
    if (sa.kind == CollisionShapeKind::Circle && sb.kind == CollisionShapeKind::Circle) {
        return World2D::circleVsCircle(pa, sa.radius, pb, sb.radius);
    }
    if (sa.kind == CollisionShapeKind::Circle) return World2D::circleVsBox(pa, sa.radius, pb, sb.size);
    if (sb.kind == CollisionShapeKind::Circle) return World2D::circleVsBox(pb, sb.radius, pa, sa.size);
    return World2D::boxVsBox(pa, sa.size, pb, sb.size);
}

static bool resolvePair(Body2D* a, Body2D* b) {
    if (!shapeOverlaps(a->shape, a->position, b->shape, b->position)) return false;
    // Triggers never resolve motion.
    if (a->shape.isTrigger || b->shape.isTrigger) return true;

    // Static bodies absorb the response.
    if (a->type == BodyType::Static && b->type == BodyType::Static) return true;

    // Compute overlap resolution direction (centers).
    Vec2 delta = a->position - b->position;
    if (delta.lengthSq() < 1e-9f) {
        // Centers coincident: resolve along up axis.
        delta = Vec2(0, 1);
    }
    Vec2 normal = delta.normalized();
    float depth;
    if (a->shape.kind == CollisionShapeKind::Circle && b->shape.kind == CollisionShapeKind::Circle) {
        depth = a->shape.radius + b->shape.radius - delta.length();
    } else if (a->shape.kind == CollisionShapeKind::Circle) {
        Vec2 h = b->shape.size * 0.5f;
        Vec2 closest(std::clamp(a->position.x, b->position.x - h.x, b->position.x + h.x),
                     std::clamp(a->position.y, b->position.y - h.y, b->position.y + h.y));
        Vec2 toCenter = a->position - closest;
        float dist = toCenter.length();
        normal = dist > 1e-9f ? toCenter / dist : Vec2(0, 1);
        depth = a->shape.radius - dist;
    } else if (b->shape.kind == CollisionShapeKind::Circle) {
        Vec2 h = a->shape.size * 0.5f;
        Vec2 closest(std::clamp(b->position.x, a->position.x - h.x, a->position.x + h.x),
                     std::clamp(b->position.y, a->position.y - h.y, a->position.y + h.y));
        Vec2 toCenter = b->position - closest;
        float dist = toCenter.length();
        normal = dist > 1e-9f ? toCenter / dist : Vec2(0, 1);
        depth = b->shape.radius - dist;
        normal = -normal;
    } else {
        Vec2 h1 = a->shape.size * 0.5f;
        Vec2 h2 = b->shape.size * 0.5f;
        float ox = (h1.x + h2.x) - std::abs(a->position.x - b->position.x);
        float oy = (h1.y + h2.y) - std::abs(a->position.y - b->position.y);
        if (ox < oy) {
            depth = ox;
            normal = Vec2(a->position.x > b->position.x ? 1.0f : -1.0f, 0.0f);
        } else {
            depth = oy;
            normal = Vec2(0.0f, a->position.y > b->position.y ? 1.0f : -1.0f);
        }
    }
    depth = std::max(depth, 0.0f);

    float invA = a->type == BodyType::Static ? 0.0f : 1.0f / std::max(a->mass, 0.0001f);
    float invB = b->type == BodyType::Static ? 0.0f : 1.0f / std::max(b->mass, 0.0001f);
    float invSum = invA + invB;
    if (invSum <= 0.0f) return true;

    // Positional correction (80%).
    float correction = depth * 0.8f / invSum;
    a->position += normal * (correction * invA);
    b->position -= normal * (correction * invB);

    // Velocity resolution (elastic impulse with restitution).
    Vec2 relVel = a->velocity - b->velocity;
    float velAlongNormal = relVel.dot(normal);
    float e = std::min(a->restitution, b->restitution);
    if (velAlongNormal > 0.0f) return true; // separating
    float j = -(1.0f + e) * velAlongNormal / invSum;
    Vec2 impulse = normal * j;
    if (a->type != BodyType::Static) a->velocity += impulse * invA;
    if (b->type != BodyType::Static) b->velocity -= impulse * invB;
    return true;
}

void World2D::step(float dt) {
    int substeps = std::max(1, maxSubSteps);
    float h = dt / substeps;
    for (int s = 0; s < substeps; ++s) {
        // Integrate.
        for (Body2D* b : bodies) {
            if (!b->enabled || b->type == BodyType::Static) continue;
            if (b->useGravity && b->type == BodyType::RigidBody) {
                b->velocity += gravity * h;
            }
            b->velocity += b->acceleration * h;
            b->acceleration = Vec2::zero();
            b->velocity *= (1.0f - std::min(1.0f, b->linearDamping * h));
            b->position += b->velocity * h;
        }
        // Resolve collisions.
        for (size_t i = 0; i < bodies.size(); ++i) {
            for (size_t j = i + 1; j < bodies.size(); ++j) {
                Body2D* a = bodies[i];
                Body2D* b = bodies[j];
                bool overlapped = resolvePair(a, b);
                if (overlapped) {
                    if (a->onCollide) a->onCollide(b);
                    if (b->onCollide) b->onCollide(a);
                }
            }
        }
        // Trigger (area) events.
        for (Body2D* t : triggers) {
            if (!t->enabled) continue;
            for (Body2D* other : bodies) {
                if (other == t || other->shape.isTrigger || !other->enabled) continue;
                bool inside = shapeOverlaps(t->shape, t->position, other->shape, other->position);
                if (inside && t->onAreaEnter) t->onAreaEnter(other);
            }
        }
    }
}

std::vector<Body2D*> World2D::queryCircle(Vec2 center, float radius, int maskLayer) {
    std::vector<Body2D*> out;
    for (Body2D* b : bodies) {
        if (!b->enabled) continue;
        if (maskLayer >= 0 && b->layer != maskLayer) continue;
        if (shapeOverlaps(CollisionShape{CollisionShapeKind::Circle, radius, Vec2(), false},
                          center, b->shape, b->position)) {
            out.push_back(b);
        }
    }
    return out;
}

std::vector<Body2D*> World2D::queryAABB(Vec2 min, Vec2 max, int maskLayer) {
    std::vector<Body2D*> out;
    for (Body2D* b : bodies) {
        if (!b->enabled) continue;
        if (maskLayer >= 0 && b->layer != maskLayer) continue;
        CollisionShape probe{CollisionShapeKind::Box, 0, Vec2(max.x - min.x, max.y - min.y), false};
        if (shapeOverlaps(probe, (min + max) * 0.5f, b->shape, b->position)) {
            out.push_back(b);
        }
    }
    return out;
}

Body2D* World2D::rayCast(Vec2 origin, Vec2 dirNormalized, float maxDistance, float* hitDistance) {
    float best = maxDistance;
    Body2D* hit = nullptr;
    Vec2 end = origin + dirNormalized * maxDistance;
    for (Body2D* b : bodies) {
        if (!b->enabled) continue;
        // Box: slab method.
        if (b->shape.kind == CollisionShapeKind::Box) {
            Vec2 h = b->shape.size * 0.5f;
            float tmin = 0.0f, tmax = maxDistance;
            bool hitBox = true;
            for (int axis = 0; axis < 2; ++axis) {
                float o = axis == 0 ? origin.x : origin.y;
                float d = axis == 0 ? dirNormalized.x : dirNormalized.y;
                float bmin = (axis == 0 ? b->position.x : b->position.y) - (axis == 0 ? h.x : h.y);
                float bmax = (axis == 0 ? b->position.x : b->position.y) + (axis == 0 ? h.x : h.y);
                if (std::abs(d) < 1e-9f) {
                    if (o < bmin || o > bmax) { hitBox = false; break; }
                } else {
                    float t1 = (bmin - o) / d;
                    float t2 = (bmax - o) / d;
                    if (t1 > t2) std::swap(t1, t2);
                    tmin = std::max(tmin, t1);
                    tmax = std::min(tmax, t2);
                    if (tmin > tmax) { hitBox = false; break; }
                }
            }
            if (hitBox && tmin <= maxDistance && tmin < best) {
                best = tmin;
                hit = b;
            }
        } else {
            // Circle: solve |o + t*d - c| = r.
            Vec2 oc = origin - b->position;
            float b2 = dirNormalized.dot(oc);
            float c2 = oc.dot(oc) - b->shape.radius * b->shape.radius;
            float disc = b2 * b2 - c2;
            if (disc >= 0.0f) {
                float t = -b2 - std::sqrt(disc);
                if (t < 0.0f) t = -b2 + std::sqrt(disc);
                if (t >= 0.0f && t <= maxDistance && t < best) {
                    best = t;
                    hit = b;
                }
            }
        }
    }
    if (hitDistance) *hitDistance = best;
    return hit;
}

} // namespace nova
