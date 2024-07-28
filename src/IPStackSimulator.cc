#include "IPStackSimulator.hh"

#include <arpa/inet.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>

#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Time.hh>
#include <string>

#include "DNSServer.hh"
#include "IPFrameInfo.hh"
#include "Loggers.hh"

using namespace std;

static const size_t DEFAULT_RESEND_PUSH_USECS = 200000; // 200ms

static string unescape_hdlc_frame(const void* data, size_t size) {
  phosg::StringReader r(data, size);
  if (r.get_u8(data) != 0x7E) {
    throw runtime_error("HDLC frame does not begin with 7E");
  }
  string ret("\x7E", 1);

  while (r.get_u8(false) != 0x7E) {
    uint8_t ch = r.get_u8();
    if (ch == 0x7D) {
      ch = r.get_u8();
      if (ch == 0x7E) {
        throw runtime_error("abort sequence received");
      }
      ret.push_back(ch ^ 0x20);
    } else {
      ret.push_back(ch);
    }
  }
  ret.push_back(0x7E);
  return ret;
}

static string unescape_hdlc_frame(const string& data) {
  return unescape_hdlc_frame(data.data(), data.size());
}

static string escape_hdlc_frame(const void* data, size_t size, uint32_t escape_control_character_flags = 0xFFFFFFFF) {
  if (size < 2) {
    throw runtime_error("HDLC frame too small for start and end sentinels");
  }

  phosg::StringReader r(data, size);
  if (r.pget_u8(size - 1) != 0x7E) {
    throw runtime_error("HDLC frame does not end with 7E");
  }
  r.truncate(size - 1);
  if (r.get_u8() != 0x7E) {
    throw runtime_error("HDLC frame does not begin with 7E");
  }
  string ret("\x7E", 1);

  while (!r.eof()) {
    uint8_t ch = r.get_u8();
    if ((ch == 0x7D) || (ch == 0x7E) || ((ch < 0x20) && ((escape_control_character_flags >> ch) & 1))) {
      ret.push_back(0x7D);
      ret.push_back(ch ^ 0x20);
    } else {
      ret.push_back(ch);
    }
  }
  ret.push_back(0x7E);
  return ret;
}

static string escape_hdlc_frame(const string& data, uint32_t escape_control_character_flags = 0xFFFFFFFF) {
  return escape_hdlc_frame(data.data(), data.size(), escape_control_character_flags);
}

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
    return phosg::string_printf("<UNKNOWN>:%hu", port);
  } else {
    return phosg::string_printf("%s:%hu", addr_str, port);
  }
}

string IPStackSimulator::str_for_tcp_connection(shared_ptr<const IPClient> c, const IPClient::TCPConnection& conn) {
  uint64_t key = IPStackSimulator::tcp_conn_key_for_connection(conn);
  string server_netloc_str = str_for_ipv4_netloc(conn.server_addr, conn.server_port);
  string client_netloc_str = str_for_ipv4_netloc(c->ipv4_addr, conn.client_port);
  int fd = bufferevent_getfd(c->bev.get());
  return phosg::string_printf("%d+%016" PRIX64 " (%s -> %s)",
      fd, key, client_netloc_str.c_str(), server_netloc_str.c_str());
}

IPStackSimulator::IPStackSimulator(
    shared_ptr<struct event_base> base,
    shared_ptr<ServerState> state)
    : base(base),
      state(state),
      next_network_id(1),
      pcap_text_log_file(state->ip_stack_debug ? fopen("IPStackSimulator-Log.txt", "wt") : nullptr) {
  this->host_mac_address_bytes.clear(0x90);
  this->broadcast_mac_address_bytes.clear(0xFF);
}

IPStackSimulator::~IPStackSimulator() {
  if (this->pcap_text_log_file) {
    fclose(this->pcap_text_log_file);
  }
}

void IPStackSimulator::listen(const string& name, const string& socket_path, Protocol proto) {
  int fd = phosg::listen(socket_path, 0, SOMAXCONN);
  ip_stack_simulator_log.info("Listening on Unix socket %s on fd %d as %s", socket_path.c_str(), fd, name.c_str());
  this->add_socket(name, fd, proto);
}

void IPStackSimulator::listen(const string& name, const string& addr, int port, Protocol proto) {
  if (port == 0) {
    this->listen(name, addr, proto);
  } else {
    int fd = phosg::listen(addr, port, SOMAXCONN);
    string netloc_str = phosg::render_netloc(addr, port);
    ip_stack_simulator_log.info("Listening on TCP interface %s on fd %d as %s", netloc_str.c_str(), fd, name.c_str());
    this->add_socket(name, fd, proto);
  }
}

void IPStackSimulator::listen(const string& name, int port, Protocol proto) {
  this->listen(name, "", port, proto);
}

void IPStackSimulator::add_socket(const string& name, int fd, Protocol proto) {
  unique_listener l(
      evconnlistener_new(
          this->base.get(),
          IPStackSimulator::dispatch_on_listen_accept,
          this,
          LEV_OPT_REUSEABLE,
          0,
          fd),
      evconnlistener_free);
  this->listening_sockets.emplace(piecewise_construct, forward_as_tuple(fd), forward_as_tuple(name, proto, std::move(l)));
}

shared_ptr<IPStackSimulator::IPClient> IPStackSimulator::get_network(uint64_t network_id) const {
  return this->network_id_to_client.at(network_id);
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

IPStackSimulator::IPClient::IPClient(
    shared_ptr<IPStackSimulator> sim, uint64_t network_id, Protocol protocol, struct bufferevent* bev)
    : sim(sim),
      network_id(network_id),
      bev(bev, bufferevent_free),
      protocol(protocol),
      mac_addr(0),
      ipv4_addr(0),
      idle_timeout_event(event_new(sim->base.get(), -1, EV_TIMEOUT, &IPStackSimulator::IPClient::dispatch_on_idle_timeout, this), event_free) {
  uint64_t idle_timeout_usecs = sim->state->client_idle_timeout_usecs;
  struct timeval tv = phosg::usecs_to_timeval(idle_timeout_usecs);
  event_add(this->idle_timeout_event.get(), &tv);
}

void IPStackSimulator::IPClient::dispatch_on_idle_timeout(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<IPStackSimulator::IPClient*>(ctx)->on_idle_timeout();
}

void IPStackSimulator::IPClient::on_idle_timeout() {
  auto sim = this->sim.lock();
  if (sim) {
    ip_stack_simulator_log.info("Idle timeout expired on virtual network %d", bufferevent_getfd(this->bev.get()));
    sim->disconnect_client(this->network_id);
  } else {
    ip_stack_simulator_log.info("Idle timeout expired on virtual network %d, but simulator is missing", bufferevent_getfd(this->bev.get()));
  }
}

static void flush_and_free_bufferevent(struct bufferevent* bev) {
  bufferevent_flush(bev, EV_READ | EV_WRITE, BEV_FINISHED);
  bufferevent_free(bev);
}

IPStackSimulator::IPClient::TCPConnection::TCPConnection()
    : server_bev(nullptr, flush_and_free_bufferevent),
      pending_data(evbuffer_new(), evbuffer_free),
      resend_push_event(nullptr, event_free),
      awaiting_first_ack(true),
      server_addr(0),
      server_port(0),
      client_port(0),
      next_client_seq(0),
      acked_server_seq(0),
      resend_push_usecs(DEFAULT_RESEND_PUSH_USECS),
      next_push_max_frame_size(1024),
      max_frame_size(1024),
      bytes_received(0),
      bytes_sent(0) {}

void IPStackSimulator::disconnect_client(uint64_t network_id) {
  ip_stack_simulator_log.info("Virtual network N-%" PRIu64 " disconnected", network_id);
  this->network_id_to_client.erase(network_id);
}

void IPStackSimulator::dispatch_on_listen_accept(
    struct evconnlistener* listener, evutil_socket_t fd,
    struct sockaddr* address, int socklen, void* ctx) {
  reinterpret_cast<IPStackSimulator*>(ctx)->on_listen_accept(listener, fd, address, socklen);
}

void IPStackSimulator::on_listen_accept(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr*, int) {
  struct sockaddr_storage remote_addr;
  phosg::get_socket_addresses(fd, nullptr, &remote_addr);
  if (this->state->banned_ipv4_ranges->check(remote_addr)) {
    close(fd);
    return;
  }

  int listen_fd = evconnlistener_get_fd(listener);

  const ListeningSocket* listening_socket;
  try {
    listening_socket = &this->listening_sockets.at(listen_fd);
  } catch (const out_of_range&) {
    ip_stack_simulator_log.info("Virtual network fd %d connected via unknown listener %d; disconnecting", fd, listen_fd);
    close(fd);
    return;
  }

  uint64_t network_id = this->next_network_id++;
  ip_stack_simulator_log.info("Virtual network N-%" PRIu64 " connected via %s", network_id, listening_socket->name.c_str());

  struct bufferevent* bev = bufferevent_socket_new(this->base.get(), fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  auto c = make_shared<IPClient>(this->shared_from_this(), network_id, listening_socket->protocol, bev);
  this->network_id_to_client.emplace(c->network_id, c);

  bufferevent_setcb(bev, &IPStackSimulator::IPClient::dispatch_on_client_input, nullptr,
      &IPStackSimulator::IPClient::dispatch_on_client_error, c.get());
  bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void IPStackSimulator::dispatch_on_listen_error(
    struct evconnlistener* listener, void* ctx) {
  reinterpret_cast<IPStackSimulator*>(ctx)->on_listen_error(listener);
}

void IPStackSimulator::on_listen_error(struct evconnlistener* listener) {
  int err = EVUTIL_SOCKET_ERROR();
  ip_stack_simulator_log.error("Failure on listening socket %d: %d (%s)",
      evconnlistener_get_fd(listener), err, evutil_socket_error_to_string(err));
  event_base_loopexit(this->base.get(), nullptr);
}

void IPStackSimulator::IPClient::dispatch_on_client_input(struct bufferevent* bev, void* ctx) {
  reinterpret_cast<IPClient*>(ctx)->on_client_input(bev);
}

void IPStackSimulator::IPClient::on_client_input(struct bufferevent* bev) {
  struct evbuffer* buf = bufferevent_get_input(bev);

  auto sim = this->sim.lock();
  if (!sim) {
    size_t bytes = evbuffer_get_length(buf);
    ip_stack_simulator_log.warning("Ignoring data from unregistered virtual network (0x%zX bytes)", bytes);
    evbuffer_drain(buf, bytes);
    return;
  }

  uint64_t idle_timeout_usecs = sim ? sim->state->client_idle_timeout_usecs : 60000000;
  struct timeval tv = phosg::usecs_to_timeval(idle_timeout_usecs);
  event_add(this->idle_timeout_event.get(), &tv);

  switch (this->protocol) {
    case Protocol::ETHERNET_TAPSERVER:
    case Protocol::HDLC_TAPSERVER:
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
          sim->on_client_frame(this->shared_from_this(), frame);
        } catch (const exception& e) {
          if (ip_stack_simulator_log.warning("Failed to process frame: %s", e.what())) {
            phosg::print_data(stderr, frame);
          }
        }
      }
      break;
    case Protocol::HDLC_RAW:
      while (evbuffer_get_length(buf) >= 2) {
        struct evbuffer_ptr res = evbuffer_search(buf, "\x7E", 1, nullptr);
        if (res.pos < 0) {
          break;
        }
        size_t start_offset = res.pos;

        if (evbuffer_ptr_set(buf, &res, 1, EVBUFFER_PTR_ADD)) {
          ip_stack_simulator_log.warning("Cannot advance search for end of frame");
          break;
        }

        struct evbuffer_ptr end_res = evbuffer_search(buf, "\x7E", 1, &res);
        if (end_res.pos < 0) {
          break;
        }
        size_t frame_size = end_res.pos + 1 - start_offset;

        if (start_offset) {
          evbuffer_drain(buf, start_offset);
        }

        string frame(frame_size, '\0');
        evbuffer_remove(buf, frame.data(), frame.size());

        try {
          sim->on_client_frame(this->shared_from_this(), frame);
        } catch (const exception& e) {
          if (ip_stack_simulator_log.warning("Failed to process frame: %s", e.what())) {
            phosg::print_data(stderr, frame);
          }
        }
      }
      break;
  }
}

void IPStackSimulator::IPClient::dispatch_on_client_error(struct bufferevent* bev, short events, void* ctx) {
  reinterpret_cast<IPClient*>(ctx)->on_client_error(bev, events);
}
void IPStackSimulator::IPClient::on_client_error(struct bufferevent*, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    ip_stack_simulator_log.warning("Virtual network caused error %d (%s)", err, evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    auto sim = this->sim.lock();
    if (sim) {
      sim->disconnect_client(this->network_id);
    }
  }
}

void IPStackSimulator::send_layer3_frame(shared_ptr<IPClient> c, FrameInfo::Protocol proto, const string& data) const {
  this->send_layer3_frame(c, proto, data.data(), data.size());
}

void IPStackSimulator::send_layer3_frame(shared_ptr<IPClient> c, FrameInfo::Protocol proto, const void* data, size_t size) const {
  struct evbuffer* out_buf = bufferevent_get_output(c->bev.get());

  switch (c->protocol) {
    case Protocol::ETHERNET_TAPSERVER: {
      EthernetHeader ether;
      ether.dest_mac = c->mac_addr;
      ether.src_mac = this->host_mac_address_bytes;
      switch (proto) {
        case FrameInfo::Protocol::NONE:
          throw logic_error("layer 3 protocol not specified");
        case FrameInfo::Protocol::LCP:
          throw logic_error("cannot send LCP frame over Ethernet");
        case FrameInfo::Protocol::IPV4:
          ether.protocol = 0x0800;
          break;
        case FrameInfo::Protocol::ARP:
          ether.protocol = 0x0806;
          break;
        default:
          throw logic_error("unknown layer 3 protocol");
      }

      le_uint16_t frame_size = size + sizeof(EthernetHeader);
      evbuffer_add(out_buf, &frame_size, 2);
      evbuffer_add(out_buf, &ether, sizeof(ether));
      evbuffer_add(out_buf, data, size);
      if (this->pcap_text_log_file) {
        phosg::StringWriter w;
        w.write(&ether, sizeof(ether));
        w.write(data, size);
        this->log_frame(w.str());
      }
      break;
    }

    case Protocol::HDLC_TAPSERVER:
    case Protocol::HDLC_RAW: {
      HDLCHeader hdlc;
      hdlc.start_sentinel1 = 0x7E;
      hdlc.address = 0xFF;
      hdlc.control = 0x03;
      switch (proto) {
        case FrameInfo::Protocol::NONE:
          throw logic_error("layer 3 protocol not specified");
        case FrameInfo::Protocol::LCP:
          hdlc.protocol = 0xC021;
          break;
        case FrameInfo::Protocol::PAP:
          hdlc.protocol = 0xC023;
          break;
        case FrameInfo::Protocol::IPCP:
          hdlc.protocol = 0x8021;
          break;
        case FrameInfo::Protocol::IPV4:
          hdlc.protocol = 0x0021;
          break;
        case FrameInfo::Protocol::ARP:
          throw runtime_error("cannot send ARP packets over HDLC");
        default:
          throw logic_error("unknown layer 3 protocol");
      }

      phosg::StringWriter w;
      w.put(hdlc);
      w.write(data, size);
      w.put_u16l(FrameInfo::computed_hdlc_checksum(w.str().data() + 1, w.size() - 1));
      w.put_u8(0x7E);

      string escaped = escape_hdlc_frame(w.str(), c->hdlc_escape_control_character_flags);
      if (ip_stack_simulator_log.debug("Sending HDLC frame to virtual network (escaped to %zX bytes)", escaped.size())) {
        phosg::print_data(stderr, w.str());
      }

      if (c->protocol == Protocol::HDLC_TAPSERVER) {
        le_uint16_t frame_size = escaped.size();
        evbuffer_add(out_buf, &frame_size, 2);
      }
      evbuffer_add(out_buf, escaped.data(), escaped.size());
      if (this->pcap_text_log_file) {
        this->log_frame(escaped);
      }
      break;
    }

    default:
      throw logic_error("unknown link type");
  }
}

void IPStackSimulator::on_client_frame(shared_ptr<IPClient> c, const string& frame) {
  FrameInfo::LinkType link_type = (c->protocol == Protocol::ETHERNET_TAPSERVER)
      ? FrameInfo::LinkType::ETHERNET
      : FrameInfo::LinkType::HDLC;

  const string* effective_data = &frame;
  string hdlc_unescaped_data;
  if (link_type == FrameInfo::LinkType::HDLC) {
    hdlc_unescaped_data = unescape_hdlc_frame(frame);
    effective_data = &hdlc_unescaped_data;
  }
  if (ip_stack_simulator_log.debug("Virtual network sent frame")) {
    phosg::print_data(stderr, *effective_data);
  }
  this->log_frame(*effective_data);

  FrameInfo fi(link_type, *effective_data);
  if (ip_stack_simulator_log.should_log(phosg::LogLevel::DEBUG)) {
    string fi_header = fi.header_str();
    ip_stack_simulator_log.debug("Frame header: %s", fi_header.c_str());
  }

  if (fi.ether) {
    if (c->mac_addr.is_filled_with(0)) {
      c->mac_addr = fi.ether->src_mac;
    } else if ((fi.ether->src_mac != c->mac_addr) && (fi.ether->src_mac != this->broadcast_mac_address_bytes)) {
      throw runtime_error("client sent IPv4 packet from different MAC address");
    }
  } else if (fi.hdlc) {
    uint16_t expected_checksum = fi.computed_hdlc_checksum();
    uint16_t stored_checksum = fi.stored_hdlc_checksum();
    if (expected_checksum != stored_checksum) {
      throw runtime_error(phosg::string_printf(
          "HDLC checksum is incorrect (%04hX expected, %04hX received)",
          expected_checksum, stored_checksum));
    }
  } else {
    throw runtime_error("frame is not Ethernet or HDLC");
  }

  if (fi.lcp) {
    this->on_client_lcp_frame(c, fi);

  } else if (fi.pap) {
    this->on_client_pap_frame(c, fi);

  } else if (fi.ipcp) {
    this->on_client_ipcp_frame(c, fi);

  } else if (fi.arp) {
    this->on_client_arp_frame(c, fi);

  } else if (fi.ipv4) {
    uint16_t expected_ipv4_checksum = fi.computed_ipv4_header_checksum();
    if (fi.ipv4->checksum != expected_ipv4_checksum) {
      throw runtime_error(phosg::string_printf(
          "IPv4 header checksum is incorrect (%04hX expected, %04hX received)",
          expected_ipv4_checksum, fi.ipv4->checksum.load()));
    }

    if ((fi.ipv4->src_addr != c->ipv4_addr) && (fi.ipv4->src_addr != 0)) {
      throw runtime_error("client sent IPv4 packet from different IPv4 address");
    }

    if (fi.udp) {
      uint16_t expected_udp_checksum = fi.computed_udp4_checksum();
      if (fi.udp->checksum != expected_udp_checksum) {
        throw runtime_error(phosg::string_printf(
            "UDP checksum is incorrect (%04hX expected, %04hX received)",
            expected_udp_checksum, fi.udp->checksum.load()));
      }
      this->on_client_udp_frame(c, fi);

    } else if (fi.tcp) {
      uint16_t expected_tcp_checksum = fi.computed_tcp4_checksum();
      if (fi.tcp->checksum != expected_tcp_checksum) {
        throw runtime_error(phosg::string_printf(
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

void IPStackSimulator::on_client_lcp_frame(shared_ptr<IPClient> c, const FrameInfo& fi) {
  switch (fi.lcp->command) {
    case 0x01: { // Configure-Request
      auto opts_r = fi.read_payload();
      while (!opts_r.eof()) {
        uint8_t opt = opts_r.get_u8();
        string opt_data = opts_r.read(opts_r.get_u8() - 2);
        phosg::StringReader opt_data_r(opt_data);
        switch (opt) {
          case 0x01: // Maximum receive unit
            // TODO: Currently we ignore this, but we probably should use it.
            opt_data_r.get_u16b();
            break;
          case 0x02: // Escaped control character flags
            c->hdlc_escape_control_character_flags = opt_data_r.get_u32b();
            break;
          case 0x05: // Magic-Number
            c->hdlc_remote_magic_number = opt_data_r.get_u32b();
            break;
          case 0x00: // RESERVED
          case 0x03: // Authentication protocol
          case 0x04: // Quality protocol
          case 0x07: // Protocol field compression
          case 0x08: // Address and control field compression
            throw runtime_error(phosg::string_printf("unimplemented LCP option %02hhX (%zu bytes)", opt, opt_data.size()));
          default:
            throw runtime_error("unknown LCP option");
        }
      }
      // Technically, we should implement the LCP state machine, but I'm too
      // lazy to do this right now. In our situation, it should suffice to
      // simply always send a Configure-Request to the client with a magic
      // number not equal to the one we received.
      phosg::StringWriter opts_w;
      opts_w.put_u8(0x01); // Maximum receive unit
      opts_w.put_u8(0x04);
      opts_w.put_u16b(1500);
      opts_w.put_u8(0x02); // Escaped control character flags (we don't require any)
      opts_w.put_u8(0x06);
      opts_w.put_u32(0);
      opts_w.put_u8(0x03); // Authentication protocol
      opts_w.put_u8(0x04);
      opts_w.put_u16b(0xC023); // Password authentication protocol
      opts_w.put_u8(0x05); // Magic number (bitwise inverse of the remote end's)
      opts_w.put_u8(0x06);
      opts_w.put_u32b(~c->hdlc_remote_magic_number);
      phosg::StringWriter request_w;
      request_w.put<LCPHeader>(LCPHeader{
          .command = 0x01, // Configure-Request
          .request_id = fi.lcp->request_id,
          .size = static_cast<uint16_t>(sizeof(LCPHeader) + opts_w.size()),
      });
      request_w.write(opts_w.str());
      this->send_layer3_frame(c, FrameInfo::Protocol::LCP, request_w.str());

      phosg::StringWriter ack_w;
      ack_w.put<LCPHeader>(LCPHeader{
          .command = 0x02, // Configure-Ack
          .request_id = fi.lcp->request_id,
          .size = fi.lcp->size,
      });
      ack_w.write(fi.payload, fi.payload_size);
      this->send_layer3_frame(c, FrameInfo::Protocol::LCP, ack_w.str());

      break;
    }

    case 0x05: { // Terminate-Request
      c->ipv4_addr = 0;
      c->tcp_connections.clear();
      string response(reinterpret_cast<const char*>(fi.payload), fi.payload_size);
      response.at(0) = 0x06; // Terminate-Ack
      this->send_layer3_frame(c, FrameInfo::Protocol::LCP, response);
      break;
    }

    case 0x09: { // Echo-Request
      string response(reinterpret_cast<const char*>(fi.payload), fi.payload_size);
      response.at(0) = 0x0A; // Echo-Reply
      this->send_layer3_frame(c, FrameInfo::Protocol::LCP, response);
      break;
    }

    case 0x0B: // Discard-Request
    case 0x02: // Configure-Ack
      break;

    case 0x03: // Configure-Nak
    case 0x04: // Configure-Reject
    case 0x06: // Terminate-Ack
    case 0x07: // Code-Reject
    case 0x08: // Protocol-Reject
    case 0x0A: // Echo-Reply
      throw runtime_error("unimplemented LCP command");
    default:
      throw runtime_error("unknown LCP command");
  }
}

void IPStackSimulator::on_client_pap_frame(shared_ptr<IPClient> c, const FrameInfo& fi) {
  if (fi.pap->command != 0x01) { // Authenticate-Request
    throw runtime_error("client sent incorrect PAP command");
  }

  auto r = fi.read_payload();
  string username = r.read(r.get_u8());
  string password = r.read(r.get_u8());
  ip_stack_simulator_log.info("Client logged in with username \"%s\" and password", username.c_str());

  static const string login_message = "newserv PPP simulator";
  phosg::StringWriter w;
  w.put<PAPHeader>(PAPHeader{
      .command = 0x02, // Authenticate-Ack
      .request_id = fi.pap->request_id,
      .size = login_message.size() + sizeof(PAPHeader) + 1,
  });
  w.put_u8(login_message.size());
  w.write(login_message);
  this->send_layer3_frame(c, FrameInfo::Protocol::PAP, w.str());
}

void IPStackSimulator::on_client_ipcp_frame(shared_ptr<IPClient> c, const FrameInfo& fi) {
  switch (fi.ipcp->command) {
    case 0x01: { // Configure-Request
      auto opts_r = fi.read_payload();

      uint32_t remote_ip = 0;
      uint32_t remote_primary_dns = 0;
      uint32_t remote_secondary_dns = 0;
      phosg::StringWriter rejected_opts_w;
      while (!opts_r.eof()) {
        uint8_t opt = opts_r.get_u8();
        string opt_data = opts_r.read(opts_r.get_u8() - 2);
        phosg::StringReader opt_data_r(opt_data);
        switch (opt) {
          case 0x01: // IP addresses (deprecated as of 1992; we don't support it at all)
            throw runtime_error("IPCP client sent IP-Addresses option");
          case 0x02: // IP compression protocol
            rejected_opts_w.put_u8(0x02);
            rejected_opts_w.put_u8(opt_data_r.size() + 2);
            rejected_opts_w.write(opt_data);
            break;
          case 0x03: // IP address
            remote_ip = opt_data_r.get_u32b();
            break;
          case 0x81: // Primary DNS server address
            remote_primary_dns = opt_data_r.get_u32b();
            break;
          case 0x83: // Secondary DNS server address
            remote_secondary_dns = opt_data_r.get_u32b();
            break;
          case 0x82: // Primary NBNS server address
          case 0x84: // Secondary NBNS server address
          case 0x04: // Mobile IP address
            throw runtime_error(phosg::string_printf("unimplemented IPCP option %02hhX (%zu bytes)", opt, opt_data.size()));
          default:
            throw runtime_error("unknown IPCP option");
        }
      }

      if (!rejected_opts_w.str().empty()) {
        // Send a Configure-Reject if the client specified IP header compression
        phosg::StringWriter reject_w;
        reject_w.put<IPCPHeader>(IPCPHeader{
            .command = 0x04, // Configure-Reject
            .request_id = fi.ipcp->request_id,
            .size = sizeof(IPCPHeader) + rejected_opts_w.size(),
        });
        reject_w.write(rejected_opts_w.str());
        this->send_layer3_frame(c, FrameInfo::Protocol::IPCP, reject_w.str());

      } else if ((remote_ip != 0x1E1E1E1E) ||
          (remote_primary_dns != 0x23232323) ||
          (remote_secondary_dns != 0x24242424)) {
        // Send a Configure-Nak if the client's request doesn't exactly match
        // what we want them to use.
        phosg::StringWriter opts_w;
        opts_w.put_u8(0x03); // IP address
        opts_w.put_u8(0x06);
        opts_w.put_u32b(0x1E1E1E1E);
        opts_w.put_u8(0x81); // Primary DNS server address
        opts_w.put_u8(0x06);
        opts_w.put_u32b(0x23232323);
        opts_w.put_u8(0x83); // Secondary DNS server address
        opts_w.put_u8(0x06);
        opts_w.put_u32b(0x24242424);

        phosg::StringWriter nak_w;
        nak_w.put<IPCPHeader>(IPCPHeader{
            .command = 0x03, // Configure-Nak
            .request_id = fi.ipcp->request_id,
            .size = static_cast<uint16_t>(opts_w.size() + sizeof(IPCPHeader)),
        });
        nak_w.write(opts_w.str());
        this->send_layer3_frame(c, FrameInfo::Protocol::IPCP, nak_w.str());

      } else { // Options OK
        c->ipv4_addr = remote_ip;

        // As with LCP, we technically should implement the state machine, but I
        // continue to be lazy.
        phosg::StringWriter opts_w;
        opts_w.put_u8(0x03); // IP address
        opts_w.put_u8(0x06);
        opts_w.put_u32b(0x39393939);
        opts_w.put_u8(0x81); // Primary DNS server address
        opts_w.put_u8(0x06);
        opts_w.put_u32b(0x23232323);
        opts_w.put_u8(0x83); // Secondary DNS server address
        opts_w.put_u8(0x06);
        opts_w.put_u32b(0x24242424);

        phosg::StringWriter request_w;
        request_w.put<IPCPHeader>(IPCPHeader{
            .command = 0x01, // Configure-Request
            .request_id = fi.ipcp->request_id,
            .size = static_cast<uint16_t>(opts_w.size() + sizeof(IPCPHeader)),
        });
        request_w.write(opts_w.str());
        this->send_layer3_frame(c, FrameInfo::Protocol::IPCP, request_w.str());

        phosg::StringWriter ack_w;
        ack_w.put<IPCPHeader>(IPCPHeader{
            .command = 0x02, // Configure-Ack
            .request_id = fi.ipcp->request_id,
            .size = fi.ipcp->size,
        });
        ack_w.write(fi.payload, fi.payload_size);
        this->send_layer3_frame(c, FrameInfo::Protocol::IPCP, ack_w.str());
      }
      break;
    }

    case 0x05: { // Terminate-Request
      c->ipv4_addr = 0;
      c->tcp_connections.clear();
      string response(reinterpret_cast<const char*>(fi.payload), fi.payload_size);
      response.at(0) = 0x06; // Terminate-Ack
      this->send_layer3_frame(c, FrameInfo::Protocol::LCP, response);
      break;
    }

    case 0x02: // Configure-Ack
      break;

    case 0x03: // Configure-Nak
    case 0x04: // Configure-Reject
    case 0x06: // Terminate-Ack
    case 0x07: // Code-Reject
      throw runtime_error("unimplemented IPCP command");
    default:
      throw runtime_error("unknown LCP command");
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

  if (c->ipv4_addr == 0) {
    c->ipv4_addr = *reinterpret_cast<const be_uint32_t*>(
        reinterpret_cast<const uint8_t*>(fi.payload) + 6);
  }

  phosg::StringWriter w;
  w.put<ARPHeader>(ARPHeader{
      .hardware_type = fi.arp->hardware_type,
      .protocol_type = fi.arp->protocol_type,
      .hwaddr_len = 6,
      .paddr_len = 4,
      .operation = 0x0002,
  });

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
  w.write(this->host_mac_address_bytes.data(), 6);
  w.write(payload_bytes + 16, 4);
  w.write(payload_bytes, 10);

  this->send_layer3_frame(c, FrameInfo::Protocol::ARP, w.str());
}

void IPStackSimulator::on_client_udp_frame(shared_ptr<IPClient> c, const FrameInfo& fi) {
  // We only implement DHCP and newserv's DNS server here.

  // Every received UDP packet will elicit exactly one UDP response from
  // newserv, so we prepare the response headers in advance

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

  string r_data;
  if (fi.udp->dest_port == 67) { // DHCP
    auto r = fi.read_payload();
    const auto& dhcp = r.get<DHCPHeader>();
    if (dhcp.hardware_type != 1) {
      throw runtime_error("unknown DHCP hardware type");
    }
    if (dhcp.hardware_address_length != 6) {
      throw runtime_error("unknown DHCP hardware address length");
    }
    if (dhcp.magic != 0x63825363) {
      throw runtime_error("incorrect DHCP magic cookie");
    }
    if (dhcp.opcode != 1) { // Request
      throw runtime_error("DHCP packet is not a request");
    }

    unordered_map<uint8_t, string> option_data;
    for (;;) {
      uint8_t option = r.get_u8();
      if (option == 0xFF) {
        break;
      }
      uint8_t size = r.get_u8();
      option_data.emplace(option, r.read(size));
    }

    uint8_t command = 0;
    try {
      command = option_data.at(53).at(0);
    } catch (const out_of_range&) {
      throw runtime_error("client did not send a DHCP command option");
    }

    if (command == 7) {
      // Release IP address (we just ignore these)

    } else if ((command == 1) || (command == 3)) {
      // Populate the client's addresses
      c->mac_addr = dhcp.client_hardware_address.data();
      c->ipv4_addr = 0x0A000105; // 10.0.1.5
      // In this case, the client doesn't know its IPv4 address or ours yet,
      // so we overwrite the existing fields with the appropriate addresses.
      r_ipv4.src_addr = 0x0A000101; // 10.0.1.1
      r_ipv4.dest_addr = c->ipv4_addr;

      if ((command != 1) && (command != 3)) {
        throw runtime_error("client sent unknown DHCP command option");
      }

      phosg::StringWriter w;
      DHCPHeader r_dhcp;
      r_dhcp.opcode = 2; // Response
      r_dhcp.hardware_type = 1; // Ethernet
      r_dhcp.hardware_address_length = 6; // Ethernet
      r_dhcp.hops = 0;
      r_dhcp.transaction_id = dhcp.transaction_id;
      r_dhcp.seconds_elapsed = 0;
      r_dhcp.flags = 0;
      r_dhcp.client_ip_address = 0;
      r_dhcp.your_ip_address = r_ipv4.dest_addr;
      r_dhcp.server_ip_address = r_ipv4.src_addr;
      r_dhcp.gateway_ip_address = 0;
      r_dhcp.client_hardware_address = c->mac_addr;
      r_dhcp.unused_bootp_legacy.clear(0);
      r_dhcp.magic = 0x63825363;
      w.put(r_dhcp);
      // DHCP message type option
      w.put_u8(53);
      w.put_u8(1);
      w.put_u8(static_cast<uint8_t>((command == 3) ? 5 : 2)); // Offer or ack
      // DHCP server ID option
      w.put_u8(54);
      w.put_u8(4);
      w.put_u32b(0x0A000101); // 10.0.1.1
      // Lease time option
      w.put_u8(51);
      w.put_u8(4);
      w.put_u32b(60 * 60 * 24 * 7); // 1 week
      // Renewal time option
      w.put_u8(58);
      w.put_u8(4);
      w.put_u32b(60 * 60 * 24 * 7); // 1 week
      // Rebind time option
      w.put_u8(59);
      w.put_u8(4);
      w.put_u32b(60 * 60 * 24 * 7); // 1 week
      // Subnet mask option
      w.put_u8(1);
      w.put_u8(4);
      w.put_u32b(0xFFFFFF00); // 255.255.255.0
      // Broadcast IP option
      w.put_u8(28);
      w.put_u8(4);
      w.put_u32b(c->ipv4_addr | 0x000000FF);
      // DNS server option
      w.put_u8(6);
      w.put_u8(4);
      w.put_u32b(0x0A000101); // 10.0.1.1
      // Domain name option
      w.put_u8(15);
      w.put_u8(7);
      w.write("newserv");
      // Default gateway option
      w.put_u8(3);
      w.put_u8(4);
      w.put_u32b(0x0A000101); // 10.0.1.1
      // End option list
      w.put_u8(0xFF);

      r_data = std::move(w.str());

    } else {
      throw runtime_error("client sent unknown DHCP command");
    }

  } else if (fi.udp->dest_port == 53) { // DNS
    if (fi.payload_size < 0x0C) {
      throw runtime_error("DNS payload too small");
    }

    uint32_t resolved_address = this->connect_address_for_remote_address(c->ipv4_addr);
    r_data = DNSServer::response_for_query(fi.payload, fi.payload_size, resolved_address);

  } else { // Not DHCP or DNS
    throw runtime_error("UDP packet is not DHCP or DNS");
  }

  if (!r_data.empty()) {
    r_ipv4.size = sizeof(IPv4Header) + sizeof(UDPHeader) + r_data.size();
    r_udp.size = sizeof(UDPHeader) + r_data.size();
    r_ipv4.checksum = FrameInfo::computed_ipv4_header_checksum(r_ipv4);
    r_udp.checksum = FrameInfo::computed_udp4_checksum(
        r_ipv4, r_udp, r_data.data(), r_data.size());

    if (ip_stack_simulator_log.should_log(phosg::LogLevel::DEBUG)) {
      string remote_str = this->str_for_ipv4_netloc(fi.ipv4->src_addr, fi.udp->src_port);
      ip_stack_simulator_log.debug("Sending UDP response to %s", remote_str.c_str());
      phosg::print_data(stderr, r_data);
    }

    phosg::StringWriter w;
    w.put(r_ipv4);
    w.put(r_udp);
    w.write(r_data);

    this->send_layer3_frame(c, FrameInfo::Protocol::IPV4, w.str());
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
  ip_stack_simulator_log.debug("Virtual network sent TCP frame (seq=%08" PRIX32 ", ack=%08" PRIX32 ")",
      fi.tcp->seq_num.load(), fi.tcp->ack_num.load());

  if (fi.tcp->flags & (TCPHeader::Flag::NS | TCPHeader::Flag::CWR | TCPHeader::Flag::ECE | TCPHeader::Flag::URG)) {
    throw runtime_error("unsupported flag in TCP packet");
  }

  if (fi.tcp->flags & TCPHeader::Flag::SYN) {
    // We never make connections back to the client, so we should never receive
    // a SYN+ACK. Essentially, no other flags should be set in any received SYN.
    if ((fi.tcp->flags & 0x0FFF) != TCPHeader::Flag::SYN) {
      throw runtime_error("TCP SYN contains extra flags");
    }

    phosg::StringReader options_r(fi.tcp + 1, fi.tcp_options_size);
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
    string conn_str;

    if (emplace_ret.second) {
      // Connection is new; initialize it
      conn.client = c;
      conn.resend_push_event.reset(event_new(this->base.get(), -1, EV_TIMEOUT,
          &IPStackSimulator::dispatch_on_resend_push, &conn));
      conn.server_addr = fi.ipv4->dest_addr;
      conn.server_port = fi.tcp->dest_port;
      conn.client_port = fi.tcp->src_port;
      conn.next_client_seq = fi.tcp->seq_num + 1;
      conn.acked_server_seq = phosg::random_object<uint32_t>();
      conn.resend_push_usecs = DEFAULT_RESEND_PUSH_USECS;
      conn.next_push_max_frame_size = max_frame_size;
      conn.awaiting_first_ack = true;
      conn.max_frame_size = max_frame_size;
      conn.bytes_received = 0;
      conn.bytes_sent = 0;

      conn_str = this->str_for_tcp_connection(c, conn);
      ip_stack_simulator_log.info("Client opened TCP connection %s (acked_server_seq=%08" PRIX32 ", next_client_seq=%08" PRIX32 ")",
          conn_str.c_str(), conn.acked_server_seq, conn.next_client_seq);

    } else {
      // Connection is NOT new; this is probably a resend of an earlier SYN
      if (!conn.awaiting_first_ack) {
        throw logic_error("SYN received on already-open connection after initial phase");
      }
      // TODO: We should check the syn/ack numbers here instead of just assuming
      // they're correct
      conn_str = this->str_for_tcp_connection(c, conn);
      ip_stack_simulator_log.debug("Client resent SYN for TCP connection %s",
          conn_str.c_str());
    }

    // Send a SYN+ACK (send_tcp_frame always adds the ACK flag)
    this->send_tcp_frame(c, conn, TCPHeader::Flag::SYN);
    ip_stack_simulator_log.debug("Sent SYN+ACK on %s (acked_server_seq=%08" PRIX32 ", next_client_seq=%08" PRIX32 ")",
        conn_str.c_str(), conn.acked_server_seq, conn.next_client_seq);

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
    bool acked_seq_changed = false;

    if (fi.tcp->flags & TCPHeader::Flag::ACK) {
      ip_stack_simulator_log.debug("Client sent ACK %08" PRIX32, fi.tcp->ack_num.load());
      if (conn->awaiting_first_ack) {
        if (fi.tcp->ack_num != conn->acked_server_seq + 1) {
          throw runtime_error("first ack_num was not acked_server_seq + 1");
        }
        conn->acked_server_seq++;
        conn->awaiting_first_ack = false;

      } else {
        if (seq_num_greater(fi.tcp->ack_num, conn->acked_server_seq)) {
          ip_stack_simulator_log.debug("Advancing acked_server_seq from %08" PRIX32, conn->acked_server_seq);
          uint32_t ack_delta = fi.tcp->ack_num - conn->acked_server_seq;
          size_t pending_bytes = evbuffer_get_length(conn->pending_data.get());
          if (pending_bytes < ack_delta) {
            throw runtime_error("client acknowledged beyond end of sent data");
          }

          evbuffer_drain(conn->pending_data.get(), ack_delta);
          conn->acked_server_seq += ack_delta;
          conn->resend_push_usecs = DEFAULT_RESEND_PUSH_USECS;
          conn->next_push_max_frame_size = conn->max_frame_size;
          acked_seq_changed = true;

          ip_stack_simulator_log.debug("Removed %08" PRIX32 " bytes from pending buffer and advanced acked_server_seq to %08" PRIX32,
              ack_delta, conn->acked_server_seq);

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

      string conn_str = this->str_for_tcp_connection(c, *conn);
      ip_stack_simulator_log.info("Client closed TCP connection %s", conn_str.c_str());

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

      string conn_str = ip_stack_simulator_log.should_log(phosg::LogLevel::WARNING)
          ? this->str_for_tcp_connection(c, *conn)
          : "";

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
        ip_stack_simulator_log.warning(
            "Client sent out-of-order sequence number (expected %08" PRIX32 ", received %08" PRIX32 ", 0x%zX data bytes)",
            conn->next_client_seq, fi.tcp->seq_num.load(), fi.payload_size);
        payload_skip_bytes = fi.payload_size;
      }

      if (payload_skip_bytes > fi.payload_size) {
        throw logic_error("payload skip bytes too large");
      }

      if (payload_skip_bytes < fi.payload_size) {
        const void* payload = reinterpret_cast<const uint8_t*>(fi.payload) + payload_skip_bytes;
        size_t payload_size = fi.payload_size - payload_skip_bytes;

        bool was_logged;
        if (payload_skip_bytes) {
          was_logged = ip_stack_simulator_log.debug("Client sent data on TCP connection %s, overlapping existing ack'ed data (0x%zX bytes ignored)",
              conn_str.c_str(), payload_skip_bytes);
        } else {
          was_logged = ip_stack_simulator_log.debug("Client sent data on TCP connection %s",
              conn_str.c_str());
        }
        if (was_logged) {
          phosg::print_data(stderr, payload, payload_size);
        }

        // Send the new data to the server
        struct evbuffer* server_out_buf = bufferevent_get_output(
            conn->server_bev.get());
        evbuffer_add(server_out_buf, payload, payload_size);

        // Update the sequence number and stats
        conn->next_client_seq += payload_size;
        conn->bytes_received += payload_size;
        if (conn->next_client_seq < payload_size) {
          ip_stack_simulator_log.warning("Client sequence number has wrapped (next=%08" PRIX32 ", bytes=%zX)",
              fi.tcp->seq_num.load(), payload_size);
        }
      }

      // Send an ACK
      this->send_tcp_frame(c, *conn);
      ip_stack_simulator_log.debug("Sent PSH ACK on %s (acked_server_seq=%08" PRIX32 ", next_client_seq=%08" PRIX32 ", bytes_received=0x%zX)",
          conn_str.c_str(), conn->acked_server_seq, conn->next_client_seq, conn->bytes_received);
    }

    if (conn_valid && acked_seq_changed) {
      // Try to send some more data if the client is waiting on it
      this->send_pending_push_frame(c, *conn, true);
    }
  }
}

void IPStackSimulator::open_server_connection(shared_ptr<IPClient> c, IPClient::TCPConnection& conn) {
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
  shared_ptr<const PortConfiguration> port_config;
  try {
    port_config = this->state->number_to_port_config.at(conn.server_port);
  } catch (const out_of_range&) {
    bufferevent_free(bevs[1]);
    throw logic_error("client connected to port missing from configuration");
  }

  string conn_str = this->str_for_tcp_connection(c, conn);
  if (port_config->behavior == ServerBehavior::PROXY_SERVER) {
    if (!this->state->proxy_server.get()) {
      ip_stack_simulator_log.error("TCP connection %s is to non-running proxy server", conn_str.c_str());
      flush_and_free_bufferevent(bevs[1]);
    } else {
      this->state->proxy_server->connect_virtual_client(bevs[1], c->network_id, conn.server_port);
      ip_stack_simulator_log.info("Connected TCP connection %s to proxy server", conn_str.c_str());
    }
  } else if (this->state->game_server.get()) {
    this->state->game_server->connect_virtual_client(
        bevs[1], c->network_id, c->ipv4_addr, conn.client_port,
        conn.server_port, port_config->version, port_config->behavior);
    ip_stack_simulator_log.info("Connected TCP connection %s to game server", conn_str.c_str());
  } else {
    ip_stack_simulator_log.error("No server available for TCP connection %s", conn_str.c_str());
    flush_and_free_bufferevent(bevs[1]);
  }
}

void IPStackSimulator::send_pending_push_frame(
    shared_ptr<IPClient> c, IPClient::TCPConnection& conn, bool always_send) {
  size_t pending_bytes = evbuffer_get_length(conn.pending_data.get());
  if (!pending_bytes) {
    event_del(conn.resend_push_event.get());
    return;
  }

  // If we're waiting to receive an ACK from the client, don't send another PSH
  // until we get the ACK (unless this is a resend of a previous PSH due to a
  // timeout)
  if (!always_send && event_pending(conn.resend_push_event.get(), EV_TIMEOUT, nullptr)) {
    return;
  }

  size_t bytes_to_send = min<size_t>(pending_bytes, conn.next_push_max_frame_size);
  if (c->protocol == Protocol::HDLC_TAPSERVER) {
    // There is a bug in Dolphin's modem implementation (which I wrote, so it's
    // my fault) that causes commands to be dropped when too much data is sent
    // at once. To work around this, we only send up to 200 bytes in each push
    // frame.
    bytes_to_send = min<size_t>(bytes_to_send, 200);
  }

  ip_stack_simulator_log.debug("Sending PSH frame with seq_num %08" PRIX32 ", 0x%zX/0x%zX data bytes",
      conn.acked_server_seq, bytes_to_send, pending_bytes);

  this->send_tcp_frame(c, conn, TCPHeader::Flag::PSH, conn.pending_data.get(), bytes_to_send);
  struct timeval resend_push_timeout = phosg::usecs_to_timeval(conn.resend_push_usecs);
  event_add(conn.resend_push_event.get(), &resend_push_timeout);

  // If the client isn't responding to our PSHes, back off exponentially up to
  // a limit of 5 seconds between PSH frames. This window is reset when
  // acked_server_seq changes (that is, when the client has acknowledged any new
  // data). It seems some situations cause GameCube clients to drop packets more
  // often; to alleviate this, we also try to resend less data.
  conn.resend_push_usecs *= 2;
  if (conn.resend_push_usecs > 5000000) {
    conn.resend_push_usecs = 5000000;
  }
  conn.next_push_max_frame_size = max<size_t>(0x100, conn.next_push_max_frame_size - 0x100);
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
  tcp.checksum = FrameInfo::computed_tcp4_checksum(ipv4, tcp, linear_data, src_bytes);

  phosg::StringWriter w;
  w.put(ipv4);
  w.put(tcp);
  if (src_bytes) {
    w.write(linear_data, src_bytes);
  }

  this->send_layer3_frame(c, FrameInfo::Protocol::IPV4, w.str());
}

void IPStackSimulator::dispatch_on_resend_push(evutil_socket_t, short, void* ctx) {
  auto* conn = reinterpret_cast<IPClient::TCPConnection*>(ctx);
  auto c = conn->client.lock();
  if (!c.get()) {
    ip_stack_simulator_log.warning("Resend push event triggered for deleted client; ignoring");
  } else {
    auto sim = c->sim.lock();
    if (!sim) {
      ip_stack_simulator_log.warning("Resend push event triggered for client on deleted simulator; ignoring");
    } else {
      sim->send_pending_push_frame(c, *conn, true);
    }
  }
}

void IPStackSimulator::dispatch_on_server_input(struct bufferevent*, void* ctx) {
  auto* conn = reinterpret_cast<IPClient::TCPConnection*>(ctx);
  auto c = conn->client.lock();
  if (!c.get()) {
    ip_stack_simulator_log.warning("Server input event triggered for deleted client; ignoring");
  } else {
    auto sim = c->sim.lock();
    if (!sim) {
      ip_stack_simulator_log.warning("Server input event triggered for client on deleted simulator; ignoring");
    } else {
      sim->on_server_input(c, *conn);
    }
  }
}

void IPStackSimulator::on_server_input(shared_ptr<IPClient> c, IPClient::TCPConnection& conn) {
  struct evbuffer* buf = bufferevent_get_input(conn.server_bev.get());
  ip_stack_simulator_log.debug("Server input event: 0x%zX bytes to read",
      evbuffer_get_length(buf));

  auto sim = c->sim.lock();
  uint64_t idle_timeout_usecs = sim ? sim->state->client_idle_timeout_usecs : 60000000;
  struct timeval tv = phosg::usecs_to_timeval(idle_timeout_usecs);
  event_add(c->idle_timeout_event.get(), &tv);

  evbuffer_add_buffer(conn.pending_data.get(), buf);
  this->send_pending_push_frame(c, conn, false);
}

void IPStackSimulator::dispatch_on_server_error(
    struct bufferevent*, short events, void* ctx) {
  auto* conn = reinterpret_cast<IPClient::TCPConnection*>(ctx);
  auto c = conn->client.lock();
  if (!c.get()) {
    ip_stack_simulator_log.warning("Server error event triggered for deleted client; ignoring");
  } else {
    auto sim = c->sim.lock();
    if (!sim) {
      ip_stack_simulator_log.warning("Server error event triggered for client on deleted simulator; ignoring");
    } else {
      sim->on_server_error(c, *conn, events);
    }
  }
}

void IPStackSimulator::on_server_error(
    shared_ptr<IPClient> c, IPClient::TCPConnection& conn, short events) {
  if (events & BEV_EVENT_ERROR) {
    int err = EVUTIL_SOCKET_ERROR();
    ip_stack_simulator_log.warning("Received error %d from virtual connection (%s)", err,
        evutil_socket_error_to_string(err));
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    // Send an RST to the client. Kind of rude (we really should use FIN) but
    // the PSO network stack always sends an RST to us when disconnecting, so
    // whatever
    this->send_tcp_frame(c, conn, TCPHeader::Flag::RST);

    // Delete the connection object (this also flushes and frees the server
    // virtual connection bufferevent)
    string conn_str = this->str_for_tcp_connection(c, conn);
    ip_stack_simulator_log.info("Server closed TCP connection %s", conn_str.c_str());
    c->tcp_connections.erase(this->tcp_conn_key_for_connection(conn));
  }
}

void IPStackSimulator::log_frame(const string& data) const {
  if (this->pcap_text_log_file) {
    phosg::print_data(this->pcap_text_log_file, data, 0, nullptr, phosg::PrintDataFlags::SKIP_SEPARATOR);
    fputc('\n', this->pcap_text_log_file);
    fflush(this->pcap_text_log_file);
  }
}
