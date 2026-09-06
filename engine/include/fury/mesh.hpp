#pragma once

#include "fury/math.hpp"

#include <cstdint>
#include <vector>

namespace fury {

struct Vertex {
  Vec3 position;
  Vec3 color;
  Vec2 uv{0.f, 0.f};
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;

  // Optional GPU handles (OpenGL). 0 = not uploaded.
  unsigned int gpu_vao{0};
  unsigned int gpu_vbo{0};
  unsigned int gpu_ibo{0};
  bool gpu_uploaded{false};
};

Mesh make_box(const Vec3& size, const Vec3& color);
Mesh make_plane(float width, float depth, const Vec3& color);
Mesh make_colored_box(const Vec3& size, const Vec3& color_top, const Vec3& color_side);

}  // namespace fury
