#include "IPStackSimulator.hh"

#include <stdint.h>
#include <string.h>

#include <phosg/Network.hh>
#include <phosg/Random.hh>
#include <phosg/Time.hh>
#include <string>

#include "DNSServer.hh"
#include "GameServer.hh"
#include "IPFrameInfo.hh"
#include "Loggers.hh"

using namespace std;

static size_t unescape_hdlc_frame_inplace(void* vdata, size_t size) {
  uint8_t* data = reinterpret_cast<uint8_t*>(vdata);
  if (size < 2) {
    throw runtime_error("escaped HDLC frame is too small");
  }
  if (data[0] != 0x7E) {
    throw runtime_error("HDLC frame does not begin with 7E");
  }
  if (data[size - 1] != 0x7E) {
    throw runtime_error("HDLC frame does not end with 7E");
  }

  size_t read_offset = 1;
  size_t write_offset = 1;
  while (read_offset < size - 1) {
    uint8_t ch = data[read_offset++];
    if (ch == 0x7D) {
      if (read_offset >= size - 1) {
        throw runtime_error("abort sequence received");
      }
      ch = data[read_offset++] ^ 0x20;
    }
    data[write_offset++] = ch;
  }
  if (write_offset > size - 1) {
    throw logic_error("unescaping HDLC frame resulted in longer data string");
  }
  data[write_offset++] = 0x7E;
  return write_offset;
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

IPSSClient::TCPConnection::TCPConnection(std::shared_ptr<IPSSClient> client)
    : client(client),
      resend_push_timer(*client->io_context) {}

void IPSSClient::TCPConnection::drain_outbound_data(size_t size) {
  this->outbound_data_bytes -= size;
  while (size > 0 && !this->outbound_data.empty()) {
    auto& front_block = this->outbound_data.front();
    if (front_block.size() <= size) {
      size -= front_block.size();
      this->outbound_data.pop_front();
    } else {
      front_block = front_block.substr(size);
      size = 0;
    }
  }
  if (size > 0) {
    throw logic_error("attempted to drain more outbound data than was present");
  }
}

void IPSSClient::TCPConnection::linearize_outbound_data(size_t size) {
  while (this->outbound_data.size() > 1 && this->outbound_data.front().size() < size) {
    auto second_block_it = this->outbound_data.begin();
    second_block_it++;
    this->outbound_data.front() += *second_block_it;
    this->outbound_data.erase(second_block_it);
  }
}

IPSSClient::IPSSClient(
    shared_ptr<IPStackSimulator> sim,
    uint64_t network_id,
    VirtualNetworkProtocol protocol,
    asio::ip::tcp::socket&& sock)
    : io_context(sim->get_io_context()),
      sim(sim),
      network_id(network_id),
      sock(std::move(sock)),
      protocol(protocol),
      mac_addr(0),
      ipv4_addr(0),
      idle_timeout_timer(*sim->get_io_context()) {
  this->reschedule_idle_timeout();
}

void IPSSClient::reschedule_idle_timeout() {
  auto sim = this->sim.lock();
  if (!sim) {
    throw runtime_error("cannot reschedule idle timeout when simulator is missing");
  }
  this->idle_timeout_timer.cancel();
  this->idle_timeout_timer.expires_after(std::chrono::microseconds(sim->get_state()->client_idle_timeout_usecs));
  this->idle_timeout_timer.async_wait([this, sim](std::error_code ec) {
    if (!ec) {
      sim->log.info_f("Idle timeout expired on N-{:X}", this->network_id);
      this->sock.close();
    }
  });
}

IPSSChannel::IPSSChannel(
    std::shared_ptr<IPStackSimulator> sim,
    std::weak_ptr<IPSSClient> ipss_client,
    std::weak_ptr<IPSSClient::TCPConnection> tcp_conn,
    Version version,
    uint8_t language,
    const std::string& name,
    phosg::TerminalFormat terminal_send_color,
    phosg::TerminalFormat terminal_recv_color)
    : Channel(version, language, name, terminal_send_color, terminal_recv_color),
      sim(sim),
      ipss_client(ipss_client),
      tcp_conn(tcp_conn),
      data_available_signal(sim->io_context->get_executor()) {}

std::string IPSSChannel::default_name() const {
  auto ipc = this->ipss_client.lock();
  if (ipc) {
    string addr_str = str_for_endpoint(ipc->sock.remote_endpoint());
    return std::format("ipss:N-{}:{}", ipc->network_id, addr_str);
  } else {
    return std::format("ipss:N-{}:__unknown_address__", ipc->network_id);
  }
}

bool IPSSChannel::connected() const {
  auto ipss_client = this->ipss_client.lock();
  auto tcp_conn = this->tcp_conn.lock();
  return tcp_conn && ipss_client && ipss_client->sock.is_open();
}

void IPSSChannel::disconnect() {
  auto c = this->ipss_client.lock();
  auto conn = this->tcp_conn.lock();
  if (c && conn) {
    sim->schedule_send_pending_push_frame(conn, 0);
    this->tcp_conn.reset();
    this->ipss_client.reset();
    this->data_available_signal.set();
  }
}

void IPSSChannel::add_inbound_data(const void* data, size_t size) {
  // If recv_buf is not null, there is a coroutine waiting to receive data, and
  // inbound_data must be empty. Copy the data directly to the waiting
  // coroutine's buffer, and put the rest in this->inbound_data if needed.
  if (this->recv_buf) {
    size_t direct_size = min<size_t>(this->recv_buf_size, size);
    memcpy(this->recv_buf, data, direct_size);
    data = reinterpret_cast<const uint8_t*>(data) + direct_size;
    size -= direct_size;
    this->recv_buf_size -= direct_size;
    this->recv_buf = this->recv_buf_size
        ? reinterpret_cast<uint8_t*>(this->recv_buf) + direct_size
        : nullptr;
  }

  // If there is still data left after the above, add it to the pending inbound
  // data buffer
  if (size > 0) {
    this->inbound_data.emplace_back(reinterpret_cast<const char*>(data), size);
  }

  // Notify the waiting coroutine (if any) that data is available
  this->data_available_signal.set();
}

void IPSSChannel::send_raw(string&& data) {
  auto c = this->ipss_client.lock();
  if (!c) {
    return;
  }
  auto conn = this->tcp_conn.lock();
  if (!conn) {
    return;
  }
  auto sim = c->sim.lock();
  if (!sim) {
    return;
  }

  conn->outbound_data_bytes += data.size();
  conn->outbound_data.emplace_back(std::move(data));

  // If we're already waiting for an ACK from the remote client, don't send
  // another PSH right now - we will either send another PSH when we receive
  // the ACK or will retry sending the PSH soon (which will then include the
  // new data, if it's within the MTU from the last acked sequence number).
  if (!conn->awaiting_ack) {
    sim->schedule_send_pending_push_frame(conn, 0);
  }
  c->reschedule_idle_timeout();
}

asio::awaitable<void> IPSSChannel::recv_raw(void* data, size_t size) {
  if (this->recv_buf) {
    throw logic_error("recv_raw called again when it was already pending");
  }

  // Receive as much data as possible from the pending inbound data buffer
  while (size && !this->inbound_data.empty()) {
    auto& front_buf = this->inbound_data.front();
    if (size >= front_buf.size()) {
      memcpy(data, front_buf.data(), front_buf.size());
      data = reinterpret_cast<uint8_t*>(data) + front_buf.size();
      size -= front_buf.size();
      this->inbound_data.pop_front();
    } else {
      memcpy(data, front_buf.data(), size);
      data = reinterpret_cast<uint8_t*>(data) + size;
      front_buf = front_buf.substr(size);
      size = 0;
    }
  }

  // If there's still more data to read, block until it's available
  // (add_inbound_data is responsible for waking this coroutine)
  if (size > 0) {
    this->recv_buf = data;
    this->recv_buf_size = size;
    while (this->recv_buf) {
      if (!this->connected()) {
        throw runtime_error("IPSS channel closed");
      }
      this->data_available_signal.clear();
      co_await this->data_available_signal.wait();
    }
  }
}

IPStackSimulator::IPStackSimulator(shared_ptr<ServerState> state)
    : Server(state->io_context, "[IPStackSimulator] "), state(state) {
  this->host_mac_address_bytes.clear(0x90);
  this->broadcast_mac_address_bytes.clear(0xFF);
}

void IPStackSimulator::listen(const std::string& name, const string& addr, int port, VirtualNetworkProtocol protocol) {
  if (port == 0) {
    throw std::runtime_error("Listening port cannot be zero");
  }
  asio::ip::address asio_addr = addr.empty() ? asio::ip::address_v4::any() : asio::ip::make_address(addr);
  auto sock = make_shared<IPSSSocket>();
  sock->name = name;
  sock->endpoint = asio::ip::tcp::endpoint(asio_addr, port);
  sock->protocol = protocol;
  this->add_socket(std::move(sock));
}

uint32_t IPStackSimulator::connect_address_for_remote_address(uint32_t remote_addr) {
  // Use an address not on the same subnet as the client, so that PSO Plus and
  // Episode III will think they're talking to a remote network and won't
  // reject the connection.
  return ((remote_addr & 0xFF000000) == 0x23000000) ? 0x24242424 : 0x23232323;
}

uint64_t IPStackSimulator::tcp_conn_key_for_connection(std::shared_ptr<const IPSSClient::TCPConnection> conn) {
  return (static_cast<uint64_t>(conn->server_addr) << 32) |
      (static_cast<uint64_t>(conn->server_port) << 16) |
      static_cast<uint64_t>(conn->client_port);
}

uint64_t IPStackSimulator::tcp_conn_key_for_client_frame(const IPv4Header& ipv4, const TCPHeader& tcp) {
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

string IPStackSimulator::str_for_ipv4_netloc(uint32_t addr, uint16_t port) {
  be_uint32_t be_addr = addr;
  char addr_str[INET_ADDRSTRLEN];
  if (!inet_ntop(AF_INET, &be_addr, addr_str, INET_ADDRSTRLEN)) {
    return std::format("<UNKNOWN>:{}", port);
  } else {
    return std::format("{}:{}", addr_str, port);
  }
}

string IPStackSimulator::str_for_tcp_connection(
    shared_ptr<const IPSSClient> c, std::shared_ptr<const IPSSClient::TCPConnection> conn) {
  uint64_t key = IPStackSimulator::tcp_conn_key_for_connection(conn);
  string server_netloc_str = str_for_ipv4_netloc(conn->server_addr, conn->server_port);
  string client_netloc_str = str_for_ipv4_netloc(c->ipv4_addr, conn->client_port);
  return std::format("{:016X} ({} -> {})", key, client_netloc_str, server_netloc_str);
}

asio::awaitable<void> IPStackSimulator::send_ethernet_tapserver_frame(
    shared_ptr<IPSSClient> c, FrameInfo::Protocol proto, const void* data, size_t size) const {

  struct {
    phosg::le_uint16_t frame_size;
    EthernetHeader ether;
  } header;
  static_assert(sizeof(header) == 0x10, "Ethernet tapserver header size is incorrect");

  header.ether.dest_mac = c->mac_addr;
  header.ether.src_mac = this->host_mac_address_bytes;
  switch (proto) {
    case FrameInfo::Protocol::NONE:
      throw logic_error("layer 3 protocol not specified");
    case FrameInfo::Protocol::LCP:
      throw logic_error("cannot send LCP frame over Ethernet");
    case FrameInfo::Protocol::IPV4:
      header.ether.protocol = 0x0800;
      break;
    case FrameInfo::Protocol::ARP:
      header.ether.protocol = 0x0806;
      break;
    default:
      throw logic_error("unknown layer 3 protocol");
  }
  header.frame_size = size + sizeof(EthernetHeader);

  array<asio::const_buffer, 2> bufs{
      asio::buffer(static_cast<const void*>(&header), sizeof(header)),
      asio::buffer(data, size)};
  co_await asio::async_write(c->sock, bufs, asio::use_awaitable);
}

asio::awaitable<void> IPStackSimulator::send_hdlc_frame(
    shared_ptr<IPSSClient> c, FrameInfo::Protocol proto, const void* data, size_t size, bool is_raw) const {

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
  if (this->log.debug_f("Sending HDLC frame to virtual network (escaped to {:X} bytes)", escaped.size())) {
    phosg::print_data(stderr, w.str());
  }

  if (!is_raw) {
    phosg::le_uint16_t frame_size = escaped.size();
    array<asio::const_buffer, 2> bufs{
        asio::buffer(static_cast<const void*>(&frame_size), sizeof(frame_size)),
        asio::buffer(escaped.data(), escaped.size())};
    co_await asio::async_write(c->sock, bufs, asio::use_awaitable);
  } else {
    co_await asio::async_write(c->sock, asio::buffer(escaped.data(), escaped.size()), asio::use_awaitable);
  }
}

asio::awaitable<void> IPStackSimulator::send_layer3_frame(
    shared_ptr<IPSSClient> c, FrameInfo::Protocol proto, const void* data, size_t size) const {
  switch (c->protocol) {
    case VirtualNetworkProtocol::ETHERNET_TAPSERVER:
      co_await this->send_ethernet_tapserver_frame(c, proto, data, size);
      break;
    case VirtualNetworkProtocol::HDLC_TAPSERVER:
      co_await this->send_hdlc_frame(c, proto, data, size, false);
      break;
    case VirtualNetworkProtocol::HDLC_RAW:
      co_await this->send_hdlc_frame(c, proto, data, size, true);
      break;
    default:
      throw logic_error("unknown link type");
  }
}

asio::awaitable<void> IPStackSimulator::on_client_frame(shared_ptr<IPSSClient> c, const void* data, size_t size) {
  FrameInfo::LinkType link_type = (c->protocol == VirtualNetworkProtocol::ETHERNET_TAPSERVER)
      ? FrameInfo::LinkType::ETHERNET
      : FrameInfo::LinkType::HDLC;

  if (this->log.debug_f("Virtual network sent frame")) {
    phosg::print_data(stderr, data, size);
  }

  FrameInfo fi(link_type, data, size);
  if (this->log.should_log(phosg::LogLevel::L_DEBUG)) {
    string fi_header = fi.header_str();
    this->log.debug_f("Frame header: {}", fi_header);
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
      throw runtime_error(std::format(
          "HDLC checksum is incorrect ({:04X} expected, {:04X} received)",
          expected_checksum, stored_checksum));
    }
  } else {
    throw runtime_error("frame is not Ethernet or HDLC");
  }

  if (fi.lcp) {
    co_await this->on_client_lcp_frame(c, fi);

  } else if (fi.pap) {
    co_await this->on_client_pap_frame(c, fi);

  } else if (fi.ipcp) {
    co_await this->on_client_ipcp_frame(c, fi);

  } else if (fi.arp) {
    co_await this->on_client_arp_frame(c, fi);

  } else if (fi.ipv4) {
    uint16_t expected_ipv4_checksum = fi.computed_ipv4_header_checksum();
    if (fi.ipv4->checksum != expected_ipv4_checksum) {
      throw runtime_error(std::format(
          "IPv4 header checksum is incorrect ({:04X} expected, {:04X} received)",
          expected_ipv4_checksum, fi.ipv4->checksum));
    }

    if ((fi.ipv4->src_addr != c->ipv4_addr) && (fi.ipv4->src_addr != 0)) {
      throw runtime_error("client sent IPv4 packet from different IPv4 address");
    }

    if (fi.udp) {
      uint16_t expected_udp_checksum = fi.computed_udp4_checksum();
      if (fi.udp->checksum != expected_udp_checksum) {
        throw runtime_error(std::format(
            "UDP checksum is incorrect ({:04X} expected, {:04X} received)",
            expected_udp_checksum, fi.udp->checksum));
      }
      co_await this->on_client_udp_frame(c, fi);

    } else if (fi.tcp) {
      uint16_t expected_tcp_checksum = fi.computed_tcp4_checksum();
      if (fi.tcp->checksum != expected_tcp_checksum) {
        throw runtime_error(std::format(
            "TCP checksum is incorrect ({:04X} expected, {:04X} received)",
            expected_tcp_checksum, fi.tcp->checksum));
      }
      co_await this->on_client_tcp_frame(c, fi);

    } else {
      throw runtime_error("frame uses unsupported IPv4 protocol");
    }

  } else {
    throw runtime_error("frame is not IPv4");
  }
}

asio::awaitable<void> IPStackSimulator::on_client_lcp_frame(shared_ptr<IPSSClient> c, const FrameInfo& fi) {
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
            throw runtime_error(std::format("unimplemented LCP option {:02X} ({} bytes)", opt, opt_data.size()));
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
      co_await this->send_layer3_frame(c, FrameInfo::Protocol::LCP, request_w.str());

      phosg::StringWriter ack_w;
      ack_w.put<LCPHeader>(LCPHeader{
          .command = 0x02, // Configure-Ack
          .request_id = fi.lcp->request_id,
          .size = fi.lcp->size,
      });
      ack_w.write(fi.payload, fi.payload_size);
      co_await this->send_layer3_frame(c, FrameInfo::Protocol::LCP, ack_w.str());

      break;
    }

    case 0x05: { // Terminate-Request
      c->ipv4_addr = 0;
      c->tcp_connections.clear();
      string response(reinterpret_cast<const char*>(fi.payload), fi.payload_size);
      response.at(0) = 0x06; // Terminate-Ack
      co_await this->send_layer3_frame(c, FrameInfo::Protocol::LCP, response);
      break;
    }

    case 0x09: { // Echo-Request
      string response(reinterpret_cast<const char*>(fi.payload), fi.payload_size);
      response.at(0) = 0x0A; // Echo-Reply
      co_await this->send_layer3_frame(c, FrameInfo::Protocol::LCP, response);
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

asio::awaitable<void> IPStackSimulator::on_client_pap_frame(shared_ptr<IPSSClient> c, const FrameInfo& fi) {
  if (fi.pap->command != 0x01) { // Authenticate-Request
    throw runtime_error("client sent incorrect PAP command");
  }

  auto r = fi.read_payload();
  string username = r.read(r.get_u8());
  string password = r.read(r.get_u8());
  this->log.info_f("Client logged in with username \"{}\" and password", username);

  static const string login_message = "newserv PPP simulator";
  phosg::StringWriter w;
  w.put<PAPHeader>(PAPHeader{
      .command = 0x02, // Authenticate-Ack
      .request_id = fi.pap->request_id,
      .size = login_message.size() + sizeof(PAPHeader) + 1,
  });
  w.put_u8(login_message.size());
  w.write(login_message);
  co_await this->send_layer3_frame(c, FrameInfo::Protocol::PAP, w.str());
}

asio::awaitable<void> IPStackSimulator::on_client_ipcp_frame(shared_ptr<IPSSClient> c, const FrameInfo& fi) {
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
            throw runtime_error(std::format("unimplemented IPCP option {:02X} ({} bytes)", opt, opt_data.size()));
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
        co_await this->send_layer3_frame(c, FrameInfo::Protocol::IPCP, reject_w.str());

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
        co_await this->send_layer3_frame(c, FrameInfo::Protocol::IPCP, nak_w.str());

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
        co_await this->send_layer3_frame(c, FrameInfo::Protocol::IPCP, request_w.str());

        phosg::StringWriter ack_w;
        ack_w.put<IPCPHeader>(IPCPHeader{
            .command = 0x02, // Configure-Ack
            .request_id = fi.ipcp->request_id,
            .size = fi.ipcp->size,
        });
        ack_w.write(fi.payload, fi.payload_size);
        co_await this->send_layer3_frame(c, FrameInfo::Protocol::IPCP, ack_w.str());
      }
      break;
    }

    case 0x05: { // Terminate-Request
      c->ipv4_addr = 0;
      c->tcp_connections.clear();
      string response(reinterpret_cast<const char*>(fi.payload), fi.payload_size);
      response.at(0) = 0x06; // Terminate-Ack
      co_await this->send_layer3_frame(c, FrameInfo::Protocol::LCP, response);
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

asio::awaitable<void> IPStackSimulator::on_client_arp_frame(shared_ptr<IPSSClient> c, const FrameInfo& fi) {
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
    c->ipv4_addr = *reinterpret_cast<const be_uint32_t*>(reinterpret_cast<const uint8_t*>(fi.payload) + 6);
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

  co_await this->send_layer3_frame(c, FrameInfo::Protocol::ARP, w.str());
}

asio::awaitable<void> IPStackSimulator::on_client_udp_frame(shared_ptr<IPSSClient> c, const FrameInfo& fi) {
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

    if (this->log.should_log(phosg::LogLevel::L_DEBUG)) {
      string remote_str = this->str_for_ipv4_netloc(fi.ipv4->src_addr, fi.udp->src_port);
      this->log.debug_f("Sending UDP response to {}", remote_str);
      phosg::print_data(stderr, r_data);
    }

    phosg::StringWriter w;
    w.put(r_ipv4);
    w.put(r_udp);
    w.write(r_data);

    co_await this->send_layer3_frame(c, FrameInfo::Protocol::IPV4, w.str());
  }
}

asio::awaitable<void> IPStackSimulator::on_client_tcp_frame(shared_ptr<IPSSClient> c, const FrameInfo& fi) {
  this->log.debug_f("Virtual network sent TCP frame (seq={:08X}, ack={:08X})",
      fi.tcp->seq_num, fi.tcp->ack_num);

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

    shared_ptr<IPSSClient::TCPConnection> conn;
    string conn_str;
    uint64_t key = this->tcp_conn_key_for_client_frame(fi);
    auto conn_it = c->tcp_connections.find(key);
    if (conn_it == c->tcp_connections.end()) {
      conn = make_shared<IPSSClient::TCPConnection>(c);
      c->tcp_connections.emplace(key, conn);
      conn->server_addr = fi.ipv4->dest_addr;
      conn->server_port = fi.tcp->dest_port;
      conn->client_port = fi.tcp->src_port;
      conn->next_client_seq = fi.tcp->seq_num + 1;
      conn->acked_server_seq = phosg::random_object<uint32_t>();
      conn->resend_push_usecs = DEFAULT_RESEND_PUSH_USECS;
      conn->next_push_max_frame_size = max_frame_size;
      conn->max_frame_size = max_frame_size;

      conn_str = this->str_for_tcp_connection(c, conn);
      this->log.info_f(
          "Client opened TCP connection {} (acked_server_seq={:08X}, next_client_seq={:08X})",
          conn_str, conn->acked_server_seq, conn->next_client_seq);

    } else {
      conn = conn_it->second;

      // Connection is NOT new; this is probably a resend of an earlier SYN
      if (!conn->awaiting_first_ack) {
        throw logic_error("SYN received on already-open connection after initial phase");
      }
      // TODO: We should check the syn/ack numbers here instead of just assuming
      // they're correct
      conn_str = this->str_for_tcp_connection(c, conn);
      this->log.debug_f("Client resent SYN for TCP connection {}", conn_str);
    }

    // Send a SYN+ACK (send_tcp_frame always adds the ACK flag)
    co_await this->send_tcp_frame(c, conn, TCPHeader::Flag::SYN);
    this->log.debug_f("Sent SYN+ACK on {} (acked_server_seq={:08X}, next_client_seq={:08X})",
        conn_str, conn->acked_server_seq, conn->next_client_seq);

  } else {
    // This frame isn't a SYN, so a connection object should already exist
    uint64_t key = this->tcp_conn_key_for_client_frame(fi);
    auto conn_it = c->tcp_connections.find(key);
    if (conn_it == c->tcp_connections.end()) {
      throw runtime_error("non-SYN frame does not correspond to any open TCP connection");
    }
    auto& conn = conn_it->second;
    bool conn_valid = true;
    bool acked_seq_changed = false;

    if (fi.tcp->flags & TCPHeader::Flag::ACK) {
      this->log.debug_f("Client sent ACK {:08X}", fi.tcp->ack_num);
      if (conn->awaiting_first_ack) {
        if (fi.tcp->ack_num != conn->acked_server_seq + 1) {
          throw runtime_error("first ack_num was not acked_server_seq + 1");
        }
        conn->acked_server_seq++;
        conn->awaiting_first_ack = false;

      } else {
        conn->awaiting_ack = false;
        if (seq_num_greater(fi.tcp->ack_num, conn->acked_server_seq)) {
          this->log.debug_f("Advancing acked_server_seq from {:08X}", conn->acked_server_seq);
          uint32_t ack_delta = fi.tcp->ack_num - conn->acked_server_seq;
          if (conn->outbound_data_bytes < ack_delta) {
            throw runtime_error("client acknowledged beyond end of sent data");
          }

          conn->drain_outbound_data(ack_delta);
          conn->acked_server_seq += ack_delta;
          conn->resend_push_usecs = DEFAULT_RESEND_PUSH_USECS;
          conn->next_push_max_frame_size = conn->max_frame_size;
          acked_seq_changed = true;

          this->log.debug_f(
              "Removed {:08X} bytes from pending buffer and advanced acked_server_seq to {:08X}",
              ack_delta, conn->acked_server_seq);

        } else if (seq_num_less(fi.tcp->ack_num, conn->acked_server_seq)) {
          throw runtime_error("client sent lower ack num than previous frame");
        }
      }

      if (!conn->server_channel) {
        co_await this->open_server_connection(c, conn);
      }
    }

    if (fi.tcp->flags & (TCPHeader::Flag::RST | TCPHeader::Flag::FIN)) {
      bool is_rst = (fi.tcp->flags & TCPHeader::Flag::RST);
      if (is_rst && (fi.tcp->flags & TCPHeader::Flag::FIN)) {
        throw runtime_error("client sent TCP FIN+RST");
      }

      string conn_str = this->str_for_tcp_connection(c, conn);
      this->log.info_f("Client closed TCP connection {}", conn_str);
      if (conn->server_channel) {
        conn->server_channel->disconnect();
        conn->server_channel.reset();
      }

      // TODO: Are we supposed to send a response to an RST? Here we do, and the
      // client probably just ignores it anyway
      co_await this->send_tcp_frame(c, conn, fi.tcp->flags & (TCPHeader::Flag::RST | TCPHeader::Flag::FIN));

      // Delete the connection object. The unique_ptr destructor flushes the
      // bufferevent, and thereby sends an EOF to the server's end.
      c->tcp_connections.erase(key);
      conn_valid = false;

    } else if (fi.payload_size != 0) {
      // Note: The PSH flag isn't required to be set on all packets that
      // contain data. The PSH flag just means "tell the application that data
      // is available", so some senders only set the PSH flag on the last frame
      // of a large segment of data, since the application wouldn't be able to
      // process the segment until all of it is available. newserv can handle
      // incomplete commands, so we just ignore the PSH flag and forward any
      // data to the server immediately (hence the lack of a flag check in the
      // above condition).

      string conn_str = this->log.should_log(phosg::LogLevel::L_WARNING)
          ? this->str_for_tcp_connection(c, conn)
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
        this->log.warning_f(
            "Client sent out-of-order sequence number (expected {:08X}, received {:08X}, 0x{:X} data bytes)",
            conn->next_client_seq, fi.tcp->seq_num, fi.payload_size);
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
          was_logged = this->log.debug_f(
              "Client sent data on TCP connection {}, overlapping existing ack'ed data (0x{:X} bytes ignored)",
              conn_str, payload_skip_bytes);
        } else {
          was_logged = this->log.debug_f("Client sent data on TCP connection {}", conn_str);
        }
        if (was_logged) {
          phosg::print_data(stderr, payload, payload_size);
        }

        // Send the new data to the server
        if (!conn->server_channel) {
          this->log.warning_f("Client sent data on TCP connection {}, but server channel is missing",
              conn_str);
        } else if (!conn->server_channel->connected()) {
          this->log.warning_f("Client sent data on TCP connection {}, but server channel is disconnected",
              conn_str);
        } else {
          conn->server_channel->add_inbound_data(payload, payload_size);
        }

        // Update the sequence number and stats
        conn->next_client_seq += payload_size;
        conn->bytes_received += payload_size;
        if (conn->next_client_seq < payload_size) {
          this->log.warning_f("Client sequence number has wrapped (next={:08X}, bytes={:X})",
              fi.tcp->seq_num, payload_size);
        }
      }

      // Send an ACK
      co_await this->send_tcp_frame(c, conn);
      this->log.debug_f("Sent PSH ACK on {} (acked_server_seq={:08X}, next_client_seq={:08X}, bytes_received=0x{:X})",
          conn_str, conn->acked_server_seq, conn->next_client_seq, conn->bytes_received);
    }

    if (conn_valid && acked_seq_changed) {
      // Try to send some more data if the client is waiting on it
      this->schedule_send_pending_push_frame(conn, 0);
    }
  }
}

void IPStackSimulator::schedule_send_pending_push_frame(shared_ptr<IPSSClient::TCPConnection> conn, uint64_t delay_usecs) {
  conn->resend_push_timer.expires_after(std::chrono::microseconds(delay_usecs));
  conn->resend_push_timer.async_wait([wconn = weak_ptr<IPSSClient::TCPConnection>(conn)](std::error_code ec) {
    if (ec) {
      return;
    }
    auto conn = wconn.lock();
    if (!conn) {
      return;
    }
    auto c = conn->client.lock();
    if (!c) {
      return;
    }
    auto sim = c->sim.lock();
    if (!sim) {
      return;
    }
    asio::co_spawn(*sim->get_io_context(), sim->send_pending_push_frame(c, conn), asio::detached);
  });
}

asio::awaitable<void> IPStackSimulator::send_pending_push_frame(
    shared_ptr<IPSSClient> c, shared_ptr<IPSSClient::TCPConnection> conn) {
  if (!conn->outbound_data_bytes) {
    if (!conn->server_channel || !conn->server_channel->connected()) {
      co_await this->close_tcp_connection(c, conn);
    }
    co_return;
  }

  size_t bytes_to_send = min<size_t>(conn->outbound_data_bytes, conn->next_push_max_frame_size);
  if (c->protocol == VirtualNetworkProtocol::HDLC_TAPSERVER) {
    // There is a bug in Dolphin's modem implementation (which I wrote, so it's
    // my fault) that causes commands to be dropped when too much data is sent
    // at once. To work around this, we only send up to 200 bytes in each push
    // frame.
    bytes_to_send = min<size_t>(bytes_to_send, 200);
  }

  this->log.debug_f("Sending PSH frame with seq_num {:08X}, 0x{:X}/0x{:X} data bytes",
      conn->acked_server_seq, bytes_to_send, conn->outbound_data_bytes);

  conn->linearize_outbound_data(bytes_to_send);
  if (conn->outbound_data.empty() || conn->outbound_data.front().size() < bytes_to_send) {
    // This should never happen because bytes_to_send should always be less
    // than or equal to conn->outbound_data_bytes, which itself should be equal
    // to the number of bytes that can be linearized
    throw logic_error("failed to linearize enough bytes before sending TCP PSH");
  }
  co_await this->send_tcp_frame(c, conn, TCPHeader::Flag::PSH, conn->outbound_data.front().data(), bytes_to_send);
  conn->awaiting_ack = true;

  // Schedule the timer for sending another PSH, in case the client doesn't
  // respond quickly enough
  this->schedule_send_pending_push_frame(conn, conn->resend_push_usecs);

  // If the client isn't responding to our PSHes, back off exponentially up to
  // a limit of 5 seconds between PSH frames. This window is reset when
  // acked_server_seq changes (that is, when the client has acknowledged any new
  // data). It seems some situations cause GameCube clients to drop packets more
  // often; to alleviate this, we also try to resend less data.
  conn->resend_push_usecs *= 2;
  if (conn->resend_push_usecs > 5000000) {
    conn->resend_push_usecs = 5000000;
  }
  conn->next_push_max_frame_size = max<size_t>(0x100, conn->next_push_max_frame_size - 0x100);
}

asio::awaitable<void> IPStackSimulator::send_tcp_frame(
    shared_ptr<IPSSClient> c,
    shared_ptr<IPSSClient::TCPConnection> conn,
    uint16_t flags,
    const void* payload_data,
    size_t payload_size) {
  if (!payload_data != !(flags & TCPHeader::Flag::PSH)) {
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
  ipv4.src_addr = conn->server_addr;
  ipv4.dest_addr = c->ipv4_addr;

  TCPHeader tcp;
  tcp.src_port = conn->server_port;
  tcp.dest_port = conn->client_port;
  tcp.seq_num = conn->acked_server_seq;
  tcp.ack_num = conn->next_client_seq;
  tcp.flags = (5 << 12) | TCPHeader::Flag::ACK | flags;
  tcp.window = 0x1000;
  tcp.urgent_ptr = 0;
  // tcp.checksum filled in later

  ipv4.size = sizeof(IPv4Header) + sizeof(TCPHeader) + payload_size;
  ipv4.checksum = FrameInfo::computed_ipv4_header_checksum(ipv4);
  tcp.checksum = FrameInfo::computed_tcp4_checksum(ipv4, tcp, payload_data, payload_size);

  phosg::StringWriter w;
  w.put(ipv4);
  w.put(tcp);
  if (payload_data) {
    w.write(payload_data, payload_size);
  }

  co_await this->send_layer3_frame(c, FrameInfo::Protocol::IPV4, w.str());
}

asio::awaitable<void> IPStackSimulator::open_server_connection(
    shared_ptr<IPSSClient> c, shared_ptr<IPSSClient::TCPConnection> conn) {
  if (conn->server_channel) {
    throw logic_error("server connection is already open");
  }

  string conn_str = this->str_for_tcp_connection(c, conn);

  // Figure out which logical port the connection should go to
  auto port_config_it = this->state->number_to_port_config.find(conn->server_port);
  if (port_config_it == this->state->number_to_port_config.end()) {
    this->log.error_f("TCP connection {} is to undefined port {}", conn_str, conn->server_port);
    co_await this->close_tcp_connection(c, conn);
    co_return;
  }
  const auto& port_config = port_config_it->second;

  conn->server_channel = make_shared<IPSSChannel>(this->shared_from_this(), c, conn, port_config->version, 1);

  if (!this->state->game_server.get()) {
    this->log.error_f("No server available for TCP connection {}", conn_str);
    co_await this->close_tcp_connection(c, conn);
    co_return;
  } else {
    this->state->game_server->connect_channel(conn->server_channel, conn->server_port, port_config->behavior);
    this->log.info_f("Connected TCP connection {} to game server", conn_str);
  }
}

asio::awaitable<void> IPStackSimulator::close_tcp_connection(
    shared_ptr<IPSSClient> c, shared_ptr<IPSSClient::TCPConnection> conn) {
  // Send an RST to the client. This is kind of rude (we really should use FIN)
  // but the PSO network stack always sends an RST to us when disconnecting, so
  // whatever
  co_await this->send_tcp_frame(c, conn, TCPHeader::Flag::RST);

  // Delete the connection object
  string conn_str = this->str_for_tcp_connection(c, conn);
  this->log.info_f("Server closed TCP connection {}", conn_str);
  c->tcp_connections.erase(this->tcp_conn_key_for_connection(conn));
}

std::shared_ptr<IPSSClient> IPStackSimulator::create_client(
    std::shared_ptr<IPSSSocket> listen_sock, asio::ip::tcp::socket&& client_sock) {
  uint32_t addr = ipv4_addr_for_asio_addr(client_sock.remote_endpoint().address());
  if (this->state->banned_ipv4_ranges->check(addr)) {
    client_sock.close();
    return nullptr;
  }

  uint64_t network_id = this->next_network_id++;
  this->log.info_f("Virtual network N-{:X} connected via {}", network_id, listen_sock->name);
  return make_shared<IPSSClient>(this->shared_from_this(), network_id, listen_sock->protocol, std::move(client_sock));
}

asio::awaitable<void> IPStackSimulator::handle_tapserver_client(std::shared_ptr<IPSSClient> c) {
  for (;;) {
    le_uint16_t frame_size;
    co_await asio::async_read(c->sock, asio::buffer(&frame_size, sizeof(frame_size)), asio::use_awaitable);
    string frame(frame_size, '\0');
    co_await asio::async_read(c->sock, asio::buffer(frame.data(), frame.size()), asio::use_awaitable);

    if (c->protocol == VirtualNetworkProtocol::HDLC_TAPSERVER) {
      frame.resize(unescape_hdlc_frame_inplace(frame.data(), frame.size()));
    }

    try {
      co_await this->on_client_frame(c, frame.data(), frame.size());
    } catch (const exception& e) {
      if (this->log.warning_f("Failed to process frame: {}", e.what())) {
        phosg::print_data(stderr, frame);
      }
    }

    c->reschedule_idle_timeout();
  }
}

asio::awaitable<void> IPStackSimulator::handle_hdlc_raw_client(std::shared_ptr<IPSSClient> c) {
  std::string buffer(0x1000, 0);
  size_t buffer_bytes = 0;
  for (;;) {
    size_t req_buffer_size = buffer_bytes + 0x400;
    if (buffer.size() < req_buffer_size) {
      buffer.resize(req_buffer_size);
    }

    auto buf = asio::buffer(buffer.data() + buffer_bytes, buffer.size() - buffer_bytes);
    buffer_bytes += co_await c->sock.async_read_some(buf, asio::use_awaitable);

    // Process as many packets as possible
    size_t frame_start_offset = 0;
    while (buffer.size() > frame_start_offset) {
      if (buffer[frame_start_offset] != 0x7E) {
        throw runtime_error("HDLC frame does not begin with 7E");
      }
      size_t frame_end_offset = buffer.find(0x7E, frame_start_offset + 1);
      if (frame_end_offset == string::npos) {
        break;
      }
      frame_end_offset++;

      // Unescaping a frame can't make it longer, so we just do it in-place
      void* frame_data = buffer.data() + frame_start_offset;
      size_t unescaped_size = unescape_hdlc_frame_inplace(frame_data, frame_end_offset - frame_start_offset);

      try {
        co_await this->on_client_frame(c, frame_data, unescaped_size);
      } catch (const exception& e) {
        if (this->log.warning_f("Failed to process frame: {}", e.what())) {
          phosg::print_data(stderr, frame_data, unescaped_size);
        }
      }

      frame_start_offset = frame_end_offset;
    }

    // Delete the processed packets from the beginning of the buffer
    if (frame_start_offset > buffer_bytes) {
      throw logic_error("frame start offset is beyond buffer bounds");
    } else if (frame_start_offset == buffer_bytes) {
      buffer_bytes = 0;
    } else if (frame_start_offset > 0) {
      memcpy(buffer.data(), buffer.data() + frame_start_offset, buffer_bytes - frame_start_offset);
      buffer_bytes -= frame_start_offset;
    }

    // Reset the idle timer, since the client has sent something valid
    c->reschedule_idle_timeout();
  }
}

asio::awaitable<void> IPStackSimulator::handle_client(std::shared_ptr<IPSSClient> c) {
  switch (c->protocol) {
    case VirtualNetworkProtocol::ETHERNET_TAPSERVER:
    case VirtualNetworkProtocol::HDLC_TAPSERVER:
      co_await this->handle_tapserver_client(c);
      break;
    case VirtualNetworkProtocol::HDLC_RAW:
      co_await this->handle_hdlc_raw_client(c);
      break;
    default:
      throw std::logic_error("unknown virtual network protocol");
  }
}

asio::awaitable<void> IPStackSimulator::destroy_client(std::shared_ptr<IPSSClient> c) {
  this->log.info_f("Virtual network N-{:X} disconnected ({} TCP connections to close)", c->network_id, c->tcp_connections.size());
  for (const auto& [conn_id, conn] : c->tcp_connections) {
    if (conn->server_channel) {
      this->log.info_f("Closing TCP connection {:016X} on N-{:X}", conn_id, c->network_id);
      conn->server_channel->disconnect();
      conn->server_channel.reset();
    } else {
      this->log.info_f("TCP connection {:016X} on N-{:X} has no server channel", conn_id, c->network_id);
    }
  }

  co_return;
}
