#pragma once
#include <filesystem>
#include <stdexcept>

inline void remove_fury_fixture(const std::filesystem::path& path) {
  const auto root=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());
  const auto target=std::filesystem::weakly_canonical(path);
  if(target.parent_path()!=root || target.filename().string().rfind("fury-",0)!=0)
    throw std::runtime_error("Refusing cleanup outside the owned Fury test fixture");
  std::filesystem::remove_all(target);
}
