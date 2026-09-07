#include "fury/math.hpp"
#include "fury/render_settings.hpp"
#include "fury/texture.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace fury;
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void close(float a,float b,const char* message) { require(std::fabs(a-b)<2e-4f,message); }
int main() {
  try {
    const Mat4 model=translate({3,-5,2})*rotate_y(.47f)*rotate_x(.26f)*scale({2,.3f,4});
    Mat4 inv;
    require(inverse(model,inv),"Invert affine transform");
    const Mat4 identity=model*inv;
    for(int r=0;r<4;++r) for(int c=0;c<4;++c) close(identity.at(c,r),r==c?1.f:0.f,"Affine inverse product");
    const Vec3 tangent=transform_direction(model,{1,0,0});
    const Vec3 normal=transform_direction(transpose(inv),{0,1,0});
    close(dot(tangent,normal),0,"Non-uniform scale preserves normal orthogonality");
    const Mat4 vp=perspective(radians(67),1.7f,.1f,500)*look_at({4,3,12},{0,0,0},{0,1,0});
    require(inverse(vp,inv),"Invert perspective view");
    const Vec4 point{2,.8f,-5,1},clip=mul(vp,point),back=mul(inv,clip);
    close(back.x/back.w,point.x,"Projection inverse x"); close(back.y/back.w,point.y,"Projection inverse y");
    close(back.z/back.w,point.z,"Projection inverse z");
    Mat4 untouched=Mat4::identity(); require(!inverse(scale({1,0,1}),untouched),"Singular matrix rejected");
    close(untouched.at(0,0),1,"Failure leaves output unchanged");
    Mat4 invalid=Mat4::identity(); invalid.m[0]=std::numeric_limits<float>::quiet_NaN();
    require(!inverse(invalid,untouched),"NaN matrix rejected");
    float x,y,x2,y2; temporal_jitter(0,8,x,y); close(x,0,"Halton X first"); close(y,-1.f/6,"Halton Y first");
    temporal_jitter(8,8,x2,y2); close(x,x2,"Jitter wraps X"); close(y,y2,"Jitter wraps Y");
    for(unsigned i=0;i<128;++i) { temporal_jitter(i,32,x,y); require(x>=-.5f && x<=.5f && y>=-.5f && y<=.5f,"Jitter range"); }
    temporal_jitter(1,0,x,y); require(std::isfinite(x)&&std::isfinite(y),"Zero phase protected");
    {
      const float width=640,height=360,px=237.5f,py=141.5f,sx=.375f,sy=-.125f;
      const Mat4 projection=perspective(radians(60),width/height,.1f,500);
      Mat4 inverse_projection; require(inverse(projection,inverse_projection),"Jitter test projection inverse");
      const Vec4 ray_sample{(px+sx)/width*2-1,1-(py+sy)/height*2,.5f,1};
      const Vec4 world=mul(inverse_projection,ray_sample);
      const auto raster=reconstruction_jitter(sx,sy);
      const Mat4 shifted=translate({2*raster.x/width,-2*raster.y/height,0})*projection;
      const Vec4 pixel=mul(shifted,world);
      close((pixel.x/pixel.w*.5f+.5f)*width,px,"Ray/raster jitter X conventions agree");
      close((.5f-pixel.y/pixel.w*.5f)*height,py,"Ray/raster jitter Y conventions agree");
    }
    Upscaler upscaler=Upscaler::Native;
    require(parse_upscaler("fsr",upscaler)&&upscaler==Upscaler::FSR,"FSR parses");
    require(!parse_upscaler("fake",upscaler)&&upscaler==Upscaler::FSR,"Unknown upscaler not silently native");
    UpscaleQuality quality=UpscaleQuality::Quality;
    require(parse_upscale_quality("native-aa",quality)&&quality==UpscaleQuality::NativeAA,"Native AA parses");
    RgbaImage checker{2,1,{0,0,0,255,255,255,255,255}};
    const auto gamma_mips=build_mip_chain(checker,TextureEncoding::SRGB);
    require(gamma_mips.size()==2 && gamma_mips[1].pixels[0]>=187 && gamma_mips[1].pixels[0]<=188,"sRGB mip average uses linear light");
    const auto linear_mips=build_mip_chain(checker,TextureEncoding::Linear);
    require(linear_mips[1].pixels[0]==128,"Linear mip average");
    const auto odd_mips=build_mip_chain({3,1,{0,0,0,255,0,0,0,255,255,255,255,255}},TextureEncoding::Linear);
    require(odd_mips.back().pixels[0]==85,"Odd mip dimensions retain final texel");
    const auto normal_mips=build_mip_chain({2,1,{255,128,128,255,128,128,255,255}},TextureEncoding::Normal);
    require(normal_mips.back().pixels[0]>210 && normal_mips.back().pixels[2]>210,"Normal mip renormalization");
    Mesh original=make_box({1,1,1},{1,1,1}),copied=original;
    require(original.geometry_identity!=copied.geometry_identity,"Copied meshes have distinct GPU identities");
    copied.mark_dirty(); require(copied.geometry_revision==1 && original.geometry_revision==0,"Deformation revisions are independent");
    std::cout<<"Rendering math and configuration checks passed\n"; return 0;
  } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
