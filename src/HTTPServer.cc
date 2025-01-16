#include "HTTPServer.hh"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>
#include <inttypes.h>
#include <stdlib.h>

#include <phosg/Network.hh>
#include <string>
#include <vector>

#include "EventUtils.hh"
#include "Loggers.hh"
#include "ProxyServer.hh"
#include "Revision.hh"
#include "Server.hh"
#include "ShellCommands.hh"

using namespace std;

const unordered_map<int, const char*> HTTPServer::explanation_for_response_code({
    {100, "Continue"},
    {101, "Switching Protocols"},
    {102, "Processing"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unathorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Requested Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I\'m a Teapot"},
    {420, "Enhance Your Calm"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {444, "No Response"},
    {449, "Retry With"},
    {451, "Unavailable For Legal Reasons"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {509, "Bandwidth Limit Exceeded"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"},
    {598, "Network Read Timeout Error"},
    {599, "Network Connect Timeout Error"},
});

HTTPServer::http_error::http_error(int code, const string& what)
    : runtime_error(what),
      code(code) {}

void HTTPServer::send_response(struct evhttp_request* req, int code, const char* content_type, struct evbuffer* b) {
  struct evkeyvalq* headers = evhttp_request_get_output_headers(req);
  evhttp_add_header(headers, "Content-Type", content_type);
  evhttp_add_header(headers, "Server", "newserv");
  evhttp_send_reply(req, code, explanation_for_response_code.at(code), b);
}

void HTTPServer::send_response(struct evhttp_request* req, int code, const char* content_type, const char* fmt, ...) {
  unique_ptr<struct evbuffer, void (*)(struct evbuffer*)> out_buffer(evbuffer_new(), evbuffer_free);
  va_list va;
  va_start(va, fmt);
  evbuffer_add_vprintf(out_buffer.get(), fmt, va);
  va_end(va);
  HTTPServer::send_response(req, code, content_type, out_buffer.get());
}

unordered_multimap<string, string> HTTPServer::parse_url_params(const string& query) {
  unordered_multimap<string, string> params;
  if (query.empty()) {
    return params;
  }
  for (auto it : phosg::split(query, '&')) {
    size_t first_equals = it.find('=');
    if (first_equals != string::npos) {
      string value(it, first_equals + 1);

      size_t write_offset = 0, read_offset = 0;
      for (; read_offset < value.size(); write_offset++) {
        if ((value[read_offset] == '%') && (read_offset < value.size() - 2)) {
          value[write_offset] =
              static_cast<char>(phosg::value_for_hex_char(value[read_offset + 1]) << 4) |
              static_cast<char>(phosg::value_for_hex_char(value[read_offset + 2]));
          read_offset += 3;
        } else if (value[write_offset] == '+') {
          value[write_offset] = ' ';
          read_offset++;
        } else {
          value[write_offset] = value[read_offset];
          read_offset++;
        }
      }
      value.resize(write_offset);

      params.emplace(piecewise_construct, forward_as_tuple(it, 0, first_equals), forward_as_tuple(value));
    } else {
      params.emplace(it, "");
    }
  }
  return params;
}

unordered_map<string, string> HTTPServer::parse_url_params_unique(const string& query) {
  unordered_map<string, string> ret;
  for (const auto& it : HTTPServer::parse_url_params(query)) {
    ret.emplace(it.first, std::move(it.second));
  }
  return ret;
}

const string& HTTPServer::get_url_param(
    const unordered_multimap<string, string>& params, const string& key, const string* _default) {

  auto range = params.equal_range(key);
  if (range.first == range.second) {
    if (!_default) {
      throw out_of_range("URL parameter " + key + " not present");
    }
    return *_default;
  }

  return range.first->second;
}

HTTPServer::HTTPServer(shared_ptr<ServerState> state, shared_ptr<struct event_base> shared_base)
    : state(state) {
  if (!shared_base) {
    this->base.reset(event_base_new(), event_base_free);
  } else {
    this->base = shared_base;
  }
  this->http.reset(evhttp_new(this->base.get()), evhttp_free);
  evhttp_set_gencb(this->http.get(), this->dispatch_handle_request, this);
  if (!shared_base) {
    this->th = thread(&HTTPServer::thread_fn, this);
  }
}

void HTTPServer::listen(const string& socket_path) {
  int fd = phosg::listen(socket_path, 0, SOMAXCONN);
  server_log.info("Listening on Unix socket %s on fd %d (HTTP)", socket_path.c_str(), fd);
  this->add_socket(fd);
}

void HTTPServer::listen(const string& addr, int port) {
  if (port == 0) {
    this->listen(addr);
  } else {
    int fd = phosg::listen(addr, port, SOMAXCONN);
    string netloc_str = phosg::render_netloc(addr, port);
    server_log.info("Listening on TCP interface %s on fd %d (HTTP)", netloc_str.c_str(), fd);
    this->add_socket(fd);
  }
}

void HTTPServer::listen(int port) {
  this->listen("", port);
}

void HTTPServer::add_socket(int fd) {
  evhttp_accept_socket(this->http.get(), fd);
}

void HTTPServer::schedule_stop() {
  event_base_loopexit(this->base.get(), nullptr);
}

void HTTPServer::wait_for_stop() {
  this->th.join();
}

HTTPServer::WebsocketClient::WebsocketClient(struct evhttp_connection* conn)
    : conn(conn),
      bev(evhttp_connection_get_bufferevent(this->conn)),
      pending_opcode(0xFF),
      last_communication_time(phosg::now()) {}

HTTPServer::WebsocketClient::~WebsocketClient() {
  evhttp_connection_free(this->conn);
}

void HTTPServer::WebsocketClient::reset_pending_frame() {
  this->pending_opcode = 0xFF;
  this->pending_data.clear();
}

shared_ptr<HTTPServer::WebsocketClient> HTTPServer::enable_websockets(struct evhttp_request* req) {
  if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
    return nullptr;
  }

  struct evkeyvalq* in_headers = evhttp_request_get_input_headers(req);
  const char* connection_header = evhttp_find_header(in_headers, "Connection");
  if (!connection_header || strcasecmp(connection_header, "upgrade")) {
    return nullptr;
  }
  const char* upgrade_header = evhttp_find_header(in_headers, "Upgrade");
  if (!upgrade_header || strcasecmp(upgrade_header, "websocket")) {
    return nullptr;
  }
  const char* sec_websocket_key_header = evhttp_find_header(in_headers, "Sec-WebSocket-Key");
  if (!sec_websocket_key_header) {
    return nullptr;
  }

  // Note: it's important that we make a copy of this header's value since
  // we're about to free the original
  string sec_websocket_key = sec_websocket_key_header;
  string sec_websocket_accept_data = sec_websocket_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  string sec_websocket_accept = phosg::base64_encode(phosg::sha1(sec_websocket_accept_data));

  // Hijack the bufferevent since it's no longer handling HTTP at all
  struct evhttp_connection* conn = evhttp_request_get_connection(req);
  struct bufferevent* bev = evhttp_connection_get_bufferevent(conn);
  bufferevent_setcb(bev, &this->dispatch_on_websocket_read, NULL, &this->dispatch_on_websocket_error, this);
  bufferevent_enable(bev, EV_READ | EV_WRITE);

  // Send the HTTP reply, which enables websockets
  struct evbuffer* out_buf = bufferevent_get_output(bev);
  evbuffer_add_printf(out_buf, "HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
\r\n",
      sec_websocket_accept.c_str());

  return this->bev_to_websocket_client.emplace(bev, new WebsocketClient(conn)).first->second;
}

void HTTPServer::dispatch_on_websocket_read(struct bufferevent* bev, void* ctx) {
  reinterpret_cast<HTTPServer*>(ctx)->on_websocket_read(bev);
}

void HTTPServer::dispatch_on_websocket_error(struct bufferevent* bev, short events, void* ctx) {
  reinterpret_cast<HTTPServer*>(ctx)->on_websocket_error(bev, events);
}

void HTTPServer::on_websocket_read(struct bufferevent* bev) {
  struct evbuffer* in_buf = bufferevent_get_input(bev);

  for (;;) {
    // We need at most 10 bytes to determine if there's a valid frame, or as
    // little as 2
    string header_data(10, '\0');
    ssize_t bytes_read = evbuffer_copyout(in_buf, const_cast<char*>(header_data.data()), header_data.size());

    if (bytes_read < 2) {
      break; // Full header not yet available
    }

    // Get the payload size
    bool has_mask = header_data[1] & 0x80;
    size_t header_size = 2;
    size_t payload_size = header_data[1] & 0x7F;
    if (payload_size == 0x7F) {
      if (bytes_read < 10) {
        break; // Full 64-bit header not yet available
      }
      payload_size = phosg::bswap64(*reinterpret_cast<const uint64_t*>(&header_data[2]));
      header_size = 10;
    } else if (payload_size == 0x7E) {
      if (bytes_read < 4) {
        break; // Full 16-bit size header not yet available
      }
      payload_size = phosg::bswap16(*reinterpret_cast<const uint16_t*>(&header_data[2]));
      header_size = 4;
    }
    if (evbuffer_get_length(in_buf) < header_size + payload_size) {
      break; // Full message not yet available
    }

    // Full message is available; skip the header bytes (we already read them)
    // and read the masking key if needed
    evbuffer_drain(in_buf, header_size);
    uint8_t mask_key[4];
    if (has_mask) {
      evbuffer_remove(in_buf, mask_key, 4);
    }

    shared_ptr<WebsocketClient> c = this->bev_to_websocket_client.at(bev);
    c->last_communication_time = phosg::now();

    // Read and unmask message data
    string payload(payload_size, '\0');
    evbuffer_remove(in_buf, const_cast<char*>(payload.data()), payload_size);
    if (has_mask) {
      for (size_t x = 0; x < payload_size; x++) {
        payload[x] ^= mask_key[x & 3];
      }
    }

    // If the current message is a control message, respond appropriately
    // (these can be sent in the middle of fragmented messages)
    uint8_t opcode = header_data[0] & 0x0F;
    if (opcode & 0x08) {
      if (opcode == 0x0A) {
        // Ping response; ignore it

      } else if (opcode == 0x08) {
        // Close message
        this->send_websocket_message(bev, payload, 0x08);
        this->disconnect_websocket_client(bev);

      } else if (opcode == 0x09) {
        // Ping message
        this->send_websocket_message(bev, payload, 0x0A);

      } else {
        // Unknown control message type
        this->disconnect_websocket_client(bev);
      }
      break;
    }

    // If there's an existing pending message, the current message's opcode
    // should be zero; if there's no pending message, it must not be zero
    if ((c->pending_opcode != 0xFF) == (opcode != 0)) {
      this->disconnect_websocket_client(bev);
      break;
    }

    // At this point, we have read a full message; we must not break out of
    // this loop in case there are further messages available.

    // Save the message opcode, if present, and append the frame data
    if (opcode) {
      c->pending_opcode = opcode;
    }
    c->pending_data += payload;

    // If the FIN bit is set, then the frame is complete - append the payload
    // to any pending payloads and call the message handler. If the FIN bit
    // isn't set, we need to receive at least one continuation frame to
    // complete the message.
    if (header_data[0] & 0x80) {
      this->handle_websocket_message(c, c->pending_opcode, c->pending_data);
      c->reset_pending_frame();
    }
  }
}

void HTTPServer::on_websocket_error(struct bufferevent* bev, short events) {
  if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
    this->disconnect_websocket_client(bev);
  }
}

void HTTPServer::disconnect_websocket_client(struct bufferevent* bev) {
  auto it = this->bev_to_websocket_client.find(bev);
  this->handle_websocket_disconnect(it->second);
  this->bev_to_websocket_client.erase(it);
}

void HTTPServer::send_websocket_message(struct bufferevent* bev,
    const string& message, uint8_t opcode) {
  string header;
  header.push_back(0x80 | (opcode & 0x0F));
  if (message.size() > 65535) {
    header.push_back(0x7F);
    header.resize(10);
    *reinterpret_cast<uint64_t*>(const_cast<char*>(header.data() + 2)) = phosg::bswap64(message.size());
  } else if (message.size() > 0x7D) {
    header.push_back(0x7E);
    header.resize(4);
    *reinterpret_cast<uint16_t*>(const_cast<char*>(header.data() + 2)) = phosg::bswap16(message.size());
  } else {
    header.push_back(message.size());
  }

  struct evbuffer* out_buf = bufferevent_get_output(bev);
  evbuffer_add(out_buf, header.data(), header.size());
  evbuffer_add(out_buf, message.data(), message.size());
}

void HTTPServer::send_websocket_message(shared_ptr<WebsocketClient> c, const string& message, uint8_t opcode) {
  this->send_websocket_message(c->bev, message, opcode);
}

void HTTPServer::handle_websocket_message(shared_ptr<WebsocketClient>, uint8_t, const string&) {
  // Currently we just ignore any messages from the client
}

void HTTPServer::handle_websocket_disconnect(shared_ptr<WebsocketClient> c) {
  this->rare_drop_subscribers.erase(c);
}

void HTTPServer::send_rare_drop_notification(shared_ptr<const phosg::JSON> message) {
  forward_to_event_thread(this->base, [this, message]() -> void {
    if (this->rare_drop_subscribers.empty()) {
      return;
    }
    string serialized = message->serialize();
    for (const auto& c : this->rare_drop_subscribers) {
      this->send_websocket_message(c, serialized);
    }
  });
}

void HTTPServer::dispatch_handle_request(struct evhttp_request* req, void* ctx) {
  reinterpret_cast<HTTPServer*>(ctx)->handle_request(req);
}

phosg::JSON HTTPServer::generate_server_version_st() {
  return phosg::JSON::dict({
      {"ServerType", "newserv"},
      {"BuildTime", BUILD_TIMESTAMP},
      {"BuildTimeStr", phosg::format_time(BUILD_TIMESTAMP)},
      {"Revision", GIT_REVISION_HASH},
  });
}

phosg::JSON HTTPServer::generate_client_config_json_st(const Client::Config& config) {
  const char* drop_notifications_mode = "unknown";
  switch (config.get_drop_notification_mode()) {
    case Client::ItemDropNotificationMode::NOTHING:
      drop_notifications_mode = "off";
      break;
    case Client::ItemDropNotificationMode::RARES_ONLY:
      drop_notifications_mode = "rare";
      break;
    case Client::ItemDropNotificationMode::ALL_ITEMS:
      drop_notifications_mode = "on";
      break;
    case Client::ItemDropNotificationMode::ALL_ITEMS_INCLUDING_MESETA:
      drop_notifications_mode = "every";
      break;
  }

  auto ret = phosg::JSON::dict({
      {"SpecificVersion", config.specific_version},
      {"SwitchAssistEnabled", (config.check_flag(Client::Flag::SWITCH_ASSIST_ENABLED) ? true : false)},
      {"InfiniteHPEnabled", (config.check_flag(Client::Flag::INFINITE_HP_ENABLED) ? true : false)},
      {"InfiniteTPEnabled", (config.check_flag(Client::Flag::INFINITE_TP_ENABLED) ? true : false)},
      {"DropNotificationMode", drop_notifications_mode},
      {"DebugEnabled", (config.check_flag(Client::Flag::DEBUG_ENABLED) ? true : false)},
      {"ProxySaveFilesEnabled", (config.check_flag(Client::Flag::PROXY_SAVE_FILES) ? true : false)},
      {"ProxyChatCommandsEnabled", (config.check_flag(Client::Flag::PROXY_CHAT_COMMANDS_ENABLED) ? true : false)},
      {"ProxyPlayerNotificationsEnabled", (config.check_flag(Client::Flag::PROXY_PLAYER_NOTIFICATIONS_ENABLED) ? true : false)},
      {"ProxySuppressClientPings", (config.check_flag(Client::Flag::PROXY_SUPPRESS_CLIENT_PINGS) ? true : false)},
      {"ProxyEp3InfiniteMesetaEnabled", (config.check_flag(Client::Flag::PROXY_EP3_INFINITE_MESETA_ENABLED) ? true : false)},
      {"ProxyEp3InfiniteTimeEnabled", (config.check_flag(Client::Flag::PROXY_EP3_INFINITE_TIME_ENABLED) ? true : false)},
      {"ProxyBlockFunctionCalls", (config.check_flag(Client::Flag::PROXY_BLOCK_FUNCTION_CALLS) ? true : false)},
      {"ProxyEp3UnmaskWhispers", (config.check_flag(Client::Flag::PROXY_EP3_UNMASK_WHISPERS) ? true : false)},
  });
  ret.emplace("OverrideRandomSeed", config.check_flag(Client::Flag::USE_OVERRIDE_RANDOM_SEED) ? config.override_random_seed : phosg::JSON(nullptr));
  ret.emplace("OverrideSectionID", (config.override_section_id != 0xFF) ? config.override_section_id : phosg::JSON(nullptr));
  ret.emplace("OverrideLobbyEvent", (config.override_lobby_event != 0xFF) ? config.override_lobby_event : phosg::JSON(nullptr));
  ret.emplace("OverrideLobbyNumber", (config.override_lobby_number != 0x80) ? config.override_lobby_number : phosg::JSON(nullptr));
  return ret;
}

phosg::JSON HTTPServer::generate_account_json_st(shared_ptr<const Account> a) {
  auto dc_nte_licenses_json = phosg::JSON::list();
  for (const auto& it : a->dc_nte_licenses) {
    dc_nte_licenses_json.emplace_back(it.first);
  }
  auto dc_licenses_json = phosg::JSON::list();
  for (const auto& it : a->dc_licenses) {
    dc_licenses_json.emplace_back(it.first);
  }
  auto pc_licenses_json = phosg::JSON::list();
  for (const auto& it : a->pc_licenses) {
    pc_licenses_json.emplace_back(it.first);
  }
  auto gc_licenses_json = phosg::JSON::list();
  for (const auto& it : a->gc_licenses) {
    gc_licenses_json.emplace_back(it.first);
  }
  auto xb_licenses_json = phosg::JSON::list();
  for (const auto& it : a->xb_licenses) {
    xb_licenses_json.emplace_back(it.first);
  }
  auto bb_licenses_json = phosg::JSON::list();
  for (const auto& it : a->bb_licenses) {
    bb_licenses_json.emplace_back(it.first);
  }
  auto auto_patches_json = phosg::JSON::list();
  for (const auto& it : a->auto_patches_enabled) {
    auto_patches_json.emplace_back(it);
  }
  return phosg::JSON::dict({
      {"AccountID", a->account_id},
      {"Flags", a->flags},
      {"BanEndTime", a->ban_end_time ? a->ban_end_time : phosg::JSON(nullptr)},
      {"Ep3CurrentMeseta", a->ep3_current_meseta},
      {"Ep3TotalMesetaEarned", a->ep3_total_meseta_earned},
      {"BBTeamID", a->bb_team_id},
      {"LastPlayerName", a->last_player_name},
      {"AutoReplyMessage", a->auto_reply_message},
      {"IsTemporary", a->is_temporary},
      {"DCNTELicenses", std::move(dc_nte_licenses_json)},
      {"DCLicenses", std::move(dc_licenses_json)},
      {"PCLicenses", std::move(pc_licenses_json)},
      {"GCLicenses", std::move(gc_licenses_json)},
      {"XBLicenses", std::move(xb_licenses_json)},
      {"BBLicenses", std::move(bb_licenses_json)},
      {"AutoPatchesEnabled", std::move(auto_patches_json)},
  });
};

phosg::JSON HTTPServer::generate_game_client_json_st(shared_ptr<const Client> c, shared_ptr<const ItemNameIndex> item_name_index) {
  auto ret = phosg::JSON::dict({
      {"ID", c->id},
      {"RemoteAddress", phosg::render_sockaddr_storage(c->channel.remote_addr)},
      {"Version", phosg::name_for_enum(c->version())},
      {"SubVersion", c->sub_version},
      {"Config", HTTPServer::generate_client_config_json_st(c->config)},
      {"Language", name_for_language_code(c->language())},
      {"LocationX", c->pos.x.load()},
      {"LocationZ", c->pos.z.load()},
      {"LocationFloor", c->floor},
      {"CanChat", c->can_chat},
  });
  ret.emplace("Account", c->login ? HTTPServer::generate_account_json_st(c->login->account) : phosg::JSON(nullptr));
  auto l = c->lobby.lock();
  if (l) {
    ret.emplace("LobbyID", l->lobby_id);
    ret.emplace("LobbyClientID", c->lobby_client_id);
  }
  if (c->version() == Version::BB_V4) {
    ret.emplace("BBCharacterIndex", c->bb_character_index);
  }
  auto p = c->character(false, false);
  if (p) {
    if (!is_ep3(c->version())) {
      if (c->version() != Version::DC_NTE) {
        ret.emplace("InventoryLanguage", name_for_language_code(p->inventory.language));
        ret.emplace("NumHPMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::HP));
        ret.emplace("NumTPMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::TP));
        if (!is_v1_or_v2(c->version())) {
          ret.emplace("NumPowerMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::POWER));
          ret.emplace("NumDefMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::DEF));
          ret.emplace("NumMindMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::MIND));
          ret.emplace("NumEvadeMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::EVADE));
          ret.emplace("NumLuckMaterialsUsed", p->get_material_usage(PSOBBCharacterFile::MaterialType::LUCK));
        }
      }
      phosg::JSON items_json = phosg::JSON::list();
      for (size_t z = 0; z < p->inventory.num_items; z++) {
        const auto& item = p->inventory.items[z];
        auto item_dict = phosg::JSON::dict({
            {"Flags", item.flags.load()},
            {"Data", item.data.hex()},
            {"ItemID", item.data.id.load()},
        });
        if (item_name_index) {
          item_dict.emplace("Description", item_name_index->describe_item(item.data, false));
        }
        items_json.emplace_back(std::move(item_dict));
      }
      ret.emplace("InventoryItems", std::move(items_json));
      ret.emplace("ATP", p->disp.stats.char_stats.atp.load());
      ret.emplace("MST", p->disp.stats.char_stats.mst.load());
      ret.emplace("EVP", p->disp.stats.char_stats.evp.load());
      ret.emplace("HP", p->disp.stats.char_stats.hp.load());
      ret.emplace("DFP", p->disp.stats.char_stats.dfp.load());
      ret.emplace("ATA", p->disp.stats.char_stats.ata.load());
      ret.emplace("LCK", p->disp.stats.char_stats.lck.load());
      ret.emplace("EXP", p->disp.stats.experience.load());
      ret.emplace("Meseta", p->disp.stats.meseta.load());
      auto tech_levels_json = phosg::JSON::dict();
      for (size_t z = 0; z < 0x13; z++) {
        auto level = p->get_technique_level(z);
        tech_levels_json.emplace(name_for_technique(z), (level != 0xFF) ? level : phosg::JSON(nullptr));
      }
      ret.emplace("TechniqueLevels", std::move(tech_levels_json));
    }
    ret.emplace("Height", p->disp.stats.height.load());
    ret.emplace("Level", p->disp.stats.level.load());
    ret.emplace("NameColor", p->disp.visual.name_color.load());
    ret.emplace("ExtraModel", (p->disp.visual.validation_flags & 2) ? p->disp.visual.extra_model : phosg::JSON(nullptr));
    ret.emplace("SectionID", name_for_section_id(p->disp.visual.section_id));
    ret.emplace("CharClass", name_for_char_class(p->disp.visual.char_class));
    ret.emplace("Costume", p->disp.visual.costume.load());
    ret.emplace("Skin", p->disp.visual.skin.load());
    ret.emplace("Face", p->disp.visual.face.load());
    ret.emplace("Head", p->disp.visual.head.load());
    ret.emplace("Hair", p->disp.visual.hair.load());
    ret.emplace("HairR", p->disp.visual.hair_r.load());
    ret.emplace("HairG", p->disp.visual.hair_g.load());
    ret.emplace("HairB", p->disp.visual.hair_b.load());
    ret.emplace("ProportionX", p->disp.visual.proportion_x.load());
    ret.emplace("ProportionY", p->disp.visual.proportion_y.load());

    ret.emplace("Name", p->disp.name.decode(c->language()));
    ret.emplace("PlayTimeSeconds", p->play_time_seconds.load());

    ret.emplace("AutoReply", p->auto_reply.decode(c->language()));
    ret.emplace("InfoBoard", p->info_board.decode(c->language()));
    auto battle_place_counts = phosg::JSON::list({
        p->battle_records.place_counts[0].load(),
        p->battle_records.place_counts[1].load(),
        p->battle_records.place_counts[2].load(),
        p->battle_records.place_counts[3].load(),
    });
    ret.emplace("BattlePlaceCounts", std::move(battle_place_counts));
    ret.emplace("BattleDisconnectCount", p->battle_records.disconnect_count.load());

    if (!is_ep3(c->version())) {
      auto json_for_challenge_times = []<size_t Count>(const parray<ChallengeTime, Count>& times) -> phosg::JSON {
        auto times_json = phosg::JSON::list();
        for (size_t z = 0; z < times.size(); z++) {
          times_json.emplace_back(times[z].decode());
        }
        return times_json;
      };
      ret.emplace("ChallengeTitleColorXRGB1555", p->challenge_records.title_color.load());
      ret.emplace("ChallengeTimesEp1Online", json_for_challenge_times(p->challenge_records.times_ep1_online));
      ret.emplace("ChallengeTimesEp2Online", json_for_challenge_times(p->challenge_records.times_ep2_online));
      ret.emplace("ChallengeTimesEp1Offline", json_for_challenge_times(p->challenge_records.times_ep1_offline));
      ret.emplace("ChallengeGraveIsEp2", p->challenge_records.grave_is_ep2 ? true : false);
      ret.emplace("ChallengeGraveStageNum", p->challenge_records.grave_stage_num);
      ret.emplace("ChallengeGraveFloor", p->challenge_records.grave_floor);
      ret.emplace("ChallengeGraveDeaths", p->challenge_records.grave_deaths.load());
      {
        uint16_t year = 2000 + ((p->challenge_records.grave_time >> 28) & 0x0F);
        uint8_t month = (p->challenge_records.grave_time >> 24) & 0x0F;
        uint8_t day = (p->challenge_records.grave_time >> 16) & 0xFF;
        uint8_t hour = (p->challenge_records.grave_time >> 8) & 0xFF;
        uint8_t minute = p->challenge_records.grave_time & 0xFF;
        ret.emplace("ChallengeGraveTime", phosg::string_printf("%04hu-%02hhu-%02hhu %02hhu:%02hhu:00", year, month, day, hour, minute));
      }
      string grave_enemy_types;
      if (p->challenge_records.grave_defeated_by_enemy_rt_index) {
        for (EnemyType type : enemy_types_for_rare_table_index(p->challenge_records.grave_is_ep2 ? Episode::EP2 : Episode::EP1, p->challenge_records.grave_defeated_by_enemy_rt_index)) {
          if (!grave_enemy_types.empty()) {
            grave_enemy_types += "/";
          }
          grave_enemy_types += phosg::name_for_enum(type);
        }
      }
      ret.emplace("ChallengeGraveDefeatedByEnemy", std::move(grave_enemy_types));
      ret.emplace("ChallengeGraveX", p->challenge_records.grave_x.load());
      ret.emplace("ChallengeGraveY", p->challenge_records.grave_y.load());
      ret.emplace("ChallengeGraveZ", p->challenge_records.grave_z.load());
      ret.emplace("ChallengeGraveTeam", p->challenge_records.grave_team.decode());
      ret.emplace("ChallengeGraveMessage", p->challenge_records.grave_message.decode());
      ret.emplace("ChallengeAwardStateEp1OnlineFlags", p->challenge_records.ep1_online_award_state.rank_award_flags.load());
      ret.emplace("ChallengeAwardStateEp1OnlineMaxRank", p->challenge_records.ep1_online_award_state.maximum_rank.decode());
      ret.emplace("ChallengeAwardStateEp2OnlineFlags", p->challenge_records.ep2_online_award_state.rank_award_flags.load());
      ret.emplace("ChallengeAwardStateEp2OnlineMaxRank", p->challenge_records.ep2_online_award_state.maximum_rank.decode());
      ret.emplace("ChallengeAwardStateEp1OfflineFlags", p->challenge_records.ep1_offline_award_state.rank_award_flags.load());
      ret.emplace("ChallengeAwardStateEp1OfflineMaxRank", p->challenge_records.ep1_offline_award_state.maximum_rank.decode());
      ret.emplace("ChallengeRankTitle", p->challenge_records.rank_title.decode());
    }
  }
  return ret;
}

phosg::JSON HTTPServer::generate_proxy_client_json_st(shared_ptr<const ProxyServer::LinkedSession> ses) {
  struct LobbyPlayer {
    uint32_t guild_card_number = 0;
    uint64_t xb_user_id = 0;
    std::string name;
    uint8_t language = 0;
    uint8_t section_id = 0;
    uint8_t char_class = 0;
  };
  std::vector<LobbyPlayer> lobby_players;

  auto lobby_players_json = phosg::JSON::list();
  for (size_t z = 0; z < ses->lobby_players.size(); z++) {
    const auto& p = ses->lobby_players[z];
    if (p.guild_card_number) {
      lobby_players_json.emplace_back(phosg::JSON::dict({
          {"GuildCardNumber", p.guild_card_number},
          {"Name", p.name},
          {"Language", name_for_language_code(p.language)},
          {"SectionID", name_for_section_id(p.section_id)},
          {"CharClass", name_for_char_class(p.char_class)},
      }));
      lobby_players_json.back().emplace("XBUserID", p.xb_user_id ? p.xb_user_id : phosg::JSON(nullptr));
    } else {
      lobby_players_json.emplace_back(nullptr);
    }
  }

  auto ret = phosg::JSON::dict({
      {"ID", ses->id},
      {"RemoteClientAddress", phosg::render_sockaddr_storage(ses->client_channel.remote_addr)},
      {"RemoteServerAddress", phosg::render_sockaddr_storage(ses->server_channel.remote_addr)},
      {"LocalPort", ses->local_port},
      {"NextDestination", phosg::render_sockaddr_storage(ses->next_destination)},
      {"Version", phosg::name_for_enum(ses->version())},
      {"SubVersion", ses->sub_version},
      {"Name", ses->character_name},
      {"DCSerialNumber2", ses->serial_number2},
      {"RemoteGuildCardNumber", ses->remote_guild_card_number},
      {"RemoteClientConfigData", phosg::format_data_string(&ses->remote_client_config_data[0], ses->remote_client_config_data.size())},
      {"Config", HTTPServer::generate_client_config_json_st(ses->config)},
      {"Language", name_for_language_code(ses->language())},
      {"LobbyClientID", ses->lobby_client_id},
      {"LeaderClientID", ses->leader_client_id},
      {"LocationX", ses->pos.x.load()},
      {"LocationZ", ses->pos.z.load()},
      {"LocationFloor", ses->floor},
      {"IsInGame", ses->is_in_game},
      {"IsInQuest", ses->is_in_quest},
      {"LobbyEvent", ses->lobby_event},
      {"LobbyDifficulty", name_for_difficulty(ses->lobby_difficulty)},
      {"LobbySectionID", name_for_section_id(ses->lobby_section_id)},
      {"LobbyMode", name_for_mode(ses->lobby_mode)},
      {"LobbyEpisode", name_for_episode(ses->lobby_episode)},
      {"LobbyRandomSeed", ses->lobby_random_seed},
      {"LobbyPlayers", std::move(lobby_players_json)},
  });
  switch (ses->drop_mode) {
    case ProxyServer::LinkedSession::DropMode::DISABLED:
      ret.emplace("DropMode", "none");
      break;
    case ProxyServer::LinkedSession::DropMode::PASSTHROUGH:
      ret.emplace("DropMode", "default");
      break;
    case ProxyServer::LinkedSession::DropMode::INTERCEPT:
      ret.emplace("DropMode", "proxy");
      break;
  }
  ret.emplace("Account", ses->login ? HTTPServer::generate_account_json_st(ses->login->account) : phosg::JSON(nullptr));
  return ret;
}

phosg::JSON HTTPServer::generate_lobby_json_st(shared_ptr<const Lobby> l, shared_ptr<const ItemNameIndex> item_name_index) {
  std::array<std::shared_ptr<Client>, 12> clients;

  auto client_ids_json = phosg::JSON::list();
  for (size_t z = 0; z < l->max_clients; z++) {
    client_ids_json.emplace_back(l->clients[z] ? l->clients[z]->id : phosg::JSON(nullptr));
  }

  auto ret = phosg::JSON::dict({
      {"ID", l->lobby_id},
      {"AllowedVersions", l->allowed_versions},
      {"Event", l->event},
      {"LeaderClientID", l->leader_id},
      {"MaxClients", l->max_clients},
      {"IdleTimeoutUsecs", l->idle_timeout_usecs},
      {"ClientIDs", std::move(client_ids_json)},
      {"IsGame", l->is_game()},
      {"IsPersistent", l->check_flag(Lobby::Flag::PERSISTENT)},
  });

  if (l->is_game()) {
    ret.emplace("CheatsEnabled", l->check_flag(Lobby::Flag::CHEATS_ENABLED));
    ret.emplace("MinLevel", l->min_level + 1);
    ret.emplace("MaxLevel", l->max_level + 1);
    ret.emplace("Episode", name_for_episode(l->episode));
    ret.emplace("HasPassword", !l->password.empty());
    ret.emplace("Name", l->name);
    ret.emplace("RandomSeed", l->random_seed);
    if (l->episode != Episode::EP3) {
      ret.emplace("QuestSelectionInProgress", l->check_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS));
      ret.emplace("QuestInProgress", l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS));
      ret.emplace("JoinableQuestInProgress", l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS));
      ret.emplace("Variations", l->variations.json());
      ret.emplace("SectionID", name_for_section_id(l->effective_section_id()));
      ret.emplace("Mode", name_for_mode(l->mode));
      ret.emplace("Difficulty", name_for_difficulty(l->difficulty));
      ret.emplace("BaseEXPMultiplier", l->base_exp_multiplier);
      ret.emplace("EXPShareMultiplier", l->exp_share_multiplier);
      ret.emplace("AllowedDropModes", l->allowed_drop_modes);
      switch (l->drop_mode) {
        case Lobby::DropMode::DISABLED:
          ret.emplace("DropMode", "none");
          break;
        case Lobby::DropMode::CLIENT:
          ret.emplace("DropMode", "client");
          break;
        case Lobby::DropMode::SERVER_SHARED:
          ret.emplace("DropMode", "shared");
          break;
        case Lobby::DropMode::SERVER_PRIVATE:
          ret.emplace("DropMode", "private");
          break;
        case Lobby::DropMode::SERVER_DUPLICATE:
          ret.emplace("DropMode", "duplicate");
          break;
      }
      if (l->mode == GameMode::CHALLENGE) {
        ret.emplace("ChallengeEXPMultiplier", l->challenge_exp_multiplier);
        if (l->challenge_params) {
          ret.emplace("ChallengeStageNumber", l->challenge_params->stage_number);
          ret.emplace("ChallengeRankColor", l->challenge_params->rank_color);
          ret.emplace("ChallengeRankText", l->challenge_params->rank_text);
          ret.emplace("ChallengeRank0ThresholdBitmask", l->challenge_params->rank_thresholds[0].bitmask);
          ret.emplace("ChallengeRank0ThresholdSeconds", l->challenge_params->rank_thresholds[0].seconds);
          ret.emplace("ChallengeRank1ThresholdBitmask", l->challenge_params->rank_thresholds[1].bitmask);
          ret.emplace("ChallengeRank1ThresholdSeconds", l->challenge_params->rank_thresholds[1].seconds);
          ret.emplace("ChallengeRank2ThresholdBitmask", l->challenge_params->rank_thresholds[2].bitmask);
          ret.emplace("ChallengeRank2ThresholdSeconds", l->challenge_params->rank_thresholds[2].seconds);
        }
      }

      auto floor_items_json = phosg::JSON::list();
      for (size_t floor = 0; floor < l->floor_item_managers.size(); floor++) {
        for (const auto& it : l->floor_item_managers[floor].items) {
          const auto& item = it.second;
          auto item_dict = phosg::JSON::dict({
              {"LocationFloor", floor},
              {"LocationX", item->pos.x.load()},
              {"LocationZ", item->pos.z.load()},
              {"DropNumber", item->drop_number},
              {"Flags", item->flags},
              {"Data", item->data.hex()},
              {"ItemID", item->data.id.load()},
          });
          if (item_name_index) {
            item_dict.emplace("Description", item_name_index->describe_item(item->data, false));
          }
          floor_items_json.emplace_back(std::move(item_dict));
        }
      }
      ret.emplace("FloorItems", std::move(floor_items_json));
      ret.emplace("Quest", l->quest ? l->quest->json() : phosg::JSON(nullptr));

    } else {
      ret.emplace("BattleInProgress", l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS));
      ret.emplace("IsSpectatorTeam", l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM));
      ret.emplace("SpectatorsForbidden", l->check_flag(Lobby::Flag::SPECTATORS_FORBIDDEN));

      auto ep3s = l->ep3_server;
      if (ep3s) {
        auto players_json = phosg::JSON::list();
        for (size_t z = 0; z < 4; z++) {
          if (!ep3s->name_entries[z].present) {
            players_json.emplace_back(nullptr);
          } else {
            auto lc = l->clients[z];

            auto deck_entry = ep3s->deck_entries[z];
            phosg::JSON deck_json = nullptr;
            if (deck_entry) {
              auto cards_json = phosg::JSON::list();
              for (size_t w = 0; w < deck_entry->card_ids.size(); w++) {
                try {
                  const auto& ce = ep3s->options.card_index->definition_for_id(deck_entry->card_ids[w]);
                  auto name = ce->def.en_name.decode();
                  if (name.empty()) {
                    name = ce->def.en_short_name.decode();
                  }
                  if (name.empty()) {
                    name = ce->def.jp_name.decode();
                  }
                  if (name.empty()) {
                    name = ce->def.jp_short_name.decode();
                  }
                  cards_json.emplace_back(name);
                } catch (const out_of_range&) {
                  cards_json.emplace_back(deck_entry->card_ids[w].load());
                }
              }
              deck_json = phosg::JSON::dict({
                  {"Name", deck_entry->name.decode(lc ? lc->language() : 1)},
                  {"TeamID", deck_entry->team_id.load()},
                  {"Cards", std::move(cards_json)},
                  {"GodWhimFlag", deck_entry->god_whim_flag},
                  {"PlayerLevel", deck_entry->player_level.load()},
              });
            }

            auto player_json = phosg::JSON::dict({
                {"PlayerName", ep3s->name_entries[z].name.decode(lc ? lc->language() : 1)},
                {"ClientID", ep3s->name_entries[z].client_id},
                {"IsCOM", !!ep3s->name_entries[z].is_cpu_player},
                {"Deck", std::move(deck_json)},
            });
            players_json.emplace_back(std::move(player_json));
          }
        }
        auto battle_state_json = phosg::JSON::dict({
            {"BehaviorFlags", ep3s->options.behavior_flags},
            {"RandomSeed", ep3s->options.opt_rand_crypt ? ep3s->options.opt_rand_crypt->seed() : phosg::JSON(nullptr)},
            {"RandomOffset", ep3s->options.opt_rand_crypt ? ep3s->options.opt_rand_crypt->absolute_offset() : phosg::JSON(nullptr)},
            {"Tournament", ep3s->options.tournament ? ep3s->options.tournament->json() : nullptr},
            {"MapNumber", ep3s->last_chosen_map ? ep3s->last_chosen_map->map_number : phosg::JSON(nullptr)},
            {"EnvironmentNumber", ep3s->map_and_rules ? ep3s->map_and_rules->environment_number : phosg::JSON(nullptr)},
            {"Rules", ep3s->map_and_rules ? ep3s->map_and_rules->rules.json() : nullptr},
            {"Players", std::move(players_json)},
            {"IsBattleFinished", ep3s->battle_finished},
            {"IsBattleInprogress", ep3s->battle_in_progress},
            {"RoundNumber", ep3s->round_num},
            {"FirstTeamTurn", ep3s->first_team_turn},
            {"CurrentTeamTurn", ep3s->current_team_turn1},
            {"BattlePhase", phosg::name_for_enum(ep3s->battle_phase)},
            {"SetupPhase", ep3s->setup_phase},
            {"RegistrationPhase", ep3s->registration_phase},
            {"ActionSubphase", ep3s->action_subphase},
            {"BattleStartTimeUsecs", ep3s->battle_start_usecs},
            {"TeamEXP", phosg::JSON::list({ep3s->team_exp[0], ep3s->team_exp[1]})},
            {"TeamDiceBonus", phosg::JSON::list({ep3s->team_dice_bonus[0], ep3s->team_dice_bonus[1]})},
        });
        // std::shared_ptr<StateFlags> state_flags;
        // std::array<std::shared_ptr<PlayerState>, 4> player_states;
        ret.emplace("Episode3BattleState", std::move(battle_state_json));
      } else {
        ret.emplace("Episode3BattleState", nullptr);
      }
      auto watched_lobby = l->watched_lobby.lock();
      if (watched_lobby) {
        ret.emplace("WatchedLobbyID", watched_lobby->lobby_id);
      }
      auto watcher_lobby_ids_json = phosg::JSON::list();
      for (const auto& watcher_lobby : l->watcher_lobbies) {
        watcher_lobby_ids_json.emplace_back(watcher_lobby->lobby_id);
      }
      ret.emplace("WatcherLobbyIDs", std::move(watcher_lobby_ids_json));
      ret.emplace("IsReplayLobby", !!l->battle_player);
    }

  } else { // Not game
    ret.emplace("IsPublic", l->check_flag(Lobby::Flag::PUBLIC));
    ret.emplace("IsDefault", l->check_flag(Lobby::Flag::DEFAULT));
    ret.emplace("IsOverflow", l->check_flag(Lobby::Flag::IS_OVERFLOW));
    ret.emplace("Block", l->block);
  }
  return ret;
}

phosg::JSON HTTPServer::generate_accounts_json() const {
  return call_on_event_thread<phosg::JSON>(this->state->base, [&]() {
    auto res = phosg::JSON::list();
    for (const auto& it : this->state->account_index->all()) {
      res.emplace_back(it->json());
    }
    return res;
  });
}

phosg::JSON HTTPServer::generate_game_server_clients_json() const {
  return call_on_event_thread<phosg::JSON>(this->state->base, [&]() {
    auto res = phosg::JSON::list();
    for (const auto& it : this->state->channel_to_client) {
      res.emplace_back(this->generate_game_client_json_st(it.second, this->state->item_name_index_opt(it.second->version())));
    }
    return res;
  });
}

phosg::JSON HTTPServer::generate_proxy_server_clients_json() const {
  return call_on_event_thread<phosg::JSON>(this->state->base, [&]() {
    phosg::JSON res = phosg::JSON::list();
    if (this->state->proxy_server) {
      for (const auto& it : this->state->proxy_server->all_sessions()) {
        res.emplace_back(this->generate_proxy_client_json_st(it.second));
      }
    }
    return res;
  });
}

phosg::JSON HTTPServer::generate_server_info_json() const {
  return call_on_event_thread<phosg::JSON>(this->state->base, [&]() {
    size_t game_count = 0;
    size_t lobby_count = 0;
    for (const auto& it : this->state->id_to_lobby) {
      if (it.second->is_game()) {
        game_count++;
      } else {
        lobby_count++;
      }
    }
    uint64_t uptime_usecs = phosg::now() - this->state->creation_time;
    return phosg::JSON::dict({
        {"StartTimeUsecs", this->state->creation_time},
        {"StartTime", phosg::format_time(this->state->creation_time)},
        {"UptimeUsecs", uptime_usecs},
        {"Uptime", phosg::format_duration(uptime_usecs)},
        {"LobbyCount", lobby_count},
        {"GameCount", game_count},
        {"ClientCount", this->state->channel_to_client.size()},
        {"ProxySessionCount", this->state->proxy_server ? this->state->proxy_server->num_sessions() : 0},
        {"ServerName", this->state->name},
    });
  });
}

phosg::JSON HTTPServer::generate_lobbies_json() const {
  return call_on_event_thread<phosg::JSON>(this->state->base, [&]() {
    phosg::JSON res = phosg::JSON::list();
    for (const auto& it : this->state->id_to_lobby) {
      auto leader = it.second->clients[it.second->leader_id];
      Version v = leader ? leader->version() : Version::BB_V4;
      res.emplace_back(this->generate_lobby_json_st(it.second, this->state->item_name_index_opt(v)));
    }
    return res;
  });
}

phosg::JSON HTTPServer::generate_summary_json() const {
  auto ret = call_on_event_thread<phosg::JSON>(this->state->base, [&]() {
    auto clients_json = phosg::JSON::list();
    for (const auto& it : this->state->channel_to_client) {
      auto c = it.second;
      auto p = c->character(false, false);
      auto l = c->lobby.lock();
      clients_json.emplace_back(phosg::JSON::dict({
          {"ID", c->id},
          {"AccountID", c->login ? c->login->account->account_id : phosg::JSON(nullptr)},
          {"Name", p ? p->disp.name.decode(it.second->language()) : phosg::JSON(nullptr)},
          {"Version", phosg::name_for_enum(it.second->version())},
          {"Language", name_for_language_code(it.second->language())},
          {"Level", p ? p->disp.stats.level + 1 : phosg::JSON(nullptr)},
          {"Class", p ? name_for_char_class(p->disp.visual.char_class) : phosg::JSON(nullptr)},
          {"SectionID", p ? name_for_section_id(p->disp.visual.section_id) : phosg::JSON(nullptr)},
          {"LobbyID", l ? l->lobby_id : phosg::JSON(nullptr)},
      }));
    }

    auto proxy_clients_json = phosg::JSON::list();
    if (this->state->proxy_server) {
      for (const auto& it : this->state->proxy_server->all_sessions()) {
        proxy_clients_json.emplace_back(phosg::JSON::dict({
            {"AccountID", it.second->login ? it.second->login->account->account_id : phosg::JSON(nullptr)},
            {"Name", it.second->character_name},
            {"Version", phosg::name_for_enum(it.second->version())},
            {"Language", name_for_language_code(it.second->language())},
        }));
      }
    }

    auto games_json = phosg::JSON::list();
    for (const auto& it : this->state->id_to_lobby) {
      auto l = it.second;
      if (l->is_game()) {
        auto game_json = phosg::JSON::dict({
            {"ID", l->lobby_id},
            {"Name", l->name},
            {"Players", l->count_clients()},
            {"CheatsEnabled", l->check_flag(Lobby::Flag::CHEATS_ENABLED)},
            {"Episode", name_for_episode(l->episode)},
            {"HasPassword", !l->password.empty()},
        });
        if (l->episode == Episode::EP3) {
          auto ep3s = l->ep3_server;
          game_json.emplace("BattleInProgress", l->check_flag(Lobby::Flag::BATTLE_IN_PROGRESS));
          game_json.emplace("IsSpectatorTeam", l->check_flag(Lobby::Flag::IS_SPECTATOR_TEAM));
          game_json.emplace("MapNumber", (ep3s && ep3s->last_chosen_map) ? ep3s->last_chosen_map->map_number : phosg::JSON(nullptr));
          game_json.emplace("Rules", (ep3s && ep3s->map_and_rules) ? ep3s->map_and_rules->rules.json() : nullptr);
        } else {
          game_json.emplace("QuestSelectionInProgress", l->check_flag(Lobby::Flag::QUEST_SELECTION_IN_PROGRESS));
          game_json.emplace("QuestInProgress", l->check_flag(Lobby::Flag::QUEST_IN_PROGRESS));
          game_json.emplace("JoinableQuestInProgress", l->check_flag(Lobby::Flag::JOINABLE_QUEST_IN_PROGRESS));
          game_json.emplace("SectionID", name_for_section_id(l->effective_section_id()));
          game_json.emplace("Mode", name_for_mode(l->mode));
          game_json.emplace("Difficulty", name_for_difficulty(l->difficulty));
          game_json.emplace("Quest", l->quest ? l->quest->json() : phosg::JSON(nullptr));
        }
        games_json.emplace_back(std::move(game_json));
      }
    }

    return phosg::JSON::dict({
        {"Clients", std::move(clients_json)},
        {"ProxyClients", std::move(proxy_clients_json)},
        {"Games", std::move(games_json)},
    });
  });
  ret.emplace("Server", this->generate_server_info_json());
  return ret;
}

phosg::JSON HTTPServer::generate_all_json() const {
  return phosg::JSON::dict({
      {"Clients", this->generate_game_server_clients_json()},
      {"ProxyClients", this->generate_proxy_server_clients_json()},
      {"Lobbies", this->generate_lobbies_json()},
      {"Server", this->generate_server_info_json()},
  });
}

phosg::JSON HTTPServer::generate_ep3_cards_json(bool trial) const {
  auto index = call_on_event_thread<shared_ptr<const Episode3::CardIndex>>(this->state->base, [&]() {
    return trial ? this->state->ep3_card_index_trial : this->state->ep3_card_index;
  });
  return index->definitions_json();
}

phosg::JSON HTTPServer::generate_common_tables_json() const {
  auto [set_v2, set_v3_v4] = call_on_event_thread<pair<shared_ptr<const CommonItemSet>, shared_ptr<const CommonItemSet>>>(this->state->base, [&]() {
    return make_pair(this->state->common_item_set_v2, this->state->common_item_set_v3_v4);
  });
  return phosg::JSON::dict({{"v1_v2", set_v2->json()}, {"v3_v4", set_v3_v4->json()}});
}

phosg::JSON HTTPServer::generate_rare_tables_json() const {
  auto sets = call_on_event_thread<unordered_map<string, shared_ptr<const RareItemSet>>>(this->state->base, [&]() {
    return this->state->rare_item_sets;
  });
  phosg::JSON ret = phosg::JSON::list();
  for (const auto& it : sets) {
    ret.emplace_back(it.first);
  }
  return ret;
}

phosg::JSON HTTPServer::generate_rare_table_json(const std::string& table_name) const {
  try {
    auto colls = call_on_event_thread<pair<shared_ptr<const RareItemSet>, shared_ptr<const ItemNameIndex>>>(this->state->base, [&]() {
      const auto& table = this->state->rare_item_sets.at(table_name);
      shared_ptr<const ItemNameIndex> name_index;
      if (phosg::ends_with(table_name, "-v1")) {
        name_index = this->state->item_name_index_opt(Version::DC_V1);
      } else if (phosg::ends_with(table_name, "-v2")) {
        name_index = this->state->item_name_index_opt(Version::PC_V2);
      } else if (phosg::ends_with(table_name, "-v3")) {
        name_index = this->state->item_name_index_opt(Version::GC_V3);
      } else if (phosg::ends_with(table_name, "-v4")) {
        name_index = this->state->item_name_index_opt(Version::BB_V4);
      }
      return make_pair(table, name_index);
    });
    return colls.first->json(colls.second);
  } catch (const out_of_range&) {
    throw http_error(404, "table does not exist");
  }
}

phosg::JSON HTTPServer::generate_quest_list_json(std::shared_ptr<const QuestIndex> quest_index) {
  return call_on_event_thread<phosg::JSON>(this->state->base, [&]() {
    return quest_index->json();
  });
}

void HTTPServer::require_GET(struct evhttp_request* req) {
  if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
    throw HTTPServer::http_error(405, "GET method required for this endpoint");
  }
}

phosg::JSON HTTPServer::require_POST(struct evhttp_request* req) {
  if (evhttp_request_get_command(req) != EVHTTP_REQ_POST) {
    throw HTTPServer::http_error(405, "POST method required for this endpoint");
  }
  const evkeyvalq* headers = evhttp_request_get_input_headers(req);
  const char* content_type = evhttp_find_header(headers, "Content-Type");
  if (!content_type || strcmp(content_type, "application/json")) {
    throw HTTPServer::http_error(400, "POST requests must use the application/json content type");
  }
  struct evbuffer* in_buf = evhttp_request_get_input_buffer(req);
  return phosg::JSON::parse(evbuffer_remove_str(in_buf));
}

void HTTPServer::handle_request(struct evhttp_request* req) {
  shared_ptr<const phosg::JSON> ret;
  uint32_t serialize_options = 0;
  uint64_t start_time = phosg::now();
  string uri = evhttp_request_get_uri(req);

  try {
    std::unordered_multimap<std::string, std::string> query;
    size_t query_pos = uri.find('?');
    if (query_pos != string::npos) {
      query = this->parse_url_params(uri.substr(query_pos + 1));
      uri.resize(query_pos);
    }

    static const string default_format_option = "false";
    if (this->get_url_param(query, "format", &default_format_option) == "true") {
      serialize_options |= phosg::JSON::SerializeOption::FORMAT | phosg::JSON::SerializeOption::SORT_DICT_KEYS;
    }
    if (this->get_url_param(query, "hex", &default_format_option) == "true") {
      serialize_options |= phosg::JSON::SerializeOption::HEX_INTEGERS;
    }

    if (uri == "/") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_server_version_st());

    } else if (uri == "/y/shell-exec") {
      auto json = this->require_POST(req);
      auto command = json.get_string("command");
      try {
        ret = make_shared<phosg::JSON>(phosg::JSON::dict(
            {{"result", phosg::join(ShellCommand::dispatch_str(this->state, command), "\n")}}));
      } catch (const exception& e) {
        throw http_error(400, e.what());
      }

    } else if (uri == "/y/rare-drops/stream") {
      this->require_GET(req);
      auto c = this->enable_websockets(req);
      if (!c) {
        throw http_error(400, "this path requires a websocket connection");
      } else {
        this->rare_drop_subscribers.emplace(c);
        auto version_message = this->generate_server_version_st();
        this->send_websocket_message(c, version_message.serialize());
        return;
      }

    } else if (uri == "/y/data/ep3-cards") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_ep3_cards_json(false));
    } else if (uri == "/y/data/ep3-cards-trial") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_ep3_cards_json(true));
    } else if (uri == "/y/data/common-tables") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_common_tables_json());
    } else if (uri == "/y/data/rare-tables") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_rare_tables_json());
    } else if (!strncmp(uri.c_str(), "/y/data/rare-tables/", 20)) {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_rare_table_json(uri.substr(20)));
    } else if (uri == "/y/data/quests") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_quest_list_json(this->state->quest_index(Version::GC_V3)));
    } else if (uri == "/y/data/config") {
      this->require_GET(req);
      ret = call_on_event_thread<shared_ptr<const phosg::JSON>>(this->state->base, [this]() { return this->state->config_json; });
    } else if (uri == "/y/accounts") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_accounts_json());
    } else if (uri == "/y/clients") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_game_server_clients_json());
    } else if (uri == "/y/proxy-clients") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_proxy_server_clients_json());
    } else if (uri == "/y/lobbies") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_lobbies_json());
    } else if (uri == "/y/server") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_server_info_json());
    } else if (uri == "/y/summary") {
      this->require_GET(req);
      ret = make_shared<phosg::JSON>(this->generate_summary_json());

    } else {
      throw http_error(404, "unknown action");
    }

  } catch (const http_error& e) {
    unique_ptr<struct evbuffer, void (*)(struct evbuffer*)> out_buffer(evbuffer_new(), evbuffer_free);
    evbuffer_add_printf(out_buffer.get(), "%s", e.what());
    this->send_response(req, e.code, "text/plain", out_buffer.get());
    return;

  } catch (const exception& e) {
    unique_ptr<struct evbuffer, void (*)(struct evbuffer*)> out_buffer(evbuffer_new(), evbuffer_free);
    evbuffer_add_printf(out_buffer.get(), "Error during request: %s", e.what());
    this->send_response(req, 500, "text/plain", out_buffer.get());
    server_log.warning("internal server error during http request: %s", e.what());
    return;
  }

  if (!ret) {
    throw logic_error("ret was not set after HTTP handler completed");
  }

  uint64_t handler_end = phosg::now();
  unique_ptr<struct evbuffer, void (*)(struct evbuffer*)> out_buffer(evbuffer_new(), evbuffer_free);
  string* serialized = new string(ret->serialize(phosg::JSON::SerializeOption::ESCAPE_CONTROLS_ONLY | serialize_options));
  size_t size = serialized->size();
  uint64_t serialize_end = phosg::now();
  auto cleanup = +[](const void*, size_t, void* s) -> void {
    delete reinterpret_cast<string*>(s);
  };
  evbuffer_add_reference(out_buffer.get(), serialized->data(), serialized->size(), cleanup, serialized);
  this->send_response(req, 200, "application/json", out_buffer.get());

  string handler_time = phosg::format_duration(handler_end - start_time);
  string serialize_time = phosg::format_duration(serialize_end - handler_end);
  string size_str = phosg::format_size(size);
  server_log.info("[HTTPServer] %s in [handler: %s, serialize: %s, size: %s]",
      uri.c_str(), handler_time.c_str(), serialize_time.c_str(), size_str.c_str());
}

void HTTPServer::thread_fn() {
  event_base_loop(this->base.get(), EVLOOP_NO_EXIT_ON_EMPTY);
}
