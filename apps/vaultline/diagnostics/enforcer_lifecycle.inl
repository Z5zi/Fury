// Diagnostic-only block included after Vaultline's real update/render
// callbacks. This uses the actual selected-mission, complication, spawn,
// cleanup and presentation code in this process. It does not duplicate an NPC
// constructor. Only the diagnostic RNG seed/timer and test camera/input are
// controlled.
if (!enforcer_lifecycle_path.empty()) {
  try {
    const auto require_audit = [](bool condition, const char *message) {
      if (!condition)
        throw std::runtime_error(message);
    };
    require_audit(
        npc_capture_id.empty() && !smoke_mode && !heist_capture_mode &&
            !photo_launch,
        "Lifecycle audit must run separately from portrait/smoke/photo modes");
    require_audit(npc_presentation.enabled(),
                  "Lifecycle audit requires detailed character presentation");
    splash_remaining = 0.f;
    intro_cutscene.skip();
    photo_mode.active = false;
    app.camera().fly_mode = false;
    app.camera().third_person = false;
    const auto count_guard = [&]() {
      return std::count_if(npcs.agents().begin(), npcs.agents().end(),
                           [](const fury::NpcAgent &a) {
                             return a.entity_name == "NpcEnforcer";
                           });
    };
    const auto guard = [&]() -> fury::NpcAgent * {
      for (auto &a : npcs.agents())
        if (a.entity_name == "NpcEnforcer")
          return &a;
      return nullptr;
    };
    const auto tick = [&](bool interact = false, bool crouch = false) {
      fury::InputState input;
      input.interact_pressed = interact;
      input.key_ctrl = crouch;
      app.camera().crouching = crouch;
      app.camera().position.y =
          crouch ? app.camera().crouch_eye_y : app.camera().stand_eye_y;
      app.on_update(1.f / 60.f, input);
    };
    std::ofstream report(enforcer_lifecycle_path);
    require_audit(bool(report), "Could not open lifecycle diagnostic report");
    report
        << std::setprecision(12)
        << "{\"kind\":\"actual-vaultline-selected-vault-complication-"
           "lifecycle\",\"diagnostic_rng_and_camera\":true,\"crouched_breach_"
           "input\":true,\"production_ai_changed\":false,\"source_sha256\":"
        << std::quoted(fury::build_source_fingerprint()) << ",\"cycles\":[";
    fury::Mesh *remembered_near = nullptr;
    fury::Mesh *remembered_far = nullptr;
    const fury::MaterialTextures *remembered_textures = nullptr;
    std::size_t remembered_mesh_count = 0, remembered_actor_count = 0;
    std::vector<Vec3> remembered_near_pose, remembered_far_pose;
    bool first_cycle = true;
    // Complete the four required base jobs before the ordinary finale gate.
    for (int mission : {0, 1, 2, 3, 5, 4})
      for (int cycle = 0; cycle < 2; ++cycle) {
        require_audit(count_guard() == 0 && !complications.enforcer_alive,
                      "Guard survived preceding cleanup");
        mission_board.select(mission);
        apply_target();
        require_audit(mission_board.selected == mission,
                      "Selected mission rejected by its canonical gate");
        const Vec3 vault = heist.vault_position;
        // The vault center overlaps maintenance interactions in Meridian.
        // Select a genuine E-to-breach point inside the heist radius and clear
        // of higher-priority interactions, without changing any world object.
        bool found_interaction = false;
        Vec3 interaction{};
        for (int direction = 0; direction < 32 && !found_interaction;
             ++direction) {
          const float angle = 6.28318530718f * direction / 32.f;
          const Vec3 candidate{vault.x + 3.65f * std::cos(angle), 1.7f,
                               vault.z + 3.65f * std::sin(angle)};
          bool occupied = nearest_driveable(candidate) >= 0 ||
                          interiors.door_at(candidate) ||
                          security.near_live_breaker(candidate);
          for (const char *name :
               {"SecBadgePad", "MMBadgeScan", "VaultMaintHatch",
                "VaultToolPoint", "VaultEmergPanel", "SecTerminalA",
                "SecTerminalB"})
            if (const auto *e = app.scene().find_by_name(name))
              occupied |= dist_xz(candidate, e->transform.position) < 2.75f;
          if (!occupied) {
            interaction = candidate;
            found_interaction = true;
          }
        }
        require_audit(found_interaction,
                      "No ordinary unconsumed E-to-breach point found");
        app.camera().position = interaction;
        app.camera().velocity = {};
        // Choose a seed through the unchanged production rare-spawn roller.
        // Set it before real E/crouched breach enters Looting: this actor rolls
        // exactly on that transition, unlike the later ExtraGuard event.
        const int tier = mission_board.current().payout_tier;
        const bool eligible = tier >= 3;
        std::uint32_t selected_seed = 1;
        bool found_seed = false;
        for (std::uint32_t seed = 1; seed < 100000 && !found_seed; ++seed) {
          fury::HeistComplications probe;
          auto trial = seed;
          if (probe.try_spawn_enforcer(tier, trial) == eligible) {
            // Keep later ordinary rolls quiet in this deterministic lifecycle
            // sample so unrelated ExtraGuard allocation does not confound this
            // actor's cache identity. No production chance or timer changes.
            auto later=trial; bool quiet=true;
            for(int roll=0;roll<4;++roll){later=later*1664525u+1013904223u;
              const float u=float((later>>8)&0xffffffu)/16777215.f;
              quiet &= u>.38f;}
            if(!quiet) continue;
            selected_seed = seed;
            found_seed = true;
          }
        }
        require_audit(found_seed, "No deterministic rare Enforcer seed found");
        complication_rng = selected_seed;
        for (int i = 0; i < 600 && heist.phase() != fury::HeistPhase::Looting;
             ++i)
          tick(i % 30 == 1, true);
        require_audit(heist.phase() == fury::HeistPhase::Looting,
                      "Normal heist did not reach Looting for lifecycle audit");
        fury::Log::info("Enforcer lifecycle witness mission=" +
                       std::string(mission_board.current().id) + " tier=" +
                       std::to_string(tier) + " phase=" + heist.phase_name() +
                       " checked=" + std::to_string(complications.enforcer_checked) +
                       " alive=" + std::to_string(complications.enforcer_alive) +
                       " agents=" + std::to_string(count_guard()) +
                       " seed=" + std::to_string(selected_seed));
        require_audit(complications.enforcer_checked,
                      "Actual Looting transition never checked rare actor");
        require_audit(eligible ? complications.enforcer_alive && count_guard() == 1
                               : !complications.enforcer_alive && count_guard() == 0,
                      "Real rare Enforcer transition has inconsistent alive flag/agent count");
        const Vec3 expected_spawn{vault.x - 4.5f, .975f, vault.z + 3.5f};
        Vec3 spawn = expected_spawn;
        float maximum_reset_error = 0, peak_speed = 0;
        double travel = 0;
        std::size_t mesh_count = app.scene().meshes().size();
        std::size_t actor_count = npc_presentation.statistics().actors;
        if (eligible) {
        auto *g = guard();
        auto *entity = app.scene().find_by_name("NpcEnforcer");
        require_audit(g && entity && entity->visible,
                      "Spawn lacks actual agent/visible scene entity");
        require_audit(g->name == "Enforcer" &&
                          g->display_name == "Syndicate Enforcer" &&
                          g->kind == fury::NpcKind::Enforcer,
                      "Canonical dynamic identity changed");
        require_audit(g->height == 1.95f && g->speed == 2.4f &&
                          g->chase_speed == 5.6f && g->chasing,
                      "Canonical dynamic role/height/speeds changed");
        require_audit(fury::length(g->position - Vec3{vault.x - 4.5f, .975f,
                                                      vault.z + 3.5f}) < 1e-5f,
                      "Spawn ignored selected current vault");
        require_audit(g->waypoints.size() == 1 &&
                          fury::length(g->waypoints[0] -
                                       Vec3{vault.x - 4.5f, 0, vault.z + 3.5f}) <
                              1e-5f,
                      "Spawn route ignored selected current vault");
        mesh_count = app.scene().meshes().size();
        actor_count = npc_presentation.statistics().actors;
        if (!remembered_near) {
          remembered_near = entity->mesh;
          remembered_far = entity->lod_mesh;
          remembered_textures = entity->material.textures.get();
          remembered_mesh_count = mesh_count;
          remembered_actor_count = actor_count;
        }
        require_audit(g->actual_speed == 0 && g->travel_distance == 0,
                      "Respawn retained prior locomotion state");
        // Match the existing npc_presentation regression's 2e-6 m Euclidean
        // pose-reset contract. apply_character_pose intentionally retains
        // cached coordinate components within 1e-6 m; this is not a bit-exact
        // renderer.
        constexpr float reset_tolerance = 2e-6f;
        maximum_reset_error = 0;
        const auto pose_error = [](const auto &vertices,
                                   const auto &remembered) {
          float result = 0;
          for (std::size_t i = 0; i < vertices.size(); ++i)
            result = std::max(
                result, fury::length(vertices[i].position - remembered[i]));
          return result;
        };
        for (int lod = 0; lod < 2; ++lod) {
          const auto &vertices =
              (lod ? entity->lod_mesh : entity->mesh)->vertices;
          auto &remembered = lod ? remembered_far_pose : remembered_near_pose;
          if (remembered.empty())
            for (const auto &v : vertices)
              remembered.push_back(v.position);
          require_audit(vertices.size() == remembered.size(),
                        "Respawn changed vertex allocation size");
          maximum_reset_error =
              std::max(maximum_reset_error, pose_error(vertices, remembered));
          require_audit(
              maximum_reset_error <= reset_tolerance,
              "Respawn pose exceeded existing 2e-6 m renderer contract");
          if (first_cycle) {
            auto injected = vertices;
            injected[0].position.x += 8e-6f;
            require_audit(pose_error(injected, remembered) > reset_tolerance,
                          "Pose reset comparator accepted its above-tolerance "
                          "negative control");
          }
        }
        require_audit(entity->mesh == remembered_near &&
                          entity->lod_mesh == remembered_far,
                      "Respawn allocated replacement character meshes");
        require_audit(entity->material.textures.get() == remembered_textures,
                      "Respawn replaced shared material allocation");
        require_audit(mesh_count == remembered_mesh_count &&
                          actor_count == remembered_actor_count,
                      "Respawn increased model/mesh registration count");
        spawn = g->position;
        for(int repeat=0;repeat<4;++repeat) spawn_enforcer_near_vault();
        require_audit(count_guard() == 1 &&
                          fury::length(guard()->position - spawn) < 1e-6f,
                      "Duplicate spawn changed active guard");
        peak_speed = 0;
        travel = 0;
        app.camera().position = {vault.x, 1.7f, vault.z + 18.f};
        for (int i = 0; i < 72 && guard(); ++i) {
          tick();
          if(i%12==0) spawn_enforcer_near_vault();
          require_audit(count_guard()==1,"Repeated live ticks duplicated or lost Enforcer");
          if (auto *a = guard()) {
            peak_speed = std::max(peak_speed, a->actual_speed);
            travel = a->travel_distance;
          }
        }
        require_audit(
            travel > 1 && peak_speed > 5.4f && peak_speed <= 5.601f,
            "Actual 5.6 m/s pursuit did not advance through live controller");
        if(mission==0 && cycle==0 && !enforcer_lifecycle_capture_directory.empty()) {
          // Capture only after the actual production Looting transition and
          // verified pursuit. Application::run keeps its normal rendering,
          // visibility, LOD, update and framebuffer-capture paths unchanged.
          // The camera follows; the actor and scenery are never repositioned.
          const auto saved_config=app.config();
          const auto saved_camera=app.camera();
          const auto production_update=app.on_update;
          app.config().max_frames=8;
          app.config().fixed_timestep=1.f/12.f;
          app.config().show_hud=false;
          app.config().capture_path.clear();
          app.config().capture_sequence_directory=enforcer_lifecycle_capture_directory;
          std::ofstream frames(enforcer_lifecycle_capture_directory+".json");
          require_audit(bool(frames),"Could not open actual pursuit frame audit");
          frames<<std::setprecision(12)<<"{\"kind\":\"real-looting-enforcer-pursuit-render\",\"diagnostic_camera\":true,\"actor_repositioned\":false,\"source_fingerprint\":"
                <<std::quoted(fury::build_source_fingerprint())<<",\"frames\":[";
          unsigned frame=0;
          // Continue the already established pursuit direction; rotating the
          // target by 90 degrees invokes legitimate corner braking and would
          // not be a steady 5.6 m/s comparison.
          const Vec3 chase_direction=fury::normalize(Vec3{guard()->velocity.x,0,guard()->velocity.z});
          const auto follow=[&](){
            auto* actor=guard();require_audit(actor!=nullptr,"Pursuit capture lost its real actor");
            const Vec3 target=actor->position;
            app.camera().position=target+chase_direction*3.2f+Vec3{0,.18f,0};
            const auto direction=fury::normalize(target-app.camera().position);
            app.camera().yaw=std::atan2(direction.z,direction.x);
            app.camera().pitch=std::asin(std::clamp(direction.y,-1.f,1.f));
            app.camera().fov_y_degrees=42.f;app.camera().fly_mode=true;
            app.camera().third_person=false;app.camera().velocity={};app.camera().snap_look();
          };
          app.on_update=[&](float dt,const fury::InputState& input){
            follow();production_update(dt,input);follow();
            const auto* actor=guard();
            fury::Log::info("Pursuit frame state phase="+std::string(heist.phase_name())+" alive="+std::to_string(complications.enforcer_alive)+" chasing="+std::to_string(actor->chasing)+" speed="+std::to_string(actor->actual_speed)+" travel="+std::to_string(actor->travel_distance));
            require_audit(heist.phase()==fury::HeistPhase::Looting && complications.enforcer_alive && actor->chasing && actor->actual_speed>5.4f,
              "Actual pursuit capture did not retain active Looting chase");
            if(frame++)frames<<',';
            frames<<"{\"frame\":"<<frame<<",\"position\":["<<actor->position.x<<','<<actor->position.y<<','<<actor->position.z
              <<"],\"yaw\":"<<actor->yaw<<",\"actual_speed\":"<<actor->actual_speed<<",\"travel_distance\":"<<actor->travel_distance
              <<",\"phase\":\"Looting\",\"alive\":true,\"chasing\":true,\"heat\":"<<heat.normalized()
              <<",\"camera_eye\":["<<app.camera().position.x<<','<<app.camera().position.y<<','<<app.camera().position.z
              <<"],\"camera_target\":["<<actor->position.x<<','<<actor->position.y<<','<<actor->position.z<<"],\"agents\":[";
            bool first_agent=true;
            for(const auto& a:npcs.agents()) {if(!first_agent)frames<<',';first_agent=false;
              frames<<"{\"id\":"<<std::quoted(a.entity_name)<<",\"position\":["<<a.position.x<<','<<a.position.y<<','<<a.position.z
                    <<"],\"yaw\":"<<a.yaw<<",\"actual_speed\":"<<a.actual_speed<<",\"travel_distance\":"<<a.travel_distance<<'}';}
            frames<<"],\"crew\":[";bool first_crew=true;
            for(const auto& c:crew.members()){if(!first_crew)frames<<',';first_crew=false;
              frames<<"{\"id\":"<<std::quoted(c.entity_name)<<",\"position\":["<<c.position.x<<','<<c.position.y<<','<<c.position.z
                    <<"],\"yaw\":"<<c.yaw<<",\"travel_distance\":"<<c.travel_distance<<'}';}
            frames<<"]}";
          };
          follow();
          const int capture_result=app.run();
          app.on_update=production_update;app.config()=saved_config;app.camera()=saved_camera;
          require_audit(capture_result==0 && frame==8,"Actual pursuit frame capture failed");
          frames<<"],\"backend\":"<<std::quoted(app.renderer().backend_name())
                <<",\"validation_errors\":"<<app.renderer().statistics().validation_errors<<"}\n";
          frames.flush();require_audit(frames.good(),"Actual pursuit frame audit write failed");
        }
        }
        if (cycle == 0) {
          heist.force_fail();
          tick();
          require_audit(heist.phase() == fury::HeistPhase::Failed,
                        "Real failed transition not observed");
        } else {
          for (int i = 0;
               i < 1200 && heist.phase() != fury::HeistPhase::Escape &&
               heist.phase() != fury::HeistPhase::Failed;
               ++i) {
            app.camera().position.z += app.camera().move_speed / 60.f;
            tick();
          }
          require_audit(heist.phase() == fury::HeistPhase::Escape,
                        "Real looting flow failed to reach Escape");
        }
        require_audit(!complications.enforcer_alive && count_guard() == 0,
                      "Actual terminal/escape transition retained guard agent");
        auto *entity = app.scene().find_by_name("NpcEnforcer");
        require_audit(!eligible || (entity && !entity->visible &&
                          entity->transform.position.y == -80.f),
                      "Actual cleanup failed to hide/park guard");
        const std::string cleanup = cycle == 0 ? "Failed" : "Escape";
        if (cycle == 1) {
          app.camera().position = heist.escape_position;
          app.camera().position.y = 1.7f;
          tick();
          require_audit(heist.phase() == fury::HeistPhase::Success,
                        "Escape did not reach genuine Success");
        }
        app.camera().position = heist.escape_position + Vec3{8, 1.7f, 8};
        tick(true);
        require_audit(heist.phase() == fury::HeistPhase::Idle,
                      "Genuine E reset did not restore Idle");
        // Cool down through the existing authored safehouse and pursuit rules.
        // Holding a camera outside lets patrol cars maintain contact
        // indefinitely. Only diagnostic camera position changes; no heat/AI
        // setting is reset.
        bool found_safehouse = false;
        for (const auto &zone : interiors.zones)
          if (zone.tag && std::strcmp(zone.tag, "loft") == 0) {
            app.camera().position = zone.center;
            app.camera().position.y = 1.7f;
            found_safehouse = true;
            break;
          }
        require_audit(found_safehouse, "Authored safehouse is unavailable");
        tick();
        require_audit(in_safehouse,
                      "Camera failed to enter real safehouse rules");
        for (int i = 0; i < 900 && heat.normalized() > 0; ++i)
          tick();
        require_audit(heat.normalized() == 0 && !pursuit.any_active(),
                      "Real safehouse did not cool heat and dismiss pursuit");
        if (!first_cycle)
          report << ',';
        first_cycle = false;
        const char* applicable=eligible ? "true" : "null";
        report << "{\"mission_index\":" << mission
               << ",\"mission_id\":" << std::quoted(mission_board.current().id)
               << ",\"cycle\":" << cycle
               << ",\"eligible\":" << (eligible ? "true" : "false")
               << ",\"payout_tier\":" << tier << ",\"vault\":[" << vault.x << ',' << vault.y << ',' << vault.z << ']'
               << ",\"diagnostic_escape_speed\":" << app.camera().move_speed
               << ",\"rare_actor_absent_by_tier\":" << (eligible ? "false" : "true")
               << ",\"spawn\":";
        if(eligible)report << '[' << spawn.x << ',' << spawn.y << ',' << spawn.z << ']';
        else report << "null";
        report << ",\"peak_actual_speed\":" << peak_speed
               << ",\"travel_distance\":" << travel
               << ",\"cleanup_phase\":" << std::quoted(cleanup)
               << ",\"cached_meshes_reused\":" << applicable
               << ",\"pose_reset_within_contract\":" << applicable
               << ",\"maximum_pose_reset_error_m\":" << maximum_reset_error
               << ",\"pose_reset_tolerance_m\":0.000002,\"above_tolerance_control_rejected\":" << applicable
               << ",\"locomotion_reset\":" << applicable
               << ",\"material_reused\":" << applicable
               << ",\"mesh_count\":" << mesh_count
               << ",\"registered_actor_count\":" << actor_count
               << ",\"duplicate_spawn_idempotent\":" << applicable
               << ",\"agent_removed\":" << applicable
               << ",\"entity_hidden\":" << applicable
               << ",\"reset_to_idle\":true}";

      }
    report << "],\"passed\":true}\n";
    report.flush();
    require_audit(report.good(), "Lifecycle report write failed");
    fury::Log::info(
        "PASS Syndicate Enforcer: 12 actual complication/spawn/chase/cleanup/reset "
        "cycles across six selected missions");
    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    fury::Log::error(std::string("Syndicate Enforcer lifecycle audit: ") + e.what());
    return EXIT_FAILURE;
  }
}
