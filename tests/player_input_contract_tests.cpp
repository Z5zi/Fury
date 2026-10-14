#include <fury/application.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace fury;
int main(){
  try{
    AppConfig config;
    config.prefer_opengl=false;
    config.preferred_backend=RenderBackendKind::Software;
    config.window.width=64;
    config.window.height=64;
    config.window.resizable=false;
    config.max_frames=6;
    config.fixed_timestep=1.f/30;
    config.capture_mouse=false;
    config.enable_collision=false;
    config.log_fps=false;
    config.show_hud=false;
    Application app(config);
    app.camera().position={
      0,1.7f,12
    };
    app.camera().fly_mode=false;
    unsigned updates=0;
    Vec3 first{};
    bool consumed_f=false;
    app.on_diagnostic_input=[](InputState&i,unsigned frame){
      i={};
      i.key_v=frame==0||frame==4;
      i.key_w=frame==1||frame==2;
      i.key_f=frame==2||frame==3;
    };
    app.on_pre_update=[&](float,const InputState&i){
      if(updates==2&&i.key_f){
        consumed_f=true;
        return true;
      }return false;
    };
    app.on_update=[&](float,const InputState&i){
      auto need=[](bool x,const char*m){
        if(!x)throw std::runtime_error(m);
      };
      need(app.last_input().key_v==i.key_v&&app.last_input().key_w==i.key_w,"Diagnostic input precedes stored and callback input");
      if(updates==0){
        need(app.camera().third_person,"V toggles before gameplay callback");
        first=app.camera().position;
      }
      if(updates==1)need(length(app.camera().position-first)>.01f,"Production Camera::update moves before gameplay callback");
      if(updates==2)need(consumed_f&&!app.camera().fly_mode,"Existing pre-update consumes F before default toggle");
      if(updates==3)need(app.camera().fly_mode,"Unconsumed F uses production toggle");
      if(updates==4)need(!app.camera().third_person,"Repeated V uses existing first-person toggle");
      ++updates;
    };
    const int result=app.run();
    if(result||updates!=6)throw std::runtime_error("Bounded production frame chain did not finish");
    std::cout<<"Diagnostic input preserves the production pre-update/F/V/camera/gameplay frame order\n";
    return 0;
  }catch(const std::exception&e){
    std::cerr<<e.what()<<'\n';
    return 1;
  }
}
