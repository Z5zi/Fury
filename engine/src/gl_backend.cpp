#include "fury/renderer.hpp"

#include "fury/gl_loader.hpp"
#include "fury/log.hpp"

#include <SDL.h>

#include <string>
#include <vector>

namespace fury {
namespace {

const char* kVertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
  vColor = aColor;
  gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* kFragSrc = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
  FragColor = vec4(vColor, 1.0);
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

    if (!build_program()) {
      destroy();
      return false;
    }

    gl::Enable(gl::GL_DEPTH_TEST);
    gl::DepthFunc(gl::GL_LESS);
    gl::Enable(gl::GL_CULL_FACE);
    gl::CullFace(gl::GL_BACK);
    gl::FrontFace(gl::GL_CCW);
    gl::Viewport(0, 0, m_width, m_height);

    Log::info("Renderer backend: OpenGL 3.3 core");
    return true;
  }

  void destroy() override {
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
  }

  void set_view_proj(const Mat4& view, const Mat4& proj) override {
    m_view = view;
    m_proj = proj;
  }

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
                            reinterpret_cast<void*>(offsetof(Vertex, color)));
    gl::BindVertexArray(0);
    mesh.gpu_uploaded = true;
  }

  void draw_mesh(const Mesh& mesh, const Mat4& model) override {
    if (mesh.indices.empty()) {
      return;
    }
    Mesh& mutable_mesh = const_cast<Mesh&>(mesh);
    if (!mutable_mesh.gpu_uploaded) {
      upload_mesh(mutable_mesh);
    }
    const Mat4 mvp = m_proj * m_view * model;
    gl::UniformMatrix4fv(m_mvp_loc, 1, gl::GL_FALSE_, mvp.m);
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
  const char* name() const override { return "OpenGL 3.3"; }

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
    m_mvp_loc = gl::GetUniformLocation(m_program, "uMVP");
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
  gl::GLint m_mvp_loc{-1};
  Mat4 m_view = Mat4::identity();
  Mat4 m_proj = Mat4::identity();
};

}  // namespace

std::unique_ptr<IRenderBackend> create_gl_backend() {
  return std::make_unique<GlBackend>();
}

}  // namespace fury
