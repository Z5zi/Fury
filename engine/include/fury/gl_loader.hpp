#pragma once

/// Minimal OpenGL 3.3 core loader via SDL_GL_GetProcAddress.

#include <cstddef>

namespace fury {
namespace gl {

using GLenum = unsigned int;
using GLbitfield = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLfloat = float;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

constexpr GLenum GL_FALSE_ = 0;
constexpr GLenum GL_TRUE_ = 1;
constexpr GLenum GL_DEPTH_BUFFER_BIT = 0x00000100;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_UNSIGNED_INT = 0x1405;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_LESS = 0x0201;
constexpr GLenum GL_CULL_FACE = 0x0B44;
constexpr GLenum GL_BACK = 0x0405;
constexpr GLenum GL_CCW = 0x0901;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;

bool load_gl_functions();

extern void (*Clear)(GLbitfield);
extern void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*Enable)(GLenum);
extern void (*Disable)(GLenum);
extern void (*DepthFunc)(GLenum);
extern void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
extern void (*CullFace)(GLenum);
extern void (*FrontFace)(GLenum);

extern void (*GenVertexArrays)(GLsizei, GLuint*);
extern void (*BindVertexArray)(GLuint);
extern void (*DeleteVertexArrays)(GLsizei, const GLuint*);
extern void (*GenBuffers)(GLsizei, GLuint*);
extern void (*BindBuffer)(GLenum, GLuint);
extern void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void (*DeleteBuffers)(GLsizei, const GLuint*);
extern void (*EnableVertexAttribArray)(GLuint);
extern void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern void (*DrawElements)(GLenum, GLsizei, GLenum, const void*);

extern GLuint (*CreateShader)(GLenum);
extern void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern void (*CompileShader)(GLuint);
extern void (*GetShaderiv)(GLuint, GLenum, GLint*);
extern void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*DeleteShader)(GLuint);
extern GLuint (*CreateProgram)();
extern void (*AttachShader)(GLuint, GLuint);
extern void (*LinkProgram)(GLuint);
extern void (*GetProgramiv)(GLuint, GLenum, GLint*);
extern void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*UseProgram)(GLuint);
extern void (*DeleteProgram)(GLuint);
extern GLint (*GetUniformLocation)(GLuint, const GLchar*);
extern void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);

}  // namespace gl
}  // namespace fury
