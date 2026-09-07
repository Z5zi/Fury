set(FURY_DXC_ROOT "" CACHE PATH "Extracted official DXC distribution")
set(FURY_FSR_ROOT "" CACHE PATH "AMD FSR SDK repository root")
set(FURY_XESS_ROOT "" CACHE PATH "Intel XeSS SDK root")

function(fury_stage_runtime target)
  if(WIN32 AND TARGET SDL2::SDL2)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:${target}>")
  endif()
  if(FURY_ENABLE_DX12 AND WIN32)
    if(FURY_FSR_ROOT)
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FURY_FSR_ROOT}/docs/license.md"
          "$<TARGET_FILE_DIR:${target}>/LICENSE-AMD-FSR.txt")
      foreach(dll amd_fidelityfx_loader_dx12.dll amd_fidelityfx_upscaler_dx12.dll)
        add_custom_command(TARGET ${target} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FURY_FSR_ROOT}/Kits/FidelityFX/signedbin/${dll}"
            "$<TARGET_FILE_DIR:${target}>")
      endforeach()
    endif()
    if(FURY_XESS_ROOT)
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${FURY_XESS_ROOT}/bin/libxess.dll" "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FURY_XESS_ROOT}/LICENSE.txt"
          "$<TARGET_FILE_DIR:${target}>/LICENSE-Intel-XeSS.txt")
    endif()
  endif()
endfunction()

if(NOT FURY_ENABLE_DX12)
  target_compile_definitions(fury_engine PRIVATE FURY_HAS_DX12=0)
  return()
endif()
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "FURY_ENABLE_DX12 currently requires 64-bit Windows")
endif()
find_program(FURY_DXC dxc HINTS "${FURY_DXC_ROOT}/bin/x64")
if(NOT FURY_DXC)
  message(FATAL_ERROR "DX12 requires the DirectX Shader Compiler; set FURY_DXC_ROOT")
endif()
set(shader_headers)
foreach(entry sky_cs trace_cs temporal_cs filter_cs present_vs present_ps hud_vs hud_ps)
  if(entry MATCHES "_cs$")
    set(profile cs_6_5)
  elseif(entry MATCHES "_vs$")
    set(profile vs_6_0)
  else()
    set(profile ps_6_0)
  endif()
  set(cso "${CMAKE_CURRENT_BINARY_DIR}/generated/${entry}.cso")
  set(header "${CMAKE_CURRENT_BINARY_DIR}/generated/${entry}.h")
  add_custom_command(OUTPUT "${header}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${FURY_DXC}" -T ${profile} -E ${entry} -HV 2021 -O3 -Ges
      -Fo "${cso}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/dx12.hlsl"
    COMMAND ${CMAKE_COMMAND} "-DINPUT=${cso}" "-DOUTPUT=${header}"
      "-DSYMBOL=${entry}_bytecode" -P "${CMAKE_SOURCE_DIR}/cmake/EmbedShader.cmake"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/dx12.hlsl"
      "${CMAKE_SOURCE_DIR}/cmake/EmbedShader.cmake" VERBATIM)
  list(APPEND shader_headers "${header}")
endforeach()
target_sources(fury_engine PRIVATE src/dx12_backend.cpp src/dx12_upscaler.cpp ${shader_headers})
target_include_directories(fury_engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(fury_engine PRIVATE d3d12 dxgi dxguid)
target_compile_definitions(fury_engine PRIVATE FURY_HAS_DX12=1)
if(FURY_FSR_ROOT)
  if(NOT EXISTS "${FURY_FSR_ROOT}/Kits/FidelityFX/upscalers/include/ffx_upscale.h")
    message(FATAL_ERROR "FURY_FSR_ROOT does not contain the FSR SDK upscaler API")
  endif()
  target_include_directories(fury_engine SYSTEM PRIVATE "${FURY_FSR_ROOT}/Kits/FidelityFX")
  target_compile_definitions(fury_engine PRIVATE FURY_HAS_FSR=1)
endif()
if(FURY_XESS_ROOT)
  if(NOT EXISTS "${FURY_XESS_ROOT}/inc/xess/xess_d3d12.h")
    message(FATAL_ERROR "FURY_XESS_ROOT does not contain the XeSS D3D12 API")
  endif()
  target_include_directories(fury_engine SYSTEM PRIVATE "${FURY_XESS_ROOT}/inc")
  target_compile_definitions(fury_engine PRIVATE FURY_HAS_XESS=1)
endif()
