cmake_minimum_required(VERSION 3.16)
project(Fury VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FURY_BUILD_DEMO "Build the Fury demo application" ON)
option(FURY_BUILD_VAULTLINE "Build the Vaultline heist slice" ON)

# Detect x86_64 for optional assembly math kernels
set(FURY_HAS_ASM OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  enable_language(ASM_NASM)
  if(CMAKE_ASM_NASM_COMPILER)
    set(FURY_HAS_ASM ON)
  endif()
endif()

message(STATUS "Fury ${PROJECT_VERSION}")
message(STATUS "  ASM kernels: ${FURY_HAS_ASM}")

add_subdirectory(engine)

if(FURY_BUILD_DEMO)
  add_subdirectory(apps/demo)
endif()

if(FURY_BUILD_VAULTLINE)
  add_subdirectory(apps/vaultline)
endif()
