cmake_minimum_required(VERSION 3.16)

add_executable(vaultline main.cpp)

target_link_libraries(vaultline PRIVATE Fury::Engine)

if(MSVC)
  target_compile_options(vaultline PRIVATE /W4)
  target_compile_definitions(vaultline PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  target_compile_options(vaultline PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(vaultline PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/vaultline"
)

# 5.1.0–5.3.0 — ship OBJ props + albedo/normal textures beside the binary
add_custom_command(TARGET vaultline POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:vaultline>/assets/meshes"
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "$<TARGET_FILE_DIR:vaultline>/assets/textures"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/meshes"
          "$<TARGET_FILE_DIR:vaultline>/assets/meshes"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/textures"
          "$<TARGET_FILE_DIR:vaultline>/assets/textures"
  COMMENT "Copy Vaultline mesh + texture assets"
)
