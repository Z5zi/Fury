// Model-only matched cost measurement: original generic Security construction
// and the exact individual Vale construction share this executable and CPU set.
#include "fury/character_animation.hpp"
#include "fury/character_profile.hpp"
#include "fury/character_surface.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace fury;
namespace {
using Clock = std::chrono::steady_clock;
double elapsed(Clock::time_point t) {
  return std::chrono::duration<double, std::milli>(Clock::now()-t).count();
}
struct Summary {
  std::string variant, lod;
  std::size_t vertices{}, triangles{}, bind_mesh_bytes{}, influence_bytes{}, presentation_geometry_bytes{}, bind_mesh_capacity_bytes{}, influence_capacity_bytes{}, estimated_presentation_capacity_bytes{};
  std::vector<double> construction_ms, skin_batch_mean_ms;
};
CharacterModel model(bool individual, CharacterLod lod) {
  return individual ? make_character_profile(1.86f, CharacterRole::Security, "NpcAlleyHmpd", lod).model
                    : make_character_model(1.86f, CharacterRole::Security, character_seed("NpcAlleyHmpd"), lod);
}
void observe(Summary& result, bool individual, CharacterLod lod) {
  const auto start = Clock::now();
  const auto m = model(individual, lod);
  result.construction_ms.push_back(elapsed(start));
  const auto& bind = m.bind_mesh;
  result.vertices = bind.vertices.size(); result.triangles = bind.indices.size()/3;
  result.bind_mesh_bytes = bind.vertices.size()*sizeof(Vertex)+bind.indices.size()*sizeof(std::uint32_t);
  result.influence_bytes = m.influences.size()*sizeof(CharacterInfluence);
  // NpcPresentation stores both a bind model and a mutable scene mesh for each
  // LOD. This accounts for logical vertex/index/influence bytes, not capacity,
  // allocator metadata, rigs, renderer acceleration, texture or driver copies.
  result.presentation_geometry_bytes = result.bind_mesh_bytes*2+result.influence_bytes;
  result.bind_mesh_capacity_bytes = bind.vertices.capacity()*sizeof(Vertex)+bind.indices.capacity()*sizeof(std::uint32_t);
  result.influence_capacity_bytes = m.influences.capacity()*sizeof(CharacterInfluence);
  Mesh posed = bind;
  result.estimated_presentation_capacity_bytes = result.bind_mesh_capacity_bytes + result.influence_capacity_bytes +
      posed.vertices.capacity()*sizeof(Vertex)+posed.indices.capacity()*sizeof(std::uint32_t);
  std::vector<CharacterPose> samples;
  for (int frame = 0; frame < 128; ++frame) {
    CharacterAnimationSample sample;
    sample.seed = m.seed; sample.phase_cycles = frame/128.; sample.idle_time = frame/128.;
    sample.move_weight = 1; sample.speed = 1.4f;
    sample.stride_length = character_stride_length(m.rig.height, sample.speed, 0);
    samples.push_back(sample_character_animation(m.rig, sample));
  }
  apply_character_pose(m, samples.back(), posed);
  const auto* vertices = posed.vertices.data();
  const auto* indices = posed.indices.data();
  const auto begin = Clock::now();
  for (const auto& pose : samples)
    if (!apply_character_pose(m, pose, posed)) throw std::runtime_error("Benchmark sampled an unchanged pose");
  result.skin_batch_mean_ms.push_back(elapsed(begin)/samples.size());
  if (posed.vertices.data() != vertices || posed.indices.data() != indices)
    throw std::runtime_error("Benchmark unexpectedly reallocated its frame geometry");
}
void numbers(const std::vector<double>& values) {
  std::cout << '[';
  for (unsigned i = 0; i < values.size(); ++i) std::cout << (i ? "," : "") << values[i];
  std::cout << ']';
}
}
int main() {
  try {
    const auto begin = Clock::now();
    const auto material = make_character_surface_material();
    const double shared_atlas_first_ms = elapsed(begin);
    std::size_t shared_atlas_bytes = 0;
    const auto& textures = *material.textures;
    for (const auto* image : {&textures.base_color, &textures.normal, &textures.metallic_roughness, &textures.emissive})
      shared_atlas_bytes += image->pixels.size();
    for (const auto* chain : {&textures.base_color_mips, &textures.normal_mips, &textures.metallic_roughness_mips, &textures.emissive_mips})
      for (const auto& image : *chain) shared_atlas_bytes += image.pixels.size();
    std::vector<Summary> rows{{"original_generic", "near"}, {"individual_vale", "near"},
                              {"original_generic", "far"}, {"individual_vale", "far"}};
    // Warm each constructor once. Interleave A/B then B/A to reduce bias from
    // changing machine conditions. No promise of isolated-core frequency.
    for (bool individual : {false, true})
      for (auto lod : {CharacterLod::Near, CharacterLod::Far}) model(individual, lod);
    for (int repetition = 0; repetition < 8; ++repetition)
      for (unsigned offset = 0; offset < rows.size(); ++offset) {
        const unsigned index = repetition%2 ? unsigned(rows.size()-1-offset) : offset;
        observe(rows[index], index%2 != 0, index<2 ? CharacterLod::Near : CharacterLod::Far);
      }
    std::cout << std::setprecision(10) << "{\n\"height_meters\":1.86,\n\"seed\":4123887574,\n"
              << "\"vertex_size_bytes\":" << sizeof(Vertex)
              << ",\n\"influence_size_bytes\":" << sizeof(CharacterInfluence)
              << ",\n\"shared_atlas_first_construction_ms\":" << shared_atlas_first_ms
              << ",\n\"shared_atlas_bytes\":" << shared_atlas_bytes
              << ",\n\"poses_per_batch\":128,\n\"rows\":[\n";
    for (unsigned i = 0; i < rows.size(); ++i) {
      const auto& row = rows[i];
      std::cout << (i ? ",\n" : "") << "{\"variant\":\"" << row.variant << "\",\"lod\":\"" << row.lod
                << "\",\"vertices\":" << row.vertices << ",\"triangles\":" << row.triangles
                << ",\"bind_mesh_bytes\":" << row.bind_mesh_bytes
                << ",\"influence_bytes\":" << row.influence_bytes
                << ",\"presentation_geometry_bytes\":" << row.presentation_geometry_bytes
                << ",\"bind_mesh_capacity_bytes\":" << row.bind_mesh_capacity_bytes
                << ",\"influence_capacity_bytes\":" << row.influence_capacity_bytes
                << ",\"estimated_presentation_capacity_bytes\":" << row.estimated_presentation_capacity_bytes
                << ",\"construction_ms\":";
      numbers(row.construction_ms); std::cout << ",\"skin_batch_mean_ms\":"; numbers(row.skin_batch_mean_ms); std::cout << '}';
    }
    std::cout << "\n],\n\"scope\":\"Warm shared atlas; 8 interleaved A/B construction and 128-pose batches; identical identity, height, source build and CPU affinity. Skin timer excludes sampling and render/upload. Logical memory excludes capacity, allocator metadata, rig, textures, driver storage and ray acceleration.\"\n}\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
