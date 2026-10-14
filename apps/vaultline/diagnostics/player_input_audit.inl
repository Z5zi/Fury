// Opt-in bounded gameplay diagnostic: InputState enters BEFORE the complete
// Application camera/collision/update/render chain. Never positions an actor,
// never teleports the player, and never hides scenery or turns off collision.
// It skips the initial splash/cinematic only so the normal on-foot input starts
// immediately. All movement/turn/toggle behavior uses production functions.
if (!player_input_audit_path.empty()) {
  try {
    if (!npc_capture_id.empty() || photo_launch || smoke_mode || heist_capture_mode ||
    render_frames != 300 || !capture_sequence.empty() || !npc_presentation.enabled())
    throw std::runtime_error("Player input audit requires --frames 300 and ordinary detailed play; excludes photo/NPC/smoke/sequence modes");
    const auto directory=std::filesystem::path(player_input_audit_path).parent_path();
    if (directory.empty()) throw std::runtime_error("Player audit needs an explicit output directory");
    std::filesystem::create_directories(directory);
    auto stream=std::make_shared<std::ofstream>(player_input_audit_path);
    if(!*stream)throw std::runtime_error("Cannot write player audit");
    *stream<<std::setprecision(17)<<"{\"kind\":\"production-player-input-camera-collision-gameplay-world-render\",\"actor\":\"PlayerBody\",\"height\":1.75,\"source_fingerprint\":"<<std::quoted(fury::build_source_fingerprint())
    <<",\"diagnostic_input\":true,\"physical_sdl_keyboard_test\":false,\"actors_relocated\":false,\"player_teleported\":false,\"scenery_hidden\":false,\"collision_enabled\":"<<(app.config().enable_collision?"true":"false")<<",\"frame_order\":\"poll, diagnostic InputState, pre-update, F/V, Camera::update, collision, gameplay, world draw\",\"frames\":[";
    splash_remaining=0;
    cutscene_pending=false;
    intro_cutscene.skip();
    app.input().set_cinematic(false);
    app.config().fixed_timestep=1.f/30.f;
    auto frame=std::make_shared<unsigned>(0);
    auto failed=std::make_shared<bool>(false);
    auto raw_update=app.on_update;
    auto raw_hud=app.on_hud;
    auto previous=std::make_shared<Vec3>(app.camera().position);
    auto initial_near=app.scene().find_by_name("PlayerBody")->mesh;
    app.on_diagnostic_input=[&,frame](fury::InputState &input,unsigned n){
      *frame=n;
      input={};
      // Ordinary downward look keeps the player's footwear in the follow view.
      if(n>=2&&n<18)input.mouse_dy=9.f;
      if(n==1||n==180||n==195||n==270||n==280)input.key_v=true;
      // Face south with production smoothed look, without moving during turn.
      if(n>=20&&n<40)input.mouse_dx=3.14159265359f/(20.f*app.camera().mouse_sensitivity);
      if(n>=45&&n<60)input.move_forward=.25f;
      if(n>=60&&n<75)input.key_w=true;
      if(n>=85&&n<100){
        input.key_w=true;
        input.key_shift=true;
      }
      if(n>=115&&n<135)input.key_ctrl=true;
      if(n>=135&&n<155){
        input.key_ctrl=true;
        input.move_forward=.25f;
      }
      // V hide/show while stopped, then F fly hide/grounded show.
      if(n>=175&&n<178)input.key_space=true;
      // Grounded Space has no jump in the production controller.
      if(n==210||n==225)input.key_f=true;
      if(n>=240&&n<260)input.mouse_dx=1.57079632679f/(20.f*app.camera().mouse_sensitivity);
    };
    app.on_update=[&,frame,stream,failed,previous,initial_near,raw_update](float dt,const fury::InputState &input){
      raw_update(dt,input);
      const auto &camera=app.camera();
      const auto *body=app.scene().find_by_name("PlayerBody");
      const bool expected=camera.third_person&&!camera.fly_mode&&!in_vehicle;
      const bool stable=body&&body->mesh==initial_near;
      const bool height=body&&(!expected||std::fabs(body->transform.position.y-.875f)<1e-6f);
      const bool attached=body&&(!expected||(std::fabs(body->transform.position.x-camera.position.x)<1e-6f&&std::fabs(body->transform.position.z-camera.position.z)<1e-6f));
      if(!body||body->visible!=expected||!stable||!height||!attached||!app.config().enable_collision)*failed=true;
      const auto vec=[&](Vec3 p){
        *stream<<'['<<p.x<<','<<p.y<<','<<p.z<<']';
      };
      if(*frame)*stream<<',';
      *stream<<"{\"frame\":"<<*frame<<",\"input\":{\"w\":"<<input.key_w<<",\"space\":"<<input.key_space<<",\"shift\":"<<input.key_shift<<",\"ctrl\":"<<input.key_ctrl<<",\"v\":"<<input.key_v<<",\"f\":"<<input.key_f<<",\"analog_forward\":"<<input.move_forward<<",\"mouse_dx\":"<<input.mouse_dx<<",\"mouse_dy\":"<<input.mouse_dy<<"},\"player\":";
      vec(camera.position);
      *stream<<",\"previous\":";
      vec(*previous);
      *previous=camera.position;
      *stream<<",\"velocity\":";
      vec(camera.velocity);
      *stream<<",\"yaw\":"<<camera.yaw<<",\"pitch\":"<<camera.pitch<<",\"third_person\":"<<camera.third_person<<",\"fly\":"<<camera.fly_mode<<",\"crouching\":"<<camera.crouching<<",\"in_vehicle\":"<<in_vehicle<<",\"body_visible\":"<<(body&&body->visible)<<",\"body_position\":";
      vec(body?body->transform.position:Vec3{});
      fury::CharacterAnimationState live_animation{};
      const bool has_animation=npc_presentation.animation_state_for_review("PlayerBody",live_animation);
      const auto &a=live_animation.sample;
      *stream<<",\"animation\":{\"available\":"<<has_animation<<",\"phase_cycles\":"<<a.phase_cycles<<",\"idle_time\":"<<a.idle_time<<",\"speed\":"<<a.speed<<",\"stride_length\":"<<a.stride_length<<",\"move_weight\":"<<a.move_weight<<",\"turn_rate\":"<<a.turn_rate<<",\"crouch_weight\":"<<a.crouch_weight<<",\"attention_weight\":"<<a.attention_weight<<",\"attention_yaw\":"<<a.attention_yaw<<",\"attention_pitch\":"<<a.attention_pitch<<",\"talk_weight\":"<<a.talk_weight<<",\"seed\":"<<a.seed<<",\"previous_yaw\":"<<live_animation.previous_yaw<<",\"initialized\":"<<live_animation.initialized<<",\"gait_started\":"<<live_animation.gait_started<<",\"support_fraction\":"<<live_animation.support_fraction<<",\"animation_profile\":\""<<fury::character_animation_profile_name(live_animation.profile)<<"\",\"recovery_root_z\":"<<live_animation.recovery_root_z<<",\"state_version\":4,\"animation_profile_id\":"<<unsigned(live_animation.profile)<<",\"state_extension_v4\":\"";
      fury::diagnostics::write_character_animation_extension(*stream,live_animation);
      *stream<<"\"}";
      *stream<<",\"body_yaw\":"<<(body?body->transform.rotation_euler.y:0)<<",\"standing_rig_height_preserved\":"<<height<<",\"mesh_storage_stable\":"<<stable<<",\"travel_distance\":"<<player_travel_distance<<",\"speed\":"<<std::hypot(camera.velocity.x,camera.velocity.z)<<",\"phase\":"<<int(heist.phase())<<"}";
    };
    app.config().show_hud=true;
    app.on_hud=[&,frame,stream,failed,raw_hud,directory](){
      if(raw_hud)raw_hud();
      const std::array<unsigned,25> shots{
        {
          0,1,18,39,44,52,65,74,90,99,110,124,140,154,170,180,194,195,209,210,224,225,259,270,299
        }
      };
      const char *sequence=std::getenv("FURY_PLAYER_AUDIT_SEQUENCE");
      const bool every_frame=sequence&&std::string(sequence)=="1";
      if(every_frame||std::find(shots.begin(),shots.end(),*frame)!=shots.end()){
        std::vector<std::uint8_t> rgb;
        int width{},height{};
        if(!app.renderer().read_rgb_framebuffer(rgb,width,height)||width<=0||height<=0)*failed=true;
        else {
          std::ostringstream name;
          name<<"frame_"<<std::setfill('0')<<std::setw(3)<<*frame<<".ppm";
          std::ofstream file(directory/name.str(),std::ios::binary);
          file<<"P6\n"<<width<<' '<<height<<"\n255\n";
          file.write(reinterpret_cast<const char*>(rgb.data()),std::streamsize(rgb.size()));
          if(!file)*failed=true;
        }
      }
      if(*frame==299){
        *stream<<"],\"invariant_failures\":"<<(*failed?1:0)<<",\"renderer\":"<<std::quoted(app.renderer().backend_name())<<",\"renderer_validation_errors\":"<<app.renderer().statistics().validation_errors<<"}\n";
        stream->flush();
        if(!*stream||*failed)throw std::runtime_error("Player input audit invariant or output failure");
      }
    };
  }catch(const std::exception &e){
    fury::Log::error(e.what());
    return EXIT_FAILURE;
  }
}
