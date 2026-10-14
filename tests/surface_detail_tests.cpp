#include "fury/surface_detail.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Exercise the internal rotation operation with an independent height fixture.
namespace fury { namespace detail {
RgbaImage rotate_surface_detail_quarter_turn(const RgbaImage& source, bool normal);
} }
using namespace fury;
namespace {
const std::array<TextureSlot,6> slots{{TextureSlot::Asphalt,TextureSlot::Concrete,
    TextureSlot::Brick,TextureSlot::Wood,TextureSlot::Metal,TextureSlot::BarrelMetal}};
const std::array<const char*,6> names{{"asphalt","concrete","brick","wood","metal","painted_metal"}};
void require(bool value, const char* message) {
  if(!value) throw std::runtime_error(message);
}
int channel(const RgbaImage& image, int x, int y, int c) {
  x=(x%image.width+image.width)%image.width;
  y=(y%image.height+image.height)%image.height;
  return image.pixels[(std::size_t(y)*image.width+x)*4+std::size_t(c)];
}
void check_normal(const RgbaImage& image) {
  for(std::size_t i=0;i<image.pixels.size();i+=4) {
    const float x=float(image.pixels[i])/127.5f-1.f;
    const float y=float(image.pixels[i+1])/127.5f-1.f;
    const float z=float(image.pixels[i+2])/127.5f-1.f;
    const float length=std::sqrt(x*x+y*y+z*z);
    require(std::isfinite(length) && std::abs(length-1.f)<.0137f && z>0.f,
            "Normals are finite unit vectors with a positive tangent-space Z");
  }
}
void check_mips(const RgbaImage& base, const std::vector<RgbaImage>& mips,
                TextureEncoding encoding) {
  const auto reference=build_mip_chain(base,encoding);
  require(mips.size()+1==reference.size(),"Mip chain contains every level exactly once");
  for(std::size_t i=0;i<mips.size();++i) {
    require(mips[i].valid() && mips[i].width==reference[i+1].width &&
            mips[i].height==reference[i+1].height && mips[i].pixels==reference[i+1].pixels,
            "Mip channels, sizes, and filtering agree with the production filter");
    if(encoding==TextureEncoding::Normal) check_normal(mips[i]);
  }
  require(mips.back().width==1 && mips.back().height==1,"Mip chain reaches 1x1");
}
void check_repeat_edges(const RgbaImage& image) {
  // The first and last texels are adjacent centres, not duplicate endpoints.
  // Compare their finite differences with interior edges instead of incorrectly
  // requiring equal border texels (which would flatten real mortar / grain).
  for(int axis=0;axis<2;++axis) {
    const int lines=axis==0?image.width:image.height;
    const int count=axis==0?image.height:image.width;
    double seam=0, largest_interior=0;
    for(int line=0;line<lines;++line) {
      double energy=0;
      for(int p=0;p<count;++p) for(int c=0;c<3;++c) {
        const int a=axis==0?channel(image,line,p,c):channel(image,p,line,c);
        const int b=axis==0?channel(image,line-1,p,c):channel(image,p,line-1,c);
        energy+=std::abs(a-b);
      }
      energy/=double(count*3);
      if(line==0) seam=energy; else largest_interior=std::max(largest_interior,energy);
    }
    require(seam<=largest_interior*1.8+1.,"Repeat-boundary derivatives are not exceptional discontinuities");
  }
  for(int y=0;y<image.height;y+=13) for(int x=0;x<image.width;x+=11)
    for(int c=0;c<3;++c)
      require(channel(image,x,y,c)==channel(image,x+image.width,y-image.height,c),
              "Repeat sampling is invariant to whole-tile translations");
}
void check_rotation(const RgbaImage& original, const RgbaImage& rotated,
                    unsigned turns, bool normal) {
  require(original.valid() && rotated.valid(),"Rotation preserves a complete image");
  require(rotated.width==original.width && rotated.height==original.height,"Square profile dimensions survive rotation");
  const int size=original.width;
  for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
    const int sx=turns==1?y:(turns==2?size-1-x:(turns==3?size-1-y:x));
    const int sy=turns==1?size-1-x:(turns==2?size-1-y:(turns==3?x:y));
    int expected[4];
    for(int c=0;c<4;++c) expected[c]=channel(original,sx,sy,c);
    if(normal) {
      const int nx=expected[0], ny=expected[1];
      if(turns==1) { expected[0]=255-ny; expected[1]=nx; }
      if(turns==2) { expected[0]=255-nx; expected[1]=255-ny; }
      if(turns==3) { expected[0]=ny; expected[1]=255-nx; }
    }
    for(int c=0;c<4;++c)
      require(channel(rotated,x,y,c)==expected[c],"Color, packed maps and vector-correct normals rotate coherently");
  }
}
void check_rotated_height_fixture() {
  constexpr int width=12,height=8;
  std::vector<float> heights(std::size_t(width)*height);
  for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
    const float u=(float(x)+.5f)/float(width), v=(float(y)+.5f)/float(height);
    heights[std::size_t(y)*width+x]=.7f*std::sin(6.2831853f*u)+
        .3f*std::cos(6.2831853f*v)+.2f*std::sin(6.2831853f*(u+v));
  }
  const auto normal_field=[](const std::vector<float>& h,int w,int rows) {
    RgbaImage normals{w,rows,std::vector<std::uint8_t>(std::size_t(w)*rows*4,255)};
    const auto at=[&](int x,int y) { return h[std::size_t((y+rows)%rows)*w+(x+w)%w]; };
    for(int y=0;y<rows;++y) for(int x=0;x<w;++x) {
      const float nx=(at(x-1,y)-at(x+1,y))*.5f;
      const float ny=(at(x,y-1)-at(x,y+1))*.5f;
      const float length=std::sqrt(nx*nx+ny*ny+1.f);
      const float n[3]={nx/length,ny/length,1.f/length};
      for(unsigned c=0;c<3;++c) normals.pixels[(std::size_t(y)*w+x)*4+c]=
          std::uint8_t((n[c]*.5f+.5f)*255.f+.5f);
    }
    return normals;
  };
  const auto original=normal_field(heights,width,height);
  auto rotated=original;
  int w=width,rows=height;
  for(unsigned turn=1;turn<=4;++turn) {
    std::vector<float> turned(heights.size());
    for(int y=0;y<w;++y) for(int x=0;x<rows;++x)
      turned[std::size_t(y)*rows+x]=heights[std::size_t(rows-1-x)*w+y];
    heights=std::move(turned); std::swap(w,rows);
    rotated=detail::rotate_surface_detail_quarter_turn(rotated,true);
    const auto from_height=normal_field(heights,w,rows);
    require(rotated.width==w && rotated.height==rows,"Rectangular height fixture rotates dimensions correctly");
    for(std::size_t i=0;i<rotated.pixels.size();++i)
      require(std::abs(int(rotated.pixels[i])-int(from_height.pixels[i]))<=1,
              "Rotated tangent vectors agree with independent derivatives of the rotated height field");
  }
  require(rotated.pixels==original.pixels,"Four normal rotations return the exact original encoded vectors");
}
double correlation(const RgbaImage& a, int ca, const RgbaImage& b, int cb) {
  double sx=0,sy=0,sxx=0,syy=0,sxy=0;
  const double n=double(a.width)*a.height;
  for(std::size_t i=0;i<a.pixels.size();i+=4) {
    const double x=a.pixels[i+std::size_t(ca)], y=b.pixels[i+std::size_t(cb)];
    sx+=x; sy+=y; sxx+=x*x; syy+=y*y; sxy+=x*y;
  }
  return (sxy-sx*sy/n)/std::sqrt((sxx-sx*sx/n)*(syy-sy*sy/n));
}
std::size_t bytes(const MaterialTextures& t) {
  std::size_t total=t.base_color.pixels.size()+t.normal.pixels.size()+t.metallic_roughness.pixels.size();
  for(const auto* chain:{&t.base_color_mips,&t.normal_mips,&t.metallic_roughness_mips})
    for(const auto& image:*chain) total+=image.pixels.size();
  return total;
}
void dump(const std::filesystem::path& path, const RgbaImage& image) {
  std::ofstream out(path,std::ios::binary);
  out<<"P6\n"<<image.width<<' '<<image.height<<"\n255\n";
  for(std::size_t i=0;i<image.pixels.size();i+=4)
    out.write(reinterpret_cast<const char*>(image.pixels.data()+i),3);
  require(bool(out),"Diagnostic image export succeeded");
}
}  // namespace

int main(int argc, char** argv) {
  try {
    std::filesystem::path dump_dir;
    if(argc==3 && std::string(argv[1])=="--dump-dir") {
      dump_dir=argv[2]; std::filesystem::create_directories(dump_dir);
    } else require(argc==1,"Usage: fury_surface_detail_tests [--dump-dir DIRECTORY]");
    for(TextureSlot slot:{TextureSlot::None,TextureSlot::Checker,TextureSlot::Water,
                          TextureSlot::Glass,TextureSlot::Count,TextureSlot(-1),TextureSlot(999)}) {
      require(!supports_surface_detail(slot) && !surface_detail_textures(slot) &&
              surface_detail_uv_per_meter(slot)==0.f,"Unsupported and invalid slots stay outside detail mapping");
    }
    for(unsigned size:{0u,1u,64u,129u,1024u,~0u}) {
      bool rejected=false;
      try { surface_detail_textures(TextureSlot::Asphalt,size); }
      catch(const std::invalid_argument&) { rejected=true; }
      require(rejected,"Unsupported resolutions reject before allocation");
    }
    for(unsigned turns:{4u,5u,255u,~0u}) {
      bool rejected=false;
      try { surface_detail_textures(TextureSlot::Wood,512,turns); }
      catch(const std::invalid_argument&) { rejected=true; }
      require(rejected,"Quarter-turn values outside 0..3 reject before allocation");
    }
    check_rotated_height_fixture();
    std::array<std::shared_ptr<const MaterialTextures>,6> full;
    const auto start=std::chrono::steady_clock::now();
    for(std::size_t i=0;i<slots.size();++i) full[i]=surface_detail_textures(slots[i]);
    const auto generated=std::chrono::steady_clock::now();
    std::size_t resident_bytes=0;
    for(std::size_t i=0;i<slots.size();++i) {
      const auto& t=*full[i];
      require(supports_surface_detail(slots[i]),"All six opaque legacy slots have profiles");
      require(surface_detail_uv_per_meter(slots[i])==(slots[i]==TextureSlot::Metal?1.f:.5f),
              "Physical dimensions are 1 m steel, 2 m other surfaces");
      require(full[i]==surface_detail_textures(slots[i]),"Repeated requests share one immutable set");
      require(!t.emissive.valid() && t.emissive_mips.empty(),"Surface detail does not manufacture emission");
      require(t.source.find("license=MIT")!=std::string::npos,"Original provenance follows each set");
      for(const auto* image:{&t.base_color,&t.normal,&t.metallic_roughness}) {
        require(image->valid() && image->width==512 && image->height==512,"Default maps are complete 512 RGBA8 images");
        for(std::size_t p=3;p<image->pixels.size();p+=4)
          require(image->pixels[p]==255,"All generated surfaces remain opaque");
        check_repeat_edges(*image);
      }
      check_normal(t.normal);
      check_mips(t.base_color,t.base_color_mips,TextureEncoding::SRGB);
      check_mips(t.normal,t.normal_mips,TextureEncoding::Normal);
      check_mips(t.metallic_roughness,t.metallic_roughness_mips,TextureEncoding::Linear);
      for(std::size_t p=0;p<t.metallic_roughness.pixels.size();p+=4) {
        require(t.metallic_roughness.pixels[p]==255,"Unused R is neutral, not invented AO");
        require(t.metallic_roughness.pixels[p+1]>=128,"Roughness stays in its restrained physical multiplier range");
        if(i<4) require(t.metallic_roughness.pixels[p+2]==0,"Asphalt, concrete, brick, and wood are dielectric");
        if(slots[i]==TextureSlot::Metal) require(t.metallic_roughness.pixels[p+2]==255,"Clean steel retains material metallic factor");
      }
      resident_bytes+=bytes(t);
      if(!dump_dir.empty()) {
        const auto stem=std::string(names[i]);
        dump(dump_dir/(stem+"-albedo.ppm"),t.base_color);
        dump(dump_dir/(stem+"-normal.ppm"),t.normal);
        dump(dump_dir/(stem+"-metallic-roughness.ppm"),t.metallic_roughness);
      }
    }
    require(resident_bytes==6u*3u*4u*349525u && resident_bytes<24u*1024u*1024u,
            "All base maps and mips fit the explicit 24 MiB cache payload budget");

    require(correlation(full[0]->base_color,0,full[0]->metallic_roughness,1)<-.5,
            "Exposed asphalt aggregate is lighter and less rough than its binder");
    std::size_t correlated_pores=0;
    const auto& concrete=*full[1];
    for(int y=0;y<512;++y) for(int x=0;x<512;++x) {
      const int around_color=(channel(concrete.base_color,x-3,y,0)+channel(concrete.base_color,x+3,y,0)+
                              channel(concrete.base_color,x,y-3,0)+channel(concrete.base_color,x,y+3,0))/4;
      const int around_rough=(channel(concrete.metallic_roughness,x-3,y,1)+channel(concrete.metallic_roughness,x+3,y,1)+
                              channel(concrete.metallic_roughness,x,y-3,1)+channel(concrete.metallic_roughness,x,y+3,1))/4;
      if(around_color-channel(concrete.base_color,x,y,0)>10 &&
         channel(concrete.metallic_roughness,x,y,1)-around_rough>5) ++correlated_pores;
    }
    require(correlated_pores>100,"Concrete air pores correlate local dark color with rougher recesses");
    const auto& brick=*full[2];
    require(channel(brick.normal,2,10,0)<115 && channel(brick.normal,61,10,0)>140,
            "Brick height rises from left mortar and falls toward right mortar");
    require(channel(brick.normal,32,2,1)<115 && channel(brick.normal,32,19,1)>140,
            "Brick course normals use the documented increasing-V convention");
    require(channel(brick.metallic_roughness,0,10,1)>channel(brick.metallic_roughness,32,10,1),
            "Recessed mortar is rougher than the fired brick face");
    require(channel(brick.base_color,32,10,0)-channel(brick.base_color,32,10,2)>
            channel(brick.base_color,0,10,0)-channel(brick.base_color,0,10,2)+30,
            "Clay color and height-defined mortar masks describe the same feature");
    std::size_t paint=0, bare=0, rust=0;
    const auto& barrel=*full[5];
    for(std::size_t p=0;p<barrel.base_color.pixels.size();p+=4) {
      const int metallic=barrel.metallic_roughness.pixels[p+2];
      if(metallic==0) ++paint;
      if(metallic>190) ++bare;
      if(barrel.base_color.pixels[p]>barrel.base_color.pixels[p+2]+40 &&
         barrel.metallic_roughness.pixels[p+1]>225 && metallic<80) ++rust;
    }
    require(paint>200000 && bare>100 && rust>100,"Paint, rough dielectric rust, and metallic chips coexist with restrained damage");

    // Each reduced quality is the exact mip of the master, not a re-seeded or
    // resolution-dependent surface. Caller-held old maps survive cache eviction.
    for(unsigned resolution:{128u,256u}) for(std::size_t i=0;i<slots.size();++i) {
      const auto small=surface_detail_textures(slots[i],resolution);
      const auto mip=resolution==256?0u:1u;
      require(small->base_color.pixels==full[i]->base_color_mips[mip].pixels &&
              small->normal.pixels==full[i]->normal_mips[mip].pixels &&
              small->metallic_roughness.pixels==full[i]->metallic_roughness_mips[mip].pixels,
              "Quality settings preserve master geometry, channels and phase");
      require(small->base_color_mips.front().width==int(resolution/2),"Reduced settings still start additional mips at level 1");
    }
    const auto regenerated=surface_detail_textures(TextureSlot::Asphalt);
    require(regenerated!=full[0] && regenerated->base_color.pixels==full[0]->base_color.pixels &&
            regenerated->normal.pixels==full[0]->normal.pixels &&
            regenerated->metallic_roughness.pixels==full[0]->metallic_roughness.pixels,
            "Regeneration after cache eviction is byte-for-byte deterministic");
    for(unsigned turns:{1u,2u,3u}) for(std::size_t i=0;i<slots.size();++i) {
      const auto variant=surface_detail_textures(slots[i],128,turns);
      require(variant==surface_detail_textures(slots[i],128,turns),"Rotation is included in the shared cache key");
      check_rotation(full[i]->base_color_mips[1],variant->base_color,turns,false);
      check_rotation(full[i]->normal_mips[1],variant->normal,turns,true);
      check_rotation(full[i]->metallic_roughness_mips[1],variant->metallic_roughness,turns,false);
      for(std::size_t m=0;m<variant->normal_mips.size();++m) {
        check_rotation(full[i]->base_color_mips[m+2],variant->base_color_mips[m],turns,false);
        check_rotation(full[i]->normal_mips[m+2],variant->normal_mips[m],turns,true);
        check_rotation(full[i]->metallic_roughness_mips[m+2],variant->metallic_roughness_mips[m],turns,false);
        check_normal(variant->normal_mips[m]);
      }
    }
    const auto turned_wood=surface_detail_textures(TextureSlot::Wood,512,1);
    require(bytes(*turned_wood)==4194300u,"One caller-retained 512 rotation adds just under 4 MiB");
    if(!dump_dir.empty()) {
      dump(dump_dir/"wood-clockwise-albedo.ppm",turned_wood->base_color);
      dump(dump_dir/"wood-clockwise-normal.ppm",turned_wood->normal);
      dump(dump_dir/"wood-clockwise-metallic-roughness.ppm",turned_wood->metallic_roughness);
    }
    auto rotation_owner=surface_detail_textures(TextureSlot::Concrete,128,1);
    std::weak_ptr<const MaterialTextures> old_rotation=rotation_owner;
    rotation_owner.reset();
    surface_detail_textures(TextureSlot::Concrete,128,2);
    require(old_rotation.expired(),"Rotation switches release unowned previous-rotation cache data");
    full.fill(nullptr);
    auto eviction=surface_detail_textures(TextureSlot::Wood,128);
    std::weak_ptr<const MaterialTextures> old=eviction;
    eviction.reset();
    surface_detail_textures(TextureSlot::Concrete,256);
    require(old.expired(),"Resolution switches release unowned previous-resolution cache data");

    std::array<std::shared_ptr<const MaterialTextures>,4> concurrent;
    std::array<std::thread,4> threads;
    for(std::size_t i=0;i<threads.size();++i)
      threads[i]=std::thread([&,i] { concurrent[i]=surface_detail_textures(TextureSlot::Wood,256); });
    for(auto& thread:threads) thread.join();
    for(const auto& entry:concurrent) require(entry==concurrent[0],"Concurrent same-key misses share one generated profile");
    const double seconds=std::chrono::duration<double>(generated-start).count();
    std::cout<<"Surface detail: 6 original 512x512 PBR profiles, "<<resident_bytes
             <<" retained bytes including mips, "<<seconds<<" s cold generation; "
             <<"normal, correlation, repeat-edge, mip, determinism, resolution, rotation/height-gradient, cache and thread tests passed\n";
    return 0;
  } catch(const std::exception& e) {
    std::cerr<<e.what()<<'\n'; return 1;
  }
}
