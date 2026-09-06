#include "fury/net.hpp"

#include "fury/log.hpp"
#include "fury/platform.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#if FURY_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
using socklen_t = int;
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace fury {
namespace net {
namespace {

#if FURY_WINDOWS
using Socket = SOCKET;
constexpr Socket kInvalid = INVALID_SOCKET;
inline void close_socket(Socket s) {
  if (s != kInvalid) closesocket(s);
}
inline bool set_nonblocking(Socket s) {
  u_long mode = 1;
  return ioctlsocket(s, FIONBIO, &mode) == 0;
}
struct WinsockLifetime {
  WinsockLifetime() {
    WSADATA wsa{};
    m_ok = WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
  }
  ~WinsockLifetime() {
    if (m_ok) WSACleanup();
  }
  bool m_ok{false};
};
inline bool ensure_sockets() {
  static WinsockLifetime once;
  return once.m_ok;
}
#else
using Socket = int;
constexpr Socket kInvalid = -1;
inline void close_socket(Socket s) {
  if (s != kInvalid) ::close(s);
}
inline bool set_nonblocking(Socket s) {
  const int flags = fcntl(s, F_GETFL, 0);
  return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
inline bool ensure_sockets() { return true; }
#endif

#pragma pack(push, 1)
struct PacketHeader {
  std::uint32_t magic{kProtocolMagic};
  std::uint16_t version{kProtocolVersion};
  std::uint16_t type{0};
  std::uint32_t payload_bytes{0};
};

struct StatePayload {
  std::uint32_t id{0};
  float px{0.f}, py{0.f}, pz{0.f};
  float yaw{0.f};
  float heat{0.f};
  std::uint8_t heist_phase{0};
  std::uint8_t flags{0};  // bit0 = in_heist
  std::uint16_t pad{0};
  /// Optional trailing field (v1+): wallet cash. Older peers omit it.
  float cash{0.f};
};

/// Core payload without optional cash (for size checks / back-compat).
constexpr std::size_t kStatePayloadCore =
    sizeof(StatePayload) - sizeof(float);
#pragma pack(pop)

constexpr std::size_t kMaxPacket = 1024;

void write_header(std::uint8_t* buf, PacketType type, std::uint32_t payload) {
  PacketHeader h;
  h.type = static_cast<std::uint16_t>(type);
  h.payload_bytes = payload;
  std::memcpy(buf, &h, sizeof(h));
}

bool read_header(const std::uint8_t* buf, int n, PacketHeader& out) {
  if (n < static_cast<int>(sizeof(PacketHeader))) return false;
  std::memcpy(&out, buf, sizeof(out));
  return out.magic == kProtocolMagic && out.version == kProtocolVersion;
}

StatePayload pack_state(const PlayerState& s) {
  StatePayload p;
  p.id = s.id;
  p.px = s.position.x;
  p.py = s.position.y;
  p.pz = s.position.z;
  p.yaw = s.yaw;
  p.heat = s.heat;
  p.heist_phase = s.heist_phase;
  p.flags = s.in_heist ? 1u : 0u;
  p.cash = s.cash;
  return p;
}

PlayerState unpack_state(const StatePayload& p, const std::string& name) {
  PlayerState s;
  s.id = p.id;
  s.display_name = name;
  s.position = {p.px, p.py, p.pz};
  s.yaw = p.yaw;
  s.heat = p.heat;
  s.heist_phase = p.heist_phase;
  s.in_heist = (p.flags & 1u) != 0;
  s.cash = p.cash;
  return s;
}

/// Unpack from wire buffer; cash is optional if payload is short.
PlayerState unpack_state_bytes(const std::uint8_t* bytes, int nbytes,
                               const std::string& name) {
  StatePayload sp{};
  const int copy_n =
      std::min(nbytes, static_cast<int>(sizeof(StatePayload)));
  if (copy_n > 0) {
    std::memcpy(&sp, bytes, static_cast<std::size_t>(copy_n));
  }
  if (nbytes < static_cast<int>(sizeof(StatePayload))) {
    sp.cash = 0.f;
  }
  return unpack_state(sp, name);
}

class LoopbackServer final : public NetServer {
 public:
  ~LoopbackServer() override { stop(); }

  bool start(std::uint16_t port) override {
    return start_internal(port, false);
  }

  bool start_threaded(std::uint16_t port) override {
    return start_internal(port, true);
  }

  void stop() override {
    m_running = false;
    if (m_thread.joinable()) {
      m_thread.join();
    }
    std::lock_guard<std::mutex> lock(m_mu);
    close_socket(m_sock);
    m_sock = kInvalid;
    m_clients.clear();
  }

  bool running() const override { return m_running.load(); }

  void tick(float dt) override {
    if (!m_running.load()) return;
    pump_recv();
    update_bot(dt);
    broadcast_snapshot();
  }

  const SessionInfo& session() const override { return m_session; }

  const std::vector<PlayerState>& players() const override {
    // Snapshot under lock into mutable cache for const API.
    std::lock_guard<std::mutex> lock(m_mu);
    m_players_cache = m_players;
    return m_players_cache;
  }

  void assign_crew_role(std::uint32_t player_id, const std::string& name,
                        CrewRole role) {
    std::lock_guard<std::mutex> lock(m_mu);
    constexpr std::size_t kMaxCrew = 2;
    for (auto& slot : m_crew) {
      if (slot.player_id == player_id) {
        slot.display_name = name;
        slot.role = role;
        return;
      }
    }
    if (m_crew.size() >= kMaxCrew) {
      m_crew.erase(m_crew.begin());
    }
    CrewAssignment a;
    a.player_id = player_id;
    a.display_name = name;
    a.role = role;
    m_crew.push_back(std::move(a));
  }

  std::vector<CrewAssignment> crew_roster_copy() const {
    std::lock_guard<std::mutex> lock(m_mu);
    return m_crew;
  }

 private:
  bool start_internal(std::uint16_t port, bool threaded) {
    if (m_running.load()) return true;
    if (!ensure_sockets()) {
      Log::error("NetServer: socket init failed");
      return false;
    }

    Socket s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s == kInvalid) {
      Log::error("NetServer: socket() failed");
      return false;
    }
    int yes = 1;
#if FURY_WINDOWS
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes),
               sizeof(yes));
#else
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
    if (!set_nonblocking(s)) {
      close_socket(s);
      Log::error("NetServer: nonblocking failed");
      return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
      close_socket(s);
      Log::error("NetServer: bind 127.0.0.1 failed");
      return false;
    }

    {
      std::lock_guard<std::mutex> lock(m_mu);
      m_sock = s;
      m_port = port;
      m_session.session_id =
          0x564C544CULL ^ (static_cast<std::uint64_t>(port) << 16);
      m_session.world_name = "Harbor Metro";
      m_session.max_players = 32;
      m_players.clear();
      m_clients.clear();

      PlayerState host;
      host.id = 1;
      host.display_name = "Operator";
      host.position = {0.f, 1.7f, 18.f};
      m_players.push_back(host);

      PlayerState bot;
      bot.id = 2;
      bot.display_name = "Ghost-Loop";
      bot.position = {8.f, 1.7f, 10.f};
      m_players.push_back(bot);
    }

    m_running = true;
    if (threaded) {
      m_thread = std::thread([this] {
        using clock = std::chrono::steady_clock;
        auto prev = clock::now();
        while (m_running.load()) {
          auto now = clock::now();
          float dt = std::chrono::duration<float>(now - prev).count();
          prev = now;
          if (dt > 0.1f) dt = 0.1f;
          tick(dt);
          std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
      });
    }

    std::ostringstream oss;
    oss << "NetServer UDP loopback session=" << m_session.session_id
        << " world=\"" << m_session.world_name << "\" port=" << static_cast<int>(port)
        << (threaded ? " (threaded)" : " (polled)");
    Log::info(oss.str());
    return true;
  }

  void update_bot(float dt) {
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_players.size() < 2) return;
    m_bot_t += dt;
    auto& bot = m_players[1];
    bot.position.x = 8.f + std::sin(m_bot_t * 0.35f) * 5.f;
    bot.position.z = 10.f + std::cos(m_bot_t * 0.35f) * 4.f;
    bot.position.y = 1.7f;
    bot.yaw = m_bot_t * 0.35f;
    // Mirror host heat/phase lightly so remote pawn shows synced gameplay.
    if (!m_players.empty()) {
      bot.heat = m_players[0].heat;
      bot.heist_phase = m_players[0].heist_phase;
      bot.in_heist = m_players[0].in_heist;
      bot.cash = m_players[0].cash;
    }
  }

  void pump_recv() {
    if (m_sock == kInvalid) return;
    for (;;) {
      std::uint8_t buf[kMaxPacket];
      sockaddr_in from{};
      socklen_t from_len = sizeof(from);
      const int n = static_cast<int>(
          ::recvfrom(m_sock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                     reinterpret_cast<sockaddr*>(&from), &from_len));
      if (n <= 0) break;

      PacketHeader hdr{};
      if (!read_header(buf, n, hdr)) continue;
      const std::uint8_t* payload = buf + sizeof(PacketHeader);
      const int pay_n = n - static_cast<int>(sizeof(PacketHeader));

      if (hdr.type == static_cast<std::uint16_t>(PacketType::Hello)) {
        {
          std::lock_guard<std::mutex> lock(m_mu);
          remember_client_unlocked(from);
        }
        send_welcome(from);
      } else if (hdr.type ==
                     static_cast<std::uint16_t>(PacketType::PlayerState) &&
                 pay_n >= static_cast<int>(kStatePayloadCore)) {
        StatePayload sp{};
        const int copy_n =
            std::min(pay_n, static_cast<int>(sizeof(StatePayload)));
        std::memcpy(&sp, payload, static_cast<std::size_t>(copy_n));
        if (pay_n < static_cast<int>(sizeof(StatePayload))) {
          sp.cash = 0.f;
        }
        std::lock_guard<std::mutex> lock(m_mu);
        remember_client_unlocked(from);
        if (!m_players.empty()) {
          auto& host = m_players[0];
          host.id = 1;
          host.position = {sp.px, sp.py, sp.pz};
          host.yaw = sp.yaw;
          host.heat = sp.heat;
          host.heist_phase = sp.heist_phase;
          host.in_heist = (sp.flags & 1u) != 0;
          host.cash = sp.cash;
          if (host.display_name.empty()) host.display_name = "Operator";
        }
      }
    }
  }

  void remember_client(const sockaddr_in& from) {
    std::lock_guard<std::mutex> lock(m_mu);
    remember_client_unlocked(from);
  }

  void remember_client_unlocked(const sockaddr_in& from) {
    for (const auto& c : m_clients) {
      if (c.sin_port == from.sin_port &&
          c.sin_addr.s_addr == from.sin_addr.s_addr) {
        return;
      }
    }
    m_clients.push_back(from);
  }

  void send_welcome(const sockaddr_in& to) {
    std::uint8_t buf[kMaxPacket];
    // payload: session_id u64 + local_id u32 + max_players u32
    std::uint8_t payload[16];
    std::uint64_t sid = 0;
    {
      std::lock_guard<std::mutex> lock(m_mu);
      sid = m_session.session_id;
    }
    std::memcpy(payload + 0, &sid, 8);
    std::uint32_t local_id = 1;
    std::uint32_t max_p = 32;
    std::memcpy(payload + 8, &local_id, 4);
    std::memcpy(payload + 12, &max_p, 4);
    write_header(buf, PacketType::Welcome, sizeof(payload));
    std::memcpy(buf + sizeof(PacketHeader), payload, sizeof(payload));
    ::sendto(m_sock, reinterpret_cast<const char*>(buf),
             sizeof(PacketHeader) + sizeof(payload), 0,
             reinterpret_cast<const sockaddr*>(&to), sizeof(to));
  }

  void broadcast_snapshot() {
    if (m_sock == kInvalid) return;
    std::vector<sockaddr_in> clients;
    std::vector<StatePayload> states;
    {
      std::lock_guard<std::mutex> lock(m_mu);
      clients = m_clients;
      states.reserve(m_players.size());
      for (const auto& p : m_players) {
        states.push_back(pack_state(p));
      }
    }
    if (clients.empty() || states.empty()) return;

    std::uint8_t buf[kMaxPacket];
    const std::uint16_t count = static_cast<std::uint16_t>(states.size());
    const std::uint32_t payload_bytes =
        static_cast<std::uint32_t>(2 + count * sizeof(StatePayload));
    if (sizeof(PacketHeader) + payload_bytes > kMaxPacket) return;
    write_header(buf, PacketType::StateSnapshot, payload_bytes);
    std::memcpy(buf + sizeof(PacketHeader), &count, 2);
    std::memcpy(buf + sizeof(PacketHeader) + 2, states.data(),
                count * sizeof(StatePayload));
    const int total =
        static_cast<int>(sizeof(PacketHeader) + payload_bytes);
    for (const auto& c : clients) {
      ::sendto(m_sock, reinterpret_cast<const char*>(buf), total, 0,
               reinterpret_cast<const sockaddr*>(&c), sizeof(c));
    }
  }

  mutable std::mutex m_mu;
  Socket m_sock{kInvalid};
  std::uint16_t m_port{0};
  SessionInfo m_session{};
  std::vector<PlayerState> m_players;
  mutable std::vector<PlayerState> m_players_cache;
  std::vector<CrewAssignment> m_crew;
  std::vector<sockaddr_in> m_clients;
  float m_bot_t{0.f};
  std::atomic<bool> m_running{false};
  std::thread m_thread;
};

// Process-wide embedded host so a single vaultline process can host+join.
LoopbackServer& embedded_server() {
  static LoopbackServer server;
  return server;
}

class LoopbackClient final : public NetClient {
 public:
  bool connect(const std::string& address, std::uint16_t port) override {
    if (!ensure_sockets()) return false;
    m_address = address.empty() ? "127.0.0.1" : address;
    m_port = port;

    // Ensure an in-process host exists (threaded UDP) for single-process demos.
    auto& host = embedded_server();
    if (!host.running()) {
      if (!host.start_threaded(port)) {
        Log::error("NetClient: failed to start embedded loopback server");
        return false;
      }
    }

    Socket s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s == kInvalid) {
      Log::error("NetClient: socket() failed");
      return false;
    }
    if (!set_nonblocking(s)) {
      close_socket(s);
      return false;
    }

    // Bind ephemeral loopback port so the server can reply.
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(0);
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
      close_socket(s);
      Log::error("NetClient: bind failed");
      return false;
    }

    m_sock = s;
    m_server_addr = {};
    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_port = htons(port);
    inet_pton(AF_INET, m_address.c_str(), &m_server_addr.sin_addr);

    // Hello handshake
    std::uint8_t buf[64];
    std::uint32_t ver = kProtocolVersion;
    write_header(buf, PacketType::Hello, sizeof(ver));
    std::memcpy(buf + sizeof(PacketHeader), &ver, sizeof(ver));
    ::sendto(m_sock, reinterpret_cast<const char*>(buf),
             sizeof(PacketHeader) + sizeof(ver), 0,
             reinterpret_cast<sockaddr*>(&m_server_addr), sizeof(m_server_addr));

    m_connected = true;
    m_local_id = 1;
    m_session = host.session();

    std::ostringstream oss;
    oss << "NetClient UDP connected to " << m_address << ":"
        << static_cast<int>(port) << " session=" << m_session.session_id
        << " as player#" << m_local_id << " proto=v" << kProtocolVersion;
    Log::info(oss.str());
    return true;
  }

  void disconnect() override {
    m_connected = false;
    close_socket(m_sock);
    m_sock = kInvalid;
  }

  bool connected() const override { return m_connected; }

  void send_player_state(const PlayerState& state) override {
    if (!m_connected || m_sock == kInvalid) return;
    StatePayload sp = pack_state(state);
    sp.id = m_local_id;
    std::uint8_t buf[sizeof(PacketHeader) + sizeof(StatePayload)];
    write_header(buf, PacketType::PlayerState, sizeof(sp));
    std::memcpy(buf + sizeof(PacketHeader), &sp, sizeof(sp));
    ::sendto(m_sock, reinterpret_cast<const char*>(buf), sizeof(buf), 0,
             reinterpret_cast<sockaddr*>(&m_server_addr), sizeof(m_server_addr));

    // Also push into embedded server players for same-process consistency.
    // (UDP path still exercised above.)
  }

  void poll() override {
    if (!m_connected || m_sock == kInvalid) return;

    // If host is polled (no thread), drive it here — threaded host self-ticks.
    auto& host = embedded_server();
    // Always refresh crew from host.
    m_crew = host.crew_roster_copy();
    m_session = host.session();

    for (;;) {
      std::uint8_t buf[kMaxPacket];
      sockaddr_in from{};
      socklen_t from_len = sizeof(from);
      const int n = static_cast<int>(
          ::recvfrom(m_sock, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                     reinterpret_cast<sockaddr*>(&from), &from_len));
      if (n <= 0) break;

      PacketHeader hdr{};
      if (!read_header(buf, n, hdr)) continue;
      const std::uint8_t* payload = buf + sizeof(PacketHeader);
      const int pay_n = n - static_cast<int>(sizeof(PacketHeader));

      if (hdr.type == static_cast<std::uint16_t>(PacketType::Welcome) &&
          pay_n >= 16) {
        std::uint64_t sid = 0;
        std::uint32_t local_id = 1;
        std::memcpy(&sid, payload, 8);
        std::memcpy(&local_id, payload + 8, 4);
        m_session.session_id = sid;
        m_local_id = local_id;
      } else if (hdr.type ==
                 static_cast<std::uint16_t>(PacketType::StateSnapshot)) {
        if (pay_n < 2) continue;
        std::uint16_t count = 0;
        std::memcpy(&count, payload, 2);
        const int stride_full = static_cast<int>(sizeof(StatePayload));
        const int stride_core = static_cast<int>(kStatePayloadCore);
        int stride = stride_full;
        int need = 2 + static_cast<int>(count) * stride;
        if (pay_n < need) {
          stride = stride_core;
          need = 2 + static_cast<int>(count) * stride;
          if (pay_n < need) continue;
        }
        m_remotes.clear();
        for (std::uint16_t i = 0; i < count; ++i) {
          const std::uint8_t* entry = payload + 2 + i * stride;
          StatePayload peek{};
          std::memcpy(&peek, entry,
                      static_cast<std::size_t>(
                          std::min(stride, static_cast<int>(sizeof(peek)))));
          if (peek.id == m_local_id) continue;
          std::string name = (peek.id == 2) ? "Ghost-Loop" : "Remote";
          m_remotes.push_back(unpack_state_bytes(entry, stride, name));
        }
      }
    }

    // Fallback: if UDP snapshot not yet received, mirror host remotes.
    if (m_remotes.empty()) {
      for (const auto& p : host.players()) {
        if (p.id != m_local_id) m_remotes.push_back(p);
      }
    }
  }

  const SessionInfo& session() const override { return m_session; }
  const std::vector<PlayerState>& remote_players() const override {
    return m_remotes;
  }
  std::uint32_t local_player_id() const override { return m_local_id; }

  void assign_crew_role(std::uint32_t player_id, const std::string& name,
                        CrewRole role) override {
    embedded_server().assign_crew_role(player_id, name, role);
    m_crew = embedded_server().crew_roster_copy();
    std::ostringstream oss;
    oss << "Crew role assigned: " << name << " -> " << crew_role_name(role)
        << " (player#" << player_id << ")";
    Log::info(oss.str());
  }

  const std::vector<CrewAssignment>& crew_roster() const override {
    return m_crew;
  }

  ~LoopbackClient() override { disconnect(); }

 private:
  Socket m_sock{kInvalid};
  bool m_connected{false};
  std::uint32_t m_local_id{1};
  std::uint16_t m_port{0};
  std::string m_address;
  sockaddr_in m_server_addr{};
  SessionInfo m_session{};
  std::vector<PlayerState> m_remotes;
  std::vector<CrewAssignment> m_crew;
};

}  // namespace

std::unique_ptr<NetServer> create_loopback_server() {
  return std::make_unique<LoopbackServer>();
}

std::unique_ptr<NetClient> create_loopback_client() {
  return std::make_unique<LoopbackClient>();
}

}  // namespace net
}  // namespace fury
