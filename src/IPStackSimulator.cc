#include "IPStackSimulator.hh"

#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#ifndef PHOSG_WINDOWS
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <string>
#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Time.hh>

#include "IPFrameInfo.hh"
#include "DNSServer.hh"

using namespace std;



static const size_t DEFAULT_RESEND_PUSH_USECS = 200000; // 200ms



// Note: these functions exist because seq nums are allowed to wrap around the
// 32-bit integer space by design. We have to do the subtraction before the
// comparison to allow integer overflow to occur if needed.

static inline bool seq_num_less(uint32_t a, uint32_t b) {
  return (a - b) & 0x80000000;
}

static inline bool seq_num_less_or_equal(uint32_t a, uint32_t b) {
  return (a == b) || seq_num_less(a, b);
}

static inline bool seq_num_greater(uint32_t a, uint32_t b) {
  return (b - a) & 0x80000000;
}

static __attribute__((unused)) inline bool seq_num_greater_or_equal(uint32_t a, uint32_t b) {
  return (a == b) || seq_num_greater(a, b);
}



string IPStackSimulator::str_for_ipv4_netloc(uint32_t addr, uint16_t port) {
  be_uint32_t be_addr = addr;
  char addr_str[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &be_addr, addr_str, INET_ADDRSTRLEN)) {
    return string_printf("<UNKNOWN>:%hu", port);
  } else {
    return string_printf("%s:%hu", addr_str, port);
  }
}

string IPStackSimulator::str_for_tcp_connection(shared_ptr<const IPClient> c,
    const IPClient::TCPConnection& conn) {
  uint64_t key = IPStackSimulator::tcp_conn_key_for_connection(conn);
  string server_netloc_str = str_for_ipv4_netloc(conn.server_addr, conn.server_port);
  string client_netloc_str = str_for_ipv4_netloc(c->ipv4_addr, conn.client_port);
  int fd = bufferevent_getfd(c->bev.get());
  return string_printf("%d+%016" PRIX64 " (%s -> %s)",
      fd, key, client_netloc_str.c_str(), server_netloc_str.c_str());
}



IPStackSimulator::IPStackSimulator(
    std::shared_ptr<struct event_base> base,
    std::shared_ptr<Server> game_server,
    std::shared_ptr<ProxyServer> proxy_server,
    std::shared_ptr<ServerState> state)
  : base(base),
    game_server(game_server),
    proxy_server(proxy_server),
    state(state),
    proxy_destination_address(0),
    pcap_text_log_file(state->ip_stack_debug ? fopen("IPStackSimulator-Log.txt", "wt") : nullptr) {
  memset(this->host_mac_address_bytes, 0x90, 6);
  memset(this->broadcast_mac_address_bytes, 0xFF, 6);
}

IPStackSimulator::~IPStackSimulator() {
  if (this->pcap_text_log_file) {
    fclose(this->pcap_text_log_file);
  }
}



void IPStackSimulator::listen(const std::string& socket_path) {
  this->add_socket(::listen(socket_path, 0, SOMAXCONN));
}

void IPStackSimulator::listen(const std::string& addr, int port) {
  this->add_socket(::listen(addr, port, SOMAXCONN));
}

void IPStackSimulator::listen(int port) {
  this->add_socket(::listen("", port, SOMAXCONN));
}

void IPStackSimulator::add_socket(int fd) {
  this->listeners.emplace(
      evconnlistener_new(
        this->base.get(),
        IPStackSimulator::dispatch_on_listen_accept,
        this,
        LEV_OPT_REUSEABLE,
        0,
        fd),
      evconnlistener_free);
}



uint32_t IPStackSimulator::connect_address_for_remote_address(uint32_t remote_addr) {
  // Use an address not on the same subnet as the client, so that PSO Plus and
  // Episode III will think they're talking to a remote network and won't reject
  // the connection.
  if ((remote_addr & 0xFF000000) != 0x23000000) {
    return 0x23232323;
  } else {
    return 0x24242424;
  }
}



IPStackSimulator::IPClient::IPClient(struct bufferevent* bev)
  : bev(bev, bufferevent_free), ipv4_addr(0) {
  memset(this->mac_addr, 0, 6);
}



static void flush_and_free_bufferevent(struct bufferevent* bev) {
  bufferevent_flush(bev, EV_READ | EV_WRITE, BEV_FINISHED);
  bufferevent_free(bev);
}

IPStackSimulator::IPClient::TCPConnection::TCPConnection()
  : server_bev(nullptr, flush_and_free_bufferevent),
    pending_data(evbuffer_new(), evbuffer_free),
    resend_push_event(nullptr, event_free) { }



void IPStackSimulator::dispatch_on_listen_accept(
    struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr *address, int socklen, void* ctx) {
  reinterpret_cast<IPStackSimulator*>(ctx)->on_listen_accept(
      listener, fd, address, socklen);
}

void IPStackSimulator::on_listen_accept(struct evconnlistener* listener,
    evutil_socket_t fd, struct sockaddr*, int) {
  int listen_fd = evconnlistener_get_fd(listener);
  log(INFO, "[IPStackSimulator] Client fd %d connected via fd %d",
      fd, listen_fd);

  struct bufferevent *bev = bufferevent_socket_new(this->base.get(), fd,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  shared_ptr<IPClient> c(new IPClient(bev));
  c->sim = this;
  this->bev_to_client.emplace(make_pair(bev, c));

  bufferevent_setcb(bev, &IPStackSimulator::dispatch_on_client_input, nullptr,
      &IPStackSimulator::dispatch_on_client_error, this);
  bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void IPStackSimulator::dispatch_on_listen_error(
    struct evconnlistener* listener, void* ctx) {
  reinterpret_cast<IPStackSimulator*>(ctx)->on_listen_error(listener);
}

void IPStackSimulator::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  log(ERROR, "[IPStackSimulator] Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), nullptr);
}



void IPStackSimulator::dispatch_on_client_input(
    struct bufferevent* bev, void* ctx) {
  reinterpret_cast<IPStackSimulator*>(ctx)->on_client_input(bev);
}

void IPStackSimulator::on_client_input(struct bufferevent* bev) {
  struct evbuffer* buf = bufferevent_get_input(bev);

  shared_ptr<IPClient> c;
  try {
    c = this->bev_to_client.at(bev);
  } catch (const out_of_range&) {
    size_t bytes = evbuffer_get_length(buf);
    log(ERROR, "[IPStackSimulator] Ignoring data received from unregistered client (0x%zX bytes)",
        bytes);
    evbuffer_drain(buf, bytes);
    return;
  }

  while (evbuffer_get_length(buf) >= 2) {
    uint16_t frame_size;
    evbuffer_copyout(buf, &frame_size, 2);
    if (evbuffer_get_length(buf) < static_cast<size_t>(frame_size + 2)) {
      break; // No complete frame available; done for now
    }

    evbuffer_drain(buf, 2);
    string frame(frame_size, '\0');
    evbuffer_remove(buf, frame.data(), frame.size());

    try {
      this->on_client_frame(c, frame);
    } catch (const exception& e) {
      log(WARNING, "[IPStackSimulator] Failed to process client frame: %s", e.what());
      print_data(stderr, frame);
    }
  }
}

void IPStackSimulator::dispatch_on_client_error(
    struct bufferevent* bev, short events, void* ctx) {
  reinterpret_cast<IPStackSimulator*>(ctx)->on_client_error(bev, events);
}
void IPStackSimulator::on_client_error(struct bufferevent* bev,
    short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[IPStackSimulator] Client caused error %d (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    log(INFO, "[IPStackSimulator] Client fd %d disconnected",
        bufferevent_getfd(bev));

    this->bev_to_client.erase(bev);
  }
}



void IPStackSimulator::on_client_frame(
    shared_ptr<IPClient> c, const string& frame) {
  if (this->state->ip_stack_debug) {
    fputc('\n', stderr);
    log(INFO, "[IPStackSimulator] Client sent frame");
    print_data(stderr, frame);
  }
  this->log_frame(frame);

  FrameInfo fi(frame);
  if (this->state->ip_stack_debug) {
    string fi_header = fi.header_str();
    log(INFO, "[IPStackSimulator] Frame header: %s", fi_header.c_str());
  }

  if (fi.arp) {
    this->on_client_arp_frame(c, fi);

  } else if (fi.ipv4) {
    uint16_t expected_ipv4_checksum = fi.computed_ipv4_header_checksum();
    if (fi.ipv4->checksum != expected_ipv4_checksum) {
      throw runtime_error(string_printf(
          "IPv4 header checksum is incorrect (%04hX expected, %04hX received)",
          expected_ipv4_checksum, fi.ipv4->checksum.load()));
    }
    if (memcmp(fi.ether->src_mac, c->mac_addr, 6)) {
      throw runtime_error("client sent IPv4 packet from different MAC address");
    }
    if (fi.ipv4->src_addr != c->ipv4_addr) {
      throw runtime_error("client sent IPv4 packet from different IPv4 address");
    }

    if (fi.udp) {
      uint16_t expected_udp_checksum = fi.computed_udp4_checksum();
      if (fi.udp->checksum != expected_udp_checksum) {
        throw runtime_error(string_printf(
            "UDP checksum is incorrect (%04hX expected, %04hX received)",
            expected_udp_checksum, fi.udp->checksum.load()));
      }
      this->on_client_udp_frame(c, fi);

    } else if (fi.tcp) {
      uint16_t expected_tcp_checksum = fi.computed_tcp4_checksum();
      if (fi.tcp->checksum != expected_tcp_checksum) {
        throw runtime_error(string_printf(
            "TCP checksum is incorrect (%04hX expected, %04hX received)",
            expected_tcp_checksum, fi.tcp->checksum.load()));
      }
      this->on_client_tcp_frame(c, fi);

    } else {
      throw runtime_error("frame uses unsupported IPv4 protocol");
    }

  } else {
    throw runtime_error("frame is not IPv4");
  }
}



void IPStackSimulator::on_client_arp_frame(
    shared_ptr<IPClient> c, const FrameInfo& fi) {
  if (fi.arp->hwaddr_len != 6 ||
      fi.arp->paddr_len != 4 ||
      fi.arp->hardware_type != 0x0001 ||
      fi.arp->protocol_type != 0x0800) {
    throw runtime_error("unsupported ARP parameters");
  }
  if (fi.payload_size < 20) {
    throw runtime_error("ARP payload too small");
  }

  // Populate the client's addresses if needed
  if (!memcmp(c->mac_addr, "\0\0\0\0\0\0", 6)) {
    memcpy(c->mac_addr, fi.ether->src_mac, 6);
  }
  if (c->ipv4_addr == 0) {
    c->ipv4_addr = *reinterpret_cast<const be_uint32_t*>(
      reinterpret_cast<const uint8_t*>(fi.payload) + 6);
  }

  EthernetHeader r_ether;
  memcpy(r_ether.dest_mac, fi.ether->src_mac, 6);
  memcpy(r_ether.src_mac, this->host_mac_address_bytes, 6);
  r_ether.protocol = fi.ether->protocol;

  ARPHeader r_arp;
  r_arp.hardware_type = fi.arp->hardware_type;
  r_arp.protocol_type = fi.arp->protocol_type;
  r_arp.hwaddr_len = 6;
  r_arp.paddr_len = 4;
  r_arp.operation = 0x0002;

  // The incoming payload is:
  // uint8_t src_mac[6]; // MAC address of client
  // uint8_t src_ip[4]; // IP address of client
  // uint8_t dest_mac[6]; // MAC address of host (all zeroes)
  // uint8_t dest_ip[4]; // IP address of host
  // The outgoing payload is:
  // uint8_t dest_mac[6]; // MAC address of host (from configuration)
  // uint8_t dest_ip[4]; // IP address of host
  // uint8_t src_mac[6]; // MAC address of client
  // uint8_t src_ip[4]; // IP address of client

  const char* payload_bytes = reinterpret_cast<const char*>(fi.payload);

  uint8_t r_payload[20];
  memcpy(&r_payload[0], this->host_mac_address_bytes, 6);
  memcpy(&r_payload[6], payload_bytes + 16, 4);
  memcpy(&r_payload[10], payload_bytes, 10);

  struct evbuffer* out_buf = bufferevent_get_output(c->bev.get());

  uint16_t frame_size = sizeof(r_ether) + sizeof(r_arp) + sizeof(r_payload);
  evbuffer_add(out_buf, &frame_size, 2);
  evbuffer_add(out_buf, &r_ether, sizeof(r_ether));
  evbuffer_add(out_buf, &r_arp, sizeof(r_arp));
  evbuffer_add(out_buf, r_payload, sizeof(r_payload));

  if (this->state->ip_stack_debug) {
    log(INFO, "[IPStackSimulator] Sending ARP response");
  }

  if (this->pcap_text_log_file) {
    StringWriter w;
    w.write(&r_ether, sizeof(r_ether));
    w.write(&r_arp, sizeof(r_arp));
    w.write(r_payload, sizeof(r_payload));
    this->log_frame(w.str());
  }
}



void IPStackSimulator::on_client_udp_frame(
    shared_ptr<IPClient> c, const FrameInfo& fi) {
  // We only implement the DNS server here
  if (fi.udp->dest_port != 53) {
    throw runtime_error("UDP packet is not DNS");
  }
  if (fi.payload_size < 0x0C) {
    throw runtime_error("DNS payload too small");
  }

  EthernetHeader r_ether;
  memcpy(r_ether.dest_mac, fi.ether->src_mac, 6);
  memcpy(r_ether.src_mac, this->host_mac_address_bytes, 6);
  r_ether.protocol = fi.ether->protocol;

  IPv4Header r_ipv4;
  r_ipv4.version_ihl = 0x45;
  r_ipv4.tos = 0;
  // r_ipv4.size filled in later
  r_ipv4.id = 0;
  r_ipv4.frag_offset = 0;
  r_ipv4.ttl = 20; // TODO: Does this value actually matter? Looks like it just has to be nonzero
  r_ipv4.protocol = 17; // UDP
  // r_ipv4.checksum filled in later
  r_ipv4.src_addr = fi.ipv4->dest_addr;
  r_ipv4.dest_addr = fi.ipv4->src_addr;

  UDPHeader r_udp;
  r_udp.src_port = fi.udp->dest_port;
  r_udp.dest_port = fi.udp->src_port;
  // r_udp.size filled in later
  // r_udp.checksum filled in later

  uint32_t resolved_address = this->proxy_destination_address
      ? this->proxy_destination_address
      : this->connect_address_for_remote_address(c->ipv4_addr);

  string r_data = DNSServer::response_for_query(
      fi.payload, fi.payload_size, resolved_address);

  r_ipv4.size = sizeof(IPv4Header) + sizeof(UDPHeader) + r_data.size();
  r_udp.size = sizeof(UDPHeader) + r_data.size();
  r_ipv4.checksum = FrameInfo::computed_ipv4_header_checksum(r_ipv4);
  r_udp.checksum = FrameInfo::computed_udp4_checksum(
      r_ipv4, r_udp, r_data.data(), r_data.size());

  struct evbuffer* out_buf = bufferevent_get_output(c->bev.get());

  if (this->state->ip_stack_debug) {
    string remote_str = this->str_for_ipv4_netloc(fi.ipv4->src_addr, fi.udp->src_port);
    log(INFO, "[IPStackSimulator] Sending DNS response to %s", remote_str.c_str());
  }

  uint16_t frame_size = sizeof(r_ether) + sizeof(r_ipv4) + sizeof(r_udp) + r_data.size();
  evbuffer_add(out_buf, &frame_size, 2);
  evbuffer_add(out_buf, &r_ether, sizeof(r_ether));
  evbuffer_add(out_buf, &r_ipv4, sizeof(r_ipv4));
  evbuffer_add(out_buf, &r_udp, sizeof(r_udp));
  evbuffer_add(out_buf, r_data.data(), r_data.size());

  if (this->pcap_text_log_file) {
    StringWriter w;
    w.write(&r_ether, sizeof(r_ether));
    w.write(&r_ipv4, sizeof(r_ipv4));
    w.write(&r_udp, sizeof(r_udp));
    w.write(r_data.data(), r_data.size());
    this->log_frame(w.str());
  }
}



uint64_t IPStackSimulator::tcp_conn_key_for_connection(
    const IPClient::TCPConnection& conn) {
  return (static_cast<uint64_t>(conn.server_addr) << 32) |
         (static_cast<uint64_t>(conn.server_port) << 16) |
         static_cast<uint64_t>(conn.client_port);
}

uint64_t IPStackSimulator::tcp_conn_key_for_client_frame(
    const IPv4Header& ipv4, const TCPHeader& tcp) {
  return (static_cast<uint64_t>(ipv4.dest_addr) << 32) |
         (static_cast<uint64_t>(tcp.dest_port) << 16) |
         static_cast<uint64_t>(tcp.src_port);
}

uint64_t IPStackSimulator::tcp_conn_key_for_client_frame(const FrameInfo& fi) {
  if (!fi.ipv4 || !fi.tcp) {
    throw logic_error("tcp_conn_key_for_frame called on non-TCP frame");
  }
  return IPStackSimulator::tcp_conn_key_for_client_frame(*fi.ipv4, *fi.tcp);
}


void IPStackSimulator::on_client_tcp_frame(
    shared_ptr<IPClient> c, const FrameInfo& fi) {
  if (this->state->ip_stack_debug) {
    log(INFO, "[IPStackSimulator] Client sent TCP frame (seq=%08" PRIX32 ", ack=%08" PRIX32 ")",
        fi.tcp->seq_num.load(), fi.tcp->ack_num.load());
  }

  if (fi.tcp->flags & (TCPHeader::Flag::NS | TCPHeader::Flag::CWR |
                       TCPHeader::Flag::ECE | TCPHeader::Flag::URG)) {
    throw runtime_error("unsupported flag in TCP packet");
  }

  if (fi.tcp->flags & TCPHeader::Flag::SYN) {
    // We never make connections back to the client, so we should never receive
    // a SYN+ACK. Essentially, no other flags should be set in any received SYN.
    if ((fi.tcp->flags & 0x0FFF) != TCPHeader::Flag::SYN) {
      throw runtime_error("TCP SYN contains extra flags");
    }

    StringReader options_r(fi.tcp + 1, fi.tcp_options_size);
    size_t max_frame_size = 1400;
    while (!options_r.eof()) {
      uint8_t option = options_r.get_u8();
      uint8_t option_size = (option < 2) ? 1 : options_r.get_u8();
      switch (option) {
        case 0: // End of options list
          options_r.go(options_r.size());
          break;
        case 1: // No option (padding)
          break;
        case 2: // Max segment size
          if (option_size != 4) {
            throw runtime_error("incorrect size for TCP max frame size option");
          }
          max_frame_size = options_r.get_u16b();
          break;
        case 3: // Window scale (ignored)
          if (option_size != 3) {
            throw runtime_error("incorrect size for TCP window scale option");
          }
          options_r.skip(option_size);
          break;
        case 4: // Selective ACK supported (ignored)
          if (option_size != 2) {
            throw runtime_error("incorrect size for TCP selective ACK supported option");
          }
          break;
        case 5: // Selective ACK (ignored)
          options_r.skip(option_size - 2);
          break;
        case 8: // Timestamps (ignored)
          if (option_size != 10) {
            throw runtime_error("incorrect size for TCP timestamps option");
          }
          options_r.skip(8);
          break;
        default:
          throw runtime_error("invalid TCP option");
      }
    }


    uint64_t key = this->tcp_conn_key_for_client_frame(fi);
    auto emplace_ret = c->tcp_connections.emplace(key, IPClient::TCPConnection());
    auto& conn = emplace_ret.first->second;
    string conn_str = this->state->ip_stack_debug ? this->str_for_tcp_connection(c, conn) : "";

    if (emplace_ret.second) {
      // Connection is new; initialize it
      conn.client = c;
      conn.resend_push_event.reset(event_new(this->base.get(), -1, EV_TIMEOUT,
          &IPStackSimulator::dispatch_on_resend_push, &conn));
      conn.server_addr = fi.ipv4->dest_addr;
      conn.server_port = fi.tcp->dest_port;
      conn.client_port = fi.tcp->src_port;
      conn.next_client_seq = fi.tcp->seq_num + 1;
      conn.acked_server_seq = random_object<uint32_t>();
      conn.resend_push_usecs = DEFAULT_RESEND_PUSH_USECS;
      conn.awaiting_first_ack = true;
      conn.max_frame_size = max_frame_size;
      if (this->state->ip_stack_debug) {
        log(INFO, "[IPStackSimulator] Client opened TCP connection %s (acked_server_seq=%08" PRIX32 ", next_client_seq=%08" PRIX32 ")",
            conn_str.c_str(), conn.acked_server_seq, conn.next_client_seq);
      }

    } else {
      // Connection is NOT new; this is probably a resend of an earlier SYN
      if (!conn.awaiting_first_ack) {
        throw logic_error("SYN received on already-open connection after initial phase");
      }
      // TODO: We should check the syn/ack numbers here instead of just assuming
      // they're correct
      if (this->state->ip_stack_debug) {
        log(INFO, "[IPStackSimulator] Client resent SYN for TCP connection %s",
            conn_str.c_str());
      }
    }

    // Send a SYN+ACK (send_tcp_frame always adds the ACK flag)
    this->send_tcp_frame(c, conn, TCPHeader::Flag::SYN);
    if (this->state->ip_stack_debug) {
      log(INFO, "[IPStackSimulator] Sent SYN+ACK on %s (acked_server_seq=%08" PRIX32 ", next_client_seq=%08" PRIX32 ")",
          conn_str.c_str(), conn.acked_server_seq, conn.next_client_seq);
    }

  } else {
    // This frame isn't a SYN, so a connection object should already exist
    uint64_t key = this->tcp_conn_key_for_client_frame(fi);
    IPClient::TCPConnection* conn;
    try {
      conn = &c->tcp_connections.at(key);
    } catch (const out_of_range&) {
      throw runtime_error("non-SYN frame does not correspond to any open TCP connection");
    }
    bool conn_valid = true;

    if (fi.tcp->flags & TCPHeader::Flag::ACK) {
      if (this->state->ip_stack_debug) {
        log(INFO, "[IPStackSimulator] Client sent ACK %08" PRIX32, fi.tcp->ack_num.load());
      }
      if (conn->awaiting_first_ack) {
        if (fi.tcp->ack_num != conn->acked_server_seq + 1) {
          throw runtime_error("first ack_num was not acked_server_seq + 1");
        }
        conn->acked_server_seq++;
        conn->awaiting_first_ack = false;

      } else {
        if (seq_num_greater(fi.tcp->ack_num, conn->acked_server_seq)) {
          if (this->state->ip_stack_debug) {
            log(INFO, "[IPStackSimulator] Advancing acked_server_seq from %08" PRIX32, conn->acked_server_seq);
          }
          uint32_t ack_delta = fi.tcp->ack_num - conn->acked_server_seq;
          size_t pending_bytes = evbuffer_get_length(conn->pending_data.get());
          if (pending_bytes < ack_delta) {
            throw runtime_error("client acknowledged beyond end of sent data");
          }

          evbuffer_drain(conn->pending_data.get(), ack_delta);
          conn->acked_server_seq += ack_delta;
          conn->resend_push_usecs = DEFAULT_RESEND_PUSH_USECS;

          if (this->state->ip_stack_debug) {
            log(INFO, "[IPStackSimulator] Removed %08" PRIX32 " bytes from pending buffer and advanced acked_server_seq to %08" PRIX32,
                ack_delta, conn->acked_server_seq);
          }

        } else if (seq_num_less(fi.tcp->ack_num, conn->acked_server_seq)) {
          throw runtime_error("client sent lower ack num than previous frame");
        }
      }

      if (!conn->server_bev.get()) {
        this->open_server_connection(c, *conn);
      }
    }

    if (fi.tcp->flags & (TCPHeader::Flag::RST | TCPHeader::Flag::FIN)) {
      bool is_rst = (fi.tcp->flags & TCPHeader::Flag::RST);
      if (is_rst && (fi.tcp->flags & TCPHeader::Flag::FIN)) {
        throw runtime_error("client sent TCP FIN+RST");
      }

      if (this->state->ip_stack_debug) {
        string conn_str = this->str_for_tcp_connection(c, *conn);
        log(INFO, "[IPStackSimulator] Client closed TCP connection %s", conn_str.c_str());
      }

      // TODO: Are we supposed to send a response to an RST? Here we do, and the
      // client probably just ignores it anyway
      this->send_tcp_frame(c, *conn, fi.tcp->flags & (TCPHeader::Flag::RST | TCPHeader::Flag::FIN));

      // Delete the connection object. The unique_ptr destructor flushes the
      // bufferevent, and thereby sends an EOF to the server's end.
      c->tcp_connections.erase(key);
      conn_valid = false;

    // Note: The PSH flag isn't required to be set on all packets that contain
    // data. The PSH flag just means "tell the application that data is
    // available", so some senders only set the PSH flag on the last frame of a
    // large segment of data, since the application wouldn't be able to process
    // the segment until all of it is available. newserv can handle incomplete
    // commands, so we just ignore the PSH flag and forward any data to the
    // server immediately.
    } else if (fi.payload_size != 0) {

      string conn_str = this->state->ip_stack_debug ? this->str_for_tcp_connection(c, *conn) : "";

      size_t payload_skip_bytes;
      if (fi.tcp->seq_num == conn->next_client_seq) {
        payload_skip_bytes = 0;

      } else if (seq_num_less(fi.tcp->seq_num, conn->next_client_seq)) {
        // If the frame overlaps an existing boundary, we'll accept some of the
        // data; otherwise we'll ignore it entirely (but still send an ACK)
        uint32_t end_seq = fi.tcp->seq_num + fi.payload_size;
        if (seq_num_less_or_equal(end_seq, conn->next_client_seq)) { // Fully "in the past"
          payload_skip_bytes = fi.payload_size;
        } else { // Partially "in the past"
          payload_skip_bytes = fi.payload_size - (end_seq - conn->next_client_seq);
        }

      } else {
        // Payload is in the future - we must have missed a data frame. We'll
        // ignore it (but warn) and send an ACK later, and the client should
        // retransmit the lost data
        if (this->state->ip_stack_debug) {
          log(WARNING,
              "[IPStackSimulator] Client sent out-of-order sequence number (expected %08" PRIX32 ", received %08" PRIX32 ", 0x%zX data bytes)",
              conn->next_client_seq, fi.tcp->seq_num.load(), fi.payload_size);
        }
        payload_skip_bytes = fi.payload_size;
      }

      if (payload_skip_bytes > fi.payload_size) {
        throw logic_error("payload skip bytes too large");
      }

      if (payload_skip_bytes < fi.payload_size) {
        const void* payload = reinterpret_cast<const uint8_t*>(fi.payload) + payload_skip_bytes;
        size_t payload_size = fi.payload_size - payload_skip_bytes;

        if (this->state->ip_stack_debug) {
          if (payload_skip_bytes) {
            log(INFO, "[IPStackSimulator] Client sent data on TCP connection %s, overlapping existing ack'ed data (0x%zX bytes ignored)",
                conn_str.c_str(), payload_skip_bytes);
          } else {
            log(INFO, "[IPStackSimulator] Client sent data on TCP connection %s",
                conn_str.c_str());
          }
          print_data(stderr, payload, payload_size);
        }

        // Send the new data to the server
        struct evbuffer* server_out_buf = bufferevent_get_output(
            conn->server_bev.get());
        evbuffer_add(server_out_buf, payload, payload_size);

        // Update the sequence number and stats
        conn->next_client_seq += payload_size;
        conn->bytes_received += payload_size;
      }

      // Send an ACK
      this->send_tcp_frame(c, *conn);
      if (this->state->ip_stack_debug) {
        log(INFO, "[IPStackSimulator] Sent PSH ACK on %s (acked_server_seq=%08" PRIX32 ", next_client_seq=%08" PRIX32 ", bytes_received=0x%zX)",
            conn_str.c_str(), conn->acked_server_seq, conn->next_client_seq, conn->bytes_received);
      }
    }

    if (conn_valid) {
      // Try to send some more data if the client is waiting on it
      this->send_pending_push_frame(c, *conn);
    }
  }
}

void IPStackSimulator::open_server_connection(
    shared_ptr<IPClient> c, IPClient::TCPConnection& conn) {
  if (conn.server_bev.get()) {
    throw logic_error("server connection is already open");
  }

  struct bufferevent* bevs[2];
  bufferevent_pair_new(this->base.get(), 0, bevs);

  // Set up the IPStackSimulator end of the virtual connection
  bufferevent_setcb(bevs[0], &IPStackSimulator::dispatch_on_server_input,
      nullptr, &IPStackSimulator::dispatch_on_server_error, &conn);
  bufferevent_enable(bevs[0], EV_READ | EV_WRITE);
  conn.server_bev.reset(bevs[0]);

  // Link the client to the server - the server sees this as a normal TCP
  // connection and treats it as if the client connected to one of its listening
  // sockets
  string conn_str = this->str_for_tcp_connection(c, conn);
  if (this->game_server.get()) {
    const PortConfiguration* port_config;
    try {
      port_config = &this->state->numbered_port_configuration.at(conn.server_port);
    } catch (const out_of_range&) {
      bufferevent_free(bevs[1]);
      throw logic_error("client connected to port missing from configuration");
    }

    this->game_server->connect_client(bevs[1], c->ipv4_addr, conn.client_port,
        port_config->version, port_config->behavior);
    log(INFO, "[IPStackSimulator] Connected TCP connection %s to game server",
        conn_str.c_str());

  } else if (this->proxy_server.get()) {
    this->proxy_server->connect_client(bevs[1], conn.server_addr, conn.server_port);
    log(INFO, "[IPStackSimulator] Connected TCP connection %s to proxy server",
        conn_str.c_str());
  }
}

void IPStackSimulator::send_pending_push_frame(
    shared_ptr<IPClient> c, IPClient::TCPConnection& conn) {
  size_t pending_bytes = evbuffer_get_length(conn.pending_data.get());
  if (!pending_bytes) {
    return;
  }

  size_t bytes_to_send = min<size_t>(pending_bytes, conn.max_frame_size);

  if (this->state->ip_stack_debug) {
    log(INFO, "[IPStackSimulator] Sending PSH frame with seq_num %08" PRIX32 ", 0x%zX/0x%zX data bytes",
        conn.acked_server_seq, bytes_to_send, pending_bytes);
  }

  this->send_tcp_frame(c, conn, TCPHeader::Flag::PSH, conn.pending_data.get(),
      bytes_to_send);
  struct timeval resend_push_timeout = usecs_to_timeval(conn.resend_push_usecs);
  event_add(conn.resend_push_event.get(), &resend_push_timeout);

  // If the client isn't responding to our PSHes, back off exponentially up to
  // a limit of 5 seconds between PSH frames. This window is reset when
  // acked_server_seq changes (that is, when the client has acknowledged any new
  // data)
  conn.resend_push_usecs *= 2;
  if (conn.resend_push_usecs > 5000000) {
    conn.resend_push_usecs = 5000000;
  }
}

void IPStackSimulator::send_tcp_frame(
    shared_ptr<IPClient> c,
    IPClient::TCPConnection& conn,
    uint16_t flags,
    struct evbuffer* src_buf,
    size_t src_bytes) {
  if (!src_bytes != !(flags & TCPHeader::Flag::PSH)) {
    throw logic_error("data should be given if and only if PSH is given");
  }

  EthernetHeader ether;
  memcpy(ether.dest_mac, c->mac_addr, 6);
  memcpy(ether.src_mac, this->host_mac_address_bytes, 6);
  ether.protocol = 0x0800; // IPv4

  IPv4Header ipv4;
  ipv4.version_ihl = 0x45;
  ipv4.tos = 0;
  // ipv4.size filled in later
  ipv4.id = 0;
  ipv4.frag_offset = 0;
  ipv4.ttl = 20;
  ipv4.protocol = 6; // TCP
  // ipv4.checksum filled in later
  ipv4.src_addr = conn.server_addr;
  ipv4.dest_addr = c->ipv4_addr;

  TCPHeader tcp;
  tcp.src_port = conn.server_port;
  tcp.dest_port = conn.client_port;
  tcp.seq_num = conn.acked_server_seq;
  tcp.ack_num = conn.next_client_seq;
  tcp.flags = (5 << 12) | TCPHeader::Flag::ACK | flags;
  tcp.window = 0x1000;
  tcp.urgent_ptr = 0;
  // tcp.checksum filled in later

  ipv4.size = sizeof(IPv4Header) + sizeof(TCPHeader) + src_bytes;
  ipv4.checksum = FrameInfo::computed_ipv4_header_checksum(ipv4);

  const void* linear_data = src_bytes ? evbuffer_pullup(src_buf, src_bytes) : nullptr;
  tcp.checksum = FrameInfo::computed_tcp4_checksum(
      ipv4, tcp, linear_data, src_bytes);

  struct evbuffer* out_buf = bufferevent_get_output(c->bev.get());

  uint16_t frame_size = sizeof(ether) + sizeof(ipv4) + sizeof(tcp) + src_bytes;
  evbuffer_add(out_buf, &frame_size, 2);
  evbuffer_add(out_buf, &ether, sizeof(ether));
  evbuffer_add(out_buf, &ipv4, sizeof(ipv4));
  evbuffer_add(out_buf, &tcp, sizeof(tcp));
  if (src_bytes) {
    evbuffer_add(out_buf, linear_data, src_bytes);
  }

  if (this->pcap_text_log_file) {
    StringWriter w;
    w.write(&ether, sizeof(ether));
    w.write(&ipv4, sizeof(ipv4));
    w.write(&tcp, sizeof(tcp));
    w.write(linear_data, src_bytes);
    this->log_frame(w.str());
  }
}

void IPStackSimulator::dispatch_on_resend_push(evutil_socket_t, short, void* ctx) {
  auto* conn = reinterpret_cast<IPClient::TCPConnection*>(ctx);
  auto c = conn->client.lock();
  if (!c.get()) {
    log(WARNING, "[IPStackSimulator] Resend push event triggered for deleted client; ignoring");
  } else {
    c->sim->on_resend_push(c, *conn);
  }
}

void IPStackSimulator::on_resend_push(shared_ptr<IPClient> c, IPClient::TCPConnection& conn) {
  this->send_pending_push_frame(c, conn);
}

void IPStackSimulator::dispatch_on_server_input(struct bufferevent*, void* ctx) {
  auto* conn = reinterpret_cast<IPClient::TCPConnection*>(ctx);
  auto c = conn->client.lock();
  if (!c.get()) {
    log(WARNING, "[IPStackSimulator] Server input event triggered for deleted client; ignoring");
  } else {
    c->sim->on_server_input(c, *conn);
  }
}

void IPStackSimulator::on_server_input(shared_ptr<IPClient> c, IPClient::TCPConnection& conn) {
  struct evbuffer* buf = bufferevent_get_input(conn.server_bev.get());
  if (this->state->ip_stack_debug) {
    log(INFO, "[IPStackSimulator] Server input event: 0x%zX bytes to read",
        evbuffer_get_length(buf));
  }

  evbuffer_add_buffer(conn.pending_data.get(), buf);
  this->send_pending_push_frame(c, conn);
}

void IPStackSimulator::dispatch_on_server_error(
    struct bufferevent*, short events, void* ctx) {
  auto* conn = reinterpret_cast<IPClient::TCPConnection*>(ctx);
  auto c = conn->client.lock();
  if (!c.get()) {
    log(WARNING, "[IPStackSimulator] Server error event triggered for deleted client; ignoring");
  } else {
    c->sim->on_server_error(c, *conn, events);
  }
}

void IPStackSimulator::on_server_error(
    shared_ptr<IPClient> c, IPClient::TCPConnection& conn, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    log(WARNING, "[IPStackSimulator] Received error %d from virtual connection (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    // Send an RST to the client. Kind of rude (we really should use FIN) but
    // the PSO network stack always sends an RST to us when disconnecting, so
    // whatever
    this->send_tcp_frame(c, conn, TCPHeader::Flag::RST);

    // Delete the connection object (this also flushes and frees the server
    // virtual connection bufferevent)
    c->tcp_connections.erase(this->tcp_conn_key_for_connection(conn));
  }
}



void IPStackSimulator::log_frame(const string& data) const {
  if (this->pcap_text_log_file) {
    print_data(this->pcap_text_log_file, data, 0, nullptr,
        PrintDataFlags::SKIP_SEPARATOR);
    fputc('\n', this->pcap_text_log_file);
    fflush(this->pcap_text_log_file);
  }
}
