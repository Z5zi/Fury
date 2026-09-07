#include "fury/render_settings.hpp"

#include <algorithm>

namespace fury {
const char* upscaler_name(Upscaler v) {
  switch (v) {
    case Upscaler::Native: return "Native";
    case Upscaler::FSR: return "AMD FSR";
    case Upscaler::XeSS: return "Intel XeSS";
  }
  return "Unknown";
}
const char* quality_name(UpscaleQuality v) {
  switch (v) {
    case UpscaleQuality::NativeAA: return "native-aa";
    case UpscaleQuality::Quality: return "quality";
    case UpscaleQuality::Balanced: return "balanced";
    case UpscaleQuality::Performance: return "performance";
    case UpscaleQuality::UltraPerformance: return "ultra-performance";
  }
  return "unknown";
}
bool parse_upscaler(const std::string& s, Upscaler& out) {
  if (s == "native") out = Upscaler::Native;
  else if (s == "fsr") out = Upscaler::FSR;
  else if (s == "xess") out = Upscaler::XeSS;
  else return false;
  return true;
}
bool parse_upscale_quality(const std::string& s, UpscaleQuality& out) {
  if (s == "native-aa") out = UpscaleQuality::NativeAA;
  else if (s == "quality") out = UpscaleQuality::Quality;
  else if (s == "balanced") out = UpscaleQuality::Balanced;
  else if (s == "performance") out = UpscaleQuality::Performance;
  else if (s == "ultra-performance") out = UpscaleQuality::UltraPerformance;
  else return false;
  return true;
}
void temporal_jitter(std::uint64_t frame, unsigned phase_count, float& x, float& y) {
  const unsigned index = static_cast<unsigned>(frame % (std::max)(1u, phase_count)) + 1;
  auto radical = [](unsigned i, unsigned base) {
    float value = 0.f, weight = 1.f;
    while (i) { weight /= float(base); value += float(i % base) * weight; i /= base; }
    return value - 0.5f;
  };
  x = radical(index, 2);
  y = radical(index, 3);
}
PixelJitter reconstruction_jitter(float sample_x,float sample_y) { return {-sample_x,-sample_y}; }
}  // namespace fury
