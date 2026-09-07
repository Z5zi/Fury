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
void (*BlendFunc)(GLenum, GLenum) = nullptr;

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
void (*DrawArrays)(GLenum, GLint, GLsizei) = nullptr;

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
void (*Uniform3fv)(GLint, GLsizei, const GLfloat*) = nullptr;
void (*Uniform4fv)(GLint, GLsizei, const GLfloat*) = nullptr;
void (*Uniform1f)(GLint, GLfloat) = nullptr;
void (*Uniform1i)(GLint, GLint) = nullptr;
void (*Uniform2f)(GLint, GLfloat, GLfloat) = nullptr;

void (*ActiveTexture)(GLenum) = nullptr;
void (*BindTexture)(GLenum, GLuint) = nullptr;
void (*GenTextures)(GLsizei, GLuint*) = nullptr;
void (*DeleteTextures)(GLsizei, const GLuint*) = nullptr;
void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum,
                   GLenum, const void*) = nullptr;
void (*TexParameteri)(GLenum, GLenum, GLint) = nullptr;
void (*GenerateMipmap)(GLenum) = nullptr;

const GLubyte* (*GetString)(GLenum) = nullptr;
void (*GenFramebuffers)(GLsizei, GLuint*) = nullptr;
void (*BindFramebuffer)(GLenum, GLuint) = nullptr;
void (*DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
GLenum (*CheckFramebufferStatus)(GLenum) = nullptr;
void (*DrawBuffer)(GLenum) = nullptr;
void (*ReadBuffer)(GLenum) = nullptr;
void (*GetIntegerv)(GLenum, GLint*) = nullptr;
void (*PixelStorei)(GLenum, GLint) = nullptr;
void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;

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


template <typename T>
bool load_optional(T& fn, const char* name) {
  fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
  return fn != nullptr;
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
  ok &= load(BlendFunc, "glBlendFunc");
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
  ok &= load(DrawArrays, "glDrawArrays");
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
  ok &= load(Uniform3fv, "glUniform3fv");
  ok &= load(Uniform4fv, "glUniform4fv");
  ok &= load(Uniform1f, "glUniform1f");
  ok &= load(Uniform1i, "glUniform1i");
  ok &= load(Uniform2f, "glUniform2f");
  ok &= load(ActiveTexture, "glActiveTexture");
  ok &= load(BindTexture, "glBindTexture");
  ok &= load(GenTextures, "glGenTextures");
  ok &= load(DeleteTextures, "glDeleteTextures");
  ok &= load(TexImage2D, "glTexImage2D");
  ok &= load(TexParameteri, "glTexParameteri");
  ok &= load(GenerateMipmap, "glGenerateMipmap");
  // Optional — used for directional shadow maps; missing = shadows disabled.
  load_optional(GetString, "glGetString");
  load_optional(GenFramebuffers, "glGenFramebuffers");
  load_optional(BindFramebuffer, "glBindFramebuffer");
  load_optional(DeleteFramebuffers, "glDeleteFramebuffers");
  load_optional(FramebufferTexture2D, "glFramebufferTexture2D");
  load_optional(CheckFramebufferStatus, "glCheckFramebufferStatus");
  load_optional(DrawBuffer, "glDrawBuffer");
  load_optional(ReadBuffer, "glReadBuffer");
  load_optional(GetIntegerv, "glGetIntegerv");
  load_optional(PixelStorei, "glPixelStorei");
  load_optional(ReadPixels, "glReadPixels");
  return ok;
}

}  // namespace gl
}  // namespace fury
