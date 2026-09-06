#include "fury/gl_loader.hpp"

#include "fury/log.hpp"

#include <SDL.h>

namespace fury {
namespace gl {

void (*Clear)(GLbitfield) = nullptr;
void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
void (*Enable)(GLenum) = nullptr;
void (*Disable)(GLenum) = nullptr;
void (*DepthFunc)(GLenum) = nullptr;
void (*Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
void (*CullFace)(GLenum) = nullptr;
void (*FrontFace)(GLenum) = nullptr;

void (*GenVertexArrays)(GLsizei, GLuint*) = nullptr;
void (*BindVertexArray)(GLuint) = nullptr;
void (*DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;
void (*GenBuffers)(GLsizei, GLuint*) = nullptr;
void (*BindBuffer)(GLenum, GLuint) = nullptr;
void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
void (*DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
void (*EnableVertexAttribArray)(GLuint) = nullptr;
void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei,
                            const void*) = nullptr;
void (*DrawElements)(GLenum, GLsizei, GLenum, const void*) = nullptr;

GLuint (*CreateShader)(GLenum) = nullptr;
void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
void (*CompileShader)(GLuint) = nullptr;
void (*GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*DeleteShader)(GLuint) = nullptr;
GLuint (*CreateProgram)() = nullptr;
void (*AttachShader)(GLuint, GLuint) = nullptr;
void (*LinkProgram)(GLuint) = nullptr;
void (*GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
void (*UseProgram)(GLuint) = nullptr;
void (*DeleteProgram)(GLuint) = nullptr;
GLint (*GetUniformLocation)(GLuint, const GLchar*) = nullptr;
void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;

namespace {

template <typename T>
bool load(T& fn, const char* name) {
  fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
  if (!fn) {
    Log::error(std::string("GL load failed: ") + name);
    return false;
  }
  return true;
}

}  // namespace

bool load_gl_functions() {
  bool ok = true;
  ok &= load(Clear, "glClear");
  ok &= load(ClearColor, "glClearColor");
  ok &= load(Enable, "glEnable");
  ok &= load(Disable, "glDisable");
  ok &= load(DepthFunc, "glDepthFunc");
  ok &= load(Viewport, "glViewport");
  ok &= load(CullFace, "glCullFace");
  ok &= load(FrontFace, "glFrontFace");
  ok &= load(GenVertexArrays, "glGenVertexArrays");
  ok &= load(BindVertexArray, "glBindVertexArray");
  ok &= load(DeleteVertexArrays, "glDeleteVertexArrays");
  ok &= load(GenBuffers, "glGenBuffers");
  ok &= load(BindBuffer, "glBindBuffer");
  ok &= load(BufferData, "glBufferData");
  ok &= load(DeleteBuffers, "glDeleteBuffers");
  ok &= load(EnableVertexAttribArray, "glEnableVertexAttribArray");
  ok &= load(VertexAttribPointer, "glVertexAttribPointer");
  ok &= load(DrawElements, "glDrawElements");
  ok &= load(CreateShader, "glCreateShader");
  ok &= load(ShaderSource, "glShaderSource");
  ok &= load(CompileShader, "glCompileShader");
  ok &= load(GetShaderiv, "glGetShaderiv");
  ok &= load(GetShaderInfoLog, "glGetShaderInfoLog");
  ok &= load(DeleteShader, "glDeleteShader");
  ok &= load(CreateProgram, "glCreateProgram");
  ok &= load(AttachShader, "glAttachShader");
  ok &= load(LinkProgram, "glLinkProgram");
  ok &= load(GetProgramiv, "glGetProgramiv");
  ok &= load(GetProgramInfoLog, "glGetProgramInfoLog");
  ok &= load(UseProgram, "glUseProgram");
  ok &= load(DeleteProgram, "glDeleteProgram");
  ok &= load(GetUniformLocation, "glGetUniformLocation");
  ok &= load(UniformMatrix4fv, "glUniformMatrix4fv");
  return ok;
}

}  // namespace gl
}  // namespace fury
