# Fingerprint rendering/program inputs, excluding generated evidence and build trees.
file(GLOB_RECURSE FURY_SOURCE_INPUTS CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/engine/src/*"
  "${CMAKE_SOURCE_DIR}/engine/include/*"
  "${CMAKE_SOURCE_DIR}/engine/shaders/*"
  "${CMAKE_SOURCE_DIR}/engine/third_party/*"
  "${CMAKE_SOURCE_DIR}/apps/*"
  "${CMAKE_SOURCE_DIR}/cmake/*.cmake")
list(APPEND FURY_SOURCE_INPUTS "CMakeLists.txt" "engine/CMakeLists.txt")
list(SORT FURY_SOURCE_INPUTS)
set(fingerprint_payload "")
set(fingerprint_files "")
foreach(input IN LISTS FURY_SOURCE_INPUTS)
  file(SHA256 "${CMAKE_SOURCE_DIR}/${input}" digest)
  string(APPEND fingerprint_payload "${input}:${digest}\n")
  string(APPEND fingerprint_files "${input}\n")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/${input}")
endforeach()
string(SHA256 FURY_SOURCE_FINGERPRINT "${fingerprint_payload}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
set(build_info "#include \"fury/render_settings.hpp\"\nnamespace fury { const char* build_source_fingerprint() { return \"${FURY_SOURCE_FINGERPRINT}\"; } }\n")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/generated/build_info.cpp" CONTENT "${build_info}")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/generated/source-files.txt" CONTENT "${fingerprint_files}")
