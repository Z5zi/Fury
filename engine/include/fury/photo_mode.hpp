#pragma once

#include "fury/camera.hpp"
#include "fury/math.hpp"

namespace fury {

/// Photo mode stub (Vaultline 2.7.0) — F9 freezes sim, free camera, hide HUD.
/// WASD + mouse look (Space/Ctrl vertical); Esc exits and restores pose.
struct PhotoMode {
  bool active{false};
  Vec3 saved_position{0.f, 1.7f, 12.f};
  float saved_yaw{-1.5707963f};
  float saved_pitch{-0.08f};
  bool saved_fly{false};
  bool saved_third{false};
  bool saved_vehicle{false};
  Vec3 saved_velocity{0.f, 0.f, 0.f};

  void enter(Camera& cam) {
    if (active) {
      return;
    }
    saved_position = cam.position;
    saved_yaw = cam.yaw;
    saved_pitch = cam.pitch;
    saved_fly = cam.fly_mode;
    saved_third = cam.third_person;
    saved_vehicle = cam.vehicle_seated;
    saved_velocity = cam.velocity;
    cam.fly_mode = true;
    cam.vehicle_seated = false;
    cam.third_person = false;
    cam.velocity = {};
    cam.snap_look();
    active = true;
  }

  void exit(Camera& cam) {
    if (!active) {
      return;
    }
    cam.position = saved_position;
    cam.yaw = saved_yaw;
    cam.pitch = saved_pitch;
    cam.fly_mode = saved_fly;
    cam.third_person = saved_third;
    cam.vehicle_seated = saved_vehicle;
    cam.velocity = saved_velocity;
    cam.snap_look();
    active = false;
  }

  void toggle(Camera& cam) {
    if (active) {
      exit(cam);
    } else {
      enter(cam);
    }
  }
};

}  // namespace fury
