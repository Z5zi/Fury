// Synthetic local-socket lifecycle and arrival-order checks using the actual
// production packet types, serializer and NetClient receiver implementation.
#include "../engine/src/net_loopback.cpp"
#include <iostream>
#include <stdexcept>
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main() {
  using namespace fury;using namespace fury::net;
  Socket server=kInvalid;
  try {
    require(ensure_sockets(),"socket runtime");server=::socket(AF_INET,SOCK_DGRAM,0);require(server!=kInvalid,"server socket");
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    require(::bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"bind localhost ephemeral port");
    socklen_t length=sizeof(address);require(::getsockname(server,reinterpret_cast<sockaddr*>(&address),&length)==0,"query localhost port");
    require(set_nonblocking(server),"nonblocking local server");
    auto client=create_loopback_client();ConnectOptions options;options.ensure_embedded_host=false;
    sockaddr_in peer{};socklen_t peer_length=sizeof(peer);
    auto connect=[&] {
      require(client->connect("127.0.0.1",ntohs(address.sin_port),options),"real client connect");
      std::uint8_t hello[128];const int count=::recvfrom(server,reinterpret_cast<char*>(hello),sizeof(hello),0,reinterpret_cast<sockaddr*>(&peer),&peer_length);
      PacketHeader header{};require(read_header(hello,count,header)&&header.type==unsigned(PacketType::Hello),"real client Hello received through localhost socket");
      require(client->connected()&&client->remote_players().empty(),"fresh connection has no stale peer before new snapshot");
    };
    auto snapshot=[&](const PlayerState* state,bool invalid=false) {
      std::uint8_t packet[64]{};const std::uint16_t count=state?1:0;const unsigned bytes=2+count*sizeof(StatePayload);
      write_header(packet,PacketType::StateSnapshot,bytes);std::memcpy(packet+sizeof(PacketHeader),&count,2);
      if(state){const auto data=pack_state(*state);std::memcpy(packet+sizeof(PacketHeader)+2,&data,sizeof(data));}
      if(invalid)packet[0]^=0x80;
      require(::sendto(server,reinterpret_cast<char*>(packet),sizeof(PacketHeader)+bytes,0,reinterpret_cast<sockaddr*>(&peer),peer_length)==int(sizeof(PacketHeader)+bytes),"snapshot sent through actual UDP socket");
    };
    PlayerState state;state.id=2;state.position={8,1.7f,10};state.yaw=.7f;state.cash=300;state.heat=.8f;
    connect();snapshot(&state);client->poll();require(client->remote_players().size()==1&&client->remote_players()[0].position.x==8,"first received remote state");
    state.position.x=9;snapshot(&state);state.position.x=8;snapshot(&state);client->poll();
    require(client->remote_players().size()==1&&client->remote_players()[0].position.x==8,"current protocol preserves last-arrival ordering, including an intentionally older position arriving last");
    state.position.x=99;snapshot(&state,true);client->poll();require(client->remote_players()[0].position.x==8,"invalid protocol magic cannot replace active roster");
    snapshot(nullptr);client->poll();require(client->remote_players().empty(),"accepted empty snapshot clears roster");
    snapshot(nullptr);client->poll();require(client->remote_players().empty(),"repeated empty snapshot remains empty");
    state.position.x=14;snapshot(&state);client->poll();require(client->remote_players().size()==1,"remote returns after empty snapshot");
    client->disconnect();require(!client->connected()&&client->remote_players().empty(),"disconnect clears received remote state");
    connect();state.position.x=40;snapshot(&state);client->poll();require(client->remote_players()[0].id==2&&client->remote_players()[0].position.x==40,"same peer reconnect uses new snapshot");
    client->disconnect();connect();state.id=3;state.position.x=12;snapshot(&state);client->poll();require(client->remote_players()[0].id==3,"different peer reconnect uses new snapshot");
    for(int i=0;i<5;++i)client->poll();require(client->remote_players().size()==1,"absent packets do not fabricate an empty snapshot or new transport timeout");
    client->disconnect();close_socket(server);server=kInvalid;
    std::cout<<"Real UDP: initial/empty/repeated-empty/disconnect/same-peer/different-peer reconnect, invalid magic and last-arrival ordering passed; no timeout redesign\n";return 0;
  } catch(const std::exception& e) {if(server!=kInvalid)close_socket(server);std::cerr<<e.what()<<'\n';return 1;}
}
