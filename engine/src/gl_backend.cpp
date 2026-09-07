#include "fury/renderer.hpp"

#include "fury/gl_loader.hpp"
#include "fury/log.hpp"
#include "fury/texture.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
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
uniform mat4 uLightVP0;
uniform mat4 uLightVP1;
uniform float uTime;
uniform vec2 uUvScroll;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vUV;
out vec2 vUVBase;
out vec4 vLightSpace0;
out vec4 vLightSpace1;

void main() {
  vec4 world = uModel * vec4(aPos, 1.0);
  vWorldPos = world.xyz;
  vNormal = mat3(uModel) * aNormal;
  vColor = aColor;
  vUVBase = aUV;
  vUV = aUV + uUvScroll * uTime;
  vLightSpace0 = uLightVP0 * world;
  vLightSpace1 = uLightVP1 * world;
  gl_Position = uProj * uView * world;
}
)";

const char* kFragSrc = R"(#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;
in vec2 vUVBase;
in vec4 vLightSpace0;
in vec4 vLightSpace1;

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
uniform float uEmissive;
uniform float uAoStrength;
uniform sampler2D uAlbedoMap;
uniform int uUseTexture;
uniform int uTextureSlot;
uniform float uWetness;
uniform float uTime;
uniform int uPointCount;
uniform vec3 uPointPos[4];
uniform vec3 uPointColor[4];
uniform float uPointIntensity[4];
uniform float uPointRadius[4];
uniform sampler2D uShadowMap;
uniform sampler2D uShadowMap1;
uniform int uShadowsEnabled;
uniform int uShadowCascades;
uniform float uShadowStrength;
uniform float uShadowTexel;
uniform int uReflectionsEnabled;
uniform float uReflectionStrength;
uniform int uBloomEnabled;
uniform float uBloomStrength;

out vec4 FragColor;

float sampleShadowMap(sampler2D shadowMap, vec4 lightSpace, float NdotL) {
  vec3 proj = lightSpace.xyz / max(lightSpace.w, 0.0001);
  proj = proj * 0.5 + 0.5;
  if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
    return 1.0;
  }
  float bias = max(0.0025 * (1.0 - NdotL), 0.0008);
  float cur = proj.z - bias;
  vec2 texel = vec2(uShadowTexel);
  float sum = 0.0;
  for (int x = -1; x <= 1; x += 2) {
    for (int y = -1; y <= 1; y += 2) {
      float d = texture(shadowMap, proj.xy + vec2(float(x), float(y)) * texel * 0.5).r;
      sum += cur > d ? 0.0 : 1.0;
    }
  }
  return sum * 0.25;
}

bool inShadowMap(vec4 lightSpace) {
  vec3 proj = lightSpace.xyz / max(lightSpace.w, 0.0001);
  proj = proj * 0.5 + 0.5;
  return proj.z <= 1.0 && proj.x >= 0.0 && proj.x <= 1.0 &&
         proj.y >= 0.0 && proj.y <= 1.0;
}

void main() {
  vec3 N = normalize(vNormal);
  vec3 L = normalize(-uSunDir);
  vec3 V = normalize(uCameraPos - vWorldPos);
  vec3 H = normalize(L + V);

  vec3 base = vColor * uAlbedo;
  if (uUseTexture != 0) {
    base *= texture(uAlbedoMap, vUV).rgb;
  }

  // Water: wave normal scroll, refraction tint, simple shore foam (soft/llvmpipe safe).
  if (uTextureSlot == 4) {
    vec2 wuv = vUV;
    float w1 = sin(wuv.x * 14.0 + uTime * 1.6) * cos(wuv.y * 11.0 + uTime * 1.15);
    float w2 = sin(wuv.x * 6.5 - uTime * 0.95 + wuv.y * 8.0);
    float w3 = cos(wuv.x * 22.0 + wuv.y * 18.0 - uTime * 2.1) * 0.35;
    N = normalize(N + vec3((w1 + w3) * 0.18, 0.0, (w2 + w3) * 0.18));
    H = normalize(L + V);

    float depthHint = clamp(0.35 + 0.45 * (1.0 - abs(N.y)), 0.2, 0.95);
    vec3 refractTint = vec3(0.18, 0.42, 0.55) * depthHint + vec3(0.05, 0.18, 0.28);
    base = mix(base, base * refractTint * 1.35 + refractTint * 0.25, 0.62);

    // Foam line near shore — unscrolled UV edges of the water plane.
    float edgeU = min(vUVBase.x, 1.0 - vUVBase.x);
    float edgeV = min(vUVBase.y, 1.0 - vUVBase.y);
    float shore = 1.0 - smoothstep(0.0, 0.085, min(edgeU, edgeV));
    float foamNoise = 0.55 + 0.45 * sin(vUV.x * 40.0 + uTime * 3.0) *
                                cos(vUV.y * 36.0 - uTime * 2.4);
    float foam = clamp(shore * foamNoise, 0.0, 1.0);
    base = mix(base, vec3(0.78, 0.90, 0.96), foam * 0.82);
  }

  float NdotL = max(dot(N, L), 0.0);
  float diff = NdotL;

  float shininess = mix(128.0, 4.0, clamp(uRoughness, 0.04, 1.0));
  float NdotH = max(dot(N, H), 0.0);
  float spec = pow(NdotH, shininess) * (1.0 - uRoughness * 0.85);

  // Anisotropic-ish specular hack for wet asphalt (stretch along road tangent).
  float wet = clamp(uWetness, 0.0, 1.0);
  if (uTextureSlot == 2 && wet > 0.01) {
    // Prefer world X as road streak direction; fall back to Z if N faces X.
    vec3 T = normalize(abs(N.x) > 0.7 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0));
    T = normalize(T - N * dot(N, T));
    float TdotH = dot(T, H);
    float aniso = pow(max(1.0 - TdotH * TdotH, 0.0), mix(8.0, 48.0, wet));
    float iso = pow(NdotH, shininess);
    spec = mix(iso, mix(iso, aniso, 0.72), wet) * (1.0 - uRoughness * 0.7);
    spec *= (1.0 + 1.4 * wet);
  }

  vec3 specCol = mix(vec3(0.04), base, clamp(uMetallic, 0.0, 1.0));
  float metalDiff = 1.0 - uMetallic * 0.9;

  // Glass: softer fresnel edge tint
  if (uTextureSlot == 7) {
    float gf = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 2.2);
    base = mix(base, base * vec3(0.55, 0.75, 0.95) + vec3(0.12, 0.22, 0.35), gf * 0.55);
    specCol = mix(specCol, vec3(0.55, 0.7, 0.9), 0.45);
  }

  // SSAO-lite (single-pass): hemisphere + cavity darkening — cheap on llvmpipe.
  float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
  float cavity = clamp(dot(N, V), 0.0, 1.0);
  float ao = mix(0.42, 1.0, hemi) * mix(0.65, 1.0, cavity);
  ao = mix(1.0, ao, clamp(uAoStrength, 0.0, 1.0));

  float shadow = 1.0;
  if (uShadowsEnabled != 0) {
    // Prefer near cascade when fragment projects inside it; else far (or single map).
    if (uShadowCascades >= 2 && inShadowMap(vLightSpace0)) {
      shadow = sampleShadowMap(uShadowMap, vLightSpace0, NdotL);
    } else if (uShadowCascades >= 2) {
      shadow = sampleShadowMap(uShadowMap1, vLightSpace1, NdotL);
    } else {
      shadow = sampleShadowMap(uShadowMap, vLightSpace0, NdotL);
    }
    shadow = mix(1.0, shadow, clamp(uShadowStrength, 0.0, 1.0));
  }

  vec3 lit = uAmbient * base * ao
           + uSunColor * uSunIntensity * (base * diff * metalDiff + specCol * spec) * ao * shadow
           + base * uEmissive;

  // Dynamic point lights (street lamps) — up to 4 nearest.
  int pc = clamp(uPointCount, 0, 4);
  for (int i = 0; i < 4; ++i) {
    if (i >= pc) break;
    vec3 toL = uPointPos[i] - vWorldPos;
    float distL = length(toL);
    float rad = max(uPointRadius[i], 0.5);
    float atten = 1.0 - clamp(distL / rad, 0.0, 1.0);
    atten = atten * atten;
    vec3 Lp = toL / max(distL, 0.001);
    float nd = max(dot(N, Lp), 0.0);
    vec3 Hp = normalize(Lp + V);
    float sp = pow(max(dot(N, Hp), 0.0), shininess) * (1.0 - uRoughness * 0.85);
    if (uTextureSlot == 2 && wet > 0.01) {
      vec3 T = normalize(abs(N.x) > 0.7 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0));
      T = normalize(T - N * dot(N, T));
      float TdotH = dot(T, Hp);
      float aniso = pow(max(1.0 - TdotH * TdotH, 0.0), mix(8.0, 40.0, wet));
      sp = mix(sp, aniso, 0.65 * wet) * (1.0 + wet);
    }
    lit += uPointColor[i] * uPointIntensity[i] * atten *
           (base * nd * metalDiff + specCol * sp) * ao;
  }

  // Reflection stub — Schlick-ish fresnel for water (planar approx without 2nd camera).
  // Disabled on soft/llvmpipe via uReflectionsEnabled.
  if (uReflectionsEnabled != 0 && uTextureSlot == 4) {
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    float F0 = 0.02;
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    fresnel = clamp(fresnel * 1.2, 0.0, 1.0);
    vec3 R = reflect(-V, N);
    // Fake env: sky/fog lobe by reflected Y + cool water specular streak
    float sky = clamp(R.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 env = mix(uFogColor * 0.55, uSunColor * 0.85 + uFogColor * 0.35, sky);
    env += uSunColor * uSunIntensity * 0.28 * pow(max(dot(R, L), 0.0), 28.0);
    lit = mix(lit, mix(lit, env, 0.78), fresnel * clamp(uReflectionStrength, 0.0, 1.0));
  }

  // Bloom-lite — bright-pass add for emissives (cheap; no fullscreen blur).
  if (uBloomEnabled != 0 && uEmissive > 0.05) {
    float bright = max(max(base.r, base.g), base.b) * uEmissive;
    float pass = max(bright - 0.55, 0.0);
    lit += base * (pass * pass) * (1.2 + uEmissive) * clamp(uBloomStrength, 0.0, 1.5);
  }

  float dist = length(uCameraPos - vWorldPos);
  float fog = clamp((uFogEnd - dist) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
  vec3 color = mix(uFogColor, lit, fog);

  // Reinhard tonemap + gamma
  color = color / (color + vec3(1.0));
  color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

  FragColor = vec4(color, 1.0);
}
)";

const char* kHudVertSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
void main() {
  gl_Position = vec4(aPos, 0.0, 1.0);
}
)";


const char* kShadowVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uModel;
uniform mat4 uLightVP;
void main() {
  gl_Position = uLightVP * uModel * vec4(aPos, 1.0);
}
)";

const char* kShadowFragSrc = R"(#version 330 core
void main() {
  // depth-only
}
)";

const char* kHudFragSrc = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
  FragColor = uColor;
}
)";

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

    if (!build_program() || !build_hud_program() || !build_textures() ||
        !build_hud_geometry()) {
      destroy();
      return false;
    }

    gl::Enable(gl::GL_DEPTH_TEST);
    gl::DepthFunc(gl::GL_LESS);
    gl::Enable(gl::GL_CULL_FACE);
    gl::CullFace(gl::GL_BACK);
    gl::FrontFace(gl::GL_CCW);
    gl::Viewport(0, 0, m_width, m_height);

    detect_soft_renderer();
    {
      int cascades = m_lighting.shadow_cascade_count;
      if (cascades < 1) cascades = 1;
      if (cascades > kMaxShadowCascades) cascades = kMaxShadowCascades;
      m_cascade_count = cascades;
    }
    init_shadow_resources();

    Log::info(std::string("Renderer backend: OpenGL 3.3 (lit + point lights + AO-lite")
              + (m_shadows_ready ? " + shadows" : "")
              + (m_reflections_ready ? " + water-reflect" : "")
              + (m_bloom_ready ? " + bloom-lite" : "")
              + " + tonemap + HUD)"
              + (m_soft_gl ? " [soft/llvmpipe — shadows/reflect off]" : ""));
    return true;
  }

  void destroy() override {
    destroy_shadow_resources();
    if (m_hud_vao) {
      gl::DeleteVertexArrays(1, &m_hud_vao);
      m_hud_vao = 0;
    }
    if (m_hud_vbo) {
      gl::DeleteBuffers(1, &m_hud_vbo);
      m_hud_vbo = 0;
    }
    for (gl::GLuint& tex : m_textures) {
      if (tex) {
        gl::DeleteTextures(1, &tex);
        tex = 0;
      }
    }
    if (m_hud_program) {
      gl::DeleteProgram(m_hud_program);
      m_hud_program = 0;
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
    gl::Enable(gl::GL_DEPTH_TEST);
    gl::Disable(gl::GL_BLEND);
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
    gl::Uniform1f(m_loc_ao, m_lighting.ao_strength);
    gl::Uniform1f(m_loc_time, m_time);
    gl::Uniform1i(m_loc_albedo_map, 0);
    gl::Uniform1i(m_loc_shadow_map, 1);
    gl::Uniform1i(m_loc_shadow_map1, 2);
    const bool shadows_on = m_shadows_ready && m_lighting.enable_shadows && !m_soft_gl;
    gl::Uniform1i(m_loc_shadows_enabled, shadows_on ? 1 : 0);
    gl::Uniform1i(m_loc_shadow_cascades, shadows_on ? m_cascade_count : 0);
    gl::Uniform1f(m_loc_shadow_strength, m_lighting.shadow_strength);
    {
      const float texel =
          1.f / static_cast<float>((std::max)(1, m_shadow_map_size));
      gl::Uniform1f(m_loc_shadow_texel, texel);
    }
    const bool refl_on =
        m_reflections_ready && m_lighting.enable_reflections && !m_soft_gl;
    gl::Uniform1i(m_loc_reflections_enabled, refl_on ? 1 : 0);
    gl::Uniform1f(m_loc_reflection_strength, m_lighting.reflection_strength);
    const bool bloom_on = m_bloom_ready && m_lighting.enable_bloom;
    gl::Uniform1i(m_loc_bloom_enabled, bloom_on ? 1 : 0);
    gl::Uniform1f(m_loc_bloom_strength, m_lighting.bloom_strength);
    gl::UniformMatrix4fv(m_loc_light_vp0, 1, gl::GL_FALSE_, m_light_vp_cascades[0].m);
    gl::UniformMatrix4fv(m_loc_light_vp1, 1, gl::GL_FALSE_, m_light_vp_cascades[1].m);
    gl::ActiveTexture(gl::GL_TEXTURE1);
    gl::BindTexture(gl::GL_TEXTURE_2D,
                    shadows_on ? m_shadow_depth_tex[0] : m_textures[0]);
    gl::ActiveTexture(gl::GL_TEXTURE2);
    gl::BindTexture(gl::GL_TEXTURE_2D,
                    (shadows_on && m_cascade_count >= 2) ? m_shadow_depth_tex[1]
                                                         : m_textures[0]);
    gl::ActiveTexture(gl::GL_TEXTURE0);

    const int pc = (std::max)(0, (std::min)(m_lighting.point_light_count,
                                        Lighting::kMaxPointLights));
    gl::Uniform1i(m_loc_point_count, pc);
    for (int i = 0; i < Lighting::kMaxPointLights; ++i) {
      float pos[3] = {0.f, 0.f, 0.f};
      float col[3] = {0.f, 0.f, 0.f};
      float intensity = 0.f;
      float radius = 1.f;
      if (i < pc) {
        const auto& pl = m_lighting.point_lights[i];
        pos[0] = pl.position.x;
        pos[1] = pl.position.y;
        pos[2] = pl.position.z;
        col[0] = pl.color.x;
        col[1] = pl.color.y;
        col[2] = pl.color.z;
        intensity = pl.intensity;
        radius = pl.radius;
      }
      gl::Uniform3fv(m_loc_point_pos[i], 1, pos);
      gl::Uniform3fv(m_loc_point_color[i], 1, col);
      gl::Uniform1f(m_loc_point_intensity[i], intensity);
      gl::Uniform1f(m_loc_point_radius[i], radius);
    }
  }

  void set_view_proj(const Mat4& view, const Mat4& proj) override {
    m_view = view;
    m_proj = proj;
  }

  void set_camera_position(const Vec3& pos) override { m_camera_pos = pos; }

  void set_lighting(const Lighting& lighting) override {
    m_lighting = lighting;
    int cascades = lighting.shadow_cascade_count;
    if (cascades < 1) cascades = 1;
    if (cascades > kMaxShadowCascades) cascades = kMaxShadowCascades;
    const bool cascade_changed = cascades != m_cascade_count;
    m_cascade_count = cascades;
    if (lighting.shadow_map_size > 0) {
      const int prev = m_shadow_map_size;
      set_shadow_map_size(lighting.shadow_map_size);
      // set_shadow_map_size no-ops when size unchanged — still rebuild on cascade flip
      if (cascade_changed && m_shadow_map_size == prev && m_glctx && !m_soft_gl) {
        init_shadow_resources();
      }
    } else if (cascade_changed && m_glctx && !m_soft_gl) {
      init_shadow_resources();
    }
  }

  void set_time(float seconds) override { m_time = seconds; }

  void upload_mesh(Mesh& mesh) override {
    if (mesh.gpu_uploaded && !mesh.gpu_dirty) {
      return;
    }
    if (!mesh.gpu_vao) {
      gl::GenVertexArrays(1, &mesh.gpu_vao);
      gl::GenBuffers(1, &mesh.gpu_vbo);
      gl::GenBuffers(1, &mesh.gpu_ibo);
    }

    gl::BindVertexArray(mesh.gpu_vao);
    gl::BindBuffer(gl::GL_ARRAY_BUFFER, mesh.gpu_vbo);
    gl::BufferData(gl::GL_ARRAY_BUFFER,
                   static_cast<gl::GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                   mesh.vertices.data(), gl::GL_DYNAMIC_DRAW);
    gl::BindBuffer(gl::GL_ELEMENT_ARRAY_BUFFER, mesh.gpu_ibo);
    gl::BufferData(
        gl::GL_ELEMENT_ARRAY_BUFFER,
        static_cast<gl::GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
        mesh.indices.data(), gl::GL_DYNAMIC_DRAW);

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
    mesh.gpu_dirty = false;
  }

  void draw_mesh(const Mesh& mesh, const Mat4& model,
                 const Material& material) override {
    if (mesh.indices.empty()) {
      return;
    }
    Mesh& mutable_mesh = const_cast<Mesh&>(mesh);
    if (!mutable_mesh.gpu_uploaded || mutable_mesh.gpu_dirty) {
      upload_mesh(mutable_mesh);
    }

    if (m_in_shadow_pass) {
      if (!m_shadow_program) return;
      gl::UseProgram(m_shadow_program);
      gl::UniformMatrix4fv(m_loc_shadow_model, 1, gl::GL_FALSE_, model.m);
      gl::UniformMatrix4fv(m_loc_shadow_light_vp, 1, gl::GL_FALSE_,
                           m_light_vp_cascades[m_active_cascade].m);
      gl::BindVertexArray(mesh.gpu_vao);
      gl::DrawElements(gl::GL_TRIANGLES,
                       static_cast<gl::GLsizei>(mesh.indices.size()),
                       gl::GL_UNSIGNED_INT, nullptr);
      gl::BindVertexArray(0);
      return;
    }

    gl::UseProgram(m_program);
    gl::UniformMatrix4fv(m_loc_model, 1, gl::GL_FALSE_, model.m);
    gl::UniformMatrix4fv(m_loc_view, 1, gl::GL_FALSE_, m_view.m);
    gl::UniformMatrix4fv(m_loc_proj, 1, gl::GL_FALSE_, m_proj.m);
    gl::Uniform2f(m_loc_uv_scroll, material.uv_scroll_u, material.uv_scroll_v);
    gl::Uniform1f(m_loc_time, m_time);

    const float albedo[3] = {material.albedo.x, material.albedo.y,
                             material.albedo.z};
    gl::Uniform3fv(m_loc_albedo, 1, albedo);
    gl::Uniform1f(m_loc_metallic, material.metallic);
    gl::Uniform1f(m_loc_roughness, material.roughness);
    gl::Uniform1f(m_loc_emissive, material.emissive);
    gl::Uniform1f(m_loc_wetness, material.wetness);

    const int slot = static_cast<int>(material.texture);
    const bool use_tex =
        slot > 0 && slot < static_cast<int>(TextureSlot::Count) &&
        m_textures[static_cast<std::size_t>(slot)] != 0;
    gl::Uniform1i(m_loc_use_texture, use_tex ? 1 : 0);
    gl::Uniform1i(m_loc_texture_slot, use_tex ? slot : 0);
    gl::ActiveTexture(gl::GL_TEXTURE0);
    if (use_tex) {
      gl::BindTexture(gl::GL_TEXTURE_2D,
                      m_textures[static_cast<std::size_t>(slot)]);
    } else {
      gl::BindTexture(gl::GL_TEXTURE_2D, m_textures[0]);
    }

    gl::BindVertexArray(mesh.gpu_vao);
    gl::DrawElements(gl::GL_TRIANGLES,
                     static_cast<gl::GLsizei>(mesh.indices.size()),
                     gl::GL_UNSIGNED_INT, nullptr);
    gl::BindVertexArray(0);
  }

  void draw_hud_rect(float x, float y, float w, float h,
                     const Color& color) override {
    if (w <= 0.f || h <= 0.f || m_width <= 0 || m_height <= 0) {
      return;
    }
    const float iw = static_cast<float>(m_width);
    const float ih = static_cast<float>(m_height);
    auto to_ndc_x = [&](float px) { return (px / iw) * 2.f - 1.f; };
    auto to_ndc_y = [&](float py) { return 1.f - (py / ih) * 2.f; };

    const float x0 = to_ndc_x(x);
    const float y0 = to_ndc_y(y);
    const float x1 = to_ndc_x(x + w);
    const float y1 = to_ndc_y(y + h);
    const float verts[12] = {
        x0, y0, x1, y0, x1, y1,
        x0, y0, x1, y1, x0, y1,
    };

    gl::Disable(gl::GL_DEPTH_TEST);
    gl::Enable(gl::GL_BLEND);
    gl::BlendFunc(gl::GL_SRC_ALPHA, gl::GL_ONE_MINUS_SRC_ALPHA);
    gl::UseProgram(m_hud_program);
    const float col[4] = {color.r / 255.f, color.g / 255.f, color.b / 255.f,
                          color.a / 255.f};
    gl::Uniform4fv(m_loc_hud_color, 1, col);
    gl::BindVertexArray(m_hud_vao);
    gl::BindBuffer(gl::GL_ARRAY_BUFFER, m_hud_vbo);
    gl::BufferData(gl::GL_ARRAY_BUFFER, sizeof(verts), verts, gl::GL_DYNAMIC_DRAW);
    gl::DrawArrays(gl::GL_TRIANGLES, 0, 6);
    gl::BindVertexArray(0);
    gl::Disable(gl::GL_BLEND);
    gl::Enable(gl::GL_DEPTH_TEST);
    gl::UseProgram(m_program);
  }

  void end_frame() override { SDL_GL_SwapWindow(m_window); }

  bool read_rgb_framebuffer(std::vector<std::uint8_t>& out_rgb, int& w,
                            int& h) override {
    if (!m_glctx || !gl::ReadPixels) {
      return false;
    }
    SDL_GL_MakeCurrent(m_window, m_glctx);
    w = m_width;
    h = m_height;
    if (w <= 0 || h <= 0) {
      return false;
    }
    // Prefer drawable size when available (matches glReadPixels / HiDPI).
    int dw = 0, dh = 0;
    SDL_GL_GetDrawableSize(m_window, &dw, &dh);
    if (dw > 0 && dh > 0) {
      w = dw;
      h = dh;
    }
    out_rgb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u);
    if (gl::PixelStorei) {
      gl::PixelStorei(gl::GL_PACK_ALIGNMENT, 1);
    }
    gl::ReadPixels(0, 0, w, h, gl::GL_RGB, gl::GL_UNSIGNED_BYTE, out_rgb.data());
    // GL origin is bottom-left — flip to top-left for PPM / viewers.
    const std::size_t row = static_cast<std::size_t>(w) * 3u;
    std::vector<std::uint8_t> tmp(row);
    for (int y = 0; y < h / 2; ++y) {
      std::uint8_t* a = out_rgb.data() + static_cast<std::size_t>(y) * row;
      std::uint8_t* b = out_rgb.data() + static_cast<std::size_t>(h - 1 - y) * row;
      std::memcpy(tmp.data(), a, row);
      std::memcpy(a, b, row);
      std::memcpy(b, tmp.data(), row);
    }
    return true;
  }

  void resize(int width, int height) override {
    m_width = width;
    m_height = height;
    if (m_glctx) {
      gl::Viewport(0, 0, width, height);
    }
  }

  RenderBackendKind kind() const override { return RenderBackendKind::OpenGL; }
  const char* name() const override {
    if (m_shadows_ready && m_reflections_ready) {
      return m_cascade_count >= 2
                 ? "OpenGL 3.3 lit+points+AO+csm2+reflect+bloom"
                 : "OpenGL 3.3 lit+points+AO+shadows+reflect+bloom";
    }
    if (m_shadows_ready) {
      return m_cascade_count >= 2 ? "OpenGL 3.3 lit+points+AO+csm2+bloom"
                                  : "OpenGL 3.3 lit+points+AO+shadows+bloom";
    }
    return m_reflections_ready ? "OpenGL 3.3 lit+points+AO+reflect+bloom"
                               : "OpenGL 3.3 lit+points+AO+bloom";
  }

 private:

  bool begin_shadow_pass(int cascade = 0) override {
    if (!m_shadows_ready || m_soft_gl || !m_lighting.enable_shadows) {
      return false;
    }
    if (cascade < 0 || cascade >= m_cascade_count) {
      return false;
    }
    m_active_cascade = cascade;

    // Build light view-proj around camera (directional sun).
    // Cascade 0 = near (tighter), cascade 1 = far — high quality only.
    Vec3 sun = m_lighting.sun_direction;
    const float sl = std::sqrt(sun.x * sun.x + sun.y * sun.y + sun.z * sun.z);
    if (sl < 1e-4f) {
      sun = Vec3{-0.4f, -0.85f, -0.3f};
    } else {
      sun.x /= sl; sun.y /= sl; sun.z /= sl;
    }
    Vec3 focus = m_camera_pos;
    focus.y = 0.f;

    float extent = 48.f;
    float eye_dist = 55.f;
    float z_far = 140.f;
    if (m_cascade_count >= 2) {
      if (cascade == 0) {
        extent = 24.f;
        eye_dist = 42.f;
        z_far = 100.f;
      } else {
        extent = 56.f;
        eye_dist = 70.f;
        z_far = 170.f;
      }
    }

    const Vec3 eye = {focus.x - sun.x * eye_dist, focus.y - sun.y * eye_dist,
                      focus.z - sun.z * eye_dist};
    const Mat4 light_view = look_at(eye, focus, Vec3{0.f, 1.f, 0.f});
    const Mat4 light_proj =
        orthographic(-extent, extent, -extent, extent, 1.f, z_far);
    m_light_vp_cascades[cascade] = light_proj * light_view;

    gl::BindFramebuffer(gl::GL_FRAMEBUFFER, m_shadow_fbo[cascade]);
    gl::Viewport(0, 0, m_shadow_map_size, m_shadow_map_size);
    gl::Clear(gl::GL_DEPTH_BUFFER_BIT);
    gl::Enable(gl::GL_DEPTH_TEST);
    gl::Disable(gl::GL_BLEND);
    gl::CullFace(gl::GL_FRONT);  // reduce shadow acne
    m_in_shadow_pass = true;
    return true;
  }

  void end_shadow_pass() override {
    if (!m_in_shadow_pass) return;
    m_in_shadow_pass = false;
    gl::CullFace(gl::GL_BACK);
    gl::BindFramebuffer(gl::GL_FRAMEBUFFER, 0);
    gl::Viewport(0, 0, m_width, m_height);
  }

  bool shadows_active() const override {
    return m_shadows_ready && !m_soft_gl && m_lighting.enable_shadows;
  }

  int shadow_cascade_count() const override {
    if (!m_shadows_ready || m_soft_gl || !m_lighting.enable_shadows) {
      return 0;
    }
    return m_cascade_count;
  }

  void set_shadow_map_size(int size) override {
    int s = size;
    if (s < 256) s = 256;
    if (s > 4096) s = 4096;
    // Snap to power-of-two-ish common sizes
    if (s <= 512) s = 512;
    else if (s <= 1024) s = 1024;
    else if (s <= 2048) s = 2048;
    else s = 4096;
    if (s == m_shadow_map_size && m_shadows_ready) return;
    m_shadow_map_size = s;
    if (m_glctx && !m_soft_gl) {
      init_shadow_resources();
    }
  }

  int shadow_map_size() const override { return m_shadow_map_size; }

  void detect_soft_renderer() {
    m_soft_gl = false;
    if (!gl::GetString) return;
    const char* renderer =
        reinterpret_cast<const char*>(gl::GetString(gl::GL_RENDERER));
    if (!renderer) return;
    std::string r = renderer;
    for (char& c : r) {
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    if (r.find("llvmpipe") != std::string::npos ||
        r.find("softpipe") != std::string::npos ||
        r.find("software") != std::string::npos ||
        r.find("swiftshader") != std::string::npos) {
      m_soft_gl = true;
      Log::info(std::string("GL renderer soft path detected (") + renderer +
                ") — directional shadows disabled");
    }
    if (const char* env = std::getenv("FURY_SHADOWS")) {
      if (env[0] == '0' || env[0] == 'f' || env[0] == 'F' || env[0] == 'n' ||
          env[0] == 'N') {
        m_soft_gl = true;  // force off
        Log::info("FURY_SHADOWS disabled — directional shadows off");
      }
    }
    m_reflections_ready = !m_soft_gl;
    m_bloom_ready = true;  // in-shader; cheap even on soft GL
    if (const char* env = std::getenv("FURY_REFLECTIONS")) {
      if (env[0] == '0' || env[0] == 'f' || env[0] == 'F' || env[0] == 'n' ||
          env[0] == 'N') {
        m_reflections_ready = false;
        Log::info("FURY_REFLECTIONS disabled — water reflection stub off");
      }
    }
    if (m_soft_gl) {
      m_reflections_ready = false;
    }
    if (const char* env = std::getenv("FURY_BLOOM")) {
      if (env[0] == '0' || env[0] == 'f' || env[0] == 'F' || env[0] == 'n' ||
          env[0] == 'N') {
        m_bloom_ready = false;
        Log::info("FURY_BLOOM disabled — bloom-lite off");
      }
    }
  }

  void destroy_shadow_resources() {
    for (int i = 0; i < kMaxShadowCascades; ++i) {
      if (m_shadow_fbo[i]) {
        gl::DeleteFramebuffers(1, &m_shadow_fbo[i]);
        m_shadow_fbo[i] = 0;
      }
      if (m_shadow_depth_tex[i]) {
        gl::DeleteTextures(1, &m_shadow_depth_tex[i]);
        m_shadow_depth_tex[i] = 0;
      }
    }
    if (m_shadow_program) {
      gl::DeleteProgram(m_shadow_program);
      m_shadow_program = 0;
    }
    m_shadows_ready = false;
    m_in_shadow_pass = false;
    m_active_cascade = 0;
  }

  void init_shadow_resources() {
    destroy_shadow_resources();
    if (m_soft_gl) return;
    if (!gl::GenFramebuffers || !gl::BindFramebuffer || !gl::FramebufferTexture2D ||
        !gl::CheckFramebufferStatus || !gl::DrawBuffer || !gl::ReadBuffer) {
      Log::info("GL FBO entry points missing — shadows disabled");
      return;
    }

    int cascades = m_cascade_count;
    if (cascades < 1) cascades = 1;
    if (cascades > kMaxShadowCascades) cascades = kMaxShadowCascades;
    m_cascade_count = cascades;

    const gl::GLuint vs = compile(gl::GL_VERTEX_SHADER, kShadowVertSrc);
    const gl::GLuint fs = compile(gl::GL_FRAGMENT_SHADER, kShadowFragSrc);
    if (!vs || !fs) {
      if (vs) gl::DeleteShader(vs);
      if (fs) gl::DeleteShader(fs);
      Log::warn("Shadow shader compile failed — shadows disabled");
      return;
    }
    m_shadow_program = gl::CreateProgram();
    gl::AttachShader(m_shadow_program, vs);
    gl::AttachShader(m_shadow_program, fs);
    gl::LinkProgram(m_shadow_program);
    gl::DeleteShader(vs);
    gl::DeleteShader(fs);
    gl::GLint ok = 0;
    gl::GetProgramiv(m_shadow_program, gl::GL_LINK_STATUS, &ok);
    if (!ok) {
      Log::warn("Shadow shader link failed — shadows disabled");
      destroy_shadow_resources();
      return;
    }
    m_loc_shadow_model = gl::GetUniformLocation(m_shadow_program, "uModel");
    m_loc_shadow_light_vp = gl::GetUniformLocation(m_shadow_program, "uLightVP");

    for (int c = 0; c < m_cascade_count; ++c) {
      gl::GenTextures(1, &m_shadow_depth_tex[c]);
      gl::BindTexture(gl::GL_TEXTURE_2D, m_shadow_depth_tex[c]);
      gl::TexImage2D(gl::GL_TEXTURE_2D, 0,
                     static_cast<gl::GLint>(gl::GL_DEPTH_COMPONENT24),
                     m_shadow_map_size, m_shadow_map_size, 0, gl::GL_DEPTH_COMPONENT,
                     gl::GL_FLOAT, nullptr);
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MIN_FILTER,
                        static_cast<gl::GLint>(gl::GL_NEAREST));
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_MAG_FILTER,
                        static_cast<gl::GLint>(gl::GL_NEAREST));
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_S,
                        static_cast<gl::GLint>(gl::GL_CLAMP_TO_EDGE));
      gl::TexParameteri(gl::GL_TEXTURE_2D, gl::GL_TEXTURE_WRAP_T,
                        static_cast<gl::GLint>(gl::GL_CLAMP_TO_EDGE));

      gl::GenFramebuffers(1, &m_shadow_fbo[c]);
      gl::BindFramebuffer(gl::GL_FRAMEBUFFER, m_shadow_fbo[c]);
      gl::FramebufferTexture2D(gl::GL_FRAMEBUFFER, gl::GL_DEPTH_ATTACHMENT,
                               gl::GL_TEXTURE_2D, m_shadow_depth_tex[c], 0);
      gl::DrawBuffer(gl::GL_NONE);
      gl::ReadBuffer(gl::GL_NONE);
      const gl::GLenum status = gl::CheckFramebufferStatus(gl::GL_FRAMEBUFFER);
      gl::BindFramebuffer(gl::GL_FRAMEBUFFER, 0);
      if (status != gl::GL_FRAMEBUFFER_COMPLETE) {
        Log::warn("Shadow FBO incomplete — shadows disabled");
        destroy_shadow_resources();
        return;
      }
    }
    m_shadows_ready = true;
    Log::info(std::string("Directional shadow map ready (") +
              std::to_string(m_shadow_map_size) + "² × " +
              std::to_string(m_cascade_count) + " cascade" +
              (m_cascade_count > 1 ? "s" : "") +
              ", feature-flag Lighting.enable_shadows / FURY_SHADOWS)");
  }

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
    m_loc_emissive = gl::GetUniformLocation(m_program, "uEmissive");
    m_loc_ao = gl::GetUniformLocation(m_program, "uAoStrength");
    m_loc_time = gl::GetUniformLocation(m_program, "uTime");
    m_loc_uv_scroll = gl::GetUniformLocation(m_program, "uUvScroll");
    m_loc_albedo_map = gl::GetUniformLocation(m_program, "uAlbedoMap");
    m_loc_use_texture = gl::GetUniformLocation(m_program, "uUseTexture");
    m_loc_texture_slot = gl::GetUniformLocation(m_program, "uTextureSlot");
    m_loc_wetness = gl::GetUniformLocation(m_program, "uWetness");
    m_loc_point_count = gl::GetUniformLocation(m_program, "uPointCount");
        m_loc_light_vp0 = gl::GetUniformLocation(m_program, "uLightVP0");
    m_loc_light_vp1 = gl::GetUniformLocation(m_program, "uLightVP1");
    m_loc_shadow_map = gl::GetUniformLocation(m_program, "uShadowMap");
    m_loc_shadow_map1 = gl::GetUniformLocation(m_program, "uShadowMap1");
    m_loc_shadows_enabled = gl::GetUniformLocation(m_program, "uShadowsEnabled");
    m_loc_shadow_cascades = gl::GetUniformLocation(m_program, "uShadowCascades");
    m_loc_shadow_strength = gl::GetUniformLocation(m_program, "uShadowStrength");
    m_loc_shadow_texel = gl::GetUniformLocation(m_program, "uShadowTexel");
    m_loc_reflections_enabled =
        gl::GetUniformLocation(m_program, "uReflectionsEnabled");
    m_loc_reflection_strength =
        gl::GetUniformLocation(m_program, "uReflectionStrength");
    m_loc_bloom_enabled = gl::GetUniformLocation(m_program, "uBloomEnabled");
    m_loc_bloom_strength = gl::GetUniformLocation(m_program, "uBloomStrength");
    for (int i = 0; i < Lighting::kMaxPointLights; ++i) {
      const std::string idx = std::to_string(i);
      m_loc_point_pos[i] =
          gl::GetUniformLocation(m_program, ("uPointPos[" + idx + "]").c_str());
      m_loc_point_color[i] =
          gl::GetUniformLocation(m_program, ("uPointColor[" + idx + "]").c_str());
      m_loc_point_intensity[i] = gl::GetUniformLocation(
          m_program, ("uPointIntensity[" + idx + "]").c_str());
      m_loc_point_radius[i] = gl::GetUniformLocation(
          m_program, ("uPointRadius[" + idx + "]").c_str());
    }
    return true;
  }

  bool build_hud_program() {
    const gl::GLuint vs = compile(gl::GL_VERTEX_SHADER, kHudVertSrc);
    const gl::GLuint fs = compile(gl::GL_FRAGMENT_SHADER, kHudFragSrc);
    if (!vs || !fs) {
      if (vs) gl::DeleteShader(vs);
      if (fs) gl::DeleteShader(fs);
      return false;
    }
    m_hud_program = gl::CreateProgram();
    gl::AttachShader(m_hud_program, vs);
    gl::AttachShader(m_hud_program, fs);
    gl::LinkProgram(m_hud_program);
    gl::DeleteShader(vs);
    gl::DeleteShader(fs);
    gl::GLint ok = 0;
    gl::GetProgramiv(m_hud_program, gl::GL_LINK_STATUS, &ok);
    if (!ok) {
      Log::error("HUD shader link failed");
      return false;
    }
    m_loc_hud_color = gl::GetUniformLocation(m_hud_program, "uColor");
    return true;
  }

  bool build_hud_geometry() {
    gl::GenVertexArrays(1, &m_hud_vao);
    gl::GenBuffers(1, &m_hud_vbo);
    gl::BindVertexArray(m_hud_vao);
    gl::BindBuffer(gl::GL_ARRAY_BUFFER, m_hud_vbo);
    gl::BufferData(gl::GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr,
                   gl::GL_DYNAMIC_DRAW);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 2, gl::GL_FLOAT, gl::GL_FALSE_,
                            static_cast<gl::GLsizei>(sizeof(float) * 2), nullptr);
    gl::BindVertexArray(0);
    return true;
  }

  bool build_textures() {
    m_textures.assign(static_cast<std::size_t>(TextureSlot::Count), 0);

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

    // 5.2.0 — file albedo (PNG via STB / PPM) for Wood/BarrelMetal/Asphalt when present
    const TextureSlot slots[] = {
        TextureSlot::Checker,     TextureSlot::Asphalt, TextureSlot::Concrete,
        TextureSlot::Water,       TextureSlot::Brick,   TextureSlot::Metal,
        TextureSlot::Glass,       TextureSlot::Wood,    TextureSlot::BarrelMetal};
    constexpr int kSize = 64;
    for (TextureSlot slot : slots) {
      const std::size_t idx = static_cast<std::size_t>(slot);
      Image img;
      if (!resolve_texture_pixels(slot, kSize, img) || img.rgb.empty()) {
        continue;
      }
      gl::GenTextures(1, &m_textures[idx]);
      gl::BindTexture(gl::GL_TEXTURE_2D, m_textures[idx]);
      gl::TexImage2D(gl::GL_TEXTURE_2D, 0, static_cast<gl::GLint>(gl::GL_RGB),
                     img.width, img.height, 0, gl::GL_RGB, gl::GL_UNSIGNED_BYTE,
                     img.rgb.data());
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
  gl::GLuint m_hud_program{0};
  gl::GLuint m_hud_vao{0};
  gl::GLuint m_hud_vbo{0};
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
  gl::GLint m_loc_emissive{-1};
  gl::GLint m_loc_ao{-1};
  gl::GLint m_loc_time{-1};
  gl::GLint m_loc_uv_scroll{-1};
  gl::GLint m_loc_albedo_map{-1};
  gl::GLint m_loc_use_texture{-1};
  gl::GLint m_loc_texture_slot{-1};
  gl::GLint m_loc_wetness{-1};
  gl::GLint m_loc_point_count{-1};
  gl::GLint m_loc_point_pos[Lighting::kMaxPointLights]{};
  gl::GLint m_loc_point_color[Lighting::kMaxPointLights]{};
  gl::GLint m_loc_point_intensity[Lighting::kMaxPointLights]{};
  gl::GLint m_loc_point_radius[Lighting::kMaxPointLights]{};
  gl::GLint m_loc_hud_color{-1};
  gl::GLint m_loc_light_vp0{-1};
  gl::GLint m_loc_light_vp1{-1};
  gl::GLint m_loc_shadow_map{-1};
  gl::GLint m_loc_shadow_map1{-1};
  gl::GLint m_loc_shadows_enabled{-1};
  gl::GLint m_loc_shadow_cascades{-1};
  gl::GLint m_loc_shadow_strength{-1};
  gl::GLint m_loc_reflections_enabled{-1};
  gl::GLint m_loc_reflection_strength{-1};
  gl::GLint m_loc_bloom_enabled{-1};
  gl::GLint m_loc_bloom_strength{-1};

  static constexpr int kMaxShadowCascades = 2;
  int m_shadow_map_size{1024};
  int m_cascade_count{1};
  int m_active_cascade{0};
  gl::GLint m_loc_shadow_texel{-1};
  gl::GLuint m_shadow_fbo[kMaxShadowCascades]{};
  gl::GLuint m_shadow_depth_tex[kMaxShadowCascades]{};
  gl::GLuint m_shadow_program{0};
  gl::GLint m_loc_shadow_model{-1};
  gl::GLint m_loc_shadow_light_vp{-1};
  bool m_shadows_ready{false};
  bool m_in_shadow_pass{false};
  bool m_soft_gl{false};
  bool m_reflections_ready{false};
  bool m_bloom_ready{true};
  Mat4 m_light_vp_cascades[kMaxShadowCascades]{Mat4::identity(), Mat4::identity()};

  Mat4 m_view = Mat4::identity();
  Mat4 m_proj = Mat4::identity();
  Vec3 m_camera_pos{};
  Lighting m_lighting{};
  float m_time{0.f};
};

}  // namespace

std::unique_ptr<IRenderBackend> create_gl_backend() {
  return std::make_unique<GlBackend>();
}

}  // namespace fury
