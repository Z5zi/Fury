#pragma once

/// Stub multiplayer / MMO session layer for Vaultline.
/// Real sockets / replication land later — these interfaces define the shape.

#include "fury/math.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fury {
namespace net {

struct PlayerState {
  std::uint32_t id{0};
  std::string display_name;
  Vec3 position{};
  float yaw{0.f};
  bool in_heist{false};
};

struct SessionInfo {
  std::uint64_t session_id{0};
  std::string world_name{"Harbor Metro"};
  int max_players{32};
};

/// Client-side network façade (localhost stub for now).
class NetClient {
 public:
  virtual ~NetClient() = default;

  virtual bool connect(const std::string& address, std::uint16_t port) = 0;
  virtual void disconnect() = 0;
  virtual bool connected() const = 0;

  virtual void send_player_state(const PlayerState& state) = 0;
  virtual void poll() = 0;

  virtual const SessionInfo& session() const = 0;
  virtual const std::vector<PlayerState>& remote_players() const = 0;
  virtual std::uint32_t local_player_id() const = 0;
};

/// Server-side network façade (in-process stub).
class NetServer {
 public:
  virtual ~NetServer() = default;

  virtual bool start(std::uint16_t port) = 0;
  virtual void stop() = 0;
  virtual bool running() const = 0;

  virtual void tick(float dt) = 0;
  virtual const SessionInfo& session() const = 0;
  virtual const std::vector<PlayerState>& players() const = 0;
};

/// Creates a localhost client/server pair that simulates 1–2 pawns in-process.
std::unique_ptr<NetClient> create_stub_client();
std::unique_ptr<NetServer> create_stub_server();

}  // namespace net
}  // namespace fury
