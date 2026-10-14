add_executable(fury_settings_panel_layout_tests settings_panel_layout_tests.cpp)
target_link_libraries(fury_settings_panel_layout_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_settings_panel_layout_tests)
add_test(NAME settings_panel_layout COMMAND fury_settings_panel_layout_tests)

# Headless public-API regression: compile the actual movement TUs directly.
# This target needs no renderer, SDL runtime, scene assets, or full engine build.
# The C++ math backend keeps the focused target portable on NASM-enabled hosts.
add_executable(fury_player_collision_sweep_tests
  player_collision_sweep_tests.cpp
  ../engine/src/collision.cpp
  ../engine/src/camera.cpp
  ../engine/src/math.cpp)
target_include_directories(fury_player_collision_sweep_tests PRIVATE
  "${PROJECT_SOURCE_DIR}/engine/include")
target_compile_features(fury_player_collision_sweep_tests PRIVATE cxx_std_17)
target_compile_definitions(fury_player_collision_sweep_tests PRIVATE FURY_HAS_ASM=0)
if(MSVC)
  target_compile_options(fury_player_collision_sweep_tests PRIVATE /W4)
else()
  target_compile_options(fury_player_collision_sweep_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME player_collision_sweep COMMAND fury_player_collision_sweep_tests)
set_tests_properties(player_collision_sweep PROPERTIES TIMEOUT 15 LABELS "collision;unit")

add_executable(fury_status_hud_tests status_hud_tests.cpp)
target_link_libraries(fury_status_hud_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_status_hud_tests)
add_test(NAME status_hud COMMAND fury_status_hud_tests)

add_executable(fury_render_tests render_tests.cpp)
target_link_libraries(fury_render_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_render_tests)
add_test(NAME rendering_math COMMAND fury_render_tests)
add_executable(fury_gltf_tests gltf_tests.cpp)
target_link_libraries(fury_gltf_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_gltf_tests)
add_test(NAME gltf_import COMMAND fury_gltf_tests)
add_executable(fury_settings_tests settings_tests.cpp)
target_link_libraries(fury_settings_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_settings_tests)
add_test(NAME rendering_settings COMMAND fury_settings_tests)

add_executable(fury_audio_tests audio_tests.cpp)
target_link_libraries(fury_audio_tests PRIVATE Fury::Engine)
target_compile_definitions(fury_audio_tests PRIVATE FURY_TEST_AUDIO_ASSETS="${PROJECT_SOURCE_DIR}/assets/audio/meridian")
fury_stage_runtime(fury_audio_tests)
add_test(NAME audio_cpu COMMAND fury_audio_tests)

add_executable(fury_software_raster_tests software_raster_tests.cpp)
target_link_libraries(fury_software_raster_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_software_raster_tests)
add_test(NAME software_rasterization COMMAND fury_software_raster_tests)

add_executable(fury_asset_pipeline_tests asset_pipeline_tests.cpp ../apps/vaultline/harbor_assets.cpp)
target_link_libraries(fury_asset_pipeline_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_asset_pipeline_tests)
add_test(NAME asset_pipeline COMMAND fury_asset_pipeline_tests)
set_tests_properties(asset_pipeline PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(fury_cpu_ray_tests cpu_ray_tests.cpp)
target_link_libraries(fury_cpu_ray_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_cpu_ray_tests)
add_test(NAME cpu_ray_tracing COMMAND fury_cpu_ray_tests)

if(TARGET fury_renderlab)
  add_test(NAME cpu_renderlab_runtime COMMAND fury_renderlab --backend cpu-ray
    --width 160 --height 90 --frames 2 --warmup 0 --hidden)
  set_tests_properties(cpu_renderlab_runtime PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;FURY_CPU_THREADS=2" TIMEOUT 60)
endif()
if(TARGET vaultline)
  add_test(NAME cpu_vaultline_runtime COMMAND vaultline --cpu-ray --smoke
    --width 160 --height 90 --frames 2)
  set_tests_properties(cpu_vaultline_runtime PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;FURY_CPU_THREADS=2;FURY_AUDIO_BACKEND=cpu"
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:vaultline>" TIMEOUT 90)
endif()

add_executable(fury_surface_detail_tests surface_detail_tests.cpp)
target_link_libraries(fury_surface_detail_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_surface_detail_tests)
add_test(NAME surface_detail COMMAND fury_surface_detail_tests)

add_executable(fury_surface_sampling_tests surface_sampling_tests.cpp)
target_link_libraries(fury_surface_sampling_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_surface_sampling_tests)
add_test(NAME surface_sampling COMMAND fury_surface_sampling_tests)

add_executable(fury_gl_material_tests gl_material_tests.cpp)
target_link_libraries(fury_gl_material_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_gl_material_tests)
find_program(FURY_XVFB_RUN xvfb-run)
if(UNIX AND FURY_XVFB_RUN)
  add_test(NAME gl_materials COMMAND ${FURY_XVFB_RUN} -a $<TARGET_FILE:fury_gl_material_tests>)
else()
  add_test(NAME gl_materials COMMAND fury_gl_material_tests)
endif()
set_tests_properties(gl_materials PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)

add_executable(fury_hero_surface_tests hero_surface_tests.cpp
  ../apps/vaultline/harbor_assets.cpp ../apps/renderlab/coastal_scene.cpp)
target_link_libraries(fury_hero_surface_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_hero_surface_tests)
add_test(NAME hero_surfaces COMMAND fury_hero_surface_tests)
set_tests_properties(hero_surfaces PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME dxr_planar_footprint COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/dxr_planar_footprint_tests.py)
endif()

add_executable(fury_surface_attachment_tests surface_attachment_tests.cpp)
target_link_libraries(fury_surface_attachment_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_surface_attachment_tests)
add_test(NAME surface_attachment COMMAND fury_surface_attachment_tests)

add_executable(fury_world_architecture_tests world_architecture_tests.cpp
  ../apps/vaultline/world_architecture.cpp)
target_link_libraries(fury_world_architecture_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_world_architecture_tests)
add_test(NAME world_architecture COMMAND fury_world_architecture_tests)

add_executable(fury_world_ground_tests world_ground_tests.cpp
  ../apps/vaultline/world_ground.cpp)
target_link_libraries(fury_world_ground_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_world_ground_tests)
add_test(NAME world_ground COMMAND fury_world_ground_tests)

add_executable(fury_world_life_tests world_life_tests.cpp
  ../apps/vaultline/world_life.cpp)
target_link_libraries(fury_world_life_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_world_life_tests)
add_test(NAME world_life COMMAND fury_world_life_tests)
set_tests_properties(world_life PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(fury_world_visibility_tests world_visibility_tests.cpp)
target_link_libraries(fury_world_visibility_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_world_visibility_tests)
add_test(NAME world_visibility COMMAND fury_world_visibility_tests)

add_executable(fury_interior_visibility_tests interior_visibility_tests.cpp
  ../apps/vaultline/npc_roster.cpp ../apps/vaultline/npc_presentation.cpp)
target_link_libraries(fury_interior_visibility_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_interior_visibility_tests)
add_test(NAME interior_visibility COMMAND fury_interior_visibility_tests)

if(Python3_Interpreter_FOUND AND TARGET vaultline)
  add_test(NAME whole_world_runtime COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/world_runtime_tests.py
    --vaultline $<TARGET_FILE:vaultline> --source-root ${CMAKE_SOURCE_DIR})
  set_tests_properties(whole_world_runtime PROPERTIES TIMEOUT 180 PROCESSORS 2)
endif()

add_executable(fury_world_water_tests world_water_tests.cpp
  ../apps/vaultline/world_water.cpp)
target_link_libraries(fury_world_water_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_world_water_tests)
add_test(NAME world_water COMMAND fury_world_water_tests)

add_executable(fury_world_audit_tests world_audit_tests.cpp
  ../apps/vaultline/world_audit.cpp)
target_link_libraries(fury_world_audit_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_world_audit_tests)
add_test(NAME world_audit COMMAND fury_world_audit_tests)

if(UNIX AND FURY_XVFB_RUN)
  add_test(NAME world_water_gl COMMAND ${FURY_XVFB_RUN} -a $<TARGET_FILE:fury_world_water_tests> --gl)
else()
  add_test(NAME world_water_gl COMMAND fury_world_water_tests --gl)
endif()
set_tests_properties(world_water_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)

foreach(test character_model character_animation npc_locomotion)
  add_executable(fury_${test}_tests ${test}_tests.cpp)
  target_link_libraries(fury_${test}_tests PRIVATE Fury::Engine)
  fury_stage_runtime(fury_${test}_tests)
  add_test(NAME ${test} COMMAND fury_${test}_tests)
endforeach()
add_executable(fury_npc_gameplay_tests npc_gameplay_tests.cpp
  ../apps/vaultline/npc_roster.cpp ../apps/vaultline/npc_interaction.cpp
  ../apps/vaultline/meridian_wishlist.cpp)
target_link_libraries(fury_npc_gameplay_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_npc_gameplay_tests)
add_test(NAME npc_gameplay COMMAND fury_npc_gameplay_tests)

add_executable(fury_npc_arrival_clearance_tests npc_arrival_clearance_tests.cpp
  ../apps/vaultline/npc_roster.cpp ../apps/vaultline/npc_presentation.cpp)
target_link_libraries(fury_npc_arrival_clearance_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_npc_arrival_clearance_tests)
add_test(NAME npc_arrival_clearance COMMAND fury_npc_arrival_clearance_tests)
set_tests_properties(npc_arrival_clearance PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy" TIMEOUT 180)

add_executable(fury_npc_dialogue_hud_tests npc_dialogue_hud_tests.cpp)
target_link_libraries(fury_npc_dialogue_hud_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_npc_dialogue_hud_tests)
add_test(NAME npc_dialogue_hud COMMAND fury_npc_dialogue_hud_tests)

add_executable(fury_application_capture_tests application_capture_tests.cpp)
target_link_libraries(fury_application_capture_tests PRIVATE Fury::Engine)
if(UNIX AND NOT APPLE)
  target_compile_definitions(fury_application_capture_tests PRIVATE FURY_TEST_CONTROLLED_TIMER)
  target_link_options(fury_application_capture_tests PRIVATE "-Wl,--wrap=SDL_GetTicks64")
endif()
fury_stage_runtime(fury_application_capture_tests)
add_test(NAME application_capture COMMAND fury_application_capture_tests)
set_tests_properties(application_capture PROPERTIES TIMEOUT 60)

add_executable(fury_npc_presentation_tests npc_presentation_tests.cpp
  ../apps/vaultline/npc_presentation.cpp)
target_link_libraries(fury_npc_presentation_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_npc_presentation_tests)
add_test(NAME npc_presentation COMMAND fury_npc_presentation_tests)

add_executable(fury_character_renderer_tests character_renderer_tests.cpp)
target_link_libraries(fury_character_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_renderer_tests)
add_test(NAME character_renderers COMMAND fury_character_renderer_tests)
if(UNIX AND FURY_XVFB_RUN)
  add_test(NAME character_renderers_gl COMMAND ${FURY_XVFB_RUN} -a $<TARGET_FILE:fury_character_renderer_tests> --gl)
else()
  add_test(NAME character_renderers_gl COMMAND fury_character_renderer_tests --gl)
endif()
set_tests_properties(character_renderers character_renderers_gl PROPERTIES TIMEOUT 90)
set_tests_properties(character_renderers_gl PROPERTIES SKIP_RETURN_CODE 77)

if(Python3_Interpreter_FOUND AND TARGET vaultline)
  add_test(NAME npc_runtime COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/npc_runtime_tests.py
    --vaultline $<TARGET_FILE:vaultline> --source-root ${CMAKE_SOURCE_DIR})
  set_tests_properties(npc_runtime PROPERTIES TIMEOUT 240 PROCESSORS 2)
endif()

foreach(test character_surface character_lod)
  add_executable(fury_${test}_tests ${test}_tests.cpp)
  target_link_libraries(fury_${test}_tests PRIVATE Fury::Engine)
  fury_stage_runtime(fury_${test}_tests)
  add_test(NAME ${test} COMMAND fury_${test}_tests)
endforeach()

add_executable(fury_character_profile_tests character_profile_tests.cpp)
target_link_libraries(fury_character_profile_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_profile_tests)
add_test(NAME character_profile COMMAND fury_character_profile_tests)

add_executable(fury_character_jon_tests character_jon_tests.cpp)
target_link_libraries(fury_character_jon_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_jon_tests)
add_test(NAME character_jon COMMAND fury_character_jon_tests)

add_executable(fury_character_skinning_tests character_skinning_tests.cpp)
target_link_libraries(fury_character_skinning_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_skinning_tests)
add_test(NAME character_skinning COMMAND fury_character_skinning_tests)

add_test(NAME character_surface_gl COMMAND fury_character_surface_tests --gl)
set_tests_properties(character_surface_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)

add_executable(fury_character_tessa_tests character_tessa_tests.cpp)
target_link_libraries(fury_character_tessa_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_tessa_tests)
add_test(NAME character_tessa COMMAND fury_character_tessa_tests)

add_executable(fury_character_owen_tests character_owen_tests.cpp)
target_link_libraries(fury_character_owen_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_owen_tests)
add_test(NAME character_owen COMMAND fury_character_owen_tests)
set_tests_properties(character_owen PROPERTIES TIMEOUT 600 PROCESSORS 2)

add_executable(fury_character_lia_tests character_lia_tests.cpp)
target_link_libraries(fury_character_lia_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_lia_tests)
add_test(NAME character_lia COMMAND fury_character_lia_tests)
set_tests_properties(character_lia PROPERTIES TIMEOUT 600 PROCESSORS 2)

add_executable(fury_teller_placement_tests teller_placement_tests.cpp
  ../apps/vaultline/npc_roster.cpp ../apps/vaultline/npc_presentation.cpp
  ../apps/vaultline/npc_interaction.cpp ../apps/vaultline/harbor_assets.cpp
  ../apps/vaultline/meridian_wishlist.cpp)
target_link_libraries(fury_teller_placement_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_teller_placement_tests)
add_test(NAME teller_placement COMMAND fury_teller_placement_tests)
set_tests_properties(teller_placement PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 90 PROCESSORS 1)

add_executable(fury_character_nell_tests character_nell_tests.cpp)
target_link_libraries(fury_character_nell_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_nell_tests)
add_test(NAME character_nell COMMAND fury_character_nell_tests)
set_tests_properties(character_nell PROPERTIES TIMEOUT 600 PROCESSORS 2)
if(Python3_Interpreter_FOUND)
  add_test(NAME nell_anatomy_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_nell_anatomy.py --check)
  add_test(NAME nell_hands_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_nell_hands.py --check)
  set_tests_properties(nell_hands_source PROPERTIES TIMEOUT 120)
endif()

add_executable(fury_character_pax_tests character_pax_tests.cpp)
target_link_libraries(fury_character_pax_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_pax_tests)
add_test(NAME character_pax COMMAND fury_character_pax_tests)
set_tests_properties(character_pax PROPERTIES TIMEOUT 600 PROCESSORS 2)

add_executable(character_rina_tests character_rina_tests.cpp)
target_link_libraries(character_rina_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_rina_tests)
add_test(NAME character_rina_tests COMMAND character_rina_tests)
if(Python3_Interpreter_FOUND)
  add_test(NAME rina_anatomy_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/rina_anatomy_source_tests.py)
  add_test(NAME rina_anatomy_reproducibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_rina_anatomy.py --check)
  add_test(NAME rina_hands_reproducibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_rina_hands.py --check)
endif()
add_executable(character_rina_contact_tests character_rina_contact_tests.cpp)
target_link_libraries(character_rina_contact_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_rina_contact_tests)
add_test(NAME character_rina_contact_tests COMMAND character_rina_contact_tests)
add_executable(character_rina_renderer_tests character_rina_renderer_tests.cpp)
target_link_libraries(character_rina_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_rina_renderer_tests)
add_test(NAME character_rina_renderers COMMAND character_rina_renderer_tests)
add_test(NAME character_rina_renderers_gl COMMAND character_rina_renderer_tests --gl)
set_tests_properties(character_rina_renderers_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
set_tests_properties(character_rina_renderers PROPERTIES TIMEOUT 90)
if(Python3_Interpreter_FOUND AND TARGET fury_characterlab)
  add_test(NAME characterlab_attention COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/characterlab_attention_tests.py
    --characterlab $<TARGET_FILE:fury_characterlab>
    --output-dir ${CMAKE_CURRENT_BINARY_DIR}/characterlab-attention)
endif()

add_executable(fury_character_hale_tests character_hale_tests.cpp)
target_link_libraries(fury_character_hale_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_hale_tests)
add_test(NAME character_hale COMMAND fury_character_hale_tests)
add_test(NAME character_hale_ordinary COMMAND fury_character_hale_tests --ordinary)

add_executable(fury_character_vale_tests character_vale_tests.cpp)
target_link_libraries(fury_character_vale_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_vale_tests)
add_test(NAME character_vale COMMAND fury_character_vale_tests)
set_tests_properties(character_vale PROPERTIES TIMEOUT 600 PROCESSORS 2)

add_executable(fury_character_vale_renderer_tests character_vale_renderer_tests.cpp)
target_link_libraries(fury_character_vale_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_vale_renderer_tests)
add_test(NAME character_vale_renderers COMMAND fury_character_vale_renderer_tests)
add_test(NAME character_vale_renderers_gl COMMAND fury_character_vale_renderer_tests --gl)
set_tests_properties(character_vale_renderers character_vale_renderers_gl PROPERTIES TIMEOUT 600 PROCESSORS 2)
set_tests_properties(character_vale_renderers_gl PROPERTIES SKIP_RETURN_CODE 77)

# Opt-in cost probe, not a thresholded timing test or part of normal CTest.
add_executable(fury_character_vale_benchmark EXCLUDE_FROM_ALL character_vale_benchmark.cpp)
target_link_libraries(fury_character_vale_benchmark PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_vale_benchmark)

add_executable(fury_character_metro_watch_tests character_metro_watch_tests.cpp)
target_link_libraries(fury_character_metro_watch_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_metro_watch_tests)
add_test(NAME character_metro_watch COMMAND fury_character_metro_watch_tests)
set_tests_properties(character_metro_watch PROPERTIES TIMEOUT 600 PROCESSORS 1)

if(Python3_Interpreter_FOUND AND FURY_BUILD_VAULTLINE)
  add_test(NAME metro_watch_lifecycle COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/metro_watch_lifecycle_tests.py
    --vaultline $<TARGET_FILE:vaultline> --source-root ${CMAKE_SOURCE_DIR}
    --output-dir ${CMAKE_BINARY_DIR}/metro-watch-lifecycle)
  set_tests_properties(metro_watch_lifecycle PROPERTIES TIMEOUT 200 PROCESSORS 1)
endif()


# The default character_metro_watch gate remains the complete strict stress test.
# This additional gate isolates ordinary states without turning a stress failure
# into a pass or changing its geometric thresholds.
add_test(NAME character_metro_watch_ordinary COMMAND fury_character_metro_watch_tests --ordinary)
set_tests_properties(character_metro_watch_ordinary PROPERTIES TIMEOUT 600 PROCESSORS 1)

add_executable(fury_character_cass_tests character_cass_tests.cpp)
target_link_libraries(fury_character_cass_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_cass_tests)
add_test(NAME character_cass COMMAND fury_character_cass_tests)
set_tests_properties(character_cass PROPERTIES TIMEOUT 600 PROCESSORS 1)

add_executable(fury_character_cass_renderer_tests character_cass_renderer_tests.cpp)
target_link_libraries(fury_character_cass_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_cass_renderer_tests)
add_test(NAME character_cass_renderers COMMAND fury_character_cass_renderer_tests)
add_test(NAME character_cass_renderers_gl COMMAND fury_character_cass_renderer_tests --gl)
set_tests_properties(character_cass_renderers character_cass_renderers_gl PROPERTIES TIMEOUT 120 PROCESSORS 1)
set_tests_properties(character_cass_renderers_gl PROPERTIES SKIP_RETURN_CODE 77)

# Retain the failing strict default; separately expose its documented ordinary scope.
add_test(NAME character_cass_ordinary_geometry COMMAND fury_character_cass_tests --ordinary-geometry)
set_tests_properties(character_cass_ordinary_geometry PROPERTIES TIMEOUT 600 PROCESSORS 1)

add_executable(fury_character_renn_tests character_renn_tests.cpp)
target_link_libraries(fury_character_renn_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_renn_tests)
add_test(NAME character_renn COMMAND fury_character_renn_tests)
set_tests_properties(character_renn PROPERTIES TIMEOUT 900 PROCESSORS 2)

# Separate ordinary coverage; the complete strict default remains unchanged.
add_test(NAME character_renn_ordinary COMMAND fury_character_renn_tests --ordinary)
set_tests_properties(character_renn_ordinary PROPERTIES TIMEOUT 900 PROCESSORS 2)

# Rook-owned individual asset regression, independent pinned-data reconstruction,
# and exact production renderer coverage. Contact stress scope is separate.
add_executable(fury_character_rook_tests character_rook_tests.cpp)
target_link_libraries(fury_character_rook_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_rook_tests)
add_test(NAME character_rook_ordinary COMMAND fury_character_rook_tests --ordinary)
set_tests_properties(character_rook_ordinary PROPERTIES TIMEOUT 300)
add_executable(fury_character_rook_renderer_tests character_rook_renderer_tests.cpp)
target_link_libraries(fury_character_rook_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_rook_renderer_tests)
add_test(NAME character_rook_renderers COMMAND fury_character_rook_renderer_tests)
set_tests_properties(character_rook_renderers PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;FURY_CPU_THREADS=2;LP_NUM_THREADS=2" TIMEOUT 180)
if(Python3_Interpreter_FOUND)
  add_test(NAME rook_anatomy_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/rook_anatomy_source_tests.py)
endif()

add_executable(fury_character_rook_contact_tests character_rook_contact_tests.cpp)
target_link_libraries(fury_character_rook_contact_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_rook_contact_tests)
add_test(NAME character_rook_contact_ordinary COMMAND fury_character_rook_contact_tests --ordinary)
set_tests_properties(character_rook_contact_ordinary PROPERTIES TIMEOUT 300)
# Strict scopes are also registered below; their failures are retained.

add_test(NAME character_rook_gl COMMAND fury_character_rook_renderer_tests --gl)
set_tests_properties(character_rook_gl PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_AUDIODRIVER=dummy;LIBGL_ALWAYS_SOFTWARE=1;FURY_CPU_THREADS=2;LP_NUM_THREADS=2" TIMEOUT 180)

add_test(NAME character_rook_strict_geometry COMMAND fury_character_rook_tests --strict)
add_test(NAME character_rook_contact_strict COMMAND fury_character_rook_contact_tests --strict)
set_tests_properties(character_rook_strict_geometry character_rook_contact_strict PROPERTIES TIMEOUT 300)
if(Python3_Interpreter_FOUND AND FURY_BUILD_VAULTLINE)
  foreach(crew_actor IN ITEMS CrewRook CrewSparrow)
    add_test(NAME crew_follow_${crew_actor} COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/crew_follow_capture_tests.py
      --vaultline $<TARGET_FILE:vaultline> --source-root ${CMAKE_SOURCE_DIR}
      --actor ${crew_actor} --width 320 --height 240
      --output-dir ${CMAKE_BINARY_DIR}/crew-follow-${crew_actor})
    set_tests_properties(crew_follow_${crew_actor} PROPERTIES TIMEOUT 200 PROCESSORS 1)
  endforeach()
endif()

add_executable(character_sparrow_tests character_sparrow_tests.cpp)
target_link_libraries(character_sparrow_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_sparrow_tests)
add_test(NAME character_sparrow COMMAND character_sparrow_tests)
set_tests_properties(character_sparrow PROPERTIES TIMEOUT 600 PROCESSORS 1)
add_executable(character_sparrow_renderer_tests character_sparrow_renderer_tests.cpp)
target_link_libraries(character_sparrow_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_sparrow_renderer_tests)
add_test(NAME character_sparrow_renderers COMMAND character_sparrow_renderer_tests)
add_test(NAME character_sparrow_renderers_gl COMMAND character_sparrow_renderer_tests --gl)
set_tests_properties(character_sparrow_renderers_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
set_tests_properties(character_sparrow_renderers PROPERTIES TIMEOUT 90)

add_executable(character_sparrow_contact_tests character_sparrow_contact_tests.cpp)
target_link_libraries(character_sparrow_contact_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_sparrow_contact_tests)
add_test(NAME character_sparrow_contact_ordinary COMMAND character_sparrow_contact_tests --ordinary)
set_tests_properties(character_sparrow_contact_ordinary PROPERTIES TIMEOUT 600 PROCESSORS 1)
# The executable's unchanged default is the strict scope, including the shared
# extreme crouch-walk witness. Run and report it separately; never WILL_FAIL it.
if(Python3_Interpreter_FOUND)
  add_test(NAME sparrow_anatomy_reproducibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_sparrow_anatomy.py --check)
  add_test(NAME sparrow_hands_reproducibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_sparrow_hands.py --check)
endif()

# The full strict contact scope remains an ordinary failing gate when assertions fail.
add_test(NAME character_sparrow_contact_strict COMMAND character_sparrow_contact_tests)
set_tests_properties(character_sparrow_contact_strict PROPERTIES TIMEOUT 600 PROCESSORS 1)

# Isolated Syndicate Enforcer exact-profile and source checks.
add_executable(fury_character_enforcer_tests character_enforcer_tests.cpp)
target_link_libraries(fury_character_enforcer_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_enforcer_tests)
add_test(NAME character_enforcer COMMAND fury_character_enforcer_tests)
add_test(NAME character_enforcer_ordinary COMMAND fury_character_enforcer_tests --ordinary)
add_executable(fury_character_enforcer_renderer_tests character_enforcer_renderer_tests.cpp)
target_link_libraries(fury_character_enforcer_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_enforcer_renderer_tests)
add_test(NAME character_enforcer_renderers COMMAND fury_character_enforcer_renderer_tests)
add_test(NAME character_enforcer_renderers_gl COMMAND fury_character_enforcer_renderer_tests --gl)
set_tests_properties(character_enforcer_renderers_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
set_tests_properties(character_enforcer_renderers PROPERTIES TIMEOUT 90)
if(Python3_Interpreter_FOUND)
 add_test(NAME enforcer_anatomy_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_enforcer_anatomy.py --check)
 add_test(NAME enforcer_hands_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_enforcer_hands.py --check)
endif()

if(Python3_Interpreter_FOUND)
 add_test(NAME enforcer_hair_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_enforcer_hair.py --check)
endif()

if(Python3_Interpreter_FOUND AND TARGET vaultline)
 add_test(NAME enforcer_lifecycle COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/enforcer_lifecycle_tests.py --vaultline $<TARGET_FILE:vaultline> --source-root ${CMAKE_SOURCE_DIR} --output-dir ${CMAKE_CURRENT_BINARY_DIR}/enforcer-lifecycle)
 set_tests_properties(enforcer_lifecycle PROPERTIES TIMEOUT 180)
endif()

# GhostLoop individual art and production-renderer contracts.
add_executable(character_ghost_tests character_ghost_tests.cpp)
target_link_libraries(character_ghost_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_ghost_tests)
add_test(NAME character_ghost_tests COMMAND character_ghost_tests)
add_executable(character_ghost_contact_tests character_ghost_contact_tests.cpp)
target_link_libraries(character_ghost_contact_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_ghost_contact_tests)
add_test(NAME character_ghost_contact_tests COMMAND character_ghost_contact_tests)
add_executable(character_ghost_renderer_tests character_ghost_renderer_tests.cpp)
target_link_libraries(character_ghost_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_ghost_renderer_tests)
add_test(NAME character_ghost_renderers COMMAND character_ghost_renderer_tests)
add_test(NAME character_ghost_renderers_gl COMMAND character_ghost_renderer_tests --gl)
set_tests_properties(character_ghost_renderers_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
set_tests_properties(character_ghost_renderers PROPERTIES TIMEOUT 90)
if(Python3_Interpreter_FOUND)
 add_test(NAME ghost_anatomy_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ghost_anatomy_source_tests.py)
 add_test(NAME ghost_anatomy_reproducibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_ghost_anatomy.py --check)
 add_test(NAME ghost_hands_reproducibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_ghost_hands.py --check)
endif()

if(UNIX)
 add_executable(ghost_loopback_peer ghost_loopback_peer.cpp)
 target_link_libraries(ghost_loopback_peer PRIVATE Fury::Engine)
endif()
if(UNIX)
 add_executable(ghost_network_protocol_tests ghost_network_protocol_tests.cpp)
 target_link_libraries(ghost_network_protocol_tests PRIVATE Fury::Engine)
 add_test(NAME ghost_network_protocol COMMAND ghost_network_protocol_tests)
endif()

# Ordinary contacts are reported independently of the retained strict gate.
add_test(NAME character_ghost_contact_ordinary COMMAND character_ghost_contact_tests --ordinary)

add_executable(character_player_body_tests character_player_body_tests.cpp)
target_link_libraries(character_player_body_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_player_body_tests)
add_test(NAME character_player_body_tests COMMAND character_player_body_tests)
if(Python3_Interpreter_FOUND)
  add_test(NAME player_body_anatomy_source COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/player_body_anatomy_source_tests.py)
  add_test(NAME player_body_anatomy_reproducibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_player_body_anatomy.py --check)
  add_test(NAME player_body_hands_reproducibility COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_player_body_hands.py --check)
endif()

add_executable(character_player_body_renderer_tests character_player_body_renderer_tests.cpp)
target_link_libraries(character_player_body_renderer_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_player_body_renderer_tests)
add_test(NAME character_player_body_renderers COMMAND character_player_body_renderer_tests)
add_test(NAME character_player_body_renderers_gl COMMAND character_player_body_renderer_tests --gl)
set_tests_properties(character_player_body_renderers_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
set_tests_properties(character_player_body_renderers PROPERTIES TIMEOUT 90)

add_executable(character_player_body_contact_tests character_player_body_contact_tests.cpp)
target_link_libraries(character_player_body_contact_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_player_body_contact_tests)
add_test(NAME character_player_body_contacts COMMAND character_player_body_contact_tests)
add_test(NAME character_player_body_contacts_strict_stress COMMAND character_player_body_contact_tests --stress)

add_executable(player_input_contract_tests player_input_contract_tests.cpp)
target_link_libraries(player_input_contract_tests PRIVATE Fury::Engine)
fury_stage_runtime(player_input_contract_tests)
add_test(NAME player_input_contract COMMAND player_input_contract_tests)
set_tests_properties(player_input_contract PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

add_executable(character_player_body_benchmark character_player_body_benchmark.cpp)
target_link_libraries(character_player_body_benchmark PRIVATE Fury::Engine)
fury_stage_runtime(character_player_body_benchmark)

# Deliberate stopped-crouch settling and interrupted recovery contracts.
foreach(test character_gait_settling character_gait_interrupt)
  add_executable(fury_${test}_tests ${test}_tests.cpp)
  target_link_libraries(fury_${test}_tests PRIVATE Fury::Engine)
  fury_stage_runtime(fury_${test}_tests)
  add_test(NAME ${test} COMMAND fury_${test}_tests)
  set_tests_properties(${test} PROPERTIES TIMEOUT 180 PROCESSORS 1)
endforeach()

# Independent frozen-reference proof for motion-profile dispatch.
add_executable(fury_character_animation_profile_tests
  character_animation_profile_tests.cpp
  fixtures/character_animation_generic_reference.cpp
  ../apps/vaultline/npc_presentation.cpp)
target_link_libraries(fury_character_animation_profile_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_character_animation_profile_tests)
add_test(NAME character_animation_profiles COMMAND fury_character_animation_profile_tests)
set_tests_properties(character_animation_profiles PROPERTIES TIMEOUT 180 PROCESSORS 1)
if(Python3_Interpreter_FOUND AND TARGET vaultline AND TARGET fury_characterlab)
  if(FURY_ENABLE_EXPERIMENTAL_EQUIPPED_CROUCH)
    set(FURY_EXPECTED_PLAYER_MOTION_PROFILE equipped_crouch_experimental)
  else()
    set(FURY_EXPECTED_PLAYER_MOTION_PROFILE generic)
  endif()
  add_test(NAME character_animation_profile_runtime
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/character_animation_profile_runtime_tests.py
      --characterlab $<TARGET_FILE:fury_characterlab> --vaultline $<TARGET_FILE:vaultline>
      --output-dir ${CMAKE_CURRENT_BINARY_DIR}/profile-runtime
      --expected-player-profile ${FURY_EXPECTED_PLAYER_MOTION_PROFILE})
  set_tests_properties(character_animation_profile_runtime PROPERTIES TIMEOUT 180 PROCESSORS 1)
endif()

# Default-off Player source groom and scoped material support.
add_executable(fury_color_detail_tests color_detail_tests.cpp)
target_link_libraries(fury_color_detail_tests PRIVATE Fury::Engine)
if(UNIX AND NOT APPLE)
  target_compile_definitions(fury_color_detail_tests PRIVATE FURY_TEST_WRAP_GL=1)
  target_link_options(fury_color_detail_tests PRIVATE "-Wl,--wrap=SDL_GL_GetProcAddress")
endif()
fury_stage_runtime(fury_color_detail_tests)
add_test(NAME color_detail_cpu COMMAND fury_color_detail_tests)
if(UNIX AND FURY_XVFB_RUN)
  add_test(NAME color_detail_gl COMMAND ${FURY_XVFB_RUN} -a $<TARGET_FILE:fury_color_detail_tests> --gl)
else()
  add_test(NAME color_detail_gl COMMAND fury_color_detail_tests --gl)
endif()
set_tests_properties(color_detail_gl PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)


add_executable(character_player_hair_integration_tests character_player_hair_integration_tests.cpp)
target_link_libraries(character_player_hair_integration_tests PRIVATE Fury::Engine)
fury_stage_runtime(character_player_hair_integration_tests)
foreach(hair_case IN ITEMS fallback source tail tail512 tail1024)
  add_test(NAME player_hair_${hair_case} COMMAND character_player_hair_integration_tests)
endforeach()
set_tests_properties(player_hair_fallback PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=0;FURY_PLAYER_SHORT_TAIL=0;FURY_PLAYER_HAIR_DETAIL=0")
set_tests_properties(player_hair_source PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=0;FURY_PLAYER_HAIR_DETAIL=0")
set_tests_properties(player_hair_tail PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=1;FURY_PLAYER_HAIR_DETAIL=0")
set_tests_properties(player_hair_tail512 PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=1;FURY_PLAYER_HAIR_DETAIL=512")
set_tests_properties(player_hair_tail1024 PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=1;FURY_PLAYER_HAIR_DETAIL=1024")
add_test(NAME character_player_source_hair COMMAND character_player_body_tests)
set_tests_properties(character_player_source_hair PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=0;FURY_PLAYER_HAIR_DETAIL=0")
add_test(NAME player_short_tail_body COMMAND character_player_body_tests)
set_tests_properties(player_short_tail_body PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=1;FURY_PLAYER_HAIR_DETAIL=0")
if(Python3_Interpreter_FOUND)
  add_test(NAME player_body_hair_generated COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_player_body_hair.py --check)
  add_test(NAME player_short_tail_generated COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/build_player_short_tail.py --check)
endif()
add_executable(player_short_tail_validation player_short_tail_validation.cpp)
target_link_libraries(player_short_tail_validation PRIVATE Fury::Engine)
fury_stage_runtime(player_short_tail_validation)
add_test(NAME player_short_tail_validation COMMAND player_short_tail_validation)
set_tests_properties(player_short_tail_validation PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=1;FURY_PLAYER_HAIR_DETAIL=512" TIMEOUT 1200)
foreach(hair_invalid IN ITEMS source tail dependency detail detail_dependency)
  add_test(NAME player_hair_invalid_${hair_invalid} COMMAND character_player_hair_integration_tests --expect-invalid)
endforeach()
set_tests_properties(player_hair_invalid_source PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=2;FURY_PLAYER_SHORT_TAIL=0;FURY_PLAYER_HAIR_DETAIL=0")
set_tests_properties(player_hair_invalid_tail PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=2;FURY_PLAYER_HAIR_DETAIL=0")
set_tests_properties(player_hair_invalid_dependency PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=0;FURY_PLAYER_SHORT_TAIL=1;FURY_PLAYER_HAIR_DETAIL=0")
set_tests_properties(player_hair_invalid_detail PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=1;FURY_PLAYER_SHORT_TAIL=0;FURY_PLAYER_HAIR_DETAIL=511")
set_tests_properties(player_hair_invalid_detail_dependency PROPERTIES ENVIRONMENT "FURY_PLAYER_SOURCE_HAIR=0;FURY_PLAYER_SHORT_TAIL=0;FURY_PLAYER_HAIR_DETAIL=512")

add_executable(player_hair_policy_tests player_hair_policy_tests.cpp)
target_link_libraries(player_hair_policy_tests PRIVATE Fury::Engine)
fury_stage_runtime(player_hair_policy_tests)
add_test(NAME player_hair_policy COMMAND player_hair_policy_tests)

# The exact app UI with a bounded test-only input/capture observer. GNU link-wrap
# simulates SDL keyboard state; controller coverage uses SDL virtual hardware.
if(UNIX AND NOT APPLE AND FURY_BUILD_VAULTLINE)
  file(READ "${PROJECT_SOURCE_DIR}/apps/vaultline/main.cpp" FURY_HAIR_UI_MAIN)
  string(PREPEND FURY_HAIR_UI_MAIN "#include \"player_hair_settings_ui_input.hpp\"\n")
  string(REPLACE "  if((!metro_watch_lifecycle_path.empty())+" "  #include \"player_hair_settings_ui_audit.inl\"\n  if((!metro_watch_lifecycle_path.empty())+" FURY_HAIR_UI_MAIN "${FURY_HAIR_UI_MAIN}")
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/generated/player_hair_settings_ui_main.cpp" CONTENT "${FURY_HAIR_UI_MAIN}")
  get_target_property(FURY_HAIR_UI_SOURCES vaultline SOURCES)
  list(REMOVE_ITEM FURY_HAIR_UI_SOURCES main.cpp)
  list(TRANSFORM FURY_HAIR_UI_SOURCES PREPEND "${PROJECT_SOURCE_DIR}/apps/vaultline/")
  add_executable(player_hair_settings_ui_tests "${CMAKE_BINARY_DIR}/generated/player_hair_settings_ui_main.cpp"
    player_hair_settings_ui_input.cpp ${FURY_HAIR_UI_SOURCES})
  target_include_directories(player_hair_settings_ui_tests PRIVATE "${PROJECT_SOURCE_DIR}/apps/vaultline" "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(player_hair_settings_ui_tests PRIVATE Fury::Engine)
  target_link_options(player_hair_settings_ui_tests PRIVATE "-Wl,--wrap=SDL_GetKeyboardState")
  fury_stage_runtime(player_hair_settings_ui_tests)
endif()
add_executable(player_hair_identity_tests player_hair_identity_tests.cpp)
target_link_libraries(player_hair_identity_tests PRIVATE Fury::Engine)
fury_stage_runtime(player_hair_identity_tests)
if(Python3_Interpreter_FOUND)
  add_test(NAME player_hair_identity COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/player_hair_identity_tests.py $<TARGET_FILE:player_hair_identity_tests>)
endif()
if(TARGET player_hair_settings_ui_tests AND Python3_Interpreter_FOUND)
  add_test(NAME player_hair_settings_ui COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/player_hair_settings_ui_tests.py
    $<TARGET_FILE:player_hair_settings_ui_tests> $<TARGET_FILE:vaultline> "${CMAKE_BINARY_DIR}/hair-settings-ui")
  set_tests_properties(player_hair_settings_ui PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
endif()
if(FURY_BUILD_VAULTLINE AND Python3_Interpreter_FOUND)
  add_test(NAME player_hair_renderer_identity COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/player_hair_renderer_identity.py
    $<TARGET_FILE:vaultline> "${CMAKE_BINARY_DIR}/hair-renderer-identity")
  set_tests_properties(player_hair_renderer_identity PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
endif()
if(UNIX)
  add_executable(player_hair_input_tests player_hair_input_tests.cpp)
  target_link_libraries(player_hair_input_tests PRIVATE Fury::Engine)
  fury_stage_runtime(player_hair_input_tests)
  add_test(NAME player_hair_input COMMAND player_hair_input_tests)
  set_tests_properties(player_hair_input PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
endif()

add_executable(fury_patrol_arrival_clearance_tests patrol_arrival_clearance_tests.cpp
  ../apps/vaultline/harbor_assets.cpp)
target_link_libraries(fury_patrol_arrival_clearance_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_patrol_arrival_clearance_tests)
add_test(NAME patrol_arrival_clearance COMMAND fury_patrol_arrival_clearance_tests)
set_tests_properties(patrol_arrival_clearance PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 90)

add_executable(fury_mission_board_hud_tests mission_board_hud_tests.cpp)
target_link_libraries(fury_mission_board_hud_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_mission_board_hud_tests)
add_test(NAME mission_board_hud COMMAND fury_mission_board_hud_tests)

add_executable(fury_map_hud_tests map_hud_tests.cpp)
target_link_libraries(fury_map_hud_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_map_hud_tests)
add_test(NAME map_hud COMMAND fury_map_hud_tests)

add_executable(fury_fence_hud_tests fence_hud_tests.cpp)
target_link_libraries(fury_fence_hud_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_fence_hud_tests)
add_test(NAME fence_hud COMMAND fury_fence_hud_tests)

add_executable(fury_inventory_hud_tests inventory_hud_tests.cpp)
target_link_libraries(fury_inventory_hud_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_inventory_hud_tests)
add_test(NAME inventory_hud COMMAND fury_inventory_hud_tests)

add_executable(fury_journal_hud_tests journal_hud_tests.cpp)
target_link_libraries(fury_journal_hud_tests PRIVATE Fury::Engine)
fury_stage_runtime(fury_journal_hud_tests)
add_test(NAME journal_hud COMMAND fury_journal_hud_tests)
