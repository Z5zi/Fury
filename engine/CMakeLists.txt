cmake_minimum_required(VERSION 3.16)

find_package(SDL2 REQUIRED)
find_package(OpenGL)
set_source_files_properties("${CMAKE_BINARY_DIR}/generated/build_info.cpp" PROPERTIES GENERATED TRUE)

set(FURY_ENGINE_SOURCES
  "${CMAKE_BINARY_DIR}/generated/build_info.cpp"
  src/log.cpp
  src/math.cpp
  src/transform.cpp
  src/mesh.cpp
  src/texture.cpp
  src/surface_detail.cpp
  src/material_sampling.cpp
  src/gltf.cpp
  src/collision.cpp
  src/camera.cpp
  src/scene.cpp
  src/timer.cpp
  src/input.cpp
  src/window.cpp
  src/gl_loader.cpp
  src/gl_backend.cpp
  src/soft_backend.cpp
  src/cpu_ray_backend.cpp
  src/renderer.cpp
  src/render_settings.cpp
  src/application.cpp
  src/visibility.cpp
  src/heist.cpp
  src/inventory.cpp
  src/net_loopback.cpp
  src/particles.cpp
  src/decals.cpp
  src/day_night.cpp
  src/npc.cpp
  src/character_model.cpp
  src/character_profile.cpp
  src/character_mira.cpp
  src/character_jon.cpp
  src/character_tessa.cpp
  src/character_owen.cpp
  src/character_lia.cpp
  src/character_nell.cpp
  src/character_pax.cpp
  src/character_rina.cpp
  src/character_hale.cpp
  src/character_vale.cpp
  src/character_metro_watch.cpp
  src/character_cass.cpp
  src/character_renn.cpp
  src/character_rook.cpp
  src/character_sparrow.cpp
  src/character_enforcer.cpp
  src/character_ghost.cpp
  src/character_player_body.cpp
  src/character_player_body_shirt.hpp
  src/character_player_body_equipment.inl
  src/character_ghost_shirt.hpp
  src/character_ghost_vest.hpp
  src/character_enforcer_uniform.hpp
  src/character_sparrow_shirt.hpp
  src/character_sparrow_pack.hpp
  src/character_rook_shirt.hpp
  src/character_rook_gear.hpp
  src/character_renn_equipment.hpp
  src/character_renn_shirt.hpp
  src/character_cass_jacket.hpp
  src/character_metro_watch_uniform.hpp
  src/character_vale_cap.hpp
  src/character_vale_shirt.hpp
  src/character_vale_vest.hpp
  src/character_hale_cap.hpp
  src/character_hale_shirt.hpp
  src/character_hale_vest.hpp
  src/character_rina_shirt.hpp
  src/character_rina_vest.hpp
  src/character_nell_shirt.hpp
  src/character_owen_jacket.hpp
  src/character_sculpt.hpp
  src/character_surface.cpp
  src/player_hair.cpp
  src/character_animation.cpp
  src/character_animation_generic.cpp
  src/character_animation_equipped_crouch.cpp
  src/audio.cpp
  src/audio_mixer.cpp
  src/heat.cpp
  src/pursuit.cpp
  src/traffic.cpp
)

set(FURY_ENGINE_HEADERS
  include/fury/fury.hpp
  include/fury/platform.hpp
  include/fury/log.hpp
  include/fury/math.hpp
  include/fury/transform.hpp
  include/fury/mesh.hpp
  include/fury/texture.hpp
  include/fury/collision.hpp
  include/fury/camera.hpp
  include/fury/scene.hpp
  include/fury/timer.hpp
  include/fury/input.hpp
  include/fury/window.hpp
  include/fury/renderer.hpp
  include/fury/gl_loader.hpp
  include/fury/application.hpp
  include/fury/heist.hpp
  include/fury/inventory.hpp
  include/fury/net.hpp
  include/fury/day_night.hpp
  include/fury/npc.hpp
  include/fury/character_model.hpp
  include/fury/character_animation.hpp
  include/fury/character_animation_trace.hpp
  include/fury/audio.hpp
  include/fury/audio_mixer.hpp
  include/fury/heat.hpp
  include/fury/pursuit.hpp
  include/fury/traffic.hpp
  include/fury/factions.hpp
  include/fury/mission.hpp
  include/fury/crew.hpp
  include/fury/banter.hpp
  include/fury/dialogue.hpp
  include/fury/particles.hpp
  include/fury/decals.hpp
  include/fury/weather.hpp
  include/fury/interior.hpp
  include/fury/security.hpp
  include/fury/crafting.hpp
  include/fury/complications.hpp
  include/fury/settings.hpp
  include/fury/player_hair.hpp
  include/fury/achievements.hpp
  include/fury/i18n.hpp
  include/fury/bitmap_font.hpp
)

add_library(fury_engine STATIC ${FURY_ENGINE_SOURCES} ${FURY_ENGINE_HEADERS})
add_library(Fury::Engine ALIAS fury_engine)
target_compile_definitions(fury_engine PUBLIC
  FURY_ENABLE_EXPERIMENTAL_EQUIPPED_CROUCH=$<BOOL:${FURY_ENABLE_EXPERIMENTAL_EQUIPPED_CROUCH}>)

include(${CMAKE_SOURCE_DIR}/cmake/FuryDX12.cmake)

target_include_directories(fury_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cgltf
)

target_compile_features(fury_engine PUBLIC cxx_std_17)
target_compile_definitions(fury_engine PUBLIC FURY_VERSION="${PROJECT_VERSION}")

if(MSVC)
  target_compile_options(fury_engine PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4>)
else()
  target_compile_options(fury_engine PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>)
endif()

# SDL2
if(TARGET SDL2::SDL2)
  target_link_libraries(fury_engine PUBLIC SDL2::SDL2)
  if(TARGET SDL2::SDL2main)
    target_link_libraries(fury_engine PUBLIC SDL2::SDL2main)
  endif()
else()
  target_include_directories(fury_engine PUBLIC ${SDL2_INCLUDE_DIRS})
  target_link_libraries(fury_engine PUBLIC ${SDL2_LIBRARIES})
endif()

# OpenGL (preferred path; software backend always available at runtime)
if(OpenGL_FOUND)
  target_link_libraries(fury_engine PUBLIC OpenGL::GL)
  target_compile_definitions(fury_engine PUBLIC FURY_HAS_OPENGL=1)
  message(STATUS "  OpenGL: FOUND")
else()
  target_compile_definitions(fury_engine PUBLIC FURY_HAS_OPENGL=0)
  message(STATUS "  OpenGL: NOT FOUND (software renderer only)")
endif()

if(FURY_HAS_ASM)
  set(FURY_ASM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/math/asm/fury_dot_x64.asm)
  target_sources(fury_engine PRIVATE ${FURY_ASM_SRC})
  set_source_files_properties(${FURY_ASM_SRC} PROPERTIES LANGUAGE ASM_NASM)
  if(WIN32)
    set_source_files_properties(${FURY_ASM_SRC} PROPERTIES COMPILE_FLAGS "-dWIN64")
  endif()
  target_compile_definitions(fury_engine PUBLIC FURY_HAS_ASM=1)
  message(STATUS "  Enabling fury_dot_x64.asm")
else()
  target_compile_definitions(fury_engine PUBLIC FURY_HAS_ASM=0)
endif()

if(WIN32)
  target_compile_definitions(fury_engine PUBLIC
    FURY_PLATFORM_WINDOWS=1
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
else()
  target_compile_definitions(fury_engine PUBLIC FURY_PLATFORM_LINUX=1)
endif()

# Optional SDL_mixer for audio cues (null backend always available)
find_package(SDL2_mixer QUIET)
if(TARGET SDL2_mixer::SDL2_mixer)
  target_link_libraries(fury_engine PUBLIC SDL2_mixer::SDL2_mixer)
  target_compile_definitions(fury_engine PUBLIC FURY_HAS_SDL_MIXER=1)
  message(STATUS "  SDL_mixer: FOUND")
elseif(TARGET SDL2_mixer::SDL2_mixer-static)
  target_link_libraries(fury_engine PUBLIC SDL2_mixer::SDL2_mixer-static)
  target_compile_definitions(fury_engine PUBLIC FURY_HAS_SDL_MIXER=1)
  message(STATUS "  SDL_mixer: FOUND (static target)")
elseif(SDL2_MIXER_FOUND OR SDL2_mixer_FOUND)
  if(SDL2_MIXER_INCLUDE_DIRS)
    target_include_directories(fury_engine PUBLIC ${SDL2_MIXER_INCLUDE_DIRS})
  endif()
  if(SDL2_MIXER_LIBRARIES)
    target_link_libraries(fury_engine PUBLIC ${SDL2_MIXER_LIBRARIES})
  endif()
  target_compile_definitions(fury_engine PUBLIC FURY_HAS_SDL_MIXER=1)
  message(STATUS "  SDL_mixer: FOUND (legacy vars)")
else()
  # pkg-config fallback
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SDL2_MIXER QUIET SDL2_mixer)
  endif()
  if(SDL2_MIXER_FOUND)
    target_include_directories(fury_engine PUBLIC ${SDL2_MIXER_INCLUDE_DIRS})
    target_link_libraries(fury_engine PUBLIC ${SDL2_MIXER_LIBRARIES})
    target_compile_definitions(fury_engine PUBLIC FURY_HAS_SDL_MIXER=1)
    message(STATUS "  SDL_mixer: FOUND (pkg-config)")
  else()
    target_compile_definitions(fury_engine PUBLIC FURY_HAS_SDL_MIXER=0)
    message(STATUS "  SDL_mixer: NOT FOUND (built-in CPU audio)")
  endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(fury_engine PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(fury_engine PUBLIC ws2_32)
endif()
