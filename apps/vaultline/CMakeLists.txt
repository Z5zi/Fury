cmake_minimum_required(VERSION 3.16)

add_executable(vaultline main.cpp)

target_link_libraries(vaultline PRIVATE Fury::Engine)

if(MSVC)
  target_compile_options(vaultline PRIVATE /W4)
else()
  target_compile_options(vaultline PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(vaultline PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/apps/vaultline"
)
