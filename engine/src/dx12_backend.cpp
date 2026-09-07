#include "fury/renderer.hpp"
#include "fury/log.hpp"
#include "fury/texture.hpp"
#include "dx12_upscaler.hpp"

#include <SDL.h>
#include <SDL_syswm.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "trace_cs.h"
#include "sky_cs.h"
#include "temporal_cs.h"
#include "filter_cs.h"
#include "present_vs.h"
#include "present_ps.h"
#include "hud_vs.h"
#include "hud_ps.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace fury {
namespace {
using Microsoft::WRL::ComPtr;
constexpr unsigned kFrames = 2;
constexpr unsigned kSrvCount = 13;
constexpr unsigned kUavCount = 10;
constexpr unsigned kMaterialSlots = 64;
constexpr unsigned kMaterialDescriptorBase = kSrvCount+kUavCount;

void check(HRESULT result, const char* operation) {
  if (FAILED(result)) {
    std::ostringstream message;
    message << operation << " failed (HRESULT 0x" << std::hex << unsigned(result) << ")";
    throw std::runtime_error(message.str());
  }
}
std::uint64_t hash_bytes(const void* bytes, std::size_t count, std::uint64_t hash=14695981039346656037ull) {
  const auto* p=static_cast<const unsigned char*>(bytes);
  for(std::size_t i=0;i<count;++i) { hash^=p[i]; hash*=1099511628211ull; }
  return hash;
}
struct GpuVertex {
  Vec3 position; unsigned material;
  Vec3 normal; float pad0{};
  Vec3 color; float pad1{};
  Vec2 uv; Vec2 pad2{};
  Vec3 previous_position; float pad3{};
};
static_assert(sizeof(GpuVertex)==80, "Shader vertex ABI");
struct GpuMaterial { Vec4 albedo, surface, optical, shading, emission, animation; };
static_assert(sizeof(GpuMaterial)==96, "Shader material ABI");
struct GpuInstance { Mat4 model,previous_model,normal_matrix; unsigned vertex_offset{},material{},pad0{},pad1{}; };
static_assert(sizeof(GpuInstance)==208,"Shader instance ABI");
struct FrameConstants {
  Mat4 inverse_vp, current_vp, previous_vp;
  Vec4 camera_time, sun_direction_intensity, sun_color, ambient, fog_color_start, fog_end_exposure;
  Vec4 point_position_radius[4], point_color_intensity[4];
  unsigned dimensions[4]{};
  unsigned sampling[4]{};
  float jitter_filter[4]{};
  unsigned options[4]{};
  Vec4 previous_jitter;
  Vec4 previous_camera;
};
static_assert(sizeof(FrameConstants)==512, "Shader cbuffer ABI");
struct HudVertex { float x,y,r,g,b,a; };
struct Buffer { ComPtr<ID3D12Resource> resource; std::uint64_t capacity{}; };
struct Texture {
  ComPtr<ID3D12Resource> resource;
  D3D12_RESOURCE_STATES state{D3D12_RESOURCE_STATE_COMMON};
};
struct ObjectHistory { Mat4 model{Mat4::identity()},normal_matrix{Mat4::identity()}; std::uint64_t frame{},mesh_identity{}; };
struct MeshEntry;
struct DrawSubmission { MeshEntry* geometry; Mat4 model; Material material; std::uint64_t id; };
struct MeshEntry {
  std::uint64_t identity{},revision{~std::uint64_t(0)},processed_frame{~std::uint64_t(0)};
  std::size_t source_vertices{},source_indices{};
  unsigned offset{},count{};
  bool needs_build{true},settle_motion{false};
  float minimum_opacity{1.f};
  std::vector<Vec3> previous_positions;
  Buffer blas;
};

class Dx12Backend final : public IRenderBackend {
 public:
  ~Dx12Backend() override { destroy(); }
  bool create(SDL_Window* window, int width, int height) override;
  void destroy() override;
  void begin_frame(const Color&) override;
  void set_view_proj(const Mat4& view, const Mat4& projection) override;
  void set_camera_position(const Vec3& p) override { m_camera=p; }
  void set_lighting(const Lighting& lighting) override { m_lighting=lighting; }
  void set_time(float seconds) override { m_time=seconds; }
  void set_object_id(std::uint64_t id) override { m_object_id=id; }
  void draw_mesh(const Mesh& mesh, const Mat4& model, const Material& material) override;
  void draw_hud_rect(float x,float y,float width,float height,const Color& color) override;
  void end_frame() override;
  void upload_mesh(Mesh& mesh) override { mesh.mark_dirty(); }
  void resize(int width,int height) override;
  RenderBackendKind kind() const override { return RenderBackendKind::Direct3D12; }
  const char* name() const override { return "Direct3D 12 / hardware DXR 1.1"; }
  bool configure(const RenderSettings& settings) override;
  RenderStatistics statistics() const override { return m_statistics; }
  RenderSettings settings() const override { return m_settings; }
  void reset_history() override { m_reset=true; m_accumulated=0; }
  bool read_rgb_framebuffer(std::vector<std::uint8_t>& rgb,int& width,int& height) override;

 private:
  void make_device(HWND window);
  void make_pipeline();
  void make_targets();
  void make_atlases();
  Texture upload_texture(RgbaImage image,TextureEncoding encoding,unsigned descriptor);
  unsigned material_slot(const Material& material);
  GpuMaterial pack_material(const Material& material,bool newborn);
  void acquire_backbuffers();
  void ensure_buffer(Buffer& buffer,std::uint64_t size,D3D12_HEAP_TYPE heap,
                     D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES state);
  void upload(Buffer& buffer,const void* bytes,std::size_t size);
  Texture make_texture(unsigned width,unsigned height,DXGI_FORMAT format,unsigned layers=1,
                       D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,unsigned mips=1);
  void transition(Texture& texture,D3D12_RESOURCE_STATES state);
  void uav_barrier(ID3D12Resource* resource);
  D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor(unsigned index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE gpu_descriptor(unsigned index) const;
  void srv(Texture& texture,unsigned index,DXGI_FORMAT format,unsigned layers=1);
  void uav(Texture& texture,unsigned index,DXGI_FORMAT format);
  void open_commands();
  void submit_and_wait();
  void wait_idle();
  void build_acceleration_structure();
  void prepare_geometry();
  MeshEntry& cache_mesh(const Mesh& mesh);
  void bind_root();
  bool render_frame();
  void collect_validation();
  void fail(const std::string& message);

  SDL_Window* m_window{};
  unsigned m_width{},m_height{},m_render_width{},m_render_height{},m_descriptor_size{},m_rtv_size{};
  ComPtr<IDXGIFactory6> m_factory;
  ComPtr<ID3D12Device5> m_device;
  ComPtr<ID3D12CommandQueue> m_queue;
  ComPtr<IDXGISwapChain3> m_swapchain;
  ComPtr<ID3D12CommandAllocator> m_allocator;
  ComPtr<ID3D12GraphicsCommandList4> m_commands;
  ComPtr<ID3D12Fence> m_fence;
  ComPtr<ID3D12InfoQueue> m_info;
  ComPtr<ID3D12DescriptorHeap> m_heap,m_rtv_heap;
  ComPtr<ID3D12RootSignature> m_root;
  ComPtr<ID3D12PipelineState> m_sky_pso,m_trace_pso,m_temporal_pso,m_filter_pso,m_present_pso,m_hud_pso;
  ComPtr<ID3D12QueryHeap> m_timestamps;
  Buffer m_query_readback;
  HANDLE m_fence_event{};
  UINT64 m_fence_value{},m_timestamp_frequency{};
  std::array<Texture,kFrames> m_backbuffers;
  Texture m_radiance,m_depth,m_motion,m_geometry,m_filtered,m_reactive,m_upscaled,m_albedo,m_normals;
  std::array<Texture,2> m_history_color,m_history_geometry;
  Texture m_sky;
  Texture m_direct;
  std::vector<std::array<Texture,4>> m_material_textures;
  std::unordered_map<const MaterialTextures*,unsigned> m_texture_slots;
  std::vector<std::shared_ptr<const MaterialTextures>> m_texture_owners;
  std::vector<unsigned char> m_minimum_texture_alpha;
  Buffer m_vertices_gpu,m_materials_gpu,m_constants_gpu,m_hud_gpu,m_tlas,m_scratch,m_instances,m_capture,m_shader_instances;
  Buffer m_vertex_staging;
  D3D12_RESOURCE_STATES m_vertex_state{D3D12_RESOURCE_STATE_COPY_DEST};
  std::vector<GpuVertex> m_vertices;
  std::vector<GpuMaterial> m_materials;
  std::vector<HudVertex> m_hud;
  std::vector<DrawSubmission> m_submissions;
  std::unordered_map<std::uint64_t,std::unique_ptr<MeshEntry>> m_mesh_cache;
  std::vector<GpuInstance> m_gpu_instances;
  std::vector<MeshEntry*> m_instance_meshes;
  std::vector<std::pair<unsigned,unsigned>> m_vertex_uploads;
  std::unordered_map<std::uint64_t,ObjectHistory> m_history;
  std::unique_ptr<Dx12Upscaler> m_upscaler;
  RenderSettings m_settings;
  RenderStatistics m_statistics;
  Lighting m_lighting;
  Mat4 m_vp{Mat4::identity()},m_inverse_vp{Mat4::identity()},m_previous_vp{Mat4::identity()};
  Vec3 m_camera{},m_previous_camera{};
  float m_time{},m_near{.1f},m_far{500},m_fov{1.0472f};
  Vec2 m_previous_jitter{};
  std::uint64_t m_frame{},m_object_id{},m_draw_number{},m_geometry_hash{},m_last_geometry_hash{},m_last_scene_hash{};
  std::uint64_t m_submission_hash{};
  unsigned m_accumulated{},m_capture_index{};
  bool m_reset{true},m_rendered{false},m_failed{false},m_has_water{false};
  bool m_stream_dirty{true};
  std::uint64_t m_sky_hash{};
  std::chrono::steady_clock::time_point m_last_frame_time{};
};

void Dx12Backend::ensure_buffer(Buffer& buffer,std::uint64_t size,D3D12_HEAP_TYPE heap,
                                D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES state) {
  size=(std::max)(std::uint64_t(256),(size+255)&~std::uint64_t(255));
  if(buffer.resource && buffer.capacity>=size) return;
  D3D12_HEAP_PROPERTIES hp{}; hp.Type=heap;
  D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width=size; desc.Height=1; desc.DepthOrArraySize=1; desc.MipLevels=1;
  desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR; desc.Flags=flags;
  ComPtr<ID3D12Resource> resource;
  check(m_device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,
                                         IID_PPV_ARGS(&resource)),"Create buffer");
  buffer.resource=std::move(resource); buffer.capacity=size;
}
void Dx12Backend::upload(Buffer& buffer,const void* bytes,std::size_t size) {
  ensure_buffer(buffer,size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_GENERIC_READ);
  void* destination{}; D3D12_RANGE no_read{0,0};
  check(buffer.resource->Map(0,&no_read,&destination),"Map upload buffer");
  if(size) std::memcpy(destination,bytes,size);
  D3D12_RANGE written{0,size}; buffer.resource->Unmap(0,&written);
}
Texture Dx12Backend::make_texture(unsigned width,unsigned height,DXGI_FORMAT format,unsigned layers,
                                  D3D12_RESOURCE_FLAGS flags,unsigned mips) {
  D3D12_HEAP_PROPERTIES hp{}; hp.Type=D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width=width; desc.Height=height; desc.DepthOrArraySize=static_cast<UINT16>(layers);
  desc.MipLevels=static_cast<UINT16>(mips); desc.Format=format; desc.SampleDesc.Count=1; desc.Flags=flags;
  Texture texture;
  check(m_device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,texture.state,nullptr,
                                         IID_PPV_ARGS(&texture.resource)),"Create texture");
  return texture;
}
void Dx12Backend::transition(Texture& texture,D3D12_RESOURCE_STATES state) {
  if(texture.state==state) return;
  D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource=texture.resource.Get(); barrier.Transition.StateBefore=texture.state;
  barrier.Transition.StateAfter=state; barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  m_commands->ResourceBarrier(1,&barrier); texture.state=state;
}
void Dx12Backend::uav_barrier(ID3D12Resource* resource) {
  D3D12_RESOURCE_BARRIER b{}; b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV; b.UAV.pResource=resource;
  m_commands->ResourceBarrier(1,&b);
}
D3D12_CPU_DESCRIPTOR_HANDLE Dx12Backend::cpu_descriptor(unsigned index) const {
  auto h=m_heap->GetCPUDescriptorHandleForHeapStart(); h.ptr+=SIZE_T(index)*m_descriptor_size; return h;
}
D3D12_GPU_DESCRIPTOR_HANDLE Dx12Backend::gpu_descriptor(unsigned index) const {
  auto h=m_heap->GetGPUDescriptorHandleForHeapStart(); h.ptr+=UINT64(index)*m_descriptor_size; return h;
}
void Dx12Backend::srv(Texture& texture,unsigned index,DXGI_FORMAT format,unsigned layers) {
  D3D12_SHADER_RESOURCE_VIEW_DESC d{}; d.Format=format; d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  if(layers>1) { d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY; d.Texture2DArray.MipLevels=1; d.Texture2DArray.ArraySize=layers; }
  else { d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D; d.Texture2D.MipLevels=1; }
  m_device->CreateShaderResourceView(texture.resource.Get(),&d,cpu_descriptor(index));
}
void Dx12Backend::uav(Texture& texture,unsigned index,DXGI_FORMAT format) {
  D3D12_UNORDERED_ACCESS_VIEW_DESC d{}; d.Format=format; d.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
  m_device->CreateUnorderedAccessView(texture.resource.Get(),nullptr,&d,cpu_descriptor(kSrvCount+index));
}

void Dx12Backend::make_device(HWND window) {
  const char* debug=std::getenv("FURY_DX12_DEBUG");
  m_settings.debug_layer=debug && std::strcmp(debug,"0")!=0;
  if(m_settings.debug_layer) {
    ComPtr<ID3D12Debug> layer;
    check(D3D12GetDebugInterface(IID_PPV_ARGS(&layer)),"DX12 debug layer (install Graphics Tools)");
    layer->EnableDebugLayer();
  }
  check(CreateDXGIFactory2(m_settings.debug_layer ? DXGI_CREATE_FACTORY_DEBUG : 0,IID_PPV_ARGS(&m_factory)),"Create DXGI factory");
  for(unsigned index=0;;++index) {
    ComPtr<IDXGIAdapter1> adapter;
    if(m_factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND) break;
    DXGI_ADAPTER_DESC1 description{}; adapter->GetDesc1(&description);
    if(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
    ComPtr<ID3D12Device5> device;
    if(FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))) continue;
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 features{};
    if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&features,sizeof(features))) ||
       features.RaytracingTier<D3D12_RAYTRACING_TIER_1_1) continue;
    D3D12_FEATURE_DATA_SHADER_MODEL shader_model{D3D_SHADER_MODEL_6_5};
    if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&shader_model,sizeof(shader_model))) ||
       shader_model.HighestShaderModel<D3D_SHADER_MODEL_6_5) continue;
    m_device=std::move(device);
    char name[512]{};
    WideCharToMultiByte(CP_UTF8,0,description.Description,-1,name,sizeof(name),nullptr,nullptr);
    m_statistics.adapter=name; break;
  }
  if(!m_device) throw std::runtime_error("No hardware adapter supports DXR 1.1 and Shader Model 6.5");
  if(m_settings.debug_layer) check(m_device.As(&m_info),"Query DX12 info queue");
  D3D12_COMMAND_QUEUE_DESC queue{}; queue.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
  check(m_device->CreateCommandQueue(&queue,IID_PPV_ARGS(&m_queue)),"Create command queue");
  check(m_queue->GetTimestampFrequency(&m_timestamp_frequency),"Query GPU timer frequency");
  DXGI_SWAP_CHAIN_DESC1 swap{}; swap.Width=m_width; swap.Height=m_height;
  swap.Format=DXGI_FORMAT_R8G8B8A8_UNORM; swap.SampleDesc.Count=1;
  swap.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; swap.BufferCount=kFrames;
  swap.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
  ComPtr<IDXGISwapChain1> chain;
  check(m_factory->CreateSwapChainForHwnd(m_queue.Get(),window,&swap,nullptr,nullptr,&chain),"Create swapchain");
  check(chain.As(&m_swapchain),"Query swapchain");
  check(m_factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER),"Configure window association");
  check(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&m_allocator)),"Create command allocator");
  check(m_device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,m_allocator.Get(),nullptr,IID_PPV_ARGS(&m_commands)),"Create command list");
  check(m_commands->Close(),"Close initial command list");
  check(m_device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&m_fence)),"Create fence");
  m_fence_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
  if(!m_fence_event) throw std::runtime_error("Create GPU fence event failed");
  D3D12_DESCRIPTOR_HEAP_DESC heap{}; heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heap.NumDescriptors=kMaterialDescriptorBase+kMaterialSlots*4; heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  check(m_device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&m_heap)),"Create resource descriptor heap");
  heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heap.NumDescriptors=kFrames; heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  check(m_device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&m_rtv_heap)),"Create RTV descriptor heap");
  m_descriptor_size=m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_rtv_size=m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  D3D12_QUERY_HEAP_DESC query{}; query.Count=2; query.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
  check(m_device->CreateQueryHeap(&query,IID_PPV_ARGS(&m_timestamps)),"Create timestamp heap");
  ensure_buffer(m_query_readback,16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
  acquire_backbuffers();
  m_statistics.hardware_ray_tracing=true;
  m_statistics.debug_layer_active=m_info!=nullptr;
  Log::info("DX12 adapter: "+m_statistics.adapter+"; hardware DXR 1.1 / SM 6.5");
}
void Dx12Backend::acquire_backbuffers() {
  auto handle=m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
  for(unsigned i=0;i<kFrames;++i) {
    check(m_swapchain->GetBuffer(i,IID_PPV_ARGS(&m_backbuffers[i].resource)),"Acquire backbuffer");
    m_backbuffers[i].state=D3D12_RESOURCE_STATE_PRESENT;
    m_device->CreateRenderTargetView(m_backbuffers[i].resource.Get(),nullptr,handle);
    handle.ptr+=m_rtv_size;
  }
}
void Dx12Backend::make_pipeline() {
  D3D12_DESCRIPTOR_RANGE ranges[3]{};
  ranges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[0].NumDescriptors=kSrvCount-2;
  ranges[0].BaseShaderRegister=5; ranges[0].OffsetInDescriptorsFromTableStart=2;
  ranges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV; ranges[1].NumDescriptors=kUavCount;
  ranges[2].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[2].NumDescriptors=kMaterialSlots*4; ranges[2].BaseShaderRegister=16;
  D3D12_ROOT_PARAMETER parameters[8]{};
  parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV; parameters[0].Descriptor.ShaderRegister=0;
  for(unsigned i=1;i<=3;++i) { parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV; parameters[i].Descriptor.ShaderRegister=i-1; }
  for(unsigned i=0;i<2;++i) { parameters[i+4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[i+4].DescriptorTable.NumDescriptorRanges=1; parameters[i+4].DescriptorTable.pDescriptorRanges=&ranges[i]; }
  parameters[6].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[6].DescriptorTable.NumDescriptorRanges=1; parameters[6].DescriptorTable.pDescriptorRanges=&ranges[2];
  parameters[7].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV; parameters[7].Descriptor.ShaderRegister=3;
  D3D12_STATIC_SAMPLER_DESC samplers[2]{};
  for(unsigned i=0;i<2;++i) {
    samplers[i].Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[i].AddressU=samplers[i].AddressV=samplers[i].AddressW=i ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplers[i].ShaderRegister=i; samplers[i].MaxLOD=D3D12_FLOAT32_MAX; samplers[i].MaxAnisotropy=1;
    samplers[i].ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
  }
  D3D12_ROOT_SIGNATURE_DESC root{}; root.NumParameters=8; root.pParameters=parameters;
  root.NumStaticSamplers=2; root.pStaticSamplers=samplers;
  root.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  ComPtr<ID3DBlob> signature,errors;
  check(D3D12SerializeRootSignature(&root,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors),"Serialize root signature");
  check(m_device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&m_root)),"Create root signature");
  D3D12_COMPUTE_PIPELINE_STATE_DESC compute{}; compute.pRootSignature=m_root.Get();
  compute.CS={sky_cs_bytecode,sizeof(sky_cs_bytecode)};
  check(m_device->CreateComputePipelineState(&compute,IID_PPV_ARGS(&m_sky_pso)),"Create atmosphere pipeline");
  compute.CS={trace_cs_bytecode,sizeof(trace_cs_bytecode)};
  check(m_device->CreateComputePipelineState(&compute,IID_PPV_ARGS(&m_trace_pso)),"Create ray-query pipeline");
  compute.CS={temporal_cs_bytecode,sizeof(temporal_cs_bytecode)};
  check(m_device->CreateComputePipelineState(&compute,IID_PPV_ARGS(&m_temporal_pso)),"Create temporal denoise pipeline");
  compute.CS={filter_cs_bytecode,sizeof(filter_cs_bytecode)};
  check(m_device->CreateComputePipelineState(&compute,IID_PPV_ARGS(&m_filter_pso)),"Create denoise pipeline");
  D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics{}; graphics.pRootSignature=m_root.Get();
  graphics.VS={present_vs_bytecode,sizeof(present_vs_bytecode)}; graphics.PS={present_ps_bytecode,sizeof(present_ps_bytecode)};
  graphics.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
  graphics.SampleMask=UINT_MAX; graphics.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
  graphics.RasterizerState.CullMode=D3D12_CULL_MODE_NONE; graphics.RasterizerState.DepthClipEnable=TRUE;
  graphics.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
  graphics.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  graphics.NumRenderTargets=1; graphics.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM; graphics.SampleDesc.Count=1;
  check(m_device->CreateGraphicsPipelineState(&graphics,IID_PPV_ARGS(&m_present_pso)),"Create display pipeline");
  graphics.VS={hud_vs_bytecode,sizeof(hud_vs_bytecode)}; graphics.PS={hud_ps_bytecode,sizeof(hud_ps_bytecode)};
  D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
                                   {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
  graphics.InputLayout={input,2};
  auto& blend=graphics.BlendState.RenderTarget[0]; blend.BlendEnable=TRUE;
  blend.SrcBlend=D3D12_BLEND_SRC_ALPHA; blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA; blend.BlendOp=D3D12_BLEND_OP_ADD;
  blend.SrcBlendAlpha=D3D12_BLEND_ONE; blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA; blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
  check(m_device->CreateGraphicsPipelineState(&graphics,IID_PPV_ARGS(&m_hud_pso)),"Create HUD pipeline");
}
void Dx12Backend::open_commands() {
  check(m_allocator->Reset(),"Reset command allocator");
  check(m_commands->Reset(m_allocator.Get(),nullptr),"Reset command list");
}
void Dx12Backend::wait_idle() {
  if(!m_queue || !m_fence || !m_fence_event) return;
  const auto value=++m_fence_value;
  check(m_queue->Signal(m_fence.Get(),value),"Signal GPU fence");
  if(m_fence->GetCompletedValue()<value) {
    check(m_fence->SetEventOnCompletion(value,m_fence_event),"Set GPU fence event");
    if(WaitForSingleObject(m_fence_event,30000)!=WAIT_OBJECT_0)
      throw std::runtime_error("GPU fence timed out after 30 seconds");
  }
  check(m_device->GetDeviceRemovedReason(),"GPU device status");
}
void Dx12Backend::submit_and_wait() {
  check(m_commands->Close(),"Close command list");
  ID3D12CommandList* lists[]={m_commands.Get()}; m_queue->ExecuteCommandLists(1,lists);
  wait_idle();
}

void Dx12Backend::make_targets() {
  m_render_width=m_upscaler->render_width(); m_render_height=m_upscaler->render_height();
  m_radiance=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R16G16B16A16_FLOAT);
  m_depth=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R32_FLOAT);
  m_motion=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R16G16_FLOAT);
  m_geometry=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R16G16B16A16_FLOAT);
  m_filtered=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R16G16B16A16_FLOAT);
  m_reactive=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R8_UNORM);
  m_upscaled=make_texture(m_width,m_height,DXGI_FORMAT_R16G16B16A16_FLOAT);
  m_sky=make_texture(512,256,DXGI_FORMAT_R16G16B16A16_FLOAT); m_sky_hash=0;
  srv(m_sky,9,DXGI_FORMAT_R16G16B16A16_FLOAT); uav(m_sky,8,DXGI_FORMAT_R16G16B16A16_FLOAT);
  m_direct=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R16G16B16A16_FLOAT);
  srv(m_direct,10,DXGI_FORMAT_R16G16B16A16_FLOAT); uav(m_direct,9,DXGI_FORMAT_R16G16B16A16_FLOAT);
  for(unsigned i=0;i<2;++i) {
    m_history_color[i]=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_history_geometry[i]=make_texture(m_render_width,m_render_height,DXGI_FORMAT_R16G16B16A16_FLOAT);
  }
  srv(m_radiance,2,DXGI_FORMAT_R16G16B16A16_FLOAT); srv(m_geometry,3,DXGI_FORMAT_R16G16B16A16_FLOAT);
  srv(m_filtered,4,DXGI_FORMAT_R16G16B16A16_FLOAT); srv(m_upscaled,5,DXGI_FORMAT_R16G16B16A16_FLOAT);
  uav(m_radiance,0,DXGI_FORMAT_R16G16B16A16_FLOAT); uav(m_depth,1,DXGI_FORMAT_R32_FLOAT);
  uav(m_motion,2,DXGI_FORMAT_R16G16_FLOAT); uav(m_geometry,3,DXGI_FORMAT_R16G16B16A16_FLOAT);
  uav(m_filtered,4,DXGI_FORMAT_R16G16B16A16_FLOAT); uav(m_reactive,5,DXGI_FORMAT_R8_UNORM);
  srv(m_depth,11,DXGI_FORMAT_R32_FLOAT); srv(m_motion,12,DXGI_FORMAT_R16G16_FLOAT);
  m_statistics.render_width=m_render_width; m_statistics.render_height=m_render_height;
  m_statistics.output_width=m_width; m_statistics.output_height=m_height;
  m_statistics.upscaler=m_upscaler->provider();
  reset_history();
}
Texture Dx12Backend::upload_texture(RgbaImage image,TextureEncoding encoding,unsigned descriptor) {
    auto levels=build_mip_chain(std::move(image),encoding);
    if(levels.empty()) throw std::runtime_error("Invalid material texture");
    auto texture=make_texture(unsigned(levels[0].width),unsigned(levels[0].height),DXGI_FORMAT_R8G8B8A8_UNORM,
                              1,D3D12_RESOURCE_FLAG_NONE,unsigned(levels.size()));
    const auto desc=texture.resource->GetDesc();
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(levels.size());
    UINT64 total{}; m_device->GetCopyableFootprints(&desc,0,UINT(levels.size()),0,layouts.data(),nullptr,nullptr,&total);
    Buffer staging; ensure_buffer(staging,total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_GENERIC_READ);
    unsigned char* mapped{}; D3D12_RANGE no_read{0,0};
    check(staging.resource->Map(0,&no_read,reinterpret_cast<void**>(&mapped)),"Map texture staging");
    for(unsigned level=0;level<levels.size();++level) {
      const auto& mip=levels[level];
      for(int y=0;y<mip.height;++y)
        std::memcpy(mapped+layouts[level].Offset+std::size_t(y)*layouts[level].Footprint.RowPitch,
                    mip.pixels.data()+std::size_t(y)*mip.width*4,std::size_t(mip.width)*4);
    }
    staging.resource->Unmap(0,nullptr);
    open_commands(); transition(texture,D3D12_RESOURCE_STATE_COPY_DEST);
    for(unsigned level=0;level<levels.size();++level) {
      D3D12_TEXTURE_COPY_LOCATION dst{}; dst.pResource=texture.resource.Get(); dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex=level;
      D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource=staging.resource.Get(); src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint=layouts[level];
      m_commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
    }
    transition(texture,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); submit_and_wait();
    D3D12_SHADER_RESOURCE_VIEW_DESC view{}; view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Format=encoding==TextureEncoding::SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels=UINT(levels.size());
    m_device->CreateShaderResourceView(texture.resource.Get(),&view,cpu_descriptor(descriptor));
    return texture;
}
void Dx12Backend::make_atlases() {
  D3D12_SHADER_RESOURCE_VIEW_DESC null_view{}; null_view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
  null_view.Format=DXGI_FORMAT_R8G8B8A8_UNORM; null_view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  null_view.Texture2D.MipLevels=1;
  for(unsigned i=0;i<kMaterialSlots*4;++i) m_device->CreateShaderResourceView(nullptr,&null_view,cpu_descriptor(kMaterialDescriptorBase+i));
  m_device->CreateShaderResourceView(nullptr,&null_view,cpu_descriptor(0));
  m_device->CreateShaderResourceView(nullptr,&null_view,cpu_descriptor(1));
  auto rgba=[](const Image& image) {
    RgbaImage out; out.width=image.width; out.height=image.height;
    out.pixels.resize(std::size_t(out.width)*out.height*4,255);
    for(std::size_t i=0;i<image.rgb.size()/3;++i) for(unsigned c=0;c<3;++c) out.pixels[i*4+c]=image.rgb[i*3+c];
    return out;
  };
  for(unsigned layer=0;layer<unsigned(TextureSlot::Count);++layer) {
    Image color,normal;
    if(layer==0) { color.width=color.height=1; color.rgb={255,255,255}; }
    else resolve_texture_pixels(static_cast<TextureSlot>(layer),64,color);
    if(!resolve_normal_pixels(static_cast<TextureSlot>(layer),64,normal)) { normal.width=normal.height=1; normal.rgb={128,128,255}; }
    std::array<Texture,4> maps;
    maps[0]=upload_texture(rgba(color),TextureEncoding::SRGB,kMaterialDescriptorBase+layer*4);
    maps[1]=upload_texture(rgba(normal),TextureEncoding::Normal,kMaterialDescriptorBase+layer*4+1);
    maps[2]=upload_texture({1,1,{255,255,255,255}},TextureEncoding::Linear,kMaterialDescriptorBase+layer*4+2);
    maps[3]=upload_texture({1,1,{255,255,255,255}},TextureEncoding::SRGB,kMaterialDescriptorBase+layer*4+3);
    m_material_textures.push_back(std::move(maps));
    m_minimum_texture_alpha.push_back(255);
  }
}
unsigned Dx12Backend::material_slot(const Material& material) {
  if(!material.textures) return std::clamp(unsigned(material.texture),0u,unsigned(TextureSlot::Count)-1);
  auto found=m_texture_slots.find(material.textures.get());
  if(found!=m_texture_slots.end()) return found->second;
  const unsigned slot=unsigned(m_material_textures.size());
  if(slot>=kMaterialSlots) throw std::runtime_error("DX12 material texture capacity exceeded (64 sets)");
  const auto& input=*material.textures;
  std::array<Texture,4> maps;
  const RgbaImage* images[]={&input.base_color,&input.normal,&input.metallic_roughness,&input.emissive};
  for(unsigned i=0;i<4;++i) {
    RgbaImage pixels=images[i]->valid() ? *images[i] : RgbaImage{1,1,i==1 ?
      std::vector<std::uint8_t>{128,128,255,255} : std::vector<std::uint8_t>{255,255,255,255}};
    const auto encoding=(i==0 || i==3) ? TextureEncoding::SRGB : (i==1 ? TextureEncoding::Normal : TextureEncoding::Linear);
    maps[i]=upload_texture(std::move(pixels),encoding,kMaterialDescriptorBase+slot*4+i);
  }
  m_material_textures.push_back(std::move(maps)); m_texture_owners.push_back(material.textures);
  unsigned char minimum_alpha=255;
  for(std::size_t i=3;i<input.base_color.pixels.size();i+=4) minimum_alpha=(std::min)(minimum_alpha,input.base_color.pixels[i]);
  m_minimum_texture_alpha.push_back(minimum_alpha);
  m_texture_slots.emplace(material.textures.get(),slot);
  Log::info("DX12 imported texture set "+std::to_string(slot)+": "+input.source);
  return slot;
}
GpuMaterial Dx12Backend::pack_material(const Material& material,bool newborn) {
  GpuMaterial gm{};
  gm.albedo=Vec4(material.albedo,material.opacity);
  gm.surface={std::clamp(material.metallic,0.f,1.f),std::clamp(material.roughness,.045f,1.f),
              (std::max)(0.f,material.emissive),float(material_slot(material))};
  gm.optical={material.wetness,material.transmission,material.index_of_refraction,newborn ? 1.f : 0.f};
  gm.shading={material.alpha_cutoff,material.normal_scale,material.double_sided ? 1.f : 0.f,material.texture==TextureSlot::Water ? 1.f : 0.f};
  Vec3 emission=material.emissive_color;
  if(!material.textures) emission={emission.x*material.albedo.x,emission.y*material.albedo.y,emission.z*material.albedo.z};
  gm.emission=Vec4(emission,0);
  gm.animation={material.uv_scroll_u,material.uv_scroll_v,material.alpha_blend ? 1.f : 0.f,0};
  return gm;
}

bool Dx12Backend::create(SDL_Window* window,int width,int height) {
  if(!window || width<=0 || height<=0) return false;
  m_window=window; m_width=unsigned(width); m_height=unsigned(height);
  try {
    SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
    if(!SDL_GetWindowWMInfo(window,&info) || info.subsystem!=SDL_SYSWM_WINDOWS)
      throw std::runtime_error("DX12 requires an SDL Windows HWND");
    make_device(info.info.win.window); make_pipeline(); make_atlases();
    m_upscaler=std::make_unique<Dx12Upscaler>();
    RenderSettings initial=m_settings;
    if(const char* upscaler=std::getenv("FURY_UPSCALER"))
      if(!parse_upscaler(upscaler,initial.upscaler)) throw std::runtime_error("Invalid FURY_UPSCALER");
    if(const char* mode=std::getenv("FURY_TRACE_MODE")) {
      if(std::strcmp(mode,"ray")==0) initial.trace_mode=TraceMode::RayTraced;
      else if(std::strcmp(mode,"path")!=0) throw std::runtime_error("Invalid FURY_TRACE_MODE (ray or path)");
    }
    if(!configure(initial)) return false;
    m_last_frame_time=std::chrono::steady_clock::now();
    return true;
  } catch(const std::exception& error) { Log::error(error.what()); destroy(); return false; }
}
bool Dx12Backend::configure(const RenderSettings& settings) {
  if(!m_device || settings.samples_per_pixel<1 || settings.samples_per_pixel>64 ||
      settings.max_bounces<1 || settings.max_bounces>16 ||
      !std::isfinite(settings.exposure) || settings.exposure<=0) return false;
  try {
    wait_idle();
    if(m_filtered.resource && m_upscaler && settings.upscaler==m_settings.upscaler &&
        settings.quality==m_settings.quality && m_statistics.output_width==m_width && m_statistics.output_height==m_height) {
      m_settings=settings; reset_history(); return true;
    }
    auto replacement=std::make_unique<Dx12Upscaler>(); std::string error;
    if(!replacement->create(m_device.Get(),settings,m_width,m_height,error)) {
      Log::error(error); return false;
    }
    m_upscaler=std::move(replacement); m_settings=settings;
    make_targets();
    Log::info("DX12 reconstruction: "+m_upscaler->provider()+" / "+quality_name(settings.quality)+
              " / "+std::to_string(m_render_width)+"x"+std::to_string(m_render_height)+" -> "+
              std::to_string(m_width)+"x"+std::to_string(m_height));
    return true;
  } catch(const std::exception& error) { fail(error.what()); return false; }
}
void Dx12Backend::destroy() {
  try { wait_idle(); } catch(const std::exception& error) { Log::error(error.what()); }
  m_upscaler.reset();
  m_backbuffers={}; m_radiance={}; m_depth={}; m_motion={}; m_geometry={}; m_filtered={};
  m_reactive={}; m_upscaled={}; m_albedo={}; m_normals={};
  m_history_color={}; m_history_geometry={};
  m_sky={}; m_direct={}; m_sky_pso.Reset();
  m_material_textures.clear(); m_texture_slots.clear(); m_texture_owners.clear();
  m_minimum_texture_alpha.clear();
  m_vertices_gpu={}; m_materials_gpu={}; m_constants_gpu={}; m_hud_gpu={};
  m_vertex_staging={};
  m_mesh_cache.clear(); m_shader_instances={}; m_tlas={}; m_scratch={}; m_instances={}; m_capture={}; m_query_readback={};
  m_trace_pso.Reset(); m_temporal_pso.Reset(); m_filter_pso.Reset(); m_present_pso.Reset(); m_hud_pso.Reset();
  m_root.Reset(); m_heap.Reset(); m_rtv_heap.Reset(); m_timestamps.Reset();
  m_commands.Reset(); m_allocator.Reset(); m_swapchain.Reset(); m_fence.Reset(); m_queue.Reset();
  m_info.Reset(); m_device.Reset(); m_factory.Reset();
  if(m_fence_event) { CloseHandle(m_fence_event); m_fence_event=nullptr; }
  m_window=nullptr;
}
void Dx12Backend::begin_frame(const Color&) {
  m_submissions.clear(); m_hud.clear();
  m_vertex_uploads.clear(); m_stream_dirty=false;
  m_draw_number=0; m_object_id=0; m_rendered=false; m_has_water=false;
}
void Dx12Backend::set_view_proj(const Mat4& view,const Mat4& projection) {
  // Fury uses OpenGL clip depth [-1,1]. D3D and the upscalers require [0,1].
  Mat4 dx_projection=projection;
  for(int c=0;c<4;++c) dx_projection.at(c,2)=.5f*(projection.at(c,2)+projection.at(c,3));
  m_vp=dx_projection*view;
  Mat4 inverse_view;
  if(!inverse(m_vp,m_inverse_vp) || !inverse(view,inverse_view)) {
    fail("Non-invertible camera matrices"); return;
  }
  m_camera=transform_point(inverse_view,{});
  m_fov=2*std::atan(1/(std::max)(.001f,projection.at(1,1)));
  const float a=projection.at(2,2),b=projection.at(3,2);
  if(std::fabs(a-1)>1e-6f && std::fabs(a+1)>1e-6f) {
    m_near=b/(a-1); m_far=b/(a+1);
  }
}
void Dx12Backend::draw_mesh(const Mesh& mesh,const Mat4& model,const Material& material) {
  if(m_failed || m_rendered || mesh.indices.empty()) return;
  const auto id=m_object_id ? m_object_id : (0x8000000000000000ull|++m_draw_number);
  try { m_submissions.push_back({&cache_mesh(mesh),model,material,id}); }
  catch(const std::exception& error) { fail(error.what()); return; }
  m_has_water=m_has_water || material.texture==TextureSlot::Water;
}
void Dx12Backend::prepare_geometry() {
  m_materials.clear(); m_gpu_instances.clear(); m_instance_meshes.clear();
  m_geometry_hash=14695981039346656037ull;
  m_statistics.triangle_count=0;
  if(m_submissions.empty()) {
    static const Mesh empty=[] { Mesh mesh; mesh.vertices.resize(3); mesh.indices={0,1,2};
      for(auto& vertex:mesh.vertices) vertex.position={0,-100000,0}; return mesh; }();
    m_submissions.push_back({&cache_mesh(empty),Mat4::identity(),{},~std::uint64_t(0)});
  }
  for(const auto& draw:m_submissions) {
    auto& history=m_history[draw.id];
    const bool valid=m_frame>0 && history.frame+1==m_frame && history.mesh_identity==draw.geometry->identity;
    Mat4 normal_matrix=history.normal_matrix;
    if(!valid || std::memcmp(history.model.m,draw.model.m,sizeof(draw.model.m))!=0) {
      Mat4 inverse_model; if(!inverse(draw.model,inverse_model)) continue;
      normal_matrix=transpose(inverse_model);
    }
    auto& mesh=*draw.geometry;
    GpuInstance instance{}; instance.model=draw.model;
    instance.previous_model=valid ? history.model : draw.model; instance.normal_matrix=normal_matrix;
    instance.vertex_offset=mesh.offset; instance.material=unsigned(m_materials.size());
    m_materials.push_back(pack_material(draw.material,!valid));
    m_gpu_instances.push_back(instance); m_instance_meshes.push_back(&mesh);
    history.model=draw.model; history.normal_matrix=normal_matrix; history.frame=m_frame;
    history.mesh_identity=draw.geometry->identity;
    m_geometry_hash=hash_bytes(draw.model.m,sizeof(draw.model.m),m_geometry_hash);
    m_geometry_hash=hash_bytes(&mesh.identity,sizeof(mesh.identity),m_geometry_hash);
    m_geometry_hash=hash_bytes(&mesh.revision,sizeof(mesh.revision),m_geometry_hash);
    const auto& packed=m_materials.back();
    const float visibility[]={packed.shading.x,packed.shading.z,packed.albedo.w,packed.animation.z,packed.surface.w};
    m_geometry_hash=hash_bytes(visibility,sizeof(visibility),m_geometry_hash);
    m_statistics.triangle_count+=mesh.count/3;
  }
  if(m_gpu_instances.empty()) throw std::runtime_error("Scene has no non-singular mesh instances");
  m_statistics.instance_count=unsigned(m_gpu_instances.size());
  m_statistics.unique_triangle_count=0;
  for(const auto& mesh:m_mesh_cache) if(mesh.second->processed_frame==m_frame)
    m_statistics.unique_triangle_count+=mesh.second->count/3;
}
MeshEntry& Dx12Backend::cache_mesh(const Mesh& mesh) {
  auto& entry_ptr=m_mesh_cache[mesh.geometry_identity];
  if(!entry_ptr) entry_ptr=std::make_unique<MeshEntry>();
  auto& entry=*entry_ptr;
  if(entry.processed_frame==m_frame) return entry;
  entry.processed_frame=m_frame;
  const bool changed=entry.identity!=mesh.geometry_identity || entry.revision!=mesh.geometry_revision ||
                     entry.source_vertices!=mesh.vertices.size() || entry.source_indices!=mesh.indices.size() ||
                     (mesh.gpu_dirty && mesh.geometry_revision==0);
  if(!changed && !entry.settle_motion) return entry;
  if(mesh.indices.empty() || mesh.indices.size()%3 || mesh.indices.size()>30000000)
    throw std::runtime_error("Invalid DXR triangle index count");
  const unsigned count=unsigned(mesh.indices.size());
  const bool topology_changed=entry.count!=count || entry.identity!=mesh.geometry_identity;
  const bool previous_valid=!topology_changed && entry.previous_positions.size()==mesh.vertices.size();
  if(topology_changed || entry.count==0) {
    if(m_vertices.size()+count>30000000) throw std::runtime_error("DXR geometry pool exceeds its 30-million-vertex budget");
    entry.offset=unsigned(m_vertices.size()); m_vertices.resize(m_vertices.size()+count); entry.count=count;
  }
  for(unsigned i=0;i<count;++i) {
    const unsigned index=mesh.indices[i];
    if(index>=mesh.vertices.size()) throw std::runtime_error("DXR vertex index out of bounds");
    const auto& vertex=mesh.vertices[index];
    GpuVertex gpu{}; gpu.position=vertex.position; gpu.normal=vertex.normal; gpu.color=vertex.color; gpu.uv=vertex.uv;
    gpu.pad1=vertex.opacity;
    gpu.previous_position=previous_valid ? entry.previous_positions[index] : vertex.position;
    m_vertices[entry.offset+i]=gpu;
  }
  entry.previous_positions.resize(mesh.vertices.size());
  entry.minimum_opacity=1.f;
  for(std::size_t i=0;i<mesh.vertices.size();++i) {
    entry.previous_positions[i]=mesh.vertices[i].position;
    entry.minimum_opacity=(std::min)(entry.minimum_opacity,mesh.vertices[i].opacity);
  }
  entry.source_vertices=mesh.vertices.size(); entry.source_indices=mesh.indices.size();
  entry.identity=mesh.geometry_identity; entry.revision=mesh.geometry_revision;
  entry.needs_build=entry.needs_build || changed;
  entry.settle_motion=changed;
  m_vertex_uploads.emplace_back(entry.offset,count); m_stream_dirty=true;
  return entry;
}
void Dx12Backend::draw_hud_rect(float x,float y,float width,float height,const Color& color) {
  if(width<=0 || height<=0 || m_rendered) return;
  const float r=color.r/255.f,g=color.g/255.f,b=color.b/255.f,a=color.a/255.f;
  for(const Vec2 p:std::array<Vec2,6>{{{x,y},{x+width,y},{x,y+height},{x,y+height},{x+width,y},{x+width,y+height}}})
    m_hud.push_back({p.x,p.y,r,g,b,a});
}
void Dx12Backend::build_acceleration_structure() {
  struct Pending { MeshEntry* mesh; D3D12_RAYTRACING_GEOMETRY_DESC geometry; D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes; };
  std::vector<Pending> pending;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottom{};
  bottom.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  bottom.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY; bottom.NumDescs=1;
  bottom.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  UINT64 scratch_size=0;
  for(auto& pair:m_mesh_cache) {
    auto& mesh=*pair.second;
    if(!mesh.needs_build || mesh.processed_frame!=m_frame) continue;
    Pending build{}; build.mesh=&mesh; build.geometry.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    build.geometry.Flags=D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    build.geometry.Triangles.VertexBuffer={m_vertices_gpu.resource->GetGPUVirtualAddress()+UINT64(mesh.offset)*sizeof(GpuVertex),sizeof(GpuVertex)};
    build.geometry.Triangles.VertexCount=mesh.count; build.geometry.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;
    bottom.pGeometryDescs=&build.geometry;
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&bottom,&build.sizes);
    if(!build.sizes.ResultDataMaxSizeInBytes) throw std::runtime_error("Invalid BLAS geometry");
    ensure_buffer(mesh.blas,build.sizes.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,
                  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
    scratch_size=(std::max)(scratch_size,build.sizes.ScratchDataSizeInBytes); pending.push_back(build);
  }
  const bool rebuild_top=!m_tlas.resource || m_geometry_hash!=m_last_geometry_hash || !pending.empty();
  if(!rebuild_top) return;
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ti{};
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top{};
  top.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
  top.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY; top.NumDescs=UINT(m_gpu_instances.size());
  top.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  m_device->GetRaytracingAccelerationStructurePrebuildInfo(&top,&ti);
  if(!ti.ResultDataMaxSizeInBytes) throw std::runtime_error("Invalid TLAS geometry");
  ensure_buffer(m_tlas,ti.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
  ensure_buffer(m_scratch,(std::max)(scratch_size,ti.ScratchDataSizeInBytes),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances(m_gpu_instances.size());
  for(unsigned i=0;i<instances.size();++i) {
    auto& instance=instances[i]; const auto& model=m_gpu_instances[i].model;
    for(unsigned r=0;r<3;++r) for(unsigned c=0;c<4;++c) instance.Transform[r][c]=model.at(c,r);
    instance.InstanceID=i; instance.InstanceMask=255;
    instance.AccelerationStructure=m_instance_meshes[i]->blas.resource->GetGPUVirtualAddress();
    const auto& material=m_materials[m_gpu_instances[i].material];
    instance.Flags=material.shading.z!=0 ? D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE : D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    const float minimum_alpha=material.albedo.w*m_instance_meshes[i]->minimum_opacity*
                               (float(m_minimum_texture_alpha[unsigned(material.surface.w)])/255.f);
    const bool opaque=(material.shading.x<0 && material.animation.z==0) ||
                      (material.animation.z!=0 ? minimum_alpha>=1 : minimum_alpha>=material.shading.x);
    if(opaque) instance.Flags|=D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
  }
  upload(m_instances,instances.data(),instances.size()*sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
  build.ScratchAccelerationStructureData=m_scratch.resource->GetGPUVirtualAddress();
  for(auto& entry:pending) {
    bottom.pGeometryDescs=&entry.geometry; build.Inputs=bottom;
    build.DestAccelerationStructureData=entry.mesh->blas.resource->GetGPUVirtualAddress();
    m_commands->BuildRaytracingAccelerationStructure(&build,0,nullptr); uav_barrier(entry.mesh->blas.resource.Get());
    ++m_statistics.blas_builds;
    uav_barrier(m_scratch.resource.Get()); entry.mesh->needs_build=false;
  }
  build.Inputs=top; build.Inputs.InstanceDescs=m_instances.resource->GetGPUVirtualAddress();
  build.DestAccelerationStructureData=m_tlas.resource->GetGPUVirtualAddress();
  m_commands->BuildRaytracingAccelerationStructure(&build,0,nullptr); uav_barrier(m_tlas.resource.Get());
  ++m_statistics.tlas_builds;
  m_last_geometry_hash=m_geometry_hash;
}
void Dx12Backend::bind_root() {
  ID3D12DescriptorHeap* heaps[]={m_heap.Get()}; m_commands->SetDescriptorHeaps(1,heaps);
  m_commands->SetComputeRootSignature(m_root.Get());
  m_commands->SetComputeRootConstantBufferView(0,m_constants_gpu.resource->GetGPUVirtualAddress());
  m_commands->SetComputeRootShaderResourceView(1,m_tlas.resource->GetGPUVirtualAddress());
  m_commands->SetComputeRootShaderResourceView(2,m_vertices_gpu.resource->GetGPUVirtualAddress());
  m_commands->SetComputeRootShaderResourceView(3,m_materials_gpu.resource->GetGPUVirtualAddress());
  m_commands->SetComputeRootDescriptorTable(4,gpu_descriptor(0));
  m_commands->SetComputeRootDescriptorTable(5,gpu_descriptor(kSrvCount));
  m_commands->SetComputeRootDescriptorTable(6,gpu_descriptor(kMaterialDescriptorBase));
  m_commands->SetComputeRootShaderResourceView(7,m_shader_instances.resource->GetGPUVirtualAddress());
}
bool Dx12Backend::render_frame() {
  if(m_rendered) return !m_failed;
  if(m_failed) return false;
  try {
    prepare_geometry();
    const auto now=std::chrono::steady_clock::now();
    const float delta_ms=std::chrono::duration<float,std::milli>(now-m_last_frame_time).count(); m_last_frame_time=now;
    const bool camera_changed=std::memcmp(m_vp.m,m_previous_vp.m,sizeof(m_vp.m))!=0;
    auto scene_hash=hash_bytes(m_materials.data(),m_materials.size()*sizeof(GpuMaterial),m_geometry_hash);
    scene_hash=hash_bytes(&m_lighting,sizeof(m_lighting),scene_hash);
    if(m_has_water) scene_hash=hash_bytes(&m_time,sizeof(m_time),scene_hash);
    if(camera_changed || scene_hash!=m_last_scene_hash || !m_settings.accumulate) m_accumulated=0;
    const bool camera_cut=m_frame==0 || length(m_camera-m_previous_camera)>2.f;
    FrameConstants c{};
    c.current_vp=m_vp; c.inverse_vp=m_inverse_vp; c.previous_vp=m_frame ? m_previous_vp : m_vp;
    c.camera_time=Vec4(m_camera,m_time);
    c.sun_direction_intensity=Vec4(normalize(m_lighting.sun_direction),m_lighting.sun_intensity);
    c.sun_color=Vec4(m_lighting.sun_color,0); c.ambient=Vec4(m_lighting.ambient,0);
    c.fog_color_start=Vec4(m_lighting.fog_color,m_lighting.fog_start);
    c.fog_end_exposure={m_lighting.fog_end,m_settings.exposure,std::tan(m_fov*.5f),float(m_settings.debug_view)};
    for(unsigned i=0;i<4;++i) {
      c.point_position_radius[i]=Vec4(m_lighting.point_lights[i].position,m_lighting.point_lights[i].radius);
      c.point_color_intensity[i]=Vec4(m_lighting.point_lights[i].color,m_lighting.point_lights[i].intensity);
    }
    c.dimensions[0]=m_render_width; c.dimensions[1]=m_render_height; c.dimensions[2]=m_width; c.dimensions[3]=m_height;
    c.sampling[0]=static_cast<unsigned>(m_frame); c.sampling[1]=m_settings.samples_per_pixel;
    c.sampling[2]=m_settings.max_bounces; c.sampling[3]=m_accumulated;
    const double scale_ratio=double(m_width)/double(m_render_width);
    const unsigned phases=unsigned(std::ceil(8.0*scale_ratio*scale_ratio));
    temporal_jitter(m_frame,phases,c.jitter_filter[0],c.jitter_filter[1]);
    c.jitter_filter[2]=m_settings.denoise ? 1.f : 0.f;
    c.jitter_filter[3]=m_settings.trace_mode==TraceMode::PathTraced ? 1.f : 0.f;
    c.options[0]=(m_reset || camera_cut) ? 1u : 0u;
    c.options[1]=unsigned(std::clamp(m_lighting.point_light_count,0,4));
    c.options[2]=(m_settings.accumulate && m_settings.upscaler==Upscaler::Native) ? 1u : 0u;
    c.options[3]=m_settings.upscaler==Upscaler::Native ? 0u : 1u;
    c.previous_jitter={m_previous_jitter.x,m_previous_jitter.y,m_settings.denoise ? 1.f : 0.f,0};
    c.previous_camera=Vec4(m_previous_camera,0);
    const unsigned current_history=unsigned(m_frame%2),previous_history=1-current_history;
    srv(m_history_color[previous_history],6,DXGI_FORMAT_R16G16B16A16_FLOAT);
    srv(m_history_geometry[previous_history],7,DXGI_FORMAT_R16G16B16A16_FLOAT);
    srv(m_history_color[current_history],8,DXGI_FORMAT_R16G16B16A16_FLOAT);
    uav(m_history_color[current_history],6,DXGI_FORMAT_R16G16B16A16_FLOAT);
    uav(m_history_geometry[current_history],7,DXGI_FORMAT_R16G16B16A16_FLOAT);
    const auto pool_size=m_vertices.size()*sizeof(GpuVertex);
    const bool replace_vertex_buffer=!m_vertices_gpu.resource || m_vertices_gpu.capacity<pool_size;
    const bool copy_vertices=replace_vertex_buffer || m_stream_dirty;
    if(replace_vertex_buffer) {
      ensure_buffer(m_vertices_gpu,pool_size,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
      m_vertex_state=D3D12_RESOURCE_STATE_COPY_DEST;
      upload(m_vertex_staging,m_vertices.data(),pool_size);
    } else if(m_stream_dirty) {
      unsigned char* mapped{}; D3D12_RANGE no_read{0,0};
      check(m_vertex_staging.resource->Map(0,&no_read,reinterpret_cast<void**>(&mapped)),"Map geometry updates");
      for(const auto& range:m_vertex_uploads)
        std::memcpy(mapped+std::size_t(range.first)*sizeof(GpuVertex),m_vertices.data()+range.first,std::size_t(range.second)*sizeof(GpuVertex));
      m_vertex_staging.resource->Unmap(0,nullptr);
    }
    upload(m_materials_gpu,m_materials.data(),m_materials.size()*sizeof(GpuMaterial));
    upload(m_shader_instances,m_gpu_instances.data(),m_gpu_instances.size()*sizeof(GpuInstance));
    upload(m_constants_gpu,&c,sizeof(c));
    if(!m_hud.empty()) upload(m_hud_gpu,m_hud.data(),m_hud.size()*sizeof(HudVertex));
    open_commands();
    m_commands->EndQuery(m_timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
    if(copy_vertices) {
      auto vertex_barrier=[&](D3D12_RESOURCE_STATES next) {
        if(m_vertex_state==next) return;
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource=m_vertices_gpu.resource.Get();
        barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore=m_vertex_state; barrier.Transition.StateAfter=next;
        m_commands->ResourceBarrier(1,&barrier); m_vertex_state=next;
      };
      vertex_barrier(D3D12_RESOURCE_STATE_COPY_DEST);
      if(replace_vertex_buffer) m_commands->CopyBufferRegion(m_vertices_gpu.resource.Get(),0,m_vertex_staging.resource.Get(),0,pool_size);
      else for(const auto& range:m_vertex_uploads) {
        const auto offset=UINT64(range.first)*sizeof(GpuVertex),size=UINT64(range.second)*sizeof(GpuVertex);
        m_commands->CopyBufferRegion(m_vertices_gpu.resource.Get(),offset,m_vertex_staging.resource.Get(),offset,size);
      }
      vertex_barrier(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    build_acceleration_structure();
    transition(m_radiance,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(m_direct,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(m_depth,D3D12_RESOURCE_STATE_UNORDERED_ACCESS); transition(m_motion,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(m_geometry,D3D12_RESOURCE_STATE_UNORDERED_ACCESS); transition(m_reactive,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    bind_root();
    auto sky_hash=hash_bytes(&c.sun_direction_intensity,sizeof(c.sun_direction_intensity));
    sky_hash=hash_bytes(&c.ambient,sizeof(c.ambient),sky_hash);
    if(m_sky_hash!=sky_hash) {
      transition(m_sky,D3D12_RESOURCE_STATE_UNORDERED_ACCESS); m_commands->SetPipelineState(m_sky_pso.Get());
      m_commands->Dispatch(64,32,1); transition(m_sky,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      m_sky_hash=sky_hash;
    }
    m_commands->SetPipelineState(m_trace_pso.Get());
    m_commands->Dispatch((m_render_width+7)/8,(m_render_height+7)/8,1);
    transition(m_radiance,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(m_direct,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(m_geometry,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    uav_barrier(m_motion.resource.Get()); uav_barrier(m_reactive.resource.Get());
    transition(m_history_color[previous_history],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(m_history_geometry[previous_history],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(m_history_color[current_history],D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(m_history_geometry[current_history],D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_commands->SetPipelineState(m_temporal_pso.Get()); m_commands->Dispatch((m_render_width+7)/8,(m_render_height+7)/8,1);
    transition(m_history_color[current_history],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(m_filtered,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_commands->SetPipelineState(m_filter_pso.Get()); m_commands->Dispatch((m_render_width+7)/8,(m_render_height+7)/8,1);
    if(m_settings.upscaler!=Upscaler::Native) {
      transition(m_filtered,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      transition(m_depth,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      transition(m_motion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      transition(m_reactive,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      transition(m_upscaled,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      UpscaleFrame f{}; f.commands=m_commands.Get(); f.color=m_filtered.resource.Get(); f.depth=m_depth.resource.Get();
      f.motion=m_motion.resource.Get(); f.reactive=m_reactive.resource.Get(); f.output=m_upscaled.resource.Get();
      f.jitter_x=c.jitter_filter[0]; f.jitter_y=c.jitter_filter[1]; f.reset=c.options[0]!=0;
      f.delta_ms=delta_ms; f.near_plane=m_near; f.far_plane=m_far; f.fov_y=m_fov;
      std::string error; if(!m_upscaler->dispatch(f,error)) throw std::runtime_error(error);
      transition(m_upscaled,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    transition(m_filtered,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(m_upscaled,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(m_radiance,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(m_geometry,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(m_depth,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(m_motion,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(m_direct,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_capture_index=m_swapchain->GetCurrentBackBufferIndex();
    auto& back=m_backbuffers[m_capture_index]; transition(back,D3D12_RESOURCE_STATE_RENDER_TARGET);
    ID3D12DescriptorHeap* heaps[]={m_heap.Get()}; m_commands->SetDescriptorHeaps(1,heaps);
    m_commands->SetGraphicsRootSignature(m_root.Get());
    m_commands->SetGraphicsRootConstantBufferView(0,m_constants_gpu.resource->GetGPUVirtualAddress());
    m_commands->SetGraphicsRootDescriptorTable(4,gpu_descriptor(0));
    m_commands->SetPipelineState(m_present_pso.Get());
    auto rtv=m_rtv_heap->GetCPUDescriptorHandleForHeapStart(); rtv.ptr+=SIZE_T(m_capture_index)*m_rtv_size;
    m_commands->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
    D3D12_VIEWPORT viewport{0,0,float(m_width),float(m_height),0,1};
    D3D12_RECT rect{0,0,LONG(m_width),LONG(m_height)};
    m_commands->RSSetViewports(1,&viewport); m_commands->RSSetScissorRects(1,&rect);
    m_commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); m_commands->DrawInstanced(3,1,0,0);
    if(!m_hud.empty()) {
      m_commands->SetPipelineState(m_hud_pso.Get());
      D3D12_VERTEX_BUFFER_VIEW vb{m_hud_gpu.resource->GetGPUVirtualAddress(),UINT(m_hud.size()*sizeof(HudVertex)),sizeof(HudVertex)};
      m_commands->IASetVertexBuffers(0,1,&vb); m_commands->DrawInstanced(UINT(m_hud.size()),1,0,0);
    }
    transition(back,D3D12_RESOURCE_STATE_PRESENT);
    m_commands->EndQuery(m_timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);
    m_commands->ResolveQueryData(m_timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,m_query_readback.resource.Get(),0);
    submit_and_wait();
    UINT64* ticks{}; D3D12_RANGE read{0,16};
    check(m_query_readback.resource->Map(0,&read,reinterpret_cast<void**>(&ticks)),"Read GPU timing");
    m_statistics.gpu_frame_ms=double(ticks[1]-ticks[0])*1000.0/double(m_timestamp_frequency);
    D3D12_RANGE no_write{0,0}; m_query_readback.resource->Unmap(0,&no_write);
    collect_validation();
    m_statistics.frame_index=++m_frame;
    m_statistics.accumulated_frames=c.options[2] ? ++m_accumulated : 0;
    m_previous_vp=m_vp; m_previous_camera=m_camera; m_last_scene_hash=scene_hash;
    m_previous_jitter={c.jitter_filter[0],c.jitter_filter[1]};
    m_reset=false; m_rendered=true;
    for(auto it=m_history.begin();it!=m_history.end();) {
      if(it->second.frame+2<m_frame) it=m_history.erase(it); else ++it;
    }
    return !m_failed;
  } catch(const std::exception& error) { fail(error.what()); return false; }
}
void Dx12Backend::collect_validation() {
  if(!m_info) return;
  const UINT64 count=m_info->GetNumStoredMessagesAllowedByRetrievalFilter();
  for(UINT64 i=0;i<count;++i) {
    SIZE_T size{}; m_info->GetMessage(i,nullptr,&size); std::vector<unsigned char> data(size);
    auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
    if(SUCCEEDED(m_info->GetMessage(i,message,&size)) && message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR) {
      ++m_statistics.validation_errors; Log::error(std::string("DX12 validation: ")+message->pDescription);
    }
  }
  m_info->ClearStoredMessages();
}
void Dx12Backend::fail(const std::string& message) {
  if(!m_failed) { Log::error("DX12: "+message); ++m_statistics.validation_errors; }
  m_failed=true; SDL_Event quit{}; quit.type=SDL_QUIT; SDL_PushEvent(&quit);
}
void Dx12Backend::end_frame() {
  if(!render_frame()) return;
  const HRESULT result=m_swapchain->Present(m_settings.vsync ? 1 : 0,0);
  if(FAILED(result)) fail("Swapchain presentation failed");
}
void Dx12Backend::resize(int width,int height) {
  if(width<=0 || height<=0 || (unsigned(width)==m_width && unsigned(height)==m_height)) return;
  try {
    wait_idle(); m_backbuffers={};
    check(m_swapchain->ResizeBuffers(kFrames,unsigned(width),unsigned(height),DXGI_FORMAT_R8G8B8A8_UNORM,0),"Resize swapchain");
    m_width=unsigned(width); m_height=unsigned(height); acquire_backbuffers();
    if(!configure(m_settings)) fail("Failed to rebuild reconstruction after resize");
  } catch(const std::exception& error) { fail(error.what()); }
}
bool Dx12Backend::read_rgb_framebuffer(std::vector<std::uint8_t>& rgb,int& width,int& height) {
  rgb.clear(); width=height=0;
  if(!render_frame()) return false;
  try {
    auto& back=m_backbuffers[m_capture_index]; const auto desc=back.resource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 total{};
    m_device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&total);
    ensure_buffer(m_capture,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
    open_commands(); transition(back,D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource=back.resource.Get(); source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION destination{}; destination.pResource=m_capture.resource.Get();
    destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint=footprint;
    m_commands->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
    transition(back,D3D12_RESOURCE_STATE_PRESENT); submit_and_wait();
    unsigned char* pixels{}; D3D12_RANGE read{0,SIZE_T(total)};
    check(m_capture.resource->Map(0,&read,reinterpret_cast<void**>(&pixels)),"Map capture");
    rgb.resize(std::size_t(m_width)*m_height*3);
    for(unsigned y=0;y<m_height;++y) for(unsigned x=0;x<m_width;++x) {
      const auto* src=pixels+footprint.Offset+y*footprint.Footprint.RowPitch+x*4;
      auto* dst=rgb.data()+(std::size_t(y)*m_width+x)*3;
      dst[0]=src[0]; dst[1]=src[1]; dst[2]=src[2];
    }
    D3D12_RANGE no_write{0,0}; m_capture.resource->Unmap(0,&no_write);
    width=int(m_width); height=int(m_height); return true;
  } catch(const std::exception& error) { fail(error.what()); return false; }
}
}  // namespace
std::unique_ptr<IRenderBackend> create_dx12_backend() { return std::make_unique<Dx12Backend>(); }
}  // namespace fury
