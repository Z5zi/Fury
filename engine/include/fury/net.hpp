#pragma once

/// Localhost UDP multiplayer session layer for Vaultline.
/// NetServer + NetClient exchange binary state packets (loopback by default;
/// host/join modes can listen / connect beyond the embedded in-process path).

#include "fury/math.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fury {
namespace net {

/// Binary protocol magic / version (see README "Networking").
constexpr std::uint32_t kProtocolMagic = 0x564C544Cu;  // 'VLTL'
constexpr std::uint16_t kProtocolVersion = 1;

enum class PacketType : std::uint16_t {
  Hello = 1,
  Welcome = 2,
  PlayerState = 3,
  StateSnapshot = 4,
  Chat = 5,
};

enum class NetMode {
  Embedded = 0,  ///< In-process threaded host on 127.0.0.1 + local client (default)
  Host,          ///< Listen (INADDR_ANY) + local client joins 127.0.0.1
  Join,          ///< Client-only connect to FURY_NET_HOST (no embedded host)
};

enum class ListenBind {
  Loopback = 0,
  Any,
};

struct ConnectOptions {
  bool ensure_embedded_host{true};
  ListenBind host_bind{ListenBind::Loopback};
};

struct PlayerState {
  std::uint32_t id{0};
  std::string display_name;
  Vec3 position{};
  float yaw{0.f};
  bool in_heist{false};
  float heat{0.f};
  /// Matches HeistPhase ordinal (Idle=0 … Failed=6).
  std::uint8_t heist_phase{0};
  /// Host-selected mission board index (0..kMissionCount-1); joiners mirror.
  std::uint8_t mission_index{0};
  /// Loot progress 0..1 while looting / after (shared for co-op HUD).
  float loot_progress{0.f};
  /// Optional synced wallet cash (co-op ready; 0 if peer omits field).
  float cash{0.f};
  /// Lobby / ready-check pip (flags bit1 on the wire).
  bool ready{false};
};

struct ChatLine {
  std::uint32_t sender_id{0};
  std::string sender_name;
  std::string text;
};

struct SessionInfo {
  std::uint64_t session_id{0};
  std::string world_name{"Harbor Metro"};
  int max_players{32};
};

enum class CrewRole {
  None = 0,
  Driver,
  Hacker,
  Muscle,
  Lookout,
};

inline const char* crew_role_name(CrewRole role) {
  switch (role) {
    case CrewRole::Driver: return "Driver";
    case CrewRole::Hacker: return "Hacker";
    case CrewRole::Muscle: return "Muscle";
    case CrewRole::Lookout: return "Lookout";
    default: return "None";
  }
}

struct CrewAssignment {
  std::uint32_t player_id{0};
  std::string display_name;
  CrewRole role{CrewRole::None};
  bool ready{false};
};

/// Client-side network façade (UDP).
class NetClient {
 public:
  virtual ~NetClient() = default;

  virtual bool connect(const std::string& address, std::uint16_t port,
                       ConnectOptions opts = {}) = 0;
  virtual void disconnect() = 0;
  virtual bool connected() const = 0;

  virtual void send_player_state(const PlayerState& state) = 0;
  /// Send a short chat line (UTF-8-ish bytes, truncated server-side).
  virtual void send_chat(const std::string& text) = 0;
  virtual void poll() = 0;

  virtual const SessionInfo& session() const = 0;
  virtual const std::vector<PlayerState>& remote_players() const = 0;
  virtual std::uint32_t local_player_id() const = 0;

  /// Last chat lines (newest last; capped at 4).
  virtual const std::vector<ChatLine>& chat_log() const = 0;

  /// Assign a session crew role (up to 2 AI/remote crew slots).
  virtual void assign_crew_role(std::uint32_t player_id, const std::string& name,
                                CrewRole role) = 0;
  virtual void set_crew_ready(std::uint32_t player_id, bool ready) = 0;
  virtual const std::vector<CrewAssignment>& crew_roster() const = 0;
};

/// Server-side network façade (UDP listen + optional worker thread).
class NetServer {
 public:
  virtual ~NetServer() = default;

  virtual bool start(std::uint16_t port,
                     ListenBind bind = ListenBind::Loopback) = 0;
  /// Start listen socket and a background tick thread (in-process host).
  virtual bool start_threaded(std::uint16_t port,
                              ListenBind bind = ListenBind::Loopback) = 0;
  virtual void stop() = 0;
  virtual bool running() const = 0;

  virtual void tick(float dt) = 0;
  virtual const SessionInfo& session() const = 0;
  virtual const std::vector<PlayerState>& players() const = 0;
};

/// Creates a localhost UDP client/server (real sockets).
std::unique_ptr<NetClient> create_loopback_client();
std::unique_ptr<NetServer> create_loopback_server();

/// Aliases — same loopback implementation (historical stub names).
inline std::unique_ptr<NetClient> create_stub_client() {
  return create_loopback_client();
}
inline std::unique_ptr<NetServer> create_stub_server() {
  return create_loopback_server();
}

inline const char* net_mode_name(NetMode mode) {
  switch (mode) {
    case NetMode::Host: return "host";
    case NetMode::Join: return "join";
    default: return "embedded";
  }
}

}  // namespace net
}  // namespace fury
