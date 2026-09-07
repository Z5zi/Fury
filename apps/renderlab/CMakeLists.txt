add_executable(fury_renderlab main.cpp coastal_scene.cpp)
target_link_libraries(fury_renderlab PRIVATE Fury::Engine)
fury_stage_runtime(fury_renderlab)
set_target_properties(fury_renderlab PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/renderlab")
add_custom_command(TARGET fury_renderlab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:fury_renderlab>/assets")
