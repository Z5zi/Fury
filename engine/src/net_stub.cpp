#include "fury/net.hpp"

#include "fury/log.hpp"

#include <cmath>
#include <sstream>

namespace fury {
namespace net {
namespace {

class StubServer final : public NetServer {
 public:
  bool start(std::uint16_t port) override {
    m_running = true;
    // Stable session id for this process (Vaultline 'VLTL' + port nibble)
    m_session.session_id = 0x564C544CULL ^ (static_cast<std::uint64_t>(port) << 16);
    m_session.world_name = "Harbor Metro";
    m_session.max_players = 32;
    m_port = port;
    m_players.clear();

    PlayerState host;
    host.id = 1;
    host.display_name = "Operator";
    host.position = {0.f, 1.7f, 18.f};
    m_players.push_back(host);

    PlayerState bot;
    bot.id = 2;
    bot.display_name = "Ghost-Stub";
    bot.position = {8.f, 1.7f, 10.f};
    m_players.push_back(bot);

    std::ostringstream oss;
    oss << "NetServer stub session=" << m_session.session_id
        << " world=\"" << m_session.world_name << "\" port=" << static_cast<int>(port)
        << " max_players=" << m_session.max_players;
    Log::info(oss.str());
    return true;
  }

  void stop() override { m_running = false; }
  bool running() const override { return m_running; }

  void tick(float dt) override {
    if (!m_running || m_players.size() < 2) {
      return;
    }
    m_bot_t += dt;
    auto& bot = m_players[1];
    // Patrol path along Pier Street toward extraction alley
    bot.position.x = 8.f + std::sin(m_bot_t * 0.35f) * 5.f;
    bot.position.z = 10.f + std::cos(m_bot_t * 0.35f) * 4.f;
    bot.position.y = 1.7f;
    bot.yaw = m_bot_t * 0.35f;
    bot.in_heist = false;
  }

  const SessionInfo& session() const override { return m_session; }
  const std::vector<PlayerState>& players() const override { return m_players; }
  std::vector<PlayerState>& players_mut() { return m_players; }

 private:
  bool m_running{false};
  std::uint16_t m_port{0};
  SessionInfo m_session{};
  std::vector<PlayerState> m_players;
  float m_bot_t{0.f};
};

class StubClient final : public NetClient {
 public:
  explicit StubClient(StubServer* server) : m_server(server) {}

  bool connect(const std::string& address, std::uint16_t port) override {
    m_address = address;
    m_port = port;
    if (!m_server) {
      return false;
    }
    if (!m_server->running() && !m_server->start(port)) {
      return false;
    }
    m_connected = true;
    m_local_id = 1;
    m_session = m_server->session();
    std::ostringstream oss;
    oss << "NetClient stub connected to " << address << ":" << static_cast<int>(port)
        << " session=" << m_session.session_id
        << " as player#" << m_local_id;
    Log::info(oss.str());
    return true;
  }

  void disconnect() override { m_connected = false; }
  bool connected() const override { return m_connected; }

  void send_player_state(const PlayerState& state) override {
    if (!m_connected || !m_server) {
      return;
    }
    auto& players = m_server->players_mut();
    if (!players.empty()) {
      players[0] = state;
      players[0].id = m_local_id;
      if (players[0].display_name.empty()) {
        players[0].display_name = "Operator";
      }
    }
  }

  void poll() override {
    if (!m_connected || !m_server) {
      return;
    }
    m_server->tick(1.f / 60.f);
    m_session = m_server->session();
    m_remotes.clear();
    for (const auto& p : m_server->players()) {
      if (p.id != m_local_id) {
        m_remotes.push_back(p);
      }
    }
  }

  const SessionInfo& session() const override { return m_session; }
  const std::vector<PlayerState>& remote_players() const override {
    return m_remotes;
  }
  std::uint32_t local_player_id() const override { return m_local_id; }

 private:
  StubServer* m_server{nullptr};
  bool m_connected{false};
  std::uint32_t m_local_id{1};
  std::uint16_t m_port{0};
  std::string m_address;
  SessionInfo m_session{};
  std::vector<PlayerState> m_remotes;
};

}  // namespace

std::unique_ptr<NetServer> create_stub_server() {
  return std::make_unique<StubServer>();
}

std::unique_ptr<NetClient> create_stub_client() {
  static StubServer server;
  if (!server.running()) {
    server.start(7777);
  }
  return std::make_unique<StubClient>(&server);
}

}  // namespace net
}  // namespace fury
