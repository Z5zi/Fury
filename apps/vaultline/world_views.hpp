#pragma once
#include <fury/math.hpp>
#include <string>

namespace vaultline {
struct WorldCaptureView {
  const char* name;
  fury::Vec3 eye;
  fury::Vec3 target;
  float fov_y; // zero preserves the player's FOV for the original capture views
};
inline const WorldCaptureView* world_capture_view(const std::string& name) {
  static const WorldCaptureView views[] = {
      {"bank", {18.f,8.f,22.f}, {0.f,4.f,0.f}, 0.f},
      {"storefront", {-10.f,5.f,36.f}, {-20.f,3.5f,22.f}, 0.f},
      {"storefront-close", {-16.5f,2.8f,29.5f}, {-20.f,2.5f,26.f}, 0.f},
      {"bench", {6.5f,1.35f,.8f}, {4.5f,.6f,-1.5f}, 0.f},
      {"metro-wide", {55.f,24.f,62.f}, {-4.f,4.f,-8.f}, 60.f},
      {"metro-street", {0.f,1.8f,12.f}, {28.f,3.5f,20.f}, 60.f},
      {"market-street", {-88.f,1.8f,34.f}, {-75.f,2.5f,46.f}, 60.f},
      {"ridge-street", {92.f,1.8f,10.f}, {107.f,3.f,18.f}, 60.f},
      {"ridge", {129.f,15.f,43.f}, {95.f,3.f,7.f}, 60.f},
      {"ashcourt", {-56.f,14.f,70.f}, {-90.f,2.f,42.f}, 60.f},
      {"depot", {78.f,14.f,-25.f}, {58.f,3.f,-48.f}, 60.f},
      {"loft", {69.f,12.f,75.f}, {42.f,3.f,53.f}, 60.f},
      {"quay", {47.f,20.f,137.f}, {10.f,4.f,96.f}, 60.f},
      {"waterfront", {-20.f,12.f,75.f}, {24.f,2.f,44.f}, 60.f},
      {"world-overview", {185.f,135.f,220.f}, {8.f,0.f,32.f}, 42.f},
  };
  for(const auto& view:views) if(name==view.name) return &view;
  return nullptr;
}
inline constexpr const char* kWorldCaptureViewNames =
    "bank|storefront|storefront-close|bench|metro-wide|metro-street|market-street|ridge-street|ridge|ashcourt|depot|loft|quay|waterfront|world-overview";
} // namespace vaultline
