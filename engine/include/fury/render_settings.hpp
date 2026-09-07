#pragma once

#include <cstdint>
#include <string>

namespace fury {

enum class TraceMode { RayTraced, PathTraced };
enum class Upscaler { Native, FSR, XeSS };
enum class UpscaleQuality { NativeAA, Quality, Balanced, Performance, UltraPerformance };
enum class RenderDebugView { Beauty, Depth, Normals, Motion, Direct, Indirect };

struct RenderSettings {
  TraceMode trace_mode{TraceMode::PathTraced};
  Upscaler upscaler{Upscaler::Native};
  UpscaleQuality quality{UpscaleQuality::Quality};
  unsigned samples_per_pixel{1};
  unsigned max_bounces{4};
  float exposure{1.f};
  bool denoise{true};
  bool accumulate{true};
  bool vsync{true};
  bool debug_layer{false};
  RenderDebugView debug_view{RenderDebugView::Beauty};
};

struct RenderStatistics {
  std::uint64_t frame_index{0};
  std::uint64_t triangle_count{0};
  std::uint64_t unique_triangle_count{0};
  std::uint64_t blas_builds{0},tlas_builds{0};
  unsigned instance_count{0};
  unsigned render_width{0}, render_height{0};
  unsigned output_width{0}, output_height{0};
  unsigned accumulated_frames{0};
  double gpu_frame_ms{0};
  unsigned validation_errors{0};
  bool hardware_ray_tracing{false};
  bool debug_layer_active{false};
  std::string adapter;
  std::string upscaler{"Native"};
};

const char* upscaler_name(Upscaler upscaler);
/// SHA-256 of the source inputs used to build this executable.
const char* build_source_fingerprint();
const char* quality_name(UpscaleQuality quality);
/// Parse only recognized strings. Invalid values leave output unchanged.
bool parse_upscaler(const std::string& text, Upscaler& out);
bool parse_upscale_quality(const std::string& text, UpscaleQuality& out);
/// Halton(2,3), centered in [-0.5,0.5]. Independent of wall-clock timing.
void temporal_jitter(std::uint64_t frame, unsigned phase_count, float& x, float& y);
struct PixelJitter { float x{},y{}; };
/// Convert ray sampling offsets to the image displacement expected by FSR/XeSS.
PixelJitter reconstruction_jitter(float sample_x,float sample_y);

}  // namespace fury
