#include "surface_gallery.hpp"
#include <array>
using namespace fury;
void create_surface_gallery(CoastalScene& scene) {
  scene.meshes.clear(); scene.instances.clear();
  scene.meshes.push_back(std::make_unique<Mesh>(make_box({1,1,1},{1,1,1})));
  auto* box=scene.meshes.back().get();
  auto add=[&](Vec3 p,Vec3 size,Material material) {
    scene.instances.push_back({box,translate(p)*scale(size),std::move(material)});
  };
  Material backing; backing.albedo={.13f,.14f,.16f}; backing.roughness=.85f;
  add({0,-.25f,0},{10,.3f,9},backing);
  const std::array<TextureSlot,6> profiles={TextureSlot::Asphalt,TextureSlot::Concrete,TextureSlot::Brick,
                                          TextureSlot::Wood,TextureSlot::Metal,TextureSlot::BarrelMetal};
  for(unsigned i=0;i<profiles.size();++i) {
    const float x=(float(i%3)-1)*2.6f,z=(float(i/3)-.5f)*2.6f;
    Material material; material.texture=profiles[i]; material.roughness=1.f;
    material.albedo={.85f,.85f,.85f};
    if(profiles[i]==TextureSlot::Metal || profiles[i]==TextureSlot::BarrelMetal) material.metallic=1.f;
    add({x,-.025f,z},{2.08f,.16f,2.08f},backing);
    add({x,.08f,z},{2,.10f,2},material);
    // Raised sample reveals edge-normal/roughness response and world-space scale.
    add({x,.30f,z-.55f},{1.5f,.35f,.30f},material);
  }
  Material brick; brick.texture=TextureSlot::Brick; brick.albedo={.85f,.8f,.76f}; brick.roughness=1.f;
  add({0,1.35f,-3.65f},{8,3,.18f},brick);
  Material concrete; concrete.texture=TextureSlot::Concrete; concrete.albedo={.75f,.76f,.76f}; concrete.roughness=1.f;
  for(float x:{-4.1f,4.1f}) add({x,1.35f,-3.6f},{.22f,3.1f,.32f},concrete);
  scene.deformation_mesh=nullptr;
}
