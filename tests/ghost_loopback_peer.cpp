// Diagnostic-only localhost peer. Include the actual production protocol
// implementation to reuse its packed payloads, pack_state/read_header and
// write_header, without exporting or changing the normal networking API.
#include "../engine/src/net_loopback.cpp"
#include <fstream>
#include <iomanip>
#include <iostream>
int main(int argc,char**argv) {
  if(argc<3||argc>6){std::cerr<<"usage: ghost_loopback_peer PORT TRACE.jsonl\n";return 2;}
  using namespace fury;using namespace fury::net;
  const int port=std::atoi(argv[1]);if(port<1024||port>65535)return 2;
  if(!ensure_sockets())return 3;
  Socket socket_fd=::socket(AF_INET,SOCK_DGRAM,0);if(socket_fd==kInvalid)return 3;
  sockaddr_in bind_address{};bind_address.sin_family=AF_INET;bind_address.sin_port=htons(port);bind_address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(::bind(socket_fd,reinterpret_cast<sockaddr*>(&bind_address),sizeof(bind_address))!=0){close_socket(socket_fd);return 4;}
  if(!set_nonblocking(socket_fd)){close_socket(socket_fd);return 4;}
  std::ofstream trace(argv[2]);if(!trace)return 5;
  const unsigned reconnect_id=argc>=4?unsigned(std::atoi(argv[3])):2;
  const unsigned silence_after=argc>=5?unsigned(std::atoi(argv[4])):0;
  const std::string ack_path=argc>=6?argv[5]:"";
  unsigned hellos=0;
  unsigned tick=0;auto latest=std::chrono::steady_clock::now();
  std::cout<<"ready "<<port<<std::endl;
  for(;;) {
    std::uint8_t in[kMaxPacket];sockaddr_in from{};socklen_t size=sizeof(from);
    const int count=::recvfrom(socket_fd,reinterpret_cast<char*>(in),sizeof(in),0,reinterpret_cast<sockaddr*>(&from),&size);
    if(count<=0){if(std::chrono::steady_clock::now()-latest>std::chrono::seconds(20))break;std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
    latest=std::chrono::steady_clock::now();PacketHeader header{};if(!read_header(in,count,header))continue;
    if(header.type==static_cast<unsigned>(PacketType::Hello)) {
      ++hellos;
      std::uint8_t out[28]{};write_header(out,PacketType::Welcome,16);
      const std::uint64_t session=0x47484f53544c4f4full;const std::uint32_t local=1,players=2;
      std::memcpy(out+sizeof(PacketHeader),&session,8);std::memcpy(out+sizeof(PacketHeader)+8,&local,4);std::memcpy(out+sizeof(PacketHeader)+12,&players,4);
      ::sendto(socket_fd,reinterpret_cast<const char*>(out),sizeof(out),0,reinterpret_cast<sockaddr*>(&from),size);continue;
    }
    if(header.type!=static_cast<unsigned>(PacketType::PlayerState))continue;
    ++tick;
    if(silence_after&&tick>silence_after){trace<<"{\"tick\":"<<tick<<",\"silence\":true}\n";trace.flush();continue;}
    PlayerState state;state.id=hellos>1?reconnect_id:2;state.position={8.f,1.7f,10.f};state.yaw=1.57079632679f;
    const char* phase="stationary";
    if(tick>8&&tick<=32){state.position.x+=.060f*(tick-8);phase="walk_x";}
    if(tick>32){state.position.x+=1.44f;state.yaw=3.14159265359f;phase="heading_stationary";}
    if(tick>40&&tick<=52){state.position.z+=.120f*(tick-40);phase="walk_z_cash_heat";}
    if(tick>52){state.position.z+=1.44f;phase="hot_stationary";}
    if(tick>40){state.heat=1.f;state.cash=500.f;state.in_heist=true;}
    const bool empty=tick>=65&&tick<=72;
    if(empty)phase="empty_received_roster";
    if(tick>72){state.position={10.8f,1.7f,10.f};state.yaw=.35f;state.heat=.35f;state.cash=0;state.in_heist=false;phase="return_without_reset";}
    std::uint8_t out[kMaxPacket]{};const std::uint16_t states=empty?0:1;const auto packed=pack_state(state);
    const unsigned payload=2+states*sizeof(StatePayload);write_header(out,PacketType::StateSnapshot,payload);
    std::memcpy(out+sizeof(PacketHeader),&states,2);if(states)std::memcpy(out+sizeof(PacketHeader)+2,&packed,sizeof(packed));
    const auto sent=::sendto(socket_fd,reinterpret_cast<const char*>(out),sizeof(PacketHeader)+payload,0,reinterpret_cast<sockaddr*>(&from),size);
    if(!ack_path.empty()){std::ofstream ack(ack_path,std::ios::trunc);ack<<tick<<'\n';ack.flush();if(!ack)return 7;}
    trace<<std::setprecision(10)<<"{\"tick\":"<<tick<<",\"phase\":"<<std::quoted(phase)<<",\"roster_count\":"<<states
      <<",\"position\":["<<state.position.x<<','<<state.position.y<<','<<state.position.z<<"],\"yaw\":"<<state.yaw
      <<",\"heat\":"<<state.heat<<",\"cash\":"<<state.cash<<",\"in_heist\":"<<(state.in_heist?"true":"false")
      <<",\"sent_bytes\":"<<sent<<",\"protocol_version\":"<<kProtocolVersion<<",\"id\":"<<state.id<<",\"packet_hex\":\"";
    for(unsigned k=0;k<sizeof(PacketHeader)+payload;++k)trace<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(out[k]);
    trace<<std::dec<<"\"}\n";trace.flush();
  }
  close_socket(socket_fd);return trace.good()?0:6;
}
