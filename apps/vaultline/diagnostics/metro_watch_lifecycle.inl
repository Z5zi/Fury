// Diagnostic-only block included after Vaultline's real update/render
// callbacks. This uses the actual selected-mission, complication, spawn,
// cleanup and presentation code in this process. It does not duplicate an NPC
// constructor. Only the diagnostic RNG seed/timer and test camera/input are
// controlled.
if (!metro_watch_lifecycle_path.empty()) {
  try {
    const auto require_audit = [](bool condition, const char *message) {
      if (!condition)
        throw std::runtime_error(message);
    };
    require_audit(
        npc_capture_id.empty() && !smoke_mode && !heist_capture_mode &&
            !photo_launch,
        "Lifecycle audit must run separately from portrait/smoke/photo modes");
    require_audit(npc_presentation.enabled() && npc_individual_enabled,
                  "Lifecycle audit requires individual detailed characters");
    splash_remaining = 0.f;
    intro_cutscene.skip();
    photo_mode.active = false;
    app.camera().fly_mode = false;
    app.camera().third_person = false;
    const auto count_guard = [&]() {
      return std::count_if(npcs.agents().begin(), npcs.agents().end(),
                           [](const fury::NpcAgent &a) {
                             return a.entity_name == "NpcExtraGuard";
                           });
    };
    const auto guard = [&]() -> fury::NpcAgent * {
      for (auto &a : npcs.agents())
        if (a.entity_name == "NpcExtraGuard")
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
    std::ofstream report(metro_watch_lifecycle_path);
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
        require_audit(count_guard() == 0 && !complications.extra_guard_alive,
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
        // Drive the real heist update with the same edge input used by E.
        for (int i = 0; i < 600 && heist.phase() != fury::HeistPhase::Looting;
             ++i)
          tick(i % 30 == 1, true);
        require_audit(heist.phase() == fury::HeistPhase::Looting,
                      "Normal heist did not reach Looting for lifecycle audit");
        // Choose a deterministic event via the real roller, never directly
        // mutate the alive flag or create a replacement/synthetic guard.
        std::uint32_t selected_seed = 0;
        bool found_seed = false;
        for (std::uint32_t seed = 1; seed < 100000 && !found_seed; ++seed) {
          auto a = seed * 1664525u + 1013904223u;
          const float u = float((a >> 8) & 0xffffffu) / 16777215.f;
          auto next = a * 1664525u + 1013904223u;
          if (u <= .38f && ((next >> 16) % 4u) == 1u) {
            selected_seed = seed;
            found_seed = true;
          }
        }
        require_audit(found_seed,
                      "No deterministic ExtraGuard event seed found");
        complication_rng = selected_seed;
        complications.next_roll_in = .001f;
        app.camera().position = {vault.x, 1.7f, vault.z + 18.f};
        tick();
        require_audit(complications.extra_guard_alive && count_guard() == 1,
                      "Real ExtraGuard complication failed to spawn once");
        auto *g = guard();
        auto *entity = app.scene().find_by_name("NpcExtraGuard");
        require_audit(g && entity && entity->visible,
                      "Spawn lacks actual agent/visible scene entity");
        require_audit(g->name == "ExtraGuard" &&
                          g->display_name == "Metro Watch" &&
                          g->kind == fury::NpcKind::Guard,
                      "Canonical dynamic identity changed");
        require_audit(g->height == 1.82f && g->speed == 1.8f &&
                          g->chase_speed == 4.f && g->chasing,
                      "Canonical dynamic role/height/speeds changed");
        require_audit(fury::length(g->position - Vec3{vault.x + 3.5f, .91f,
                                                      vault.z + 2.5f}) < 1e-5f,
                      "Spawn ignored selected current vault");
        require_audit(g->waypoints.size() == 3 &&
                          fury::length(g->waypoints[1] -
                                       Vec3{vault.x - 2.f, 0, vault.z + 1.f}) <
                              1e-5f,
                      "Spawn route ignored selected current vault");
        const auto mesh_count = app.scene().meshes().size(),
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
        float maximum_reset_error = 0;
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
        const Vec3 spawn = g->position;
        spawn_extra_guard_near_vault();
        require_audit(count_guard() == 1 &&
                          fury::length(guard()->position - spawn) < 1e-6f,
                      "Duplicate spawn changed active guard");
        float peak_speed = 0;
        double travel = 0;
        for (int i = 0; i < 72 && guard(); ++i) {
          tick();
          if (auto *a = guard()) {
            peak_speed = std::max(peak_speed, a->actual_speed);
            travel = a->travel_distance;
          }
        }
        require_audit(
            travel > 1 && peak_speed > 3.8f && peak_speed <= 4.001f,
            "Actual 4.0 m/s pursuit did not advance through live controller");
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
            app.camera().position.z += 4.5f / 60.f;
            tick();
          }
          require_audit(heist.phase() == fury::HeistPhase::Escape,
                        "Real looting flow failed to reach Escape");
        }
        require_audit(!complications.extra_guard_alive && count_guard() == 0,
                      "Actual terminal/escape transition retained guard agent");
        entity = app.scene().find_by_name("NpcExtraGuard");
        require_audit(entity && !entity->visible &&
                          entity->transform.position.y == -80.f,
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
        report << "{\"mission_index\":" << mission
               << ",\"mission_id\":" << std::quoted(mission_board.current().id)
               << ",\"cycle\":" << cycle << ",\"vault\":[" << vault.x << ','
               << vault.y << ',' << vault.z << "],\"spawn\":[" << spawn.x << ','
               << spawn.y << ',' << spawn.z
               << "],\"peak_actual_speed\":" << peak_speed
               << ",\"travel_distance\":" << travel
               << ",\"cleanup_phase\":" << std::quoted(cleanup)
               << ",\"cached_meshes_reused\":true,\"pose_reset_within_"
                  "contract\":true,\"maximum_pose_reset_error_m\":"
               << maximum_reset_error
               << ",\"pose_reset_tolerance_m\":0.000002,\"above_tolerance_"
                  "control_rejected\":true,\"locomotion_reset\":true,"
                  "\"material_reused\":true,\"mesh_count\":"
               << mesh_count << ",\"registered_actor_count\":" << actor_count
               << ",\"duplicate_spawn_idempotent\":true,\"agent_removed\":true,"
                  "\"entity_hidden\":true,\"reset_to_idle\":true}";
      }
    report << "],\"passed\":true}\n";
    report.flush();
    require_audit(report.good(), "Lifecycle report write failed");
    fury::Log::info(
        "PASS Metro Watch: 12 actual complication/spawn/chase/cleanup/reset "
        "cycles across six selected missions");
    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    fury::Log::error(std::string("Metro Watch lifecycle audit: ") + e.what());
    return EXIT_FAILURE;
  }
}
