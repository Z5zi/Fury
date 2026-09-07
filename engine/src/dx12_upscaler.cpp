#include "dx12_upscaler.hpp"
#include "fury/log.hpp"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <vector>

#if FURY_HAS_FSR
#include <api/include/ffx_api.h>
#include <api/include/dx12/ffx_api_dx12.h>
#include <upscalers/include/ffx_upscale.h>
#endif
#if FURY_HAS_XESS
#include <xess/xess_d3d12.h>
#endif

namespace fury {
namespace {
HMODULE load_local_library(const wchar_t* name) {
  std::vector<wchar_t> path(32768);
  const DWORD length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
  if (!length || length >= path.size()) return nullptr;
  std::wstring full(path.data(), length);
  const auto slash = full.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return nullptr;
  full.resize(slash + 1);
  full += name;
  return LoadLibraryExW(full.c_str(), nullptr,
                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}
template<class T> bool load_proc(HMODULE module, const char* name, T& function) {
  function = reinterpret_cast<T>(GetProcAddress(module, name));
  return function != nullptr;
}
}  // namespace

struct Dx12Upscaler::Impl {
  Upscaler kind{Upscaler::Native};
  unsigned width{}, height{}, render_width{}, render_height{};
  std::string provider{"Native"};
  HMODULE library{};
#if FURY_HAS_FSR
  ffxContext fsr{};
  PfnFfxCreateContext fsr_create{};
  PfnFfxDestroyContext fsr_destroy{};
  PfnFfxQuery fsr_query{};
  PfnFfxDispatch fsr_dispatch{};
  ffxCreateBackendDX12Desc fsr_backend{};
  ffxCreateContextDescUpscale fsr_description{};
  ffxCreateContextDescUpscaleVersion fsr_version{};
#endif
#if FURY_HAS_XESS
  xess_context_handle_t xess{};
  decltype(&xessD3D12CreateContext) xess_create{};
  decltype(&xessD3D12Init) xess_init{};
  decltype(&xessD3D12Execute) xess_execute{};
  decltype(&xessDestroyContext) xess_destroy{};
  decltype(&xessGetOptimalInputResolution) xess_resolution{};
  decltype(&xessGetVersion) xess_version{};
#endif
};

Dx12Upscaler::Dx12Upscaler() : m(std::make_unique<Impl>()) {}
Dx12Upscaler::~Dx12Upscaler() { destroy(); }
void Dx12Upscaler::destroy() {
#if FURY_HAS_FSR
  if (m->fsr && m->fsr_destroy) m->fsr_destroy(&m->fsr, nullptr);
#endif
#if FURY_HAS_XESS
  if (m->xess && m->xess_destroy) m->xess_destroy(m->xess);
#endif
  if (m->library) FreeLibrary(m->library);
  m = std::make_unique<Impl>();
}
unsigned Dx12Upscaler::render_width() const { return m->render_width; }
unsigned Dx12Upscaler::render_height() const { return m->render_height; }
const std::string& Dx12Upscaler::provider() const { return m->provider; }

bool Dx12Upscaler::create(ID3D12Device* device, const RenderSettings& settings,
                          unsigned width, unsigned height, std::string& error) {
  destroy();
  if (!device || !width || !height) { error = "Invalid upscaler device or dimensions"; return false; }
  m->kind = settings.upscaler;
  m->width = m->render_width = width;
  m->height = m->render_height = height;
  if (settings.upscaler == Upscaler::Native) return true;
  if (settings.upscaler == Upscaler::FSR) {
#if FURY_HAS_FSR
    m->library = load_local_library(L"amd_fidelityfx_loader_dx12.dll");
    if (!m->library ||
        !load_proc(m->library, "ffxCreateContext", m->fsr_create) ||
        !load_proc(m->library, "ffxDestroyContext", m->fsr_destroy) ||
        !load_proc(m->library, "ffxQuery", m->fsr_query) ||
        !load_proc(m->library, "ffxDispatch", m->fsr_dispatch)) {
      error = "AMD FSR runtime missing or incompatible beside the executable"; return false;
    }
    ffxQueryDescUpscaleGetRenderResolutionFromQualityMode resolution{};
    resolution.header.type = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE;
    resolution.displayWidth = width; resolution.displayHeight = height;
    resolution.qualityMode = static_cast<unsigned>(settings.quality);
    resolution.pOutRenderWidth = &m->render_width;
    resolution.pOutRenderHeight = &m->render_height;
    auto result = m->fsr_query(nullptr, &resolution.header);
    if (result != FFX_API_RETURN_OK) { error = "FSR resolution query failed: " + std::to_string(result); return false; }
    m->fsr_backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
    m->fsr_backend.device = device;
    m->fsr_version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE_VERSION;
    m->fsr_version.header.pNext = &m->fsr_backend.header;
    m->fsr_version.version = FFX_UPSCALER_VERSION;
    auto& description = m->fsr_description;
    description.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
    description.header.pNext = &m->fsr_version.header;
    description.flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
    description.maxRenderSize = {m->render_width, m->render_height};
    description.maxUpscaleSize = {width, height};
    result = m->fsr_create(&m->fsr, &description.header, nullptr);
    if (result != FFX_API_RETURN_OK) { error = "FSR context creation failed: " + std::to_string(result); return false; }
    ffxQueryGetProviderVersion provider{};
    provider.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
    result = m->fsr_query(&m->fsr, &provider.header);
    m->provider = "AMD FSR";
    if (result == FFX_API_RETURN_OK && provider.versionName) m->provider += std::string(" / ") + provider.versionName;
    return true;
#else
    error = "AMD FSR support was not built; configure FURY_FSR_ROOT"; return false;
#endif
  }
#if FURY_HAS_XESS
  m->library = load_local_library(L"libxess.dll");
  if (!m->library ||
      !load_proc(m->library, "xessD3D12CreateContext", m->xess_create) ||
      !load_proc(m->library, "xessD3D12Init", m->xess_init) ||
      !load_proc(m->library, "xessD3D12Execute", m->xess_execute) ||
      !load_proc(m->library, "xessDestroyContext", m->xess_destroy) ||
      !load_proc(m->library, "xessGetOptimalInputResolution", m->xess_resolution) ||
      !load_proc(m->library, "xessGetVersion", m->xess_version)) {
    error = "Intel XeSS runtime missing or incompatible beside the executable"; return false;
  }
  auto result = m->xess_create(device, &m->xess);
  if (result < XESS_RESULT_SUCCESS) { error = "XeSS context creation failed: " + std::to_string(result); return false; }
  xess_quality_settings_t quality = XESS_QUALITY_SETTING_QUALITY;
  switch (settings.quality) {
    case UpscaleQuality::NativeAA: quality = XESS_QUALITY_SETTING_AA; break;
    case UpscaleQuality::Quality: break;
    case UpscaleQuality::Balanced: quality = XESS_QUALITY_SETTING_BALANCED; break;
    case UpscaleQuality::Performance: quality = XESS_QUALITY_SETTING_PERFORMANCE; break;
    case UpscaleQuality::UltraPerformance: quality = XESS_QUALITY_SETTING_ULTRA_PERFORMANCE; break;
  }
  xess_2d_t output{width, height}, input{},minimum{},maximum{};
  result = m->xess_resolution(m->xess, &output, quality, &input,&minimum,&maximum);
  if (result < XESS_RESULT_SUCCESS || !input.x || !input.y) { error = "XeSS resolution query failed"; return false; }
  m->render_width = input.x; m->render_height = input.y;
  xess_d3d12_init_params_t init{};
  init.outputResolution = output;
  init.qualitySetting = quality;
  init.initFlags = XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE | XESS_INIT_FLAG_RESPONSIVE_PIXEL_MASK;
  result = m->xess_init(m->xess, &init);
  if (result < XESS_RESULT_SUCCESS) { error = "XeSS initialization failed: " + std::to_string(result); return false; }
  xess_version_t version{};
  m->xess_version(&version);
  m->provider = "Intel XeSS " + std::to_string(version.major) + "." +
                std::to_string(version.minor) + "." + std::to_string(version.patch);
  return true;
#else
  error = "Intel XeSS support was not built; configure FURY_XESS_ROOT"; return false;
#endif
}

bool Dx12Upscaler::dispatch(const UpscaleFrame& f, std::string& error) {
  if (m->kind == Upscaler::Native) return true;
  // Both APIs expect the raster image's displacement. Sampling a ray at p+j
  // shifts the rendered image by -j; passing +j produces temporal blur.
  const auto jitter=reconstruction_jitter(f.jitter_x,f.jitter_y);
  if (m->kind == Upscaler::FSR) {
#if FURY_HAS_FSR
    ffxDispatchDescUpscale d{};
    d.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
    d.commandList = f.commands;
    d.color = ffxApiGetResourceDX12(f.color);
    d.depth = ffxApiGetResourceDX12(f.depth);
    d.motionVectors = ffxApiGetResourceDX12(f.motion);
    d.reactive = ffxApiGetResourceDX12(f.reactive);
    d.output = ffxApiGetResourceDX12(f.output, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
    d.jitterOffset = {jitter.x, jitter.y};
    d.motionVectorScale = {float(m->render_width) / float(m->width),
                           float(m->render_height) / float(m->height)};
    d.renderSize = {m->render_width, m->render_height};
    d.upscaleSize = {m->width, m->height};
    d.enableSharpening = true; d.sharpness = 0.2f;
    d.frameTimeDelta = (std::max)(0.1f, f.delta_ms);
    d.preExposure = 1.f; d.reset = f.reset;
    d.cameraNear = f.near_plane; d.cameraFar = f.far_plane;
    d.cameraFovAngleVertical = f.fov_y; d.viewSpaceToMetersFactor = 1.f;
    const auto result = m->fsr_dispatch(&m->fsr, &d.header);
    if (result != FFX_API_RETURN_OK) { error = "FSR dispatch failed: " + std::to_string(result); return false; }
    return true;
#endif
  }
#if FURY_HAS_XESS
  if (m->kind == Upscaler::XeSS) {
    xess_d3d12_execute_params_t d{};
    d.pColorTexture = f.color; d.pDepthTexture = f.depth;
    d.pVelocityTexture = f.motion; d.pResponsivePixelMaskTexture = f.reactive;
    d.pOutputTexture = f.output;
    d.jitterOffsetX = jitter.x; d.jitterOffsetY = jitter.y;
    d.exposureScale = 1.f; d.resetHistory = f.reset ? 1u : 0u;
    d.inputWidth = m->render_width; d.inputHeight = m->render_height;
    const auto result = m->xess_execute(m->xess, f.commands, &d);
    if (result < XESS_RESULT_SUCCESS) { error = "XeSS dispatch failed: " + std::to_string(result); return false; }
    return true;
  }
#endif
  error = "Upscaler is not initialized";
  return false;
}
}  // namespace fury
