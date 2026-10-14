#include "fury/gltf.hpp"
#include "fury/texture.hpp"
#include "test_files.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <cmath>
#include <limits>

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
    auto material_scene = prefix + "triangle.bin" + suffix;
    auto marker = material_scene.find("\"meshes\"");
    material_scene.insert(marker,
      R"("materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.4,0.6,0.8,0.2]},"alphaMode":"OPAQUE"}],)");
    marker = material_scene.find(R"("attributes":{"POSITION":0})");
    material_scene.insert(marker, R"("material":0,)");
    { std::ofstream file(path); file<<material_scene; }
    require(load_gltf(path.u8string(),asset,error),error.c_str());
    require(asset.primitives[0].material.opacity==1.f,"Opaque glTF ignores alpha factor");
    require(asset.primitives[0].material.textures &&
            !asset.primitives[0].material.textures->base_color.valid(),
            "Untextured glTF retains imported-material identity without an image");
    auto emissive_scene=material_scene;
    emissive_scene.replace(emissive_scene.find("0.4,0.6,0.8,0.2"),15,"0.0,0.0,0.0,0.2");
    emissive_scene.insert(emissive_scene.find("\"alphaMode\""),R"("emissiveFactor":[0,1,0],)");
    { std::ofstream file(path); file<<emissive_scene; }
    require(load_gltf(path.u8string(),asset,error),error.c_str());
    const auto& emitter=asset.primitives[0].material;
    require(emitter.albedo.x==0 && emitter.albedo.y==0 && emitter.albedo.z==0 &&
            emitter.emissive_color.y==1 && emitter.emissive==1 && emitter.textures,
            "Black-base glTF emitter retains independent colored emission marker");
    { std::ofstream file(path); file<<material_scene; }
    require(load_gltf(path.u8string(),asset,error),error.c_str());

    auto nonfinite_material=material_scene;
    nonfinite_material.replace(nonfinite_material.find("0.4,0.6,0.8,0.2"),15,"1e39,0.6,0.8,0.2");
    { std::ofstream file(path); file<<nonfinite_material; }
    require(!load_gltf(path.u8string(),asset,error),"Nonfinite material factors are rejected");
    require(asset.primitives[0].material.opacity==1.f,"Failed material import preserves prior data");
    { std::ofstream file(path); file<<material_scene; }
    const float invalid_positions[]={0,0,0,1,0,0,0,std::numeric_limits<float>::quiet_NaN(),0};
    { std::ofstream data(root/"triangle.bin",std::ios::binary); data.write(reinterpret_cast<const char*>(invalid_positions),sizeof(invalid_positions)); }
    require(!load_gltf(path.u8string(),asset,error),"Nonfinite vertex data is rejected");

    const float degenerate_positions[]={0,0,0,1,0,0,2,0,0};
    { std::ofstream data(root/"triangle.bin",std::ios::binary); data.write(reinterpret_cast<const char*>(degenerate_positions),sizeof(degenerate_positions)); }
    require(!load_gltf(path.u8string(),asset,error),"All-degenerate glTF fails with no renderable primitives");
    require(asset.primitives.size()==1,"Degenerate import is transactional");

    Image image;
    const auto ppm=root/"texture.ppm";
    { std::ofstream file(ppm,std::ios::binary); file<<"P6\n1 1\n15\n"; const char rgb[]={15,8,0}; file.write(rgb,3); }
    require(load_ppm(ppm.u8string(),image),"P6 loads valid non-255 maximum");
    require(image.rgb[0]==255 && image.rgb[1]==136 && image.rgb[2]==0,"P6 scales maxval to RGB8");
    { std::ofstream file(ppm); file<<"P3\n1 # width\n1\n15\n15 # red\n8 0\n"; }
    require(load_ppm(ppm.u8string(),image) && image.rgb[1]==136,"P3 supports comments between every sample/header value");
    { std::ofstream file(ppm); file<<"P3\n999999999 999999999\n255\n"; }
    require(!load_ppm(ppm.u8string(),image) && image.rgb.empty(),"Oversized PPM fails before allocation");
    { std::ofstream file(ppm); file<<"P3\n1 1\n15\n16 0 0\n"; }
    require(!load_ppm(ppm.u8string(),image) && image.rgb.empty(),"Out-of-range PPM samples rejected");
    image={1,1,{17,25,30}};
    const auto sampled=sample_image(image,std::numeric_limits<float>::quiet_NaN(),0);
    require(sampled.x==1 && sampled.y==1 && sampled.z==1,"Nonfinite texture coordinates safely return white");
    remove_fury_fixture(root);
    std::cout<<"glTF transforms, materials, degenerate rejection, texture bounds and PPM semantics passed\n"; return 0;
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';
    // The only cleanup target is the unique temporary test directory created above.
    try { remove_fury_fixture(root); } catch(...) {} return 1;
  }
}
