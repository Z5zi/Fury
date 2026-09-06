#include "fury/renderer.hpp"

#include "fury/gl_loader.hpp"
#include "fury/log.hpp"

#include <SDL.h>

#include <cmath>
#include <string>
#include <vector>

namespace fury {
namespace {

const char* kVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vUV;

void main() {
  vec4 world = uModel * vec4(aPos, 1.0);
  vWorldPos = world.xyz;
  // Uniform scale / axis-aligned props: mat3(model) is fine for normals
  vNormal = mat3(uModel) * aNormal;
  vColor = aColor;
  vUV = aUV;
  gl_Position = uProj * uView * world;
}
)";

const char* kFragSrc = R"(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;

uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform float uSunIntensity;
uniform vec3 uAmbient;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec3 uFogColor;
uniform vec3 uAlbedo;
uniform float uMetallic;
uniform float uRoughness;
uniform sampler2D uAlbedoMap;
uniform int uUseTexture;

out vec4 FragColor;

void main() {
  vec3 N = normalize(vNormal);
  vec3 L = normalize(-uSunDir);
  vec3 V = normalize(uCameraPos - vWorldPos);
  vec3 H = normalize(L + V);

  vec3 base = vColor * uAlbedo;
  if (uUseTexture != 0) {
    base *= texture(uAlbedoMap, vUV).rgb;
  }

  float NdotL = max(dot(N, L), 0.0);
  float diff = NdotL;

  // Specular: Blinn-Phong shaped by roughness (PBR-ish knob)
  float shininess = mix(128.0, 4.0, clamp(uRoughness, 0.04, 1.0));
  float spec = pow(max(dot(N, H), 0.0), shininess) * (1.0 - uRoughness * 0.85);
  // Metals push specular toward albedo, dielectrics stay white-ish
  vec3 specCol = mix(vec3(0.04), base, clamp(uMetallic, 0.0, 1.0));
  float metalDiff = 1.0 - uMetallic * 0.9;

  vec3 lit = uAmbient * base
           + uSunColor * uSunIntensity * (base * diff * metalDiff + specCol * spec);

  float dist = length(uCameraPos - vWorldPos);
  float fog = clamp((uFogEnd - dist) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
  vec3 color = mix(uFogColor, lit, fog);

  FragColor = vec4(color, 1.0);
}
)";

void fill_texture_pixels(TextureSlot slot, int size, std::vector<std::uint8_t>& rgb) {
  rgb.resize(static_cast<std::size_t>(size * size * 3));
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const std::size_t i = static_cast<std::size_t>((y * size + x) * 3);
      std::uint8_t r = 128, g = 128, b = 128;
      switch (slot) {
        case TextureSlot::Checker: {
          const bool c = ((x / 8) ^ (y / 8)) & 1;
          r = g = b = c ? 210 : 55;
          break;
        }
        case TextureSlot::Asphalt: {
          const int n = ((x * 13 + y * 7) ^ (x * y)) & 31;
          r = static_cast<std::uint8_t>(40 + n);
          g = static_cast<std::uint8_t>(40 + n);
          b = static_cast<std::uint8_t>(44 + n);
          if ((x + y) % 17 == 0) {
            r = g = b = 70;
          }
          break;
        }
        case TextureSlot::Concrete: {
          const int n = ((x * 3 + y * 5) ^ (x << 2)) & 47;
          r = static_cast<std::uint8_t>(150 + n);
          g = static_cast<std::uint8_t>(148 + n);
          b = static_cast<std::uint8_t>(142 + n / 2);
          break;
        }
        case TextureSlot::Water: {
          const float fx = static_cast<float>(x) / static_cast<float>(size);
          const float fy = static_cast<float>(y) / static_cast<float>(size);
          const float w =
              0.5f + 0.5f * std::sin(fx * 18.f + fy * 6.f) * std::cos(fy * 14.f);
          r = static_cast<std::uint8_t>(20 + w * 30.f);
          g = static_cast<std::uint8_t>(70 + w * 50.f);
          b = static_cast<std::uint8_t>(120 + w * 80.f);
          break;
        }
        default:
          r = g = b = 255;
          break;
      }
      rgb[i] = r;
      rgb[i + 1] = g;
      rgb[i + 2] = b;
    }
  }
}

class GlBackend final : public IRenderBackend {
 public:
  ~GlBackend() override { destroy(); }

  bool create(SDL_Window* window, int width, int height) override {
    destroy();
    m_window = window;
    m_width = width;
    m_height = height;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_glctx = SDL_GL_CreateContext(window);
    if (!m_glctx) {
      Log::warn(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
      return false;
    }
    SDL_GL_MakeCurrent(window, m_glctx);
    SDL_GL_SetSwapInterval(0);

    if (!gl::load_gl_functions()) {
      Log::warn("OpenGL function loading failed");
      destroy();
      return false;
    }

    if (!build_program()) {
      destroy();
      return false;
    }

    if (!build_textures()) {
      destroy();
      return false;
    }

    gl::Enable(gl::GL_DEPTH_TEST);
    gl::DepthFunc(gl::GL_LESS);
    gl::Enable(gl::GL_CULL_FACE);
    gl::CullFace(gl::GL_BACK);
    gl::FrontFace(gl::GL_CCW);
    gl::Viewport(0, 0, m_width, m_height);

    Log::info("Renderer backend: OpenGL 3.3 core (lit + fog + textures)");
    return true;
  }

  void destroy() override {
    for (gl::GLuint& tex : m_textures) {
      if (tex) {
        gl::DeleteTextures(1, &tex);
        tex = 0;
      }
    }
    if (m_program) {
      gl::DeleteProgram(m_program);
      m_program = 0;
    }
    if (m_glctx) {
      SDL_GL_DeleteContext(m_glctx);
      m_glctx = nullptr;
    }
    m_window = nullptr;
  }

  void begin_frame(const Color& clear) override {
    SDL_GL_MakeCurrent(m_window, m_glctx);
    gl::Viewport(0, 0, m_width, m_height);
    gl::ClearColor(clear.r / 255.f, clear.g / 255.f, clear.b / 255.f,
                   clear.a / 255.f);
    gl::Clear(gl::GL_COLOR_BUFFER_BIT | gl::GL_DEPTH_BUFFER_BIT);
    gl::UseProgram(m_program);

    const float sun[3] = {m_lighting.sun_direction.x, m_lighting.sun_direction.y,
                          m_lighting.sun_direction.z};
    const float sun_c[3] = {m_lighting.sun_color.x, m_lighting.sun_color.y,
                            m_lighting.sun_color.z};
    const float amb[3] = {m_lighting.ambient.x, m_lighting.ambient.y,
                          m_lighting.ambient.z};
    const float fog_c[3] = {m_lighting.fog_color.x, m_lighting.fog_color.y,
                            m_lighting.fog_color.z};
    const float cam[3] = {m_camera_pos.x, m_camera_pos.y, m_camera_pos.z};

    gl::Uniform3fv(m_loc_sun_dir, 1, sun);
    gl::Uniform3fv(m_loc_sun_color, 1, sun_c);
    gl::Uniform1f(m_loc_sun_intensity, m_lighting.sun_intensity);
    gl::Uniform3fv(m_loc_ambient, 1, amb);
    gl::Uniform1f(m_loc_fog_start, m_lighting.fog_start);
    gl::Uniform1f(m_loc_fog_end, m_lighting.fog_end);
    gl::Uniform3fv(m_loc_fog_color, 1, fog_c);
    gl::Uniform3fv(m_loc_camera, 1, cam);
    gl::Uniform1i(m_loc_albedo_map, 0);
  }

  void set_view_proj(const Mat4& view, const Mat4& proj) override {
    m_view = view;
    m_proj = proj;
  }

  void set_camera_position(const Vec3& pos) override { m_camera_pos = pos; }

  void set_lighting(const Lighting& lighting) override { m_lighting = lighting; }

  void upload_mesh(Mesh& mesh) override {
    if (mesh.gpu_uploaded) {
      return;
    }
    gl::GenVertexArrays(1, &mesh.gpu_vao);
    gl::GenBuffers(1, &mesh.gpu_vbo);
    gl::GenBuffers(1, &mesh.gpu_ibo);

    gl::BindVertexArray(mesh.gpu_vao);
    gl::BindBuffer(gl::GL_ARRAY_BUFFER, mesh.gpu_vbo);
    gl::BufferData(gl::GL_ARRAY_BUFFER,
                   static_cast<gl::GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                   mesh.vertices.data(), gl::GL_STATIC_DRAW);
    gl::BindBuffer(gl::GL_ELEMENT_ARRAY_BUFFER, mesh.gpu_ibo);
    gl::BufferData(
        gl::GL_ELEMENT_ARRAY_BUFFER,
        static_cast<gl::GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
        mesh.indices.data(), gl::GL_STATIC_DRAW);

    const gl::GLsizei stride = static_cast<gl::GLsizei>(sizeof(Vertex));
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 3, gl::GL_FLOAT, gl::GL_FALSE_, stride,
                            reinterpret_cast<void*>(offsetof(Vertex, position)));
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 3, gl::GL_FLOAT, gl::GL_FALSE_, stride,
                            reinterpret_cast<void*>(offsetof(Vertex, normal)));
    gl::EnableVertexAttribArray(2);
    gl::VertexAttribPointer(2, 3, gl::GL_FLOAT, gl::GL_FALSE_, stride,
                            reinterpret_cast<void*>(offsetof(Vertex, color)));
    gl::EnableVertexAttribArray(3);
    gl::VertexAttribPointer(3, 2, gl::GL_FLOAT, gl::GL_FALSE_, stride,
                            reinterpret_cast<void*>(offsetof(Vertex, uv)));
    gl::BindVertexArray(0);
    mesh.gpu_uploaded = true;
  }

  void draw_mesh(const Mesh& mesh, const Mat4& model,
                 const Material& material) override {
    if (mesh.indices.empty()) {
      return;
    }
    Mesh& mutable_mesh = const_cast<Mesh&>(mesh);
    if (!mutable_mesh.gpu_uploaded) {
      upload_mesh(mutable_mesh);
    }

    gl::UniformMatrix4fv(m_loc_model, 1, gl::GL_FALSE_, model.m);
    gl::UniformMatrix4fv(m_loc_view, 1, gl::GL_FALSE_, m_view.m);
    gl::UniformMatrix4fv(m_loc_proj, 1, gl::GL_FALSE_, m_proj.m);

    const float albedo[3] = {material.albedo.x, material.albedo.y,
                             material.albedo.z};
    gl::Uniform3fv(m_loc_albedo, 1, albedo);
    gl::Uniform1f(m_loc_metallic, material.metallic);
    gl::Uniform1f(m_loc_roughness, material.roughness);

    const int slot = static_cast<int>(material.texture);
    const bool use_tex =
        slot > 0 && slot < static_cast<int>(TextureSlot::Count) &&
        m_textures[static_cast<std::size_t>(slot)] != 0;
    gl::Uniform1i(m_loc_use_texture, use_tex ? 1 : 0);
    gl::ActiveTexture(gl::GL_TEXTURE0);
    if (use_tex) {
      gl::BindTexture(gl::GL_TEXTURE_2D,
                      m_textures[static_cast<std::size_t>(slot)]);
    } else {
      gl::BindTexture(gl::GL_TEXTURE_2D, m_textures[0]);  // white 1x1
    }

    gl::BindVertexArray(mesh.gpu_vao);
    gl::DrawElements(gl::GL_TRIANGLES,
                     static_cast<gl::GLsizei>(mesh.indices.size()),
                     gl::GL_UNSIGNED_INT, nullptr);
    gl::BindVertexArray(0);
  }

  void end_frame() override { SDL_GL_SwapWindow(m_window); }

  void resize(int width, int height) override {
    m_width = width;
    m_height = height;
    if (m_glctx) {
      gl::Viewport(0, 0, width, height);
    }
  }

  RenderBackendKind kind() const override { return RenderBackendKind::OpenGL; }
  const char* name() const override { return "OpenGL 3.3 lit"; }

 private:
  bool build_program() {
    const gl::GLuint vs = compile(gl::GL_VERTEX_SHADER, kVertSrc);
    const gl::GLuint fs = compile(gl::GL_FRAGMENT_SHADER, kFragSrc);
    if (!vs || !fs) {
      if (vs) gl::DeleteShader(vs);
      if (fs) gl::DeleteShader(fs);
      return false;
    }
    m_program = gl::CreateProgram();
    gl::AttachShader(m_program, vs);
    gl::AttachShader(m_program, fs);
    gl::LinkProgram(m_program);
    gl::DeleteShader(vs);
    gl::DeleteShader(fs);

    gl::GLint ok = 0;
    gl::GetProgramiv(m_program, gl::GL_LINK_STATUS, &ok);
    if (!ok) {
      gl::GLint len = 0;
      gl::GetProgramiv(m_program, gl::GL_INFO_LOG_LENGTH, &len);
      std::string log(static_cast<std::size_t>(len), '\0');
      gl::GetProgramInfoLog(m_program, len, nullptr, log.data());
      Log::error(std::string("GL link failed: ") + log);
      return false;
    }

    m_loc_model = gl::GetUniformLocation(m_program, "uModel");
    m_loc_view = gl::GetUniformLocation(m_program, "uView");
    m_loc_proj = gl::GetUniformLocation(m_program, "uProj");
    m_loc_camera = gl::GetUniformLocation(m_program, "uCameraPos");
    m_loc_sun_dir = gl::GetUniformLocation(m_program, "uSunDir");
    m_loc_sun_color = gl::GetUniformLocation(m_program, "uSunColor");
    m_loc_sun_intensity = gl::GetUniformLocation(m_program, "uSunIntensity");
    m_loc_ambient = gl::GetUniformLocation(m_program, "uAmbient");
    m_loc_fog_start = gl::GetUniformLocation(m_program, "uFogStart");
    m_loc_fog_end = gl::GetUniformLocation(m_program, "uFogEnd");
    m_loc_fog_color = gl::GetUniformLocation(m_program, "uFogColor");
    m_loc_albedo = gl::GetUniformLocation(m_program, "uAlbedo");
    m_loc_metallic = gl::GetUniformLocation(m_program, "uMetallic");
    m_loc_roughness = gl::GetUniformLocation(m_program, "uRoughness");
    m_loc_albedo_map = gl::GetUniformLocation(m_program, "uAlbedoMap");
    m_loc_use_texture = gl::GetUniformLocation(m_program, "uUseTexture");
    return true;
  }

  bool build_textures() {
    m_textures.assign(static_cast<std::size_t>(TextureSlot::Count), 0);

    // Slot 0: solid white
    {
      const std::uint8_t white[3] = {255, 255, 255};
      gl::GenTextures(1, &m_textures[0]);
      gl::BindTexture(gl::GL_TEXTURE_2D, m_textures[0]);
      gl::TexImage2D(gl::GL_TEXTURE_2D, 0, static_cast<gl::GLint>(gl::GL_RGB), 1,
                     1, 0, gl::GL_RGB, gl::GL_UNSIGNED_BYTE, white);
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER,
                        static_cast<gl::GLint>(gl::GL_LINEAR));
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER,
                        static_cast<gl::GLint>(gl::GL_LINEAR));
    }

    const TextureSlot slots[] = {TextureSlot::Checker, TextureSlot::Asphalt,
                                 TextureSlot::Concrete, TextureSlot::Water};
    std::vector<std::uint8_t> pixels;
    constexpr int kSize = 64;
    for (TextureSlot slot : slots) {
      const std::size_t idx = static_cast<std::size_t>(slot);
      fill_texture_pixels(slot, kSize, pixels);
      gl::GenTextures(1, &m_textures[idx]);
      gl::BindTexture(gl::GL_TEXTURE_2D, m_textures[idx]);
      gl::TexImage2D(gl::GL_TEXTURE_2D, 0, static_cast<gl::GLint>(gl::GL_RGB),
                     kSize, kSize, 0, gl::GL_RGB, gl::GL_UNSIGNED_BYTE,
                     pixels.data());
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S,
                        static_cast<gl::GLint>(gl::GL_REPEAT));
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T,
                        static_cast<gl::GLint>(gl::GL_REPEAT));
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER,
                        static_cast<gl::GLint>(gl::GL_LINEAR_MIPMAP_LINEAR));
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER,
                        static_cast<gl::GLint>(gl::GL_LINEAR));
      gl::GenerateMipmap(gl::GL_TEXTURE_2D);
    }
    gl::BindTexture(gl::GL_TEXTURE_2D, 0);
    return true;
  }

  static gl::GLuint compile(gl::GLenum type, const char* src) {
    const gl::GLuint sh = gl::CreateShader(type);
    gl::ShaderSource(sh, 1, &src, nullptr);
    gl::CompileShader(sh);
    gl::GLint ok = 0;
    gl::GetShaderiv(sh, gl::GL_COMPILE_STATUS, &ok);
    if (!ok) {
      gl::GLint len = 0;
      gl::GetShaderiv(sh, gl::GL_INFO_LOG_LENGTH, &len);
      std::string log(static_cast<std::size_t>(len), '\0');
      gl::GetShaderInfoLog(sh, len, nullptr, log.data());
      Log::error(std::string("GL compile failed: ") + log);
      gl::DeleteShader(sh);
      return 0;
    }
    return sh;
  }

  SDL_Window* m_window{nullptr};
  SDL_GLContext m_glctx{nullptr};
  int m_width{0};
  int m_height{0};
  gl::GLuint m_program{0};
  std::vector<gl::GLuint> m_textures;

  gl::GLint m_loc_model{-1};
  gl::GLint m_loc_view{-1};
  gl::GLint m_loc_proj{-1};
  gl::GLint m_loc_camera{-1};
  gl::GLint m_loc_sun_dir{-1};
  gl::GLint m_loc_sun_color{-1};
  gl::GLint m_loc_sun_intensity{-1};
  gl::GLint m_loc_ambient{-1};
  gl::GLint m_loc_fog_start{-1};
  gl::GLint m_loc_fog_end{-1};
  gl::GLint m_loc_fog_color{-1};
  gl::GLint m_loc_albedo{-1};
  gl::GLint m_loc_metallic{-1};
  gl::GLint m_loc_roughness{-1};
  gl::GLint m_loc_albedo_map{-1};
  gl::GLint m_loc_use_texture{-1};

  Mat4 m_view = Mat4::identity();
  Mat4 m_proj = Mat4::identity();
  Vec3 m_camera_pos{};
  Lighting m_lighting{};
};

}  // namespace

std::unique_ptr<IRenderBackend> create_gl_backend() {
  return std::make_unique<GlBackend>();
}

}  // namespace fury
