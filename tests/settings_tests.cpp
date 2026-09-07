#include "fury/settings.hpp"
#include "test_files.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
int main() {
  const auto root=std::filesystem::temp_directory_path()/
    ("fury-settings-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    std::filesystem::create_directory(root); const auto path=(root/"settings.json").string();
    fury::VaultlineSettings saved;
    saved.trace_mode=0; saved.upscaler=2; saved.upscale_quality=3;
    saved.mouse_sensitivity=.004f; saved.invert_y=true;
    require(fury::save_settings_json(path,saved),"Save rendering settings");
    fury::VaultlineSettings loaded;
    require(fury::load_settings_json(path,loaded),"Load rendering settings");
    require(loaded.trace_mode==0 && loaded.upscaler==2 && loaded.upscale_quality==3,"Rendering choices survive save/load");
    require(loaded.invert_y && std::fabs(loaded.mouse_sensitivity-.004f)<1e-6f,"Existing controls survive save/load");
    { std::ofstream file(path); file<<"{\"quality\":2,\"language\":1}"; }
    fury::VaultlineSettings legacy;
    require(fury::load_settings_json(path,legacy),"Legacy settings load");
    require(legacy.quality==2 && legacy.language==1 && legacy.upscaler==0 && legacy.trace_mode==1,"Legacy defaults remain compatible");
    remove_fury_fixture(root);
    std::cout<<"Rendering settings persistence and legacy compatibility passed\n"; return 0;
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n'; try { remove_fury_fixture(root); } catch(...) {} return 1;
  }
}
