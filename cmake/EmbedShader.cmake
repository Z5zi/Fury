file(READ "${INPUT}" shader_hex HEX)
string(REGEX REPLACE "(..)" "0x\\1," shader_bytes "${shader_hex}")
file(WRITE "${OUTPUT}" "#pragma once\n#include <cstddef>\ninline constexpr unsigned char ${SYMBOL}[] = {${shader_bytes}};\n")
