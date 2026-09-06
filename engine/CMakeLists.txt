cmake_minimum_required(VERSION 3.16)

find_package(SDL2 REQUIRED)
find_package(OpenGL)

set(FURY_ENGINE_SOURCES
  src/log.cpp
  src/math.cpp
  src/transform.cpp
  src/mesh.cpp
  src/collision.cpp
  src/camera.cpp
  src/scene.cpp
  src/timer.cpp
  src/input.cpp
  src/window.cpp
  src/gl_loader.cpp
  src/gl_backend.cpp
  src/soft_backend.cpp
  src/renderer.cpp
  src/application.cpp
  src/heist.cpp
  src/inventory.cpp
  src/net_stub.cpp
  src/day_night.cpp
  src/npc.cpp
  src/audio.cpp
  src/heat.cpp
)

set(FURY_ENGINE_HEADERS
  include/fury/fury.hpp
  include/fury/platform.hpp
  include/fury/log.hpp
  include/fury/math.hpp
  include/fury/transform.hpp
  include/fury/mesh.hpp
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
  include/fury/audio.hpp
  include/fury/heat.hpp
  include/fury/mission.hpp
  include/fury/crew.hpp
)

add_library(fury_engine STATIC ${FURY_ENGINE_SOURCES} ${FURY_ENGINE_HEADERS})
add_library(Fury::Engine ALIAS fury_engine)

target_include_directories(fury_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(fury_engine PUBLIC cxx_std_17)

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
  target_compile_definitions(fury_engine PUBLIC FURY_PLATFORM_WINDOWS=1)
else()
  target_compile_definitions(fury_engine PUBLIC FURY_PLATFORM_LINUX=1)
endif()

# Optional SDL_mixer for audio cues (null backend always available)
find_package(SDL2_mixer QUIET)
if(TARGET SDL2_mixer::SDL2_mixer)
  target_link_libraries(fury_engine PUBLIC SDL2_mixer::SDL2_mixer)
  target_compile_definitions(fury_engine PUBLIC FURY_HAS_SDL_MIXER=1)
  message(STATUS "  SDL_mixer: FOUND")
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
    message(STATUS "  SDL_mixer: NOT FOUND (null audio)")
  endif()
endif()
