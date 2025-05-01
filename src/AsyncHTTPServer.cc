#include "AsyncHTTPServer.hh"

#include <inttypes.h>
#include <stdlib.h>

#include <phosg/Encoding.hh>
#include <phosg/Network.hh>
#include <phosg/Time.hh>
#include <string>
#include <vector>

#include "AsyncUtils.hh"
#include "Loggers.hh"
#include "Revision.hh"
#include "Server.hh"

using namespace std;

static const unordered_map<int, const char*> explanation_for_response_code{
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
};

HTTPError::HTTPError(int code, const std::string& what)
    : std::runtime_error(what), code(code) {}

const std::string* HTTPRequest::get_header(const std::string& name) const {
  auto its = this->headers.equal_range(name);
  if (its.first == its.second) {
    return nullptr;
  }
  const string* ret = &its.first->second;
  its.first++;
  if (its.first != its.second) {
    throw std::out_of_range("Header appears multiple times: " + name);
  }
  return ret;
}

const std::string* HTTPRequest::get_query_param(const std::string& name) const {
  auto its = this->query_params.equal_range(name);
  if (its.first == its.second) {
    return nullptr;
  }
  const string* ret = &its.first->second;
  its.first++;
  if (its.first != its.second) {
    throw std::out_of_range("Query parameter appears multiple times: " + name);
  }
  return ret;
}

static void url_decode_inplace(string& s) {
  size_t write_offset = 0, read_offset = 0;
  for (; read_offset < s.size(); write_offset++) {
    if ((s[read_offset] == '%') && (read_offset < s.size() - 2)) {
      s[write_offset] =
          static_cast<char>(phosg::value_for_hex_char(s[read_offset + 1]) << 4) |
          static_cast<char>(phosg::value_for_hex_char(s[read_offset + 2]));
      read_offset += 3;
    } else if (s[write_offset] == '+') {
      s[write_offset] = ' ';
      read_offset++;
    } else {
      s[write_offset] = s[read_offset];
      read_offset++;
    }
  }
  s.resize(write_offset);
}

HTTPClient::HTTPClient(asio::ip::tcp::socket&& sock) : r(std::move(sock)) {}

asio::awaitable<HTTPRequest> HTTPClient::recv_http_request(size_t max_line_size, size_t max_body_size) {
  HTTPRequest req;
  std::string request_line = co_await this->r.read_line("\r\n", max_line_size);
  auto line_tokens = phosg::split(request_line, ' ');
  if (line_tokens.size() != 3) {
    throw runtime_error("invalid HTTP request line");
  }
  const auto& method_token = line_tokens[0];
  if (method_token == "GET") {
    req.method = HTTPRequest::Method::GET;
  } else if (method_token == "POST") {
    req.method = HTTPRequest::Method::POST;
  } else if (method_token == "DELETE") {
    req.method = HTTPRequest::Method::DELETE;
  } else if (method_token == "HEAD") {
    req.method = HTTPRequest::Method::HEAD;
  } else if (method_token == "PATCH") {
    req.method = HTTPRequest::Method::PATCH;
  } else if (method_token == "PUT") {
    req.method = HTTPRequest::Method::PUT;
  } else if (method_token == "UPDATE") {
    req.method = HTTPRequest::Method::UPDATE;
  } else if (method_token == "OPTIONS") {
    req.method = HTTPRequest::Method::OPTIONS;
  } else if (method_token == "CONNECT") {
    req.method = HTTPRequest::Method::CONNECT;
  } else if (method_token == "TRACE") {
    req.method = HTTPRequest::Method::TRACE;
  } else {
    throw HTTPError(400, "unknown request method");
  }

  req.http_version = std::move(line_tokens[2]);

  size_t fragment_start_offset = line_tokens[1].find('#');
  if (fragment_start_offset != string::npos) {
    req.fragment = line_tokens[1].substr(fragment_start_offset + 1);
    line_tokens[1].resize(fragment_start_offset);
  }

  size_t query_start_offset = line_tokens[1].find('?');
  string query;
  if (query_start_offset != string::npos) {
    query = line_tokens[1].substr(query_start_offset + 1);
    line_tokens[1].resize(query_start_offset);
  }

  req.path = std::move(line_tokens[1]);
  if (req.path.empty()) {
    throw std::runtime_error("request path is missing");
  }

  auto query_tokens = phosg::split(query, '&');
  for (auto& token : query_tokens) {
    size_t equals_pos = token.find('=');
    if (equals_pos == string::npos) {
      url_decode_inplace(token);
      req.query_params.emplace(std::move(token), "");
    } else {
      string key = token.substr(0, equals_pos);
      string value = token.substr(equals_pos + 1);
      url_decode_inplace(key);
      url_decode_inplace(value);
      req.query_params.emplace(std::move(key), std::move(value));
    }
  }

  auto prev_header_it = req.headers.end();
  for (;;) {
    std::string line = co_await this->r.read_line("\r\n", max_line_size);
    if (line.empty()) {
      break;
    }
    if (line[0] == ' ' || line[0] == '\t') {
      if (prev_header_it == req.headers.end()) {
        throw std::runtime_error("received header continuation line before any header");
      } else {
        phosg::strip_whitespace(line);
        prev_header_it->second.append(1, ' ');
        prev_header_it->second += line;
      }
    } else {
      size_t colon_pos = line.find(':');
      if (colon_pos == string::npos) {
        throw runtime_error("malformed header line");
      }
      string key = line.substr(0, colon_pos);
      string value = line.substr(colon_pos + 1);
      phosg::strip_whitespace(key);
      phosg::strip_whitespace(value);
      prev_header_it = req.headers.emplace(phosg::tolower(key), std::move(value));
    }
  }

  auto transfer_encoding_header = req.get_header("transfer-encoding");
  if (transfer_encoding_header && phosg::tolower(*transfer_encoding_header) == "chunked") {
    deque<string> chunks;
    size_t total_data_bytes = 0;
    for (;;) {
      auto line = co_await this->r.read_line("\r\n", 0x20);
      size_t parse_offset = 0;
      size_t chunk_size = stoull(line, &parse_offset, 16);
      if (parse_offset != line.size()) {
        throw HTTPError(400, "invalid chunk header during chunked encoding");
      }
      if (chunk_size == 0) {
        break;
      }
      total_data_bytes += chunk_size;
      if (total_data_bytes > max_body_size) {
        throw HTTPError(400, "request data size too large");
      }
      chunks.emplace_back(co_await this->r.read_data(chunk_size));
      auto after_chunk_data = co_await this->r.read_line("\r\n", 0x20);
      if (!after_chunk_data.empty()) {
        throw HTTPError(400, "incorrect trailing sequence after chunk data");
      }
    }
  } else {
    auto content_length_header = req.get_header("content-length");
    size_t content_length = content_length_header ? stoull(*content_length_header) : 0;
    if (content_length > max_body_size) {
      throw HTTPError(400, "request data size too large");
    } else if (content_length > 0) {
      req.data = co_await this->r.read_data(content_length);
    }
  }

  co_return req;
}

asio::awaitable<void> HTTPClient::send_http_response(const HTTPResponse& resp) {
  AsyncWriteCollector w;
  w.add(std::format("{} {} {}\r\n",
      resp.http_version, resp.response_code, explanation_for_response_code.at(resp.response_code)));
  for (const auto& it : resp.headers) {
    w.add(it.first + ": " + it.second + "\r\n");
  }
  if (!resp.data.empty()) {
    w.add(std::format("Content-Length: {}\r\n", resp.data.size()));
  }
  w.add("\r\n");
  if (!resp.data.empty()) {
    w.add_reference(resp.data.data(), resp.data.size());
  }
  co_await w.write(this->r.get_socket());
}

asio::awaitable<WebSocketMessage> HTTPClient::recv_websocket_message(size_t max_data_size) {
  WebSocketMessage prev_msg;
  bool prev_msg_present = false;

  while (this->r.get_socket().is_open()) {
    WebSocketMessage msg;

    // We need at most 10 bytes to determine if there's a valid frame, or as
    // little as 2
    co_await this->r.read_data_into(msg.header, 2);

    // Get the payload size
    bool has_mask = msg.header[1] & 0x80;
    size_t payload_size = msg.header[1] & 0x7F;
    if (payload_size == 0x7F) {
      phosg::be_uint64_t wire_size;
      co_await this->r.read_data_into(&wire_size, sizeof(wire_size));
      payload_size = wire_size;
    } else if (payload_size == 0x7E) {
      phosg::be_uint16_t wire_size;
      co_await this->r.read_data_into(&wire_size, sizeof(wire_size));
      payload_size = wire_size;
    }

    if (payload_size > max_data_size) {
      throw runtime_error("Incoming WebSocket message exceeds size limit");
    }

    // Read the masking key if present
    if (has_mask) {
      co_await this->r.read_data_into(msg.mask_key, sizeof(msg.mask_key));
    }

    // Read and unmask message data
    msg.data = co_await this->r.read_data(payload_size);
    if (has_mask) {
      for (size_t x = 0; x < msg.data.size(); x++) {
        msg.data[x] ^= msg.mask_key[x & 3];
      }
    }

    this->last_communication_time = phosg::now();

    // If the current message is a control message, respond appropriately
    // (these can be sent in the middle of fragmented messages)
    uint8_t opcode = msg.header[0] & 0x0F;
    if (opcode & 0x08) {
      if (opcode == 0x0A) {
        // Ping response; ignore it

      } else if (opcode == 0x08) {
        // Close message
        co_await this->send_websocket_message(msg.data, msg.opcode);
        this->r.get_socket().close();

      } else if (opcode == 0x09) {
        // Ping message
        co_await this->send_websocket_message(msg.data, 0x0A);

      } else {
        // Unknown control message type
        this->r.get_socket().close();
      }
      continue;
    }

    // If there's an existing fragment, the current message's opcode should be
    // zero; if there's no pending message, it must not be zero
    if (prev_msg_present == (opcode != 0)) {
      this->r.get_socket().close();
      continue;
    }

    // Save the message opcode, if present, and append the frame data
    if (!prev_msg_present) {
      prev_msg = std::move(msg);
    } else {
      prev_msg.header[0] = msg.header[0];
      prev_msg.header[1] = msg.header[1];
      if (opcode) {
        prev_msg.opcode = msg.opcode;
      }
      if (has_mask) {
        prev_msg.mask_key[0] = msg.mask_key[0];
        prev_msg.mask_key[1] = msg.mask_key[1];
        prev_msg.mask_key[2] = msg.mask_key[2];
        prev_msg.mask_key[3] = msg.mask_key[3];
      }
      prev_msg.data += msg.data;
    }

    // If the FIN bit is set, then the frame is complete - append the payload
    // to any pending payloads and call the message handler. If the FIN bit
    // isn't set, we need to receive at least one continuation frame to
    // complete the message.
    if (prev_msg.header[0] & 0x80) {
      co_return prev_msg;
    }
  }

  throw logic_error("failed to receive websocket message");
}

asio::awaitable<void> HTTPClient::send_websocket_message(const void* data, size_t size, uint8_t opcode) {
  phosg::StringWriter w;
  w.put_u8(0x80 | (opcode & 0x0F));
  if (size > 0xFFFF) {
    w.put_u8(0x7F);
    w.put_u64b(size);
  } else if (size > 0x7D) {
    w.put_u8(0x7E);
    w.put_u16b(size);
  } else {
    w.put_u8(size);
  }

  array<asio::const_buffer, 2> bufs = {asio::const_buffer(w.data(), w.size()), asio::const_buffer(data, size)};
  co_await asio::async_write(this->r.get_socket(), bufs, asio::use_awaitable);
}

asio::awaitable<void> HTTPClient::send_websocket_message(const std::string& data, uint8_t opcode) {
  return this->send_websocket_message(data.data(), data.size(), opcode);
}

const HTTPServerLimits DEFAULT_HTTP_LIMITS;
