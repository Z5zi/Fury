#include "coastal_scene.hpp"
#include "surface_gallery.hpp"
#include "fury/log.hpp"
#include "fury/window.hpp"
#include "fury/gltf.hpp"
#include "fury/bitmap_font.hpp"
#include "fury/texture.hpp"

#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

using namespace fury;
namespace {
float asset_fit_radius(const GltfAsset& asset,unsigned width,unsigned height) {
  // Fit a bounding sphere to the narrower FOV, with margin for thin/tall assets.
  const float half_vertical=radians(57)*.5f;
  const float half_horizontal=std::atan(std::tan(half_vertical)*float(width)/height);
  const float sphere_radius=length(asset.bounds_max-asset.bounds_min)*.5f;
  return std::max(.01f,sphere_radius*1.08f/(std::sin(std::min(half_vertical,half_horizontal))*std::sqrt(1.61f)));
}
double percentile(std::vector<double> samples,double p) {
  if(samples.empty()) return 0;
  std::sort(samples.begin(),samples.end());
  return samples[std::size_t(std::ceil(p*(samples.size()-1)))];
}
std::string json_string(const std::string& text) {
  std::string out="\"";
  for(char c:text) { if(c=='"' || c=='\\') out+='\\'; if(c=='\n') out+="\\n"; else if(c!='\r') out+=c; }
  return out+'"';
}
void ensure_parent(const std::string& path) {
  auto parent=std::filesystem::path(path).parent_path(); if(!parent.empty()) std::filesystem::create_directories(parent);
}
void set_environment(const char* key,const char* value) {
#ifdef _WIN32
  if(_putenv_s(key,value)!=0) throw std::runtime_error("Setting process environment failed");
#else
  if(setenv(key,value,1)!=0) throw std::runtime_error("Setting process environment failed");
#endif
}
void draw_line(Renderer& renderer,float x,float y,std::string text) {
  for(char& c:text) c=char(std::toupper(static_cast<unsigned char>(c)));
  for(std::size_t i=0;i<text.size();i+=12) {
    const auto part=text.substr(i,12);
    draw_bitmap_text(renderer,x+float(i)*9.f,y,part.c_str(),{230,239,247,255},1.5f);
  }
}
}
int main(int argc,char** argv) {
  try {
    RenderSettings settings;
    unsigned width=1280,height=720,frames=0,warmup=16;
    float asset_yaw=0.f;
    bool surface_gallery=false;
    bool hidden=false,animate=false,moving_camera=false,resize_test=false,camera_cut=false,gltf_only=false,deform=false,cycle_upscalers=false,alpha_test=false;
    #ifdef _WIN32
    std::string backend="dx12";
#else
    std::string backend="cpu-ray";
#endif
    std::string capture,report,gltf_path,obj_path,pier_path,tree_path;
    for(int i=1;i<argc;++i) {
      const std::string arg=argv[i];
      auto value=[&]() -> std::string { if(i+1>=argc) throw std::runtime_error("Missing value for "+arg); return argv[++i]; };
      if(arg=="--width") width=unsigned(std::stoul(value()));
      else if(arg=="--height") height=unsigned(std::stoul(value()));
      else if(arg=="--frames") frames=unsigned(std::stoul(value()));
      else if(arg=="--warmup") warmup=unsigned(std::stoul(value()));
      else if(arg=="--spp") settings.samples_per_pixel=unsigned(std::stoul(value()));
      else if(arg=="--bounces") settings.max_bounces=unsigned(std::stoul(value()));
      else if(arg=="--exposure") settings.exposure=std::stof(value());
      else if(arg=="--asset-yaw") asset_yaw=radians(std::stof(value()));
      else if(arg=="--upscaler") { if(!parse_upscaler(value(),settings.upscaler)) throw std::runtime_error("Invalid upscaler"); }
      else if(arg=="--quality") { if(!parse_upscale_quality(value(),settings.quality)) throw std::runtime_error("Invalid quality"); }
      else if(arg=="--debug-view") {
        const auto view=value(); const std::string names[]={"beauty","depth","normals","motion","direct","indirect"};
        auto found=std::find(std::begin(names),std::end(names),view);
        if(found==std::end(names)) throw std::runtime_error("Invalid debug view");
        settings.debug_view=static_cast<RenderDebugView>(found-std::begin(names));
      }
      else if(arg=="--mode") { auto mode=value(); if(mode=="ray")settings.trace_mode=TraceMode::RayTraced; else if(mode=="path")settings.trace_mode=TraceMode::PathTraced; else throw std::runtime_error("Invalid trace mode"); }
      else if(arg=="--backend") backend=value();
      else if(arg=="--gltf") gltf_path=value();
      else if(arg=="--obj") obj_path=value();
      else if(arg=="--pier") pier_path=value();
      else if(arg=="--trees") tree_path=value();
      else if(arg=="--gltf-only" || arg=="--asset-only") gltf_only=true;
      else if(arg=="--capture") capture=value();
      else if(arg=="--report") report=value();
      else if(arg=="--hidden") hidden=true;
      else if(arg=="--animate") animate=true;
      else if(arg=="--deform-test") deform=true;
      else if(arg=="--upscaler-cycle-test") cycle_upscalers=true;
      else if(arg=="--alpha-test") alpha_test=true;
      else if(arg=="--surface-gallery") surface_gallery=true;
      else if(arg=="--camera-motion") moving_camera=true;
      else if(arg=="--resize-test") resize_test=true;
      else if(arg=="--camera-cut") camera_cut=true;
      else if(arg=="--no-denoise") settings.denoise=false;
      else if(arg=="--no-accumulate") settings.accumulate=false;
      else if(arg=="--no-vsync") settings.vsync=false;
      else if(arg=="--debug") settings.debug_layer=true;
      else if(arg=="--help") {
        std::cout<<"Fury Renderlab: --backend dx12|opengl|software|cpu-ray --mode ray|path --upscaler native|fsr|xess\n"
          "--quality native-aa|quality|balanced|performance|ultra-performance --width N --height N\n"
          "--frames N (0=interactive) --spp 1..64 --bounces 1..16 --exposure N --capture image.ppm\n"
          "--report metrics.json --warmup N --hidden --animate --camera-motion --camera-cut\n"
          "--resize-test --no-denoise --no-accumulate --no-vsync --debug --gltf asset.gltf --gltf-only\n"
          "--surface-gallery (physical-scale six-material comparison fixture)\n"
          "--obj asset.obj --asset-only --asset-yaw degrees (neutral asset preview; OBJ materials not imported)\n"
          "--pier modular_wooden_pier_2k.gltf --trees island_tree_01_2k.gltf (CC0 detail assets) --deform-test\n"
          "--debug-view beauty|depth|normals|motion|direct|indirect\n"
          "Interactive: right mouse look; WASD/QE fly; Shift fast; F1 lighting; F2 upscaler; F3 quality; F4 debug; Space water; F11 fullscreen\n"; return 0;
      } else throw std::runtime_error("Unknown argument: "+arg);
    }
    if(!std::isfinite(asset_yaw)) throw std::runtime_error("Invalid asset yaw");
    if(width<64 || height<64 || width>7680 || height>4320) throw std::runtime_error("Dimensions outside 64..7680 by 64..4320");
    if(backend!="dx12" && backend!="opengl" && backend!="software" && backend!="cpu-ray") throw std::runtime_error("Invalid backend");
    if(surface_gallery && (!gltf_path.empty() || !obj_path.empty() || alpha_test)) throw std::runtime_error("Surface gallery cannot be combined with imported/alpha fixtures");
    if(!gltf_path.empty() && !obj_path.empty()) throw std::runtime_error("Choose --gltf or --obj");
    set_environment("FURY_RENDERER",backend.c_str());
    set_environment("FURY_UPSCALER","native");
    set_environment("FURY_TRACE_MODE","path");
    set_environment("FURY_DX12_DEBUG",settings.debug_layer ? "1" : "0");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_TIMER)!=0) throw std::runtime_error(SDL_GetError());
    int result=0;
    {
      Window window; WindowDesc desc; desc.title="Fury — Coastal Rendering Lab";
      desc.width=int(width); desc.height=int(height); desc.opengl=backend=="opengl"; desc.msaa_samples=0;
      if(!window.create(desc)) throw std::runtime_error("Window creation failed");
      SDL_SetWindowResizable(window.handle(),SDL_TRUE);
      if(hidden) SDL_HideWindow(window.handle());
      Renderer renderer;
      if(!renderer.create(window.handle(),int(width),int(height),desc.opengl)) throw std::runtime_error("Renderer creation failed");
      if((backend=="dx12" || backend=="cpu-ray") && !renderer.configure(settings)) throw std::runtime_error("Renderer configuration failed");
      if(backend!="dx12" && settings.upscaler!=Upscaler::Native) throw std::runtime_error("FSR/XeSS require the DX12 backend");
      CoastalScene scene;
      if(surface_gallery) create_surface_gallery(scene);
      else if(!gltf_only && !alpha_test) scene.create(pier_path,tree_path);
      Mesh alpha_quad; Material alpha_front,alpha_back;
      if(alpha_test) {
        alpha_quad.vertices={{{-1,-1,0},{0,0,1},{1,1,1},{0,1}},{{1,-1,0},{0,0,1},{1,1,1},{1,1}},
                             {{1,1,0},{0,0,1},{1,1,1},{1,0}},{{-1,1,0},{0,0,1},{1,1,1},{0,0}}};
        alpha_quad.indices={0,1,2,0,2,3};
        auto maps=std::make_shared<MaterialTextures>();
        maps->base_color={2,2,{255,255,255,255,255,255,255,0,255,255,255,0,255,255,255,255}};
        maps->source="alpha-mask regression fixture";
        alpha_front.albedo={0,0,0}; alpha_front.emissive=3; alpha_front.emissive_color={0,0,1}; alpha_front.textures=maps;
        alpha_back.albedo={0,0,0}; alpha_back.emissive=3; alpha_back.emissive_color={0,1,0};
        alpha_back.textures=std::make_shared<MaterialTextures>();
      }
      GltfAsset imported;
      if(!gltf_path.empty()) {
        std::string error;
        if(!load_gltf(gltf_path,imported,error)) throw std::runtime_error("glTF import: "+error);
        Log::info("Imported glTF: "+std::to_string(imported.primitives.size())+" primitive instances");
        Log::info("glTF bounds: "+std::to_string(imported.bounds_min.x)+","+std::to_string(imported.bounds_min.y)+","+
                  std::to_string(imported.bounds_min.z)+" to "+std::to_string(imported.bounds_max.x)+","+
                  std::to_string(imported.bounds_max.y)+","+std::to_string(imported.bounds_max.z));
      } else if(!obj_path.empty()) {
        auto mesh=std::make_shared<Mesh>();
        if(!load_obj(obj_path,*mesh)) throw std::runtime_error("OBJ import failed: "+obj_path);
        imported.bounds_min=imported.bounds_max=mesh->vertices.front().position;
        for(const auto& vertex:mesh->vertices) {
          const auto p=vertex.position;
          imported.bounds_min={std::min(imported.bounds_min.x,p.x),std::min(imported.bounds_min.y,p.y),std::min(imported.bounds_min.z,p.z)};
          imported.bounds_max={std::max(imported.bounds_max.x,p.x),std::max(imported.bounds_max.y,p.y),std::max(imported.bounds_max.z,p.z)};
        }
        GltfPrimitive primitive; primitive.mesh=std::move(mesh); primitive.transform=Mat4::identity();
        primitive.material.albedo={.6f,.62f,.65f}; primitive.material.roughness=.65f;
        imported.primitives.push_back(std::move(primitive));
      } else if(gltf_only) throw std::runtime_error("--asset-only requires --gltf or --obj");
      Lighting light;
      light.sun_direction=normalize({-.7f,-.14f,-.48f}); light.sun_color={1,.73f,.43f}; light.sun_intensity=4.8f;
      light.ambient={.18f,.23f,.30f}; light.fog_color={.64f,.52f,.37f}; light.fog_start=15; light.fog_end=140;
      light.point_light_count=2;
      light.point_lights[0]={{-.1f,3.55f,-4.5f},{1,.53f,.18f},24,8};
      light.point_lights[1]={{6.1f,3.55f,-4.5f},{1,.53f,.18f},24,8};
      if(gltf_only || surface_gallery) {
        light.sun_direction=normalize({-.6f,-1.f,-.8f}); light.sun_color={1,1,1}; light.sun_intensity=2.5f;
        light.ambient={.18f,.18f,.18f}; light.point_light_count=0;
        light.fog_start=1e6f; light.fog_end=2e6f;
      }
      renderer.set_lighting(light);
      unsigned completed=0;
      std::vector<double> gpu_times,cpu_times,wall_times;
      bool quit=false,captured=false;
      const unsigned change_frame=frames ? frames/2 : 120;
      const auto start=std::chrono::steady_clock::now();
      auto last_input_time=start;
      Vec3 free_eye{10,4.7f,15},free_target{0,1,-1};
      if(surface_gallery) { free_eye={6.6f,5.6f,9.2f}; free_target={0,.7f,-.8f}; }
      if(gltf_only) {
        free_target=(imported.bounds_min+imported.bounds_max)*.5f;
        const float radius=asset_fit_radius(imported,width,height);
        free_eye=free_target+Vec3{radius*(.6f*std::cos(asset_yaw)+std::sin(asset_yaw)),radius*.5f,
                                 radius*(std::cos(asset_yaw)-.6f*std::sin(asset_yaw))};
      }
      const Vec3 initial_direction=normalize(free_target-free_eye);
      float yaw=std::atan2(initial_direction.z,initial_direction.x),pitch=std::asin(initial_direction.y);
      bool look=false,fullscreen=false;
      while(!quit && (!frames || completed<frames)) {
        const auto input_time=std::chrono::steady_clock::now();
        const float input_dt=std::min(.1f,std::chrono::duration<float>(input_time-last_input_time).count());
        last_input_time=input_time;
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
          if(event.type==SDL_QUIT || (event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_ESCAPE)) quit=true;
          if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED) {
            int w=event.window.data1,h=event.window.data2;
            if(w>0 && h>0) { width=unsigned(w); height=unsigned(h); renderer.resize(w,h); }
          }
          if(!frames) {
            if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_RIGHT) { look=true; SDL_SetRelativeMouseMode(SDL_TRUE); }
            if(event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_RIGHT) { look=false; SDL_SetRelativeMouseMode(SDL_FALSE); }
            if(event.type==SDL_MOUSEMOTION && look) {
              yaw+=float(event.motion.xrel)*.0025f; pitch=std::clamp(pitch-float(event.motion.yrel)*.0025f,-1.5f,1.5f);
            }
            if(event.type==SDL_KEYDOWN && !event.key.repeat) {
              auto next=settings; bool changed=true;
              switch(event.key.keysym.sym) {
                case SDLK_F1: next.trace_mode=settings.trace_mode==TraceMode::PathTraced ? TraceMode::RayTraced : TraceMode::PathTraced; break;
                case SDLK_F2: if(backend!="dx12") { changed=false; break; } next.upscaler=static_cast<Upscaler>((int(settings.upscaler)+1)%3); break;
                case SDLK_F3: if(backend!="dx12") { changed=false; break; } next.quality=static_cast<UpscaleQuality>((int(settings.quality)+1)%5); break;
                case SDLK_F4:
                  next.debug_view=next_debug_view(settings.debug_view,backend!="cpu-ray");
                  break;
                case SDLK_SPACE: animate=!animate; changed=false; break;
                case SDLK_F11:
                  fullscreen=!fullscreen; SDL_SetWindowFullscreen(window.handle(),fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                  changed=false; break;
                default: changed=false; break;
              }
              if(changed && renderer.configure(next)) settings=next;
            }
          }
        }
        if(quit) break;
        if(cycle_upscalers && frames>=3 && completed>0 && completed%(frames/3)==0) {
          auto next=settings; next.upscaler=static_cast<Upscaler>((int(settings.upscaler)+1)%3);
          if(!renderer.configure(next)) throw std::runtime_error("Runtime upscaler switch failed");
          settings=next;
        }
        if(resize_test && completed==change_frame) {
          width=(std::max)(64u,width*3/4); height=(std::max)(64u,height*3/4);
          SDL_SetWindowSize(window.handle(),int(width),int(height)); renderer.resize(int(width),int(height));
        }
        float time=animate ? (frames ? float(completed)/60 : std::chrono::duration<float>(input_time-start).count()) : 0;
        if(deform) scene.deform(float(completed)/60);
        float angle=asset_yaw+(moving_camera ? float(completed)*.002f : 0);
        if(camera_cut && completed>=change_frame) angle+=.6f;
        if(camera_cut && completed==change_frame) renderer.reset_history();
        Vec3 eye{10*std::cos(angle)+15*std::sin(angle),4.7f,15*std::cos(angle)-10*std::sin(angle)};
        Vec3 target{0,1,-1};
        if(surface_gallery) { eye={6.6f,5.6f,9.2f}; target={0,.7f,-.8f}; }
        if(gltf_only) {
          target=(imported.bounds_min+imported.bounds_max)*.5f;
          float radius=asset_fit_radius(imported,width,height);
          eye=target+Vec3{radius*(.6f*std::cos(angle)+std::sin(angle)),radius*.5f,
                          radius*(std::cos(angle)-.6f*std::sin(angle))};
        }
        if(!frames && !moving_camera) {
          const Vec3 direction{std::cos(pitch)*std::cos(yaw),std::sin(pitch),std::cos(pitch)*std::sin(yaw)};
          const Vec3 right=normalize(cross(direction,{0,1,0}));
          const auto* keys=SDL_GetKeyboardState(nullptr);
          Vec3 movement=direction*float(int(keys[SDL_SCANCODE_W])-int(keys[SDL_SCANCODE_S]))+
                        right*float(int(keys[SDL_SCANCODE_D])-int(keys[SDL_SCANCODE_A]))+
                        Vec3{0,float(int(keys[SDL_SCANCODE_E])-int(keys[SDL_SCANCODE_Q])),0};
          if(length(movement)>0) free_eye+=normalize(movement)*input_dt*(keys[SDL_SCANCODE_LSHIFT] ? 18.f : 5.f);
          eye=free_eye; target=eye+direction;
        }
        if(alpha_test) { eye={0,0,4}; target={0,0,0}; }
        const auto before=std::chrono::steady_clock::now();
        renderer.begin_frame({65,89,118,255}); renderer.set_camera_position(eye); renderer.set_time(time);
        const float fitted=gltf_only ? asset_fit_radius(imported,width,height):100.f;
        renderer.set_view_proj(look_at(eye,target,{0,1,0}),perspective(radians(57),float(width)/height,
          std::clamp(fitted*.001f,1e-5f,.1f),std::max(500.f,fitted*5.f)));
        scene.draw(renderer,time);
        if(alpha_test) {
          alpha_front.alpha_cutoff=completed>=change_frame ? .5f : -1.f;
          renderer.draw_mesh(alpha_quad,translate({0,0,-.5f})*scale({2,2,1}),alpha_back,10001);
          renderer.draw_mesh(alpha_quad,Mat4::identity(),alpha_front,10002);
        }
        for(std::size_t i=0;i<imported.primitives.size();++i) {
          const auto& primitive=imported.primitives[i];
          renderer.draw_mesh(*primitive.mesh,primitive.transform,primitive.material,1000000+i);
        }
        if(!frames) {
          renderer.draw_hud_rect(10,10,760,70,{9,14,22,215});
          draw_line(renderer,20,20,std::string((backend=="software" || backend=="opengl") ? "RASTERIZED" :
                    settings.trace_mode==TraceMode::PathTraced ? "PATH TRACED" : "RAY TRACED")+
                    "  "+(backend=="cpu-ray" ? std::string("CPU  SPP ")+std::to_string(settings.samples_per_pixel) :
                    std::string(upscaler_name(settings.upscaler))+"  "+quality_name(settings.quality)));
          draw_line(renderer,20,40,backend=="dx12" ? "F1 LIGHTING   F2 UPSCALER   F3 QUALITY   F4 DEBUG   F11 FULLSCREEN" :
                    backend=="cpu-ray" ? "F1 LIGHTING   F4 DEBUG   F11 FULLSCREEN" : "F11 FULLSCREEN");
          draw_line(renderer,20,60,"RIGHT MOUSE LOOK   WASD MOVE   Q/E UP/DOWN   SHIFT FAST   SPACE WATER");
        }
        if(frames && completed+1==frames && !capture.empty()) {
          std::vector<std::uint8_t> pixels; int w{},h{};
          if(!renderer.read_rgb_framebuffer(pixels,w,h)) throw std::runtime_error("GPU capture failed");
          ensure_parent(capture); std::ofstream image(capture,std::ios::binary);
          image<<"P6\n"<<w<<" "<<h<<"\n255\n";
          image.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()));
          if(!image) throw std::runtime_error("Writing capture failed");
          captured=true;
        }
        renderer.end_frame();
        const auto stats=renderer.statistics();
        if(!frames && completed%30==0) {
          std::ostringstream title;
          const double ms=stats.cpu_frame_ms>0 ? stats.cpu_frame_ms : stats.gpu_frame_ms>0 ? stats.gpu_frame_ms :
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count();
          title<<"Fury Coastal Lab | "<<renderer.backend_name()<<" | "<<ms<<" ms";
          SDL_SetWindowTitle(window.handle(),title.str().c_str());
        }
        if(stats.validation_errors) { result=2; break; }
        const auto after=std::chrono::steady_clock::now();
        if(completed>=warmup && (!frames || completed+1<frames)) {
          gpu_times.push_back(stats.gpu_frame_ms);
          cpu_times.push_back(stats.cpu_frame_ms);
          wall_times.push_back(std::chrono::duration<double,std::milli>(after-before).count());
        }
        ++completed;
      }
      const auto stats=renderer.statistics();
      if(frames && completed!=frames) result=2;
      if(!capture.empty() && !captured) result=2;
      std::ostringstream json;
      json<<"{\n  \"backend\": "<<json_string(renderer.backend_name())
          <<",\n  \"source_sha256\": "<<json_string(build_source_fingerprint())
          <<",\n  \"adapter\": "<<json_string(stats.adapter)
          <<",\n  \"hardware_ray_tracing\": "<<(stats.hardware_ray_tracing ? "true":"false")
          <<",\n  \"debug_layer_active\": "<<(stats.debug_layer_active ? "true":"false")
          <<",\n  \"upscaler\": "<<json_string(stats.upscaler)
          <<",\n  \"quality\": "<<json_string(quality_name(settings.quality))
          <<",\n  \"trace_mode\": \""<<((backend=="dx12" || backend=="cpu-ray") ? (settings.trace_mode==TraceMode::PathTraced ? "path":"ray") : "raster")<<"\""
          <<",\n  \"render_width\": "<<stats.render_width<<", \"render_height\": "<<stats.render_height
          <<",\n  \"output_width\": "<<width<<", \"output_height\": "<<height
          <<",\n  \"frames\": "<<completed<<", \"measured_frames\": "<<gpu_times.size()
          <<",\n  \"spp\": "<<settings.samples_per_pixel<<", \"bounces\": "<<settings.max_bounces
          <<",\n  \"triangles\": "<<stats.triangle_count<<", \"accumulated_frames\": "<<stats.accumulated_frames
          <<",\n  \"unique_triangles\": "<<stats.unique_triangle_count<<", \"instances\": "<<stats.instance_count
          <<",\n  \"blas_builds\": "<<stats.blas_builds<<", \"tlas_builds\": "<<stats.tlas_builds
          <<",\n  \"gpu_ms\": {\"median\": "<<percentile(gpu_times,.5)<<", \"p95\": "<<percentile(gpu_times,.95)<<", \"p99\": "<<percentile(gpu_times,.99)<<"}"
          <<",\n  \"wall_ms\": {\"median\": "<<percentile(wall_times,.5)<<", \"p95\": "<<percentile(wall_times,.95)<<", \"p99\": "<<percentile(wall_times,.99)<<"}"
          <<",\n  \"software_ray_tracing\": "<<(stats.software_ray_tracing ? "true":"false")
          <<",\n  \"cpu_threads\": "<<stats.cpu_threads<<", \"rays_traced\": "<<stats.rays_traced
          <<",\n  \"cpu_ms\": {\"median\": "<<percentile(cpu_times,.5)<<", \"p95\": "<<percentile(cpu_times,.95)<<"}"
          <<",\n  \"elapsed_seconds\": "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()
          <<",\n  \"validation_errors\": "<<stats.validation_errors<<", \"exit_code\": "<<result<<"\n}\n";
      std::cout<<json.str();
      if(!report.empty()) { ensure_parent(report); std::ofstream file(report); file<<json.str(); if(!file)throw std::runtime_error("Writing report failed"); }
    }
    SDL_Quit(); return result;
  } catch(const std::exception& error) {
    std::cerr<<"Fury Renderlab: "<<error.what()<<"\n"; SDL_Quit(); return 1;
  }
}
