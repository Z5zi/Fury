#pragma once

#if defined(_WIN32) || defined(FURY_PLATFORM_WINDOWS)
#  define FURY_WINDOWS 1
#else
#  define FURY_WINDOWS 0
#endif

#if defined(__linux__) || defined(FURY_PLATFORM_LINUX)
#  define FURY_LINUX 1
#else
#  define FURY_LINUX 0
#endif

#ifndef FURY_HAS_ASM
#  define FURY_HAS_ASM 0
#endif

namespace fury {

constexpr const char* platform_name() {
#if FURY_WINDOWS
  return "Windows";
#elif FURY_LINUX
  return "Linux";
#else
  return "Unknown";
#endif
}

}  // namespace fury
