// Opt-in integration diagnostic. Drive the production on_update closure using
// real interaction inputs and ordinary timers; render the actual scene through
// Application::run. Only the diagnostic player/review camera is positioned.
// This is not a navigated UI playthrough and never overwrites actor positions.
if (!crew_follow_audit_path.empty()) {
  try {
    auto need = [](bool pass, const std::string &message) {
      if (!pass)
        throw std::runtime_error(message);
    };
    need(npc_capture_id.empty() && !smoke_mode && !heist_capture_mode &&
             !photo_launch,
         "Crew follow audit conflicts with presentation capture modes");
    need(crew_capture_actor == "CrewRook" ||
             crew_capture_actor == "CrewSparrow",
         "Unknown crew capture actor");
    need(crew.members().size() == 2, "Both production crew members must exist");
    need(npc_presentation.enabled(),
         "Crew audit requires detailed presentation");
    std::filesystem::path frames_dir =
        std::filesystem::path(crew_follow_audit_path).parent_path() / "frames";
    need(!std::filesystem::exists(frames_dir) ||
             std::filesystem::is_empty(frames_dir),
         "Crew frame directory must be new or empty");
    std::filesystem::create_directories(frames_dir);
    splash_remaining = 0;
    intro_cutscene.skip();
    photo_mode.active = false;
    app.camera().fly_mode = false;
    app.camera().third_person = false;
    const auto member = [&](const std::string &id) -> const fury::CrewMember & {
      for (const auto &c : crew.members())
        if (c.entity_name == id)
          return c;
      throw std::runtime_error("Crew identity absent");
    };
    for (const auto &c : crew.members()) {
      bool rook = c.entity_name == "CrewRook";
      need(rook || c.entity_name == "CrewSparrow", "Unexpected crew actor");
      need(c.display_name == (rook ? "Rook" : "Sparrow") &&
               c.height == (rook ? 1.70f : 1.72f) && c.active &&
               c.follow_speed == 6.5f,
           "Canonical crew identity/height/activity/speed changed");
      need(c.follow_offset.x == (rook ? -1.8f : 1.8f) &&
               c.follow_offset.y == 0 &&
               c.follow_offset.z == (rook ? -1.4f : -1.2f),
           "Canonical trailing offset changed");
    }
    const auto vec = [](std::ostream &o, Vec3 p) {
      o << '[' << p.x << ',' << p.y << ',' << p.z << ']';
    };
    const auto state = [&]() {
      std::ostringstream s;
      s << std::setprecision(12)
        << "{\"phase\":" << std::quoted(heist.phase_name())
        << ",\"phase_time\":" << heist.time_in_phase() << ",\"player\":";
      vec(s, app.camera().position);
      s << ",\"player_yaw\":" << app.camera().yaw
        << ",\"heat\":" << heat.normalized()
        << ",\"loot_multiplier\":" << heist.loot_speed_mul << ",\"crew\":[";
      bool first = true;
      for (const auto &c : crew.members()) {
        if (!first)
          s << ',';
        first = false;
        s << "{\"id\":" << std::quoted(c.entity_name) << ",\"position\":";
        vec(s, c.position);
        s << ",\"offset\":";
        vec(s, c.follow_offset);
        s << ",\"velocity\":";
        vec(s, c.velocity);
        s << ",\"height\":" << c.height << ",\"yaw\":" << c.yaw
          << ",\"actual_speed\":" << c.actual_speed
          << ",\"travel_distance\":" << c.travel_distance
          << ",\"move_weight\":" << c.move_weight
          << ",\"active\":" << (c.active ? "true" : "false") << '}';
      }
      s << "],\"npcs\":[";
      first = true;
      for (const auto &a : npcs.agents()) {
        if (!first)
          s << ',';
        first = false;
        s << "{\"id\":" << std::quoted(a.entity_name) << ",\"position\":";
        vec(s, a.position);
        s << ",\"home\":";
        vec(s, a.home);
        s << ",\"radius\":" << a.radius << ",\"yaw\":" << a.yaw
          << ",\"waypoint_index\":" << a.waypoint_index
          << ",\"travel_distance\":" << a.travel_distance
          << ",\"actual_speed\":" << a.actual_speed
          << ",\"chasing\":" << (a.chasing ? "true" : "false")
          << ",\"on_duty\":" << (a.on_duty ? "true" : "false") << '}';
      }
      s << "]}";
      return s.str();
    };
    std::ofstream report(crew_follow_audit_path);
    need(bool(report), "Could not open crew audit report");
    report << std::setprecision(12)
           << "{\"kind\":\"actual-vaultline-crew-controller-and-world-render\","
              "\"source_sha256\":"
           << std::quoted(fury::build_source_fingerprint())
           << ",\"actor\":" << std::quoted(crew_capture_actor)
           << ",\"production_ai_changed\":false,\"actors_relocated_for_"
              "capture\":false,\"diagnostic_player_camera\":true,\"ui_"
              "playthrough\":false,\"snapshots\":[";
    // Refresh provenance is explicit in every snapshot record below.
    bool first_snapshot = true;
    unsigned ticks = 0, captures = 0;
    std::array<float, 2> peak{};
    auto snapshot = [&](const std::string &label, float orbit = 45.f) {
      const auto before = state();
      const auto saved_camera = app.camera();
      const auto saved_config = app.config();
      const auto saved_update = app.on_update;
      const auto saved_pre = app.on_pre_update;
      const auto saved_hud = app.on_hud;
      const auto &c = member(crew_capture_actor);
      const float angle = c.yaw + orbit * 3.14159265359f / 180.f;
      const Vec3 target = c.position + Vec3{0, .08f, 0};
      app.camera().position =
          target + Vec3{std::sin(angle) * 3.2f, .12f, std::cos(angle) * 3.2f};
      const Vec3 direction = target - app.camera().position;
      app.camera().yaw = std::atan2(direction.z, direction.x);
      app.camera().pitch =
          std::atan2(direction.y, std::hypot(direction.x, direction.z));
      app.camera().snap_look();
      app.camera().velocity = {};
      app.camera().coyote_timer = 0;
      app.camera().coyote_wish = {};
      app.camera().fly_mode = true;
      app.on_update = {};
      app.on_pre_update = {};
      app.on_hud = {};
      app.config().max_frames = 1;
      app.config().fixed_timestep = 1.f / 60.f;
      app.config().freeze_render_time = true;
      app.config().show_hud = false;
      app.config().capture_sequence_directory.clear();
      app.config().capture_path = (frames_dir / (label + ".ppm")).string();
      const Vec3 eye = app.camera().position;
      const float camera_yaw = app.camera().yaw,
                  camera_pitch = app.camera().pitch;
      // Sample the already-updated live animation at the newly visible LOD.
      // This does not manufacture an animation frame or advance a game timer.
      need(npc_presentation.refresh_for_review(
               crew_capture_actor,
               fury::length(app.camera().position - c.position),
               app.config().lod_mid_distance),
           "Crew review could not refresh its production presentation");
      const int result = app.run();
      const auto metrics = app.renderer().statistics();
      app.camera() = saved_camera;
      app.config() = saved_config;
      app.on_update = saved_update;
      app.on_pre_update = saved_pre;
      app.on_hud = saved_hud;
      need(result == 0 && metrics.validation_errors == 0,
           "Actual world renderer failed crew frame");
      need(state() == before, "A review frame changed live gameplay state");
      if (!first_snapshot)
        report << ',';
      first_snapshot = false;
      report << "{\"label\":" << std::quoted(label)
             << ",\"presentation_refresh_uses_live_state\":true,\"presentation_"
                "refresh_advances_time\":false,\"simulation_tick\":"
             << ticks << ",\"state\":" << before
             << ",\"frame\":" << std::quoted("frames/" + label + ".ppm")
             << ",\"review_camera\":{\"eye\":";
      vec(report, eye);
      report << ",\"target\":";
      vec(report, target);
      report << ",\"yaw\":" << camera_yaw << ",\"pitch\":" << camera_pitch
             << "},\"backend\":" << std::quoted(app.renderer().backend_name())
             << ",\"cpu_frame_ms\":" << metrics.cpu_frame_ms
             << ",\"triangles\":" << metrics.triangle_count
             << ",\"instances\":" << metrics.instance_count
             << ",\"validation_errors\":" << metrics.validation_errors << '}';
      report.flush();
      ++captures;
    };
    auto tick = [&](bool interact = false, bool crouch = true) {
      fury::InputState input;
      input.interact_pressed = interact;
      input.key_ctrl = crouch;
      app.camera().crouching = crouch;
      app.camera().position.y =
          crouch ? app.camera().crouch_eye_y : app.camera().stand_eye_y;
      const auto previous = crew.members();
      app.on_update(1.f / 60.f, input);
      ++ticks;
      for (std::size_t i = 0; i < crew.members().size(); ++i) {
        const auto &c = crew.members()[i];
        need(c.position.y == c.height * .5f,
             "Crew lost individual ground height");
        need(fury::length(c.position - previous[i].position) <=
                 c.follow_speed / 60.f + .00001f,
             "Crew teleported or exceeded ordinary follow speed");
        need(c.actual_speed <= c.follow_speed + .001f,
             "Measured follow speed exceeds authored limit");
        peak[i] = std::max(peak[i], c.actual_speed);
      }
    };
    snapshot("00-original-spawn", 45);
    snapshot("01-original-rear", 135);
    const auto initial = crew.members();
    app.camera().position = {5.f, 1.7f, 23.f};
    app.camera().yaw = 0;
    app.camera().snap_look();
    for (int i = 0; i < 60; ++i)
      tick(false, false);
    need(heist.phase() == fury::HeistPhase::Idle,
         "Idle witness entered an unexpected mission");
    for (std::size_t i = 0; i < initial.size(); ++i)
      need(fury::length(crew.members()[i].position - initial[i].position) <
               1e-6f,
           "Idle crew changed position");
    mission_board.select(0);
    apply_target();
    const Vec3 vault = heist.vault_position;
    Vec3 interaction{};
    bool found = false;
    for (int direction = 0; direction < 32 && !found; ++direction) {
      float angle = 6.28318530718f * direction / 32.f;
      Vec3 candidate{vault.x + 3.65f * std::cos(angle), 1.7f,
                     vault.z + 3.65f * std::sin(angle)};
      bool occupied = nearest_driveable(candidate) >= 0 ||
                      interiors.door_at(candidate) ||
                      security.near_live_breaker(candidate);
      for (const char *name :
           {"SecBadgePad", "MMBadgeScan", "VaultMaintHatch", "VaultToolPoint",
            "VaultEmergPanel", "SecTerminalA", "SecTerminalB"})
        if (const auto *e = app.scene().find_by_name(name))
          occupied |= dist_xz(candidate, e->transform.position) < 2.75f;
      if (!occupied) {
        interaction = candidate;
        found = true;
      }
    }
    need(found, "No ordinary E-to-breach point found");
    app.camera().position = interaction;
    app.camera().velocity = {};
    tick();
    need(heist.phase() == fury::HeistPhase::Approach,
         "Production approach did not start");
    for (int i = 0; i < 180; ++i)
      tick();
    snapshot("02-approach-follow", 45);
    auto maximum_target_error = [&]() {
      float error = 0;
      const float cy = std::cos(app.camera().yaw),
                  sy = std::sin(app.camera().yaw);
      for (const auto &c : crew.members()) {
        Vec3 goal{app.camera().position.x - sy * c.follow_offset.x +
                      cy * c.follow_offset.z,
                  c.height * .5f,
                  app.camera().position.z + cy * c.follow_offset.x +
                      sy * c.follow_offset.z};
        error = std::max(error, dist_xz(c.position, goal));
      }
      return error;
    };
    // Arrival is established from production distance, not an assumed travel
    // duration. The integration guard remains bounded at 20 simulated seconds.
    for (int i = 0; i < 1200 && maximum_target_error() > .05f; ++i)
      tick();
    need(maximum_target_error() <= .05f,
         "Crew did not finish initial real travel");
    // Four ordinary Camera headings pin the authored right/left trailing basis.
    for (int heading = 0; heading < 4; ++heading) {
      app.camera().yaw = ((heading + 1) % 4) * 1.57079632679f;
      app.camera().snap_look();
      for (int i = 0; i < 90; ++i) {
        tick();
        if (i == 20)
          snapshot("03-cardinal-moving-" + std::to_string(heading),
                   heading % 2 ? 135.f : 45.f);
      }
      const float cy = std::cos(app.camera().yaw),
                  sy = std::sin(app.camera().yaw);
      for (const auto &c : crew.members()) {
        Vec3 goal{app.camera().position.x - sy * c.follow_offset.x +
                      cy * c.follow_offset.z,
                  c.height * .5f,
                  app.camera().position.z + cy * c.follow_offset.x +
                      sy * c.follow_offset.z};
        need(dist_xz(c.position, goal) < .10f,
             "Crew missed its real cardinal trailing target");
      }
    }
    need(heist.phase() == fury::HeistPhase::Approach,
         "Cardinal following unexpectedly advanced/faulted the heist");
    tick(true);
    need(heist.phase() == fury::HeistPhase::Breach,
         "Actual E input did not start Breach");
    snapshot("04-real-breach", 45);
    for (int i = 0; i < 600 && heist.phase() == fury::HeistPhase::Breach; ++i)
      tick();
    need(heist.phase() == fury::HeistPhase::Looting,
         "Real breach timer failed to enter Looting");
    snapshot("05-real-looting", 135);
    app.camera().position = {vault.x, 1.7f, vault.z + 18.f};
    app.camera().yaw = 1.57079632679f;
    app.camera().snap_look();
    for (int i = 0; i < 1200 && heist.phase() == fury::HeistPhase::Looting;
         ++i) {
      app.camera().position.z += 4.5f / 60.f;
      tick(false, false);
      if (i == 35)
        snapshot("06-active-travel", 45);
    }
    need(heist.phase() == fury::HeistPhase::Escape,
         "Ordinary loot timer did not reach Escape");
    snapshot("07-real-escape", 135);
    app.camera().position = heist.escape_position;
    app.camera().position.y = 1.7f;
    tick(false, false);
    need(heist.phase() == fury::HeistPhase::Success,
         "Actual extraction did not succeed");
    snapshot("08-real-success", 45);
    app.camera().position = heist.escape_position + Vec3{8, 1.7f, 8};
    tick(true, false);
    need(heist.phase() == fury::HeistPhase::Idle,
         "Actual E reset did not return Idle");
    const auto stopped = crew.members();
    app.camera().position.x += 8;
    for (int i = 0; i < 90; ++i)
      tick(false, false);
    for (std::size_t i = 0; i < stopped.size(); ++i) {
      const auto &c = crew.members()[i];
      need(fury::length(c.position - stopped[i].position) < 1e-6f &&
               c.actual_speed == 0,
           "Reset crew continued following");
      need(c.travel_distance > initial[i].travel_distance + 10 && peak[i] > 3.f,
           "Crew lacked real active-heist travel");
    }
    snapshot("09-reset-stationary", 135);
    report << "],\"simulation_ticks\":" << ticks
           << ",\"rendered_snapshots\":" << captures
           << ",\"idle_stop_preserved\":true,\"cardinal_offsets_preserved\":"
              "true,\"real_breach_loot_escape_success_reset\":true,\"render_"
              "preserved_gameplay_state\":true,\"peak_speeds\":["
           << peak[0] << ',' << peak[1] << "],\"passed\":true}\n";
    report.close();
    need(bool(report), "Crew audit report write failed");
    fury::Log::info(
        "PASS actual crew follow, cardinal offsets, real E/timer heist, "
        "successful extraction, reset and world-rendered snapshots");
    net_client->disconnect();
    audio->shutdown();
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    fury::Log::error(std::string("Crew follow audit: ") + error.what());
    return EXIT_FAILURE;
  }
}
