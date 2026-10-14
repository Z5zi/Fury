#pragma once

#include "harbor_assets.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace vaultline {

/// Conservative car-root XZ envelope, calculated once from actual loaded parts.
/// Body parts already have their authored hierarchy transforms baked by Harbor.
/// Light placement is independent; include its actual root-local offset too.
/// An empty/null/nonfinite/zero-size mesh is invalid: -1 disables approach.
inline float patrol_clearance_radius(const harbor::HarborPrimSet& body,
                                     const fury::Mesh& light,
                                     const fury::Vec3& light_offset) {
  if (body.parts.empty()) return -1.f;
  double maximum = 0.;
  const auto include = [&](const fury::Mesh* mesh, const fury::Vec3& offset) {
    if (!mesh || mesh->vertices.empty() ||
        !std::isfinite(offset.x) || !std::isfinite(offset.y) ||
        !std::isfinite(offset.z)) return false;
    double part_maximum = 0.;
    for (const auto& vertex : mesh->vertices) {
      const auto& p = vertex.position;
      if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        return false;
      const double radius = std::hypot(double(p.x) + offset.x,
                                       double(p.z) + offset.z);
      part_maximum = (std::max)(part_maximum, radius);
    }
    maximum = (std::max)(maximum, part_maximum);
    return part_maximum > 0.;
  };
  for (const auto& part : body.parts)
    if (!include(part.mesh, {})) return -1.f;
  if (!include(&light, light_offset) ||
      maximum >= std::numeric_limits<float>::max()) return -1.f;
  // Round outward once so the float content value never understates the bound.
  const float radius = static_cast<float>(maximum);
  const float outward = std::nextafter(radius, std::numeric_limits<float>::infinity());
  return std::isfinite(outward) ? outward : -1.f;
}

inline const fury::Vec3 kPatrolLightOffset{0.f, 1.55f, 0.f};

}  // namespace vaultline
