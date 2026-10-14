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
    saved.player_hair=2;
    saved.trace_mode=0; saved.upscaler=2; saved.upscale_quality=3;
    saved.mouse_sensitivity=.004f; saved.invert_y=true;
    require(fury::save_settings_json(path,saved),"Save rendering settings");
    fury::VaultlineSettings loaded;
    require(fury::load_settings_json(path,loaded),"Load rendering settings");
    require(loaded.trace_mode==0 && loaded.upscaler==2 && loaded.upscale_quality==3,"Rendering choices survive save/load");
    require(loaded.player_hair==2,"Requested Player Hair survives roundtrip");
    require(loaded.invert_y && std::fabs(loaded.mouse_sensitivity-.004f)<1e-6f,"Existing controls survive save/load");
    { std::ofstream file(path); file<<"{\"quality\":2,\"language\":1}"; }
    fury::VaultlineSettings legacy;
    require(fury::load_settings_json(path,legacy),"Legacy settings load");
    require(legacy.player_hair==0,"Legacy saves default Player Hair to Auto");
    require(legacy.quality==2 && legacy.language==1 && legacy.upscaler==0 && legacy.trace_mode==1,"Legacy defaults remain compatible");
    { std::ofstream file(path); file << R"({"quality":0,"player_hair":1,"future":{"text":"comma, and quote \"","list":[1,true]},"language":1})"; }
    fury::VaultlineSettings extended;
    require(fury::load_settings_json(path,extended),"Extended settings load");
    extended.player_hair=2;
    require(fury::save_settings_json(path,extended),"Extended settings save");
    { std::ifstream file(path);std::ostringstream text;text<<file.rdbuf();
      require(text.str().find(R"("future":{"text":"comma, and quote \"","list":[1,true]})")!=std::string::npos,"Unrelated nested JSON survives menu save"); }
    fury::VaultlineSettings final;
    require(fury::load_settings_json(path,final)&&final.quality==0&&final.language==1&&final.player_hair==2,"Unrelated known fields preserved");
    for (const auto& entry : {R"({"future":{"player_hair":2},"quality":1})",
         R"({"player_hair":9999999999999999999999999999999999})", R"({"player_hair":1.5})",
         R"({"player_hair":1oops})", R"({"player_hair":"1"})", R"({"player_hair":-1})"}) {
      {std::ofstream file(path);file<<entry;}
      fury::VaultlineSettings malformed;malformed.player_hair=2;
      require(fury::load_settings_json(path,malformed)&&malformed.player_hair==0,"Top-level bounded Player Hair parsing");
    }
    remove_fury_fixture(root);
    std::cout<<"Rendering settings persistence and legacy compatibility passed\n"; return 0;
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n'; try { remove_fury_fixture(root); } catch(...) {} return 1;
  }
}
