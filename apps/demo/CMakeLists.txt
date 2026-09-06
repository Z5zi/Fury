cmake_minimum_required(VERSION 3.16)

add_executable(fury_demo main.cpp)

target_link_libraries(fury_demo PRIVATE Fury::Engine)

if(MSVC)
  target_compile_options(fury_demo PRIVATE /W4)
else()
  target_compile_options(fury_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Place binary under build/apps/demo for a predictable run path
set_target_properties(fury_demo PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/demo"
)
