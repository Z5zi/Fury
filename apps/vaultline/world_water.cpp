#include "world_water.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace vaultline {
namespace {
constexpr int kSize=256;
constexpr float kPatchMetres=16.f;
constexpr double kTau=6.283185307179586476925286766559;
constexpr const char* kSource="fury://world-water/v1";

// Integer wavevectors make both height and analytic derivatives exactly
// periodic. Amplitudes are metres, not arbitrary normal-map strength. Oblique,
// unequal wavevectors avoid a crossed/checkered ripple lattice. Wavelengths
// range from 7.16 m to 0.63 m; each amplitude is at most 14 mm.
struct Wave { int x,z; double amplitude,phase; };
constexpr std::array<Wave,12> kWaves{{
  {2,1,.014, .31}, {4,1,.009, 1.83}, {3,4,.006, 4.71},
  {6,2,.0045,2.93}, {7,-3,.0032,.89}, {4,9,.0028,5.67},
  {11,3,.0021,3.61}, {13,-4,.0018,1.47}, {7,14,.0014,4.19},
  {17,5,.0011,2.21}, {19,-7,.0009,5.13}, {11,23,.00065,.17}
}};

std::uint8_t byte(double v) {
  return static_cast<std::uint8_t>(std::clamp(v*255.+.5,0.,255.));
}
double srgb(double value) {
  return value<=.0031308?12.92*value:1.055*std::pow(value,1./2.4)-.055;
}
void pixel(fury::RgbaImage& image,std::size_t offset,double r,double g,double b) {
  image.pixels[offset]=byte(r); image.pixels[offset+1]=byte(g);
  image.pixels[offset+2]=byte(b); image.pixels[offset+3]=255;
}
void finish(fury::RgbaImage& image,std::vector<fury::RgbaImage>& mips,
            fury::TextureEncoding encoding) {
  // build_mip_chain includes the base. MaterialTextures stores only levels 1+
  // in *_mips; retaining the base there would shift every filtered level.
  auto chain=fury::build_mip_chain(std::move(image),encoding);
  image=std::move(chain.front()); mips.reserve(chain.size()-1);
  for(std::size_t i=1;i<chain.size();++i) mips.push_back(std::move(chain[i]));
}
std::size_t bytes(const fury::MaterialTextures& maps) {
  std::size_t total=maps.base_color.pixels.size()+maps.normal.pixels.size()+
      maps.metallic_roughness.pixels.size();
  for(const auto* chain:{&maps.base_color_mips,&maps.normal_mips,&maps.metallic_roughness_mips})
    for(const auto& mip:*chain) total+=mip.pixels.size();
  return total;
}
fury::Vec3 color_compensation(const fury::Mesh& mesh) {
  // Legacy planes bake a strong blue vertex tint. Compensate in the material
  // instead of mutating the original/shared mesh. On the original uniformly
  // colored planes this is exact. The floor bounds unusual future source data.
  fury::Vec3 mean{};
  for(const auto& v:mesh.vertices) mean+=v.color;
  mean=mean*(1.f/static_cast<float>(mesh.vertices.size()));
  return {1.f/std::max(mean.x,.1f),1.f/std::max(mean.y,.1f),1.f/std::max(mean.z,.1f)};
}
}  // namespace

std::shared_ptr<const fury::MaterialTextures> make_world_water_textures() {
  auto maps=std::make_shared<fury::MaterialTextures>(); maps->source=kSource;
  for(auto* image:{&maps->base_color,&maps->normal,&maps->metallic_roughness}) {
    image->width=image->height=kSize; image->pixels.resize(kSize*kSize*4);
  }
  for(int y=0;y<kSize;++y) for(int x=0;x<kSize;++x) {
    const double u=(x+.5)/kSize,v=(y+.5)/kSize;
    double dx=0,dz=0;
    for(const Wave& wave:kWaves) {
      const double derivative=wave.amplitude*kTau/kPatchMetres*
          std::cos(kTau*(wave.x*u+wave.z*v)+wave.phase);
      dx+=wave.x*derivative; dz+=wave.z*derivative;
    }
    const double inverse_length=1./std::sqrt(1.+dx*dx+dz*dz);
    const auto offset=(static_cast<std::size_t>(y)*kSize+x)*4;
    pixel(maps->normal,offset,.5-.5*dx*inverse_length,
          .5-.5*dz*inverse_length,.5+.5*inverse_length);

    // Very low-contrast color variation; wave lighting comes from normals,
    // never painted blue/white wave crests or UV-edge foam. Values here are
    // linear reflectance; only the base-color image is encoded as sRGB.
    const double drift=.6*std::sin(kTau*(u+2*v)+.7)+
        .4*std::sin(kTau*(3*u-v)+2.4);
    const double density=1.+.025*drift;
    pixel(maps->base_color,offset,srgb(.039*density),srgb(.070*density),srgb(.074*density));
    // glTF-style linear channels: R unused (white), G roughness, B metallic.
    pixel(maps->metallic_roughness,offset,1.,.265+.02*drift,0.);
  }
  finish(maps->base_color,maps->base_color_mips,fury::TextureEncoding::SRGB);
  finish(maps->normal,maps->normal_mips,fury::TextureEncoding::Normal);
  finish(maps->metallic_roughness,maps->metallic_roughness_mips,fury::TextureEncoding::Linear);
  return maps;
}

WorldWaterStats upgrade_world_water(fury::Scene& scene) {
  WorldWaterStats stats;
  std::shared_ptr<const fury::MaterialTextures> maps;
  // Find existing generated maps first, even when a newly added district is
  // earlier in entity order. Partial upgrades must not allocate a second set.
  for(const auto& e:scene.entities()) {
    const bool named=e.name=="HarborWater" || e.name=="RidgeWater" || e.name=="NorthQuayWater";
    if(named && e.mesh && !e.mesh->vertices.empty() && e.material.texture==fury::TextureSlot::Water &&
       e.material.textures && e.material.textures->source==kSource) {
      maps=e.material.textures; break;
    }
  }
  std::size_t already=0;
  for(auto& e:scene.entities()) {
    std::size_t* count=nullptr;
    if(e.name=="HarborWater") count=&stats.harbor_surfaces;
    else if(e.name=="RidgeWater") count=&stats.ridge_surfaces;
    else if(e.name=="NorthQuayWater") count=&stats.north_quay_surfaces;
    if(!count || !e.mesh || e.mesh->vertices.empty() || e.material.texture!=fury::TextureSlot::Water) continue;
    if(e.material.textures) {
      if(e.material.textures->source==kSource) {
        ++*count; ++already; if(!maps) maps=e.material.textures;
      } else ++stats.preserved_source_materials;
      continue;
    }
    if(!maps) maps=make_world_water_textures();
    auto& m=e.material;
    m.textures=maps; m.albedo=color_compensation(*e.mesh);
    m.world_uv_scale=1.f/kPatchMetres;
    // One world-anchored patch drifts at 0.12 m/s X, 0.045 m/s Z.
    m.uv_scroll_u=.12f/kPatchMetres; m.uv_scroll_v=.045f/kPatchMetres;
    m.normal_scale=1.f; m.roughness=1.f; m.metallic=0.f;
    m.index_of_refraction=1.333f; m.transmission=0.f;
    m.emissive=0.f; m.wetness=0.f;
    m.opacity=1.f; m.alpha_cutoff=-1.f; m.alpha_blend=false;
    m.detail_texture=fury::TextureSlot::None;
    ++*count; ++stats.updated_surfaces;
  }
  stats.applied=stats.updated_surfaces>0;
  stats.already_applied=!stats.applied && already>0;
  if(maps) stats.shared_map_bytes=bytes(*maps);
  return stats;
}
}  // namespace vaultline
