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
using GLubyte = unsigned char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

constexpr GLenum GL_FALSE_ = 0;
constexpr GLenum GL_TRUE_ = 1;
constexpr GLenum GL_DEPTH_BUFFER_BIT = 0x00000100;
constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_UNSIGNED_INT = 0x1405;
constexpr GLenum GL_FLOAT = 0x1406;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_LESS = 0x0201;
constexpr GLenum GL_CULL_FACE = 0x0B44;
constexpr GLenum GL_FRONT = 0x0404;
constexpr GLenum GL_BACK = 0x0405;
constexpr GLenum GL_CCW = 0x0901;
constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
constexpr GLenum GL_STATIC_DRAW = 0x88E4;
constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_RGB = 0x1907;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_REPEAT = 0x2901;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_LINEAR_MIPMAP_LINEAR = 0x2703;


constexpr GLenum GL_TEXTURE1 = 0x84C1;
constexpr GLenum GL_TEXTURE2 = 0x84C2;
constexpr GLenum GL_TEXTURE3 = 0x84C3;
constexpr GLenum GL_DEPTH_COMPONENT = 0x1902;
constexpr GLenum GL_DEPTH_COMPONENT24 = 0x81A6;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_DEPTH_ATTACHMENT = 0x8D00;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum GL_NONE = 0;
constexpr GLenum GL_FRAMEBUFFER_BINDING = 0x8CA6;
constexpr GLenum GL_DRAW_FRAMEBUFFER = 0x8CA9;
constexpr GLenum GL_READ_FRAMEBUFFER = 0x8CA8;
constexpr GLenum GL_RENDERER = 0x1F01;
constexpr GLenum GL_VENDOR = 0x1F00;
constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
constexpr GLenum GL_VIEWPORT = 0x0BA2;

bool load_gl_functions();

extern void (*Clear)(GLbitfield);
extern void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
extern void (*Enable)(GLenum);
extern void (*Disable)(GLenum);
extern void (*DepthFunc)(GLenum);
extern void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
extern void (*CullFace)(GLenum);
extern void (*FrontFace)(GLenum);
extern void (*BlendFunc)(GLenum, GLenum);

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
extern void (*DrawArrays)(GLenum, GLint, GLsizei);

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
extern void (*Uniform3fv)(GLint, GLsizei, const GLfloat*);
extern void (*Uniform4fv)(GLint, GLsizei, const GLfloat*);
extern void (*Uniform1f)(GLint, GLfloat);
extern void (*Uniform1i)(GLint, GLint);
extern void (*Uniform2f)(GLint, GLfloat, GLfloat);

extern void (*ActiveTexture)(GLenum);
extern void (*BindTexture)(GLenum, GLuint);
extern void (*GenTextures)(GLsizei, GLuint*);
extern void (*DeleteTextures)(GLsizei, const GLuint*);
extern void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum,
                          GLenum, const void*);
extern void (*TexParameteri)(GLenum, GLenum, GLint);
extern void (*GenerateMipmap)(GLenum);

extern const GLubyte* (*GetString)(GLenum);
extern void (*GenFramebuffers)(GLsizei, GLuint*);
extern void (*BindFramebuffer)(GLenum, GLuint);
extern void (*DeleteFramebuffers)(GLsizei, const GLuint*);
extern void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
extern GLenum (*CheckFramebufferStatus)(GLenum);
extern void (*DrawBuffer)(GLenum);
extern void (*ReadBuffer)(GLenum);
extern void (*GetIntegerv)(GLenum, GLint*);
extern void (*PixelStorei)(GLenum, GLint);
extern void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);


}  // namespace gl
}  // namespace fury
