// Compiled only into the test-generated copy of Vaultline main. Product binaries
// have no test CLI, synthetic device, input override, or additional file writes.
{
  const bool pad = std::getenv("FURY_SETTINGS_TEST_PAD") != nullptr;
  const auto output=std::filesystem::current_path();
  auto trace=std::make_shared<std::ofstream>(output/"ui-trace.jsonl");
  SDL_Joystick* joystick=nullptr;
  if(pad) {
    SDL_VirtualJoystickDesc desc{}; desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;desc.naxes=SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;desc.name="Fury settings UI test";
    desc.button_mask=(1u<<SDL_CONTROLLER_BUTTON_MAX)-1;desc.axis_mask=(1u<<SDL_CONTROLLER_AXIS_MAX)-1;
    const int index=SDL_JoystickAttachVirtualEx(&desc);
    if(index<0 || !(joystick=SDL_JoystickOpen(index))) throw std::runtime_error("Cannot create test SDL controller");
  }
  splash_remaining=0; cutscene_pending=false; intro_cutscene.skip();
  app.input().set_cinematic(false);app.config().fixed_timestep=1.f/30.f;
  auto frame=std::make_shared<unsigned>(0);
  const auto* player=app.scene().find_by_name("PlayerBody");
  if(!player) throw std::runtime_error("No PlayerBody in UI test");
  const auto mesh=player->mesh;
  const auto textures=player->material.textures;
  const auto color_detail=player->material.color_detail;
  const auto update=app.on_update;const auto hud=app.on_hud;
  app.on_diagnostic_input=[&,frame,pad,joystick](fury::InputState& input,unsigned n) {
    *frame=n;
    std::fill_n(fury_settings_test_keys,SDL_NUM_SCANCODES,Uint8(0));
    if(pad) for(int b=0;b<SDL_CONTROLLER_BUTTON_MAX;++b) SDL_JoystickSetVirtualButton(joystick,b,0);
    auto press=[&](SDL_Scancode key,SDL_GameControllerButton button) {
      if(pad) SDL_JoystickSetVirtualButton(joystick,button,1);
      else fury_settings_test_keys[key]=1;
    };
    if(n==1||(n==13&&pad)||n==15||n==27||n==33||n==49)press(SDL_SCANCODE_O,SDL_CONTROLLER_BUTTON_START);
    if(n==13&&!pad) {
      SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_ESCAPE;
      event.key.keysym.scancode=SDL_SCANCODE_ESCAPE;SDL_PushEvent(&event);
    }
    if(n==3||n==23)press(SDL_SCANCODE_UP,SDL_CONTROLLER_BUTTON_DPAD_UP);
    if(n==5||n==6||n==25||n==45)press(SDL_SCANCODE_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    if(n==9||n==19||n==35)press(SDL_SCANCODE_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    if(n==11||n==15||n==16)press(SDL_SCANCODE_RETURN,SDL_CONTROLLER_BUTTON_A);
    if(!pad&&(n==11||n==15)) {
      SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;
      event.key.keysym.scancode=SDL_SCANCODE_RETURN;SDL_PushEvent(&event);
    }
    if(n==21||n==37||n==39||n==41||n==43)press(SDL_SCANCODE_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    // F6 has no controller equivalent; exercise the same production key path in
    // both runs, then return to controller-only navigation inside the modal.
    if((n==29&&quality_level!=fury::QualityLevel::High)||n==31)fury_settings_test_keys[SDL_SCANCODE_F6]=1;
    SDL_JoystickUpdate();
    input={};app.input().poll(input);
  };
  app.on_update=[&,frame,trace,mesh,textures,color_detail,update](float dt,const fury::InputState& input) {
    update(dt,input);
    const unsigned n=*frame;
    const auto* body=app.scene().find_by_name("PlayerBody");
    if(!body || body->mesh!=mesh || body->material.textures!=textures || body->material.color_detail!=color_detail)
      throw std::runtime_error("UI or F6 rebuilt Player mesh/material resources");
    const auto next=fury::preview_player_hair(active_player_hair,vl_settings.player_hair_preference(),
          vl_settings.quality_level(),app.renderer().backend_kind(),quality_environment_override ? &quality_level : nullptr);
    const bool pending=!fury::same_player_hair_groom(active_player_hair,next);
    const auto expect=[&](bool ok,const char* why) { if(!ok) throw std::runtime_error(std::string("UI frame ")+std::to_string(n)+": "+why); };
    if(n==1)expect(settings_panel.open,"Open settings");
    if(n==3)expect(settings_panel.selected==13,"Up wraps to Player Hair");
    if(n==5||n==6||n==11||n==15||n==16||n==25) {
      expect(vl_settings.player_hair==1,"Original selected without held-key repeat or reopen mutation");
      expect(pending==!active_player_hair.environment_override,"Pending or env override state");
    }
    if(n==9||n==19)expect(vl_settings.player_hair==0&&!pending,"Revert clears restart notice");
    if(n==13||n==27||n==49)expect(!settings_panel.open,"Close settings");
    if(n==13)std::filesystem::copy_file(fury::kSettingsPath,"saved-original.json",std::filesystem::copy_options::overwrite_existing);
    if(n==21)expect(settings_panel.selected==0,"Down wraps after last row");
    if(n==23)expect(settings_panel.selected==13,"Repeated Up returns to hair");
    if(n==29)expect(vl_settings.quality==2,"F6 moves Med to High");
    if(n==31)expect(vl_settings.quality==0,"F6 moves High to Low");
    if(n==35)expect(vl_settings.player_hair==0&&pending==(!active_player_hair.environment_override&&!quality_environment_override),"Auto Low pending respects restart quality override");
    if(n==43)expect(settings_panel.selected==3,"Navigate to quality row");
    if(n==45)expect(vl_settings.quality==1&&!pending,"Med reversion clears pending without rebuilding");
    *trace<<"{\"frame\":"<<n<<",\"menu_open\":"<<settings_panel.open<<",\"row\":"<<settings_panel.selected
       <<",\"preference\":"<<vl_settings.player_hair<<",\"quality\":"<<vl_settings.quality<<",\"pending\":"<<pending
       <<",\"environment_override\":"<<active_player_hair.environment_override<<",\"resource_identity_unchanged\":true}\n";
    trace->flush();
  };
  app.on_hud=[&,frame,hud,trace]() {
    if(hud)hud();
    if(*frame==5||*frame==9||*frame==35||*frame==45) {
      std::vector<std::uint8_t> pixels;int width{},height{};
      if(!app.renderer().read_rgb_framebuffer(pixels,width,height))throw std::runtime_error("UI framebuffer unavailable");
      const auto layout=vaultline::layout_settings_panel(float(height));
      std::ofstream layout_file("ui-"+std::to_string(*frame)+"-layout.json");
      layout_file<<"{\"footer_y\":"<<layout.footer_line_y(0)
                 <<",\"footer_second_y\":"<<layout.footer_line_y(1)
                 <<",\"footer_glyph_height\":"<<fury::kBitmapCellH*layout.footer_scale<<"}\n";
      std::ofstream file("ui-"+std::to_string(*frame)+".ppm",std::ios::binary);
      file<<"P6\n"<<width<<' '<<height<<"\n255\n";
      file.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()));
    }
    if(*frame==50) {
      if(app.renderer().statistics().validation_errors)throw std::runtime_error("UI renderer validation errors");
      fury::Log::info("PLAYER HAIR UI: production keyboard/modal/controller navigation, pending/revert, persistence and stable resources passed");
    }
  };
}
