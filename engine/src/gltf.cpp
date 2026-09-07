#include "fury/gltf.hpp"
#include "fury/texture.hpp"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fury {
namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
std::filesystem::path asset_path(const std::filesystem::path& root,const char* uri) {
  require(uri && *uri,"Empty glTF resource URI");
  std::string decoded(uri); cgltf_decode_uri(decoded.data()); decoded.resize(std::strlen(decoded.c_str()));
  require(decoded.find(':')==std::string::npos,"Remote or absolute glTF resource URI is not supported");
  const auto relative=std::filesystem::u8path(decoded);
  require(!relative.is_absolute() && !relative.has_root_name(),"Absolute glTF resource path rejected");
  const auto resolved=std::filesystem::weakly_canonical(root/relative);
  const auto scoped=resolved.lexically_relative(root);
  require(!scoped.empty(),"glTF resource path cannot be resolved inside the asset directory");
  for(const auto& component:scoped) require(component!="..","glTF resource escapes its asset directory");
  return resolved;
}
bool data_uri(const char* uri) { return uri && std::strncmp(uri,"data:",5)==0; }
RgbaImage decode_image(const cgltf_image* image,const std::filesystem::path& root) {
  require(image!=nullptr,"Texture has no image");
  RgbaImage result;
  if(image->buffer_view) {
    const auto* view=image->buffer_view;
    require(view->buffer && view->buffer->data && view->offset<=view->buffer->size &&
            view->size<=view->buffer->size-view->offset,"Invalid embedded image buffer view");
    const auto* bytes=static_cast<const std::uint8_t*>(view->buffer->data)+view->offset;
    require(decode_rgba_image(bytes,view->size,result),"Unsupported or corrupt embedded glTF image");
  } else if(data_uri(image->uri)) {
    const char* comma=std::strchr(image->uri,',');
    require(comma && std::string(image->uri,std::size_t(comma-image->uri)).find(";base64")!=std::string::npos,"Only base64 data images are supported");
    const std::size_t encoded=std::strlen(comma+1);
    require(encoded && encoded%4==0 && encoded<256*1024*1024,"Invalid base64 image size");
    std::size_t decoded=encoded/4*3;
    if(comma[encoded]=='=') --decoded;
    if(encoded>1 && comma[encoded-1]=='=') --decoded;
    cgltf_options options{}; void* bytes{};
    require(cgltf_load_buffer_base64(&options,decoded,comma+1,&bytes)==cgltf_result_success,"Invalid base64 image");
    const bool ok=decode_rgba_image(static_cast<const std::uint8_t*>(bytes),decoded,result);
    std::free(bytes); require(ok,"Unsupported or corrupt base64 glTF image");
  } else require(load_rgba_image(asset_path(root,image->uri).u8string(),result),"Unable to decode glTF PNG/JPEG image");
  return result;
}
std::vector<float> floats(const cgltf_accessor* accessor,unsigned components) {
  require(accessor && accessor->count<=10000000,"Invalid or oversized glTF accessor");
  require(cgltf_num_components(accessor->type)==components,"Incorrect glTF accessor component count");
  std::vector<float> values(accessor->count*components);
  require(cgltf_accessor_unpack_floats(accessor,values.data(),values.size())==values.size(),"Cannot unpack glTF accessor");
  for(float value:values) require(std::isfinite(value),"Non-finite glTF vertex attribute");
  return values;
}
}

bool load_gltf(const std::string& path,GltfAsset& out,std::string& error) {
  cgltf_data* parsed{};
  try {
    const auto file=std::filesystem::weakly_canonical(std::filesystem::u8path(path));
    const auto root=file.parent_path();
    require(std::filesystem::file_size(file)<=256*1024*1024,"glTF container exceeds 256 MiB");
    cgltf_options options{};
    require(cgltf_parse_file(&options,file.u8string().c_str(),&parsed)==cgltf_result_success,"Cannot parse glTF 2.0/GLB");
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> owner(parsed,&cgltf_free); parsed=nullptr;
    cgltf_data* data=owner.get();
    const std::unordered_set<std::string> supported={"KHR_materials_transmission","KHR_materials_ior","KHR_materials_emissive_strength","KHR_texture_transform"};
    for(std::size_t i=0;i<data->extensions_required_count;++i)
      require(supported.count(data->extensions_required[i])!=0,"Unsupported required glTF extension");
    std::size_t buffer_bytes=0;
    for(std::size_t i=0;i<data->buffers_count;++i) {
      auto& buffer=data->buffers[i];
      require(buffer.size<=512*1024*1024 && buffer_bytes<=512*1024*1024-buffer.size,"glTF buffers exceed 512 MiB");
      buffer_bytes+=buffer.size;
      if(buffer.uri && !data_uri(buffer.uri)) asset_path(root,buffer.uri);
    }
    require(cgltf_load_buffers(&options,data,file.u8string().c_str())==cgltf_result_success,"Cannot load glTF buffers");
    require(cgltf_validate(data)==cgltf_result_success,"glTF validation failed");
    require(data->nodes_count<=100000,"Too many glTF nodes");
    GltfAsset result;
    std::unordered_map<const cgltf_material*,Material> material_cache;
    std::unordered_map<const cgltf_primitive*,std::shared_ptr<Mesh>> mesh_cache;
    std::unordered_map<const cgltf_image*,RgbaImage> image_cache;
    auto load_map=[&](const cgltf_texture_view& view,RgbaImage& image) {
      if(!view.texture) return;
      require(view.texcoord==0 && (!view.has_transform || !view.transform.has_texcoord || view.transform.texcoord==0),"Only TEXCOORD_0 is supported");
      auto* source=view.texture->image;
      auto found=image_cache.find(source);
      if(found==image_cache.end()) found=image_cache.emplace(source,decode_image(source,root)).first;
      image=found->second;
    };
    auto material_for=[&](const cgltf_material* source) -> Material {
      if(!source) { Material defaults; defaults.metallic=1; defaults.roughness=1; defaults.double_sided=false; return defaults; }
      auto found=material_cache.find(source); if(found!=material_cache.end()) return found->second;
      require(!source->has_pbr_specular_glossiness || source->has_pbr_metallic_roughness,"The glTF specular-glossiness workflow is not supported");
      const auto& pbr=source->pbr_metallic_roughness;
      Material m; m.albedo={pbr.base_color_factor[0],pbr.base_color_factor[1],pbr.base_color_factor[2]};
      m.opacity=pbr.base_color_factor[3]; m.metallic=pbr.metallic_factor; m.roughness=pbr.roughness_factor;
      m.double_sided=source->double_sided!=0;
      m.alpha_blend=source->alpha_mode==cgltf_alpha_mode_blend;
      m.alpha_cutoff=source->alpha_mode==cgltf_alpha_mode_mask ? source->alpha_cutoff : -1.f;
      m.normal_scale=source->normal_texture.scale;
      m.emissive_color={source->emissive_factor[0],source->emissive_factor[1],source->emissive_factor[2]};
      m.emissive=source->has_emissive_strength ? source->emissive_strength.emissive_strength : 1.f;
      if(source->has_transmission) {
        require(!source->transmission.transmission_texture.texture,"Transmission texture is not yet supported");
        m.transmission=source->transmission.transmission_factor;
      }
      if(source->has_ior) m.index_of_refraction=source->ior.ior;
      auto textures=std::make_shared<MaterialTextures>(); textures->source=file.u8string();
      load_map(pbr.base_color_texture,textures->base_color); load_map(source->normal_texture,textures->normal);
      load_map(pbr.metallic_roughness_texture,textures->metallic_roughness); load_map(source->emissive_texture,textures->emissive);
      m.textures=std::move(textures);
      material_cache.emplace(source,m); return m;
    };
    std::unordered_set<const cgltf_node*> active;
    std::vector<const cgltf_node*> work;
    if(data->scene) for(std::size_t i=0;i<data->scene->nodes_count;++i) work.push_back(data->scene->nodes[i]);
    else for(std::size_t i=0;i<data->nodes_count;++i) if(!data->nodes[i].parent) work.push_back(&data->nodes[i]);
    while(!work.empty()) {
      const auto* node=work.back(); work.pop_back();
      require(active.insert(node).second,"glTF scene contains a node cycle or duplicate root");
      for(std::size_t i=0;i<node->children_count;++i) work.push_back(node->children[i]);
      if(!node->mesh) continue;
      require(!node->skin,"Skinned glTF meshes require an animation renderer");
      std::size_t ancestors=0;
      for(const auto* ancestor=node;ancestor;ancestor=ancestor->parent)
        require(++ancestors<=data->nodes_count,"glTF parent chain contains a cycle");
      Mat4 world; cgltf_node_transform_world(node,world.m);
      for(float value:world.m) require(std::isfinite(value),"Non-finite glTF node transform");
      for(std::size_t pi=0;pi<node->mesh->primitives_count;++pi) {
        const auto& primitive=node->mesh->primitives[pi];
        require(primitive.type==cgltf_primitive_type_triangles,"Only triangle-list glTF primitives are supported");
        require(!primitive.targets_count,"Morph-target glTF meshes are not yet supported");
        auto cached=mesh_cache.find(&primitive);
        if(cached==mesh_cache.end()) {
          const cgltf_accessor *positions=nullptr,*normals=nullptr,*uv=nullptr,*colors=nullptr;
          for(std::size_t i=0;i<primitive.attributes_count;++i) {
            const auto& a=primitive.attributes[i];
            if(a.type==cgltf_attribute_type_position) positions=a.data;
            else if(a.type==cgltf_attribute_type_normal) normals=a.data;
            else if(a.type==cgltf_attribute_type_texcoord && a.index==0) uv=a.data;
            else if(a.type==cgltf_attribute_type_color && a.index==0) colors=a.data;
          }
          auto p=floats(positions,3);
          std::vector<float> n,t,c;
          if(normals) n=floats(normals,3);
          if(uv) t=floats(uv,2);
          const unsigned color_components=colors ? unsigned(cgltf_num_components(colors->type)) : 0;
          if(colors) { require(color_components==3 || color_components==4,"Invalid glTF color attribute"); c=floats(colors,color_components); }
          require((!normals || normals->count==positions->count) && (!uv || uv->count==positions->count) &&
                  (!colors || colors->count==positions->count),"Mismatched glTF attribute lengths");
          auto mesh=std::make_shared<Mesh>(); mesh->vertices.resize(positions->count);
          for(std::size_t i=0;i<mesh->vertices.size();++i) {
            auto& v=mesh->vertices[i]; v.position={p[i*3],p[i*3+1],p[i*3+2]};
            v.normal=normals ? normalize({n[i*3],n[i*3+1],n[i*3+2]}) : Vec3{};
            if(uv) v.uv={t[i*2],t[i*2+1]};
            if(colors) {
              v.color={c[i*color_components],c[i*color_components+1],c[i*color_components+2]};
              if(color_components==4) v.opacity=c[i*color_components+3];
            }
          }
          const std::size_t count=primitive.indices ? primitive.indices->count : positions->count;
          require(count && count%3==0 && count<=30000000,"Invalid glTF triangle index count");
          mesh->indices.reserve(count);
          for(std::size_t i=0;i<count;++i) {
            const auto index=primitive.indices ? cgltf_accessor_read_index(primitive.indices,i) : i;
            require(index<mesh->vertices.size(),"glTF triangle index out of bounds"); mesh->indices.push_back(std::uint32_t(index));
          }
          if(!normals) {
            for(std::size_t i=0;i<count;i+=3) {
              auto& a=mesh->vertices[mesh->indices[i]]; auto& b=mesh->vertices[mesh->indices[i+1]]; auto& c=mesh->vertices[mesh->indices[i+2]];
              const auto normal=cross(b.position-a.position,c.position-a.position); a.normal+=normal; b.normal+=normal; c.normal+=normal;
            }
            for(auto& v:mesh->vertices) v.normal=normalize(v.normal);
          }
          // glTF permits distinct transforms per texture. Keep one shared UV set
          // only when all maps use the same transform; reject silent mismatches.
          if(primitive.material) {
            const auto& base=primitive.material->pbr_metallic_roughness.base_color_texture;
            const cgltf_texture_view* views[]={&base,&primitive.material->normal_texture,
              &primitive.material->pbr_metallic_roughness.metallic_roughness_texture,&primitive.material->emissive_texture};
            const cgltf_texture_transform* transform=nullptr;
            for(auto* view:views) if(view->texture && view->has_transform) { transform=&view->transform; break; }
            if(transform) {
              for(auto* view:views) if(view->texture) {
                require(view->has_transform && view->transform.offset[0]==transform->offset[0] &&
                  view->transform.offset[1]==transform->offset[1] && view->transform.scale[0]==transform->scale[0] &&
                  view->transform.scale[1]==transform->scale[1] && view->transform.rotation==transform->rotation,
                  "Different transforms per glTF texture are not yet supported");
              }
              const float cosine=std::cos(transform->rotation),sine=std::sin(transform->rotation);
              for(auto& v:mesh->vertices) {
                float u=v.uv.x*transform->scale[0],w=v.uv.y*transform->scale[1];
                v.uv={cosine*u-sine*w+transform->offset[0],sine*u+cosine*w+transform->offset[1]};
              }
            }
          }
          cached=mesh_cache.emplace(&primitive,std::move(mesh)).first;
        }
        GltfPrimitive item; item.mesh=cached->second; item.material=material_for(primitive.material); item.transform=world;
        item.name=node->name ? node->name : "glTF primitive";
        result.primitives.push_back(std::move(item));
      }
    }
    require(!result.primitives.empty(),"glTF scene contains no triangle meshes");
    const float huge=(std::numeric_limits<float>::max)(); result.bounds_min={huge,huge,huge}; result.bounds_max={-huge,-huge,-huge};
    for(const auto& primitive:result.primitives) for(const auto& vertex:primitive.mesh->vertices) {
      Vec3 p=transform_point(primitive.transform,vertex.position);
      result.bounds_min={(std::min)(result.bounds_min.x,p.x),(std::min)(result.bounds_min.y,p.y),(std::min)(result.bounds_min.z,p.z)};
      result.bounds_max={(std::max)(result.bounds_max.x,p.x),(std::max)(result.bounds_max.y,p.y),(std::max)(result.bounds_max.z,p.z)};
    }
    out=std::move(result); error.clear(); return true;
  } catch(const std::exception& exception) {
    if(parsed) cgltf_free(parsed);
    error=exception.what(); return false;
  }
}
}  // namespace fury
