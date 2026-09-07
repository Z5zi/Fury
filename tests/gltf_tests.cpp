#include "fury/gltf.hpp"
#include "test_files.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>

using namespace fury;
void require(bool value,const char* text) { if(!value) throw std::runtime_error(text); }
int main() {
  const auto root=std::filesystem::temp_directory_path()/
    ("fury-gltf-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    std::filesystem::create_directory(root);
    const float positions[]={0,0,0,1,0,0,0,1,0};
    { std::ofstream data(root/"triangle.bin",std::ios::binary); data.write(reinterpret_cast<const char*>(positions),sizeof(positions)); }
    const std::string prefix=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36,"uri":")";
    const std::string suffix=R"("}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0,"translation":[2,3,4]}],"scenes":[{"nodes":[0]}],"scene":0})";
    const auto path=root/"triangle.gltf";
    { std::ofstream file(path); file<<prefix<<"triangle.bin"<<suffix; }
    GltfAsset asset; std::string error;
    require(load_gltf(path.u8string(),asset,error),error.c_str());
    require(asset.primitives.size()==1 && asset.primitives[0].mesh->indices.size()==3,"Triangle scene imports");
    require(asset.bounds_min.x==2 && asset.bounds_max.y==4 && asset.bounds_min.z==4,"Node transform and world bounds");
    require(asset.primitives[0].mesh->vertices[0].normal.z>.99f,"Missing normals generated");
    { std::ofstream file(path); file<<prefix<<"../outside.bin"<<suffix; }
    require(!load_gltf(path.u8string(),asset,error),"Resource traversal rejected");
    require(error.find("escapes")!=std::string::npos,"Path failure is explicit");
    require(asset.primitives.size()==1,"Failed import preserves previous asset");
    { std::ofstream file(path); file<<"{invalid"; }
    require(!load_gltf(path.u8string(),asset,error),"Malformed glTF rejected");
    remove_fury_fixture(root);
    std::cout<<"glTF scene, transforms, generated normals, and failure semantics passed\n"; return 0;
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';
    // The only cleanup target is the unique temporary test directory created above.
    try { remove_fury_fixture(root); } catch(...) {} return 1;
  }
}
