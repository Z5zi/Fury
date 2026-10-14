#pragma once
#include "world_audit.hpp"
namespace vaultline {
/// Deterministic, reversible-at-startup visual upgrade. No gameplay collider,
/// original transform or tag is changed. False leaves baseline geometry intact.
WorldCoverage upgrade_playable_world(fury::Scene& scene, bool enabled);
}
