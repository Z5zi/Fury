#pragma once

#include "fury/render_settings.hpp"
#include <d3d12.h>
#include <memory>
#include <string>

namespace fury {
struct UpscaleFrame {
  ID3D12GraphicsCommandList* commands{};
  ID3D12Resource* color{};
  ID3D12Resource* depth{};
  ID3D12Resource* motion{};
  ID3D12Resource* reactive{};
  ID3D12Resource* output{};
  float jitter_x{}, jitter_y{}; // ray sample displacement, positive X right / Y down
  float delta_ms{16.6667f};
  float near_plane{0.1f}, far_plane{500.f}, fov_y{1.0472f};
  bool reset{true};
};

/// Owns vendor contexts. Destroy/reconfigure only after the host GPU fence completes.
/// Inputs: HDR linear color, standard [0,1] depth, current-to-previous motion in
/// display pixels (positive Y down), and a reactive mask. Output: HDR linear color.
class Dx12Upscaler {
 public:
  Dx12Upscaler();
  ~Dx12Upscaler();
  bool create(ID3D12Device* device, const RenderSettings& settings, unsigned width,
              unsigned height, std::string& error);
  bool dispatch(const UpscaleFrame& frame, std::string& error);
  void destroy();
  unsigned render_width() const;
  unsigned render_height() const;
  const std::string& provider() const;
 private:
  struct Impl;
  std::unique_ptr<Impl> m;
};
}  // namespace fury
