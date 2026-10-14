#pragma once
// Opt-in observation for synthetic localhost network review. The production
// NetClient, received roster and GhostLoop update remain authoritative.
#include <fury/net.hpp>
#include <fury/scene.hpp>
#include <fury/render_settings.hpp>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
namespace vaultline {
class GhostNetworkReview {
  std::ofstream stream_;
  unsigned frame_{};
  unsigned disconnect_frame_{};
  unsigned reconnect_frame_{};
  std::string peer_ack_path_;
  unsigned sent_states_{};
  bool timing_failed_{};
  static std::uint64_t pose_hash(const fury::Mesh* mesh) {
    std::uint64_t hash=1469598103934665603ull;
    if(mesh)for(const auto& v:mesh->vertices)for(float value:{v.position.x,v.position.y,v.position.z}) {
      std::uint32_t word{};std::memcpy(&word,&value,sizeof(word));
      for(int k=0;k<4;++k){hash^=(word>>(k*8))&255u;hash*=1099511628211ull;}
    }
    return hash;
  }
public:
  GhostNetworkReview(const std::string& path,unsigned disconnect_frame,unsigned reconnect_frame,const std::string& peer_ack):disconnect_frame_(disconnect_frame),reconnect_frame_(reconnect_frame),peer_ack_path_(peer_ack) {
    if(!path.empty())stream_.open(path,std::ios::out|std::ios::trunc);
  }
  bool good()const{return stream_.good()&&!timing_failed_;}
  bool await_sent_snapshot(const fury::net::NetClient& client) {
    if(peer_ack_path_.empty()||!client.connected())return true;
    ++sent_states_;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    // The fixture writes this barrier only after sendto has queued the real
    // UDP snapshot. The following production poll still receives/parses it.
    for(;;) {
      unsigned acknowledged=0;std::ifstream(peer_ack_path_)>>acknowledged;
      if(acknowledged>=sent_states_)return true;
      if(std::chrono::steady_clock::now()>=deadline){timing_failed_=true;return false;}
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  void begin_frame(fury::net::NetClient& client,std::uint16_t port) {
    ++frame_;
    if(disconnect_frame_&&frame_==disconnect_frame_)client.disconnect();
    if(reconnect_frame_&&frame_==reconnect_frame_) {
      fury::net::ConnectOptions options;options.ensure_embedded_host=false;
      client.connect("127.0.0.1",port,options);
    }
  }
  void record(const fury::net::NetClient& client,const fury::Entity& e,float dt,
              double travel,bool has_position,float last_cash,float cash_flash,const fury::Scene& scene,float time_of_day) {
    auto vector=[&](fury::Vec3 p){stream_<<'['<<p.x<<','<<p.y<<','<<p.z<<']';};
    stream_<<std::setprecision(10)<<"{\"frame\":"<<frame_<<",\"dt\":"<<dt
      <<",\"source_fingerprint\":"<<std::quoted(fury::build_source_fingerprint())
      <<",\"connected\":"<<(client.connected()?"true":"false")
      <<",\"received_roster_size\":"<<client.remote_players().size()<<",\"received\":[";
    bool first=true;
    for(const auto& remote:client.remote_players()) {
      if(!first)stream_<<',';
      first=false;
      stream_<<"{\"id\":"<<remote.id<<",\"position\":";vector(remote.position);
      stream_<<",\"yaw\":"<<remote.yaw<<",\"heat\":"<<remote.heat<<",\"cash\":"<<remote.cash
        <<",\"in_heist\":"<<(remote.in_heist?"true":"false")<<'}';
    }
    stream_<<"],\"entity_position\":";vector(e.transform.position);
    stream_<<",\"entity_yaw\":"<<e.transform.rotation_euler.y
      <<",\"visible\":"<<(e.visible?"true":"false")
      <<",\"travel_distance\":"<<travel<<",\"position_valid\":"<<(has_position?"true":"false")
      <<",\"last_cash\":"<<last_cash<<",\"cash_flash\":"<<cash_flash<<",\"albedo\":";vector(e.material.albedo);
    stream_<<",\"emissive\":"<<e.material.emissive
      <<",\"near_pose_hash\":"<<pose_hash(e.mesh)<<",\"far_pose_hash\":"<<pose_hash(e.lod_mesh)
      <<",\"near_revision\":"<<(e.mesh?e.mesh->geometry_revision:0)
      <<",\"far_revision\":"<<(e.lod_mesh?e.lod_mesh->geometry_revision:0)
      <<",\"near_mesh_identity\":"<<(e.mesh?e.mesh->geometry_identity:0)
      <<",\"far_mesh_identity\":"<<(e.lod_mesh?e.lod_mesh->geometry_identity:0)
      <<",\"near_vertex_capacity\":"<<(e.mesh?e.mesh->vertices.capacity():0)
      <<",\"far_vertex_capacity\":"<<(e.lod_mesh?e.lod_mesh->vertices.capacity():0)
      <<",\"scene_mesh_count\":"<<scene.meshes().size()<<",\"time_of_day\":"<<time_of_day
      <<",\"peer_send_barrier\":"<<(!peer_ack_path_.empty()?"true":"false")<<",\"PlayerBody\":";
    const fury::Entity* player=nullptr;
    for(const auto& entity:scene.entities())if(entity.name=="PlayerBody"){player=&entity;break;}
    if(player) {
      stream_<<"{\"position\":";vector(player->transform.position);
      stream_<<",\"rotation\":";vector(player->transform.rotation_euler);
      stream_<<",\"scale\":";vector(player->transform.scale);
      stream_<<",\"visible\":"<<(player->visible?"true":"false")<<",\"near_pose_hash\":"<<pose_hash(player->mesh)
        <<",\"far_pose_hash\":"<<pose_hash(player->lod_mesh)<<",\"albedo\":";vector(player->material.albedo);
      stream_<<",\"emissive\":"<<player->material.emissive<<'}';
    } else stream_<<"null";
    stream_<<"}\n";
    stream_.flush();
  }
};
} // namespace vaultline
