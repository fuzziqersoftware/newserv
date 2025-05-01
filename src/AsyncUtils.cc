#include "AsyncUtils.hh"

#include <asio.hpp>
#include <exception>
#include <functional>
#include <optional>
#include <phosg/Strings.hh>
#include <string>

using namespace std;

AsyncEvent::AsyncEvent(asio::any_io_executor ex)
    : executor(ex), is_set(false) {}

void AsyncEvent::set() {
  lock_guard g(this->lock);
  this->is_set = true;
  for (auto& waiter : this->waiters) {
    asio::post(this->executor,
        [handler = std::move(waiter)]() mutable {
          (*handler)();
        });
  }
  this->waiters.clear();
}

void AsyncEvent::clear() {
  lock_guard g(this->lock);
  this->is_set = false;
}

asio::awaitable<void> AsyncEvent::wait() {
  auto token = asio::use_awaitable_t<>{};
  co_await asio::async_initiate<asio::use_awaitable_t<>, void()>(
      [this](auto&& handler) -> void {
        lock_guard g(this->lock);
        if (this->is_set) {
          handler();
        } else {
          this->waiters.emplace_back(make_unique<asio::detail::awaitable_handler<asio::any_io_executor>>(std::move(handler)));
        }
      },
      token);
}

AsyncSocketReader::AsyncSocketReader(asio::ip::tcp::socket&& sock)
    : sock(std::move(sock)) {}

asio::awaitable<string> AsyncSocketReader::read_line(const char* delimiter, size_t max_length) {
  size_t delimiter_size = strlen(delimiter);
  if (delimiter_size == 0) {
    throw logic_error("delimiter is empty");
  }
  size_t delimiter_backup_bytes = delimiter_size - 1;

  size_t delimiter_pos = this->pending_data.find(delimiter);
  while ((delimiter_pos == string::npos) && (!max_length || (this->pending_data.size() < max_length))) {
    size_t pre_size = this->pending_data.size();
    this->pending_data.resize(min(max_length, this->pending_data.size() + 0x400));

    auto buf = asio::buffer(this->pending_data.data() + pre_size, this->pending_data.size() - pre_size);
    size_t bytes_read = co_await this->sock.async_read_some(buf, asio::use_awaitable);
    this->pending_data.resize(pre_size + bytes_read);
    delimiter_pos = this->pending_data.find(
        delimiter,
        (delimiter_backup_bytes > pre_size) ? 0 : (pre_size - delimiter_backup_bytes));
  }

  if (delimiter_pos == string::npos) {
    throw runtime_error("line exceeds max length");
  }

  // TODO: It's not great that we copy the data here. There's probably a more
  // idiomatic and efficient way to do this.
  string ret = this->pending_data.substr(0, delimiter_pos);
  this->pending_data = this->pending_data.substr(delimiter_pos + delimiter_size);
  co_return ret;
}

asio::awaitable<string> AsyncSocketReader::read_data(size_t size) {
  string ret;
  if (this->pending_data.size() == size) {
    this->pending_data.swap(ret);
  } else if (this->pending_data.size() > size) {
    ret = this->pending_data.substr(0, size);
    this->pending_data = this->pending_data.substr(size);
  } else {
    size_t bytes_to_read = size - this->pending_data.size();
    this->pending_data.swap(ret);
    ret.resize(size);
    co_await asio::async_read(this->sock, asio::buffer(ret.data() + size - bytes_to_read, bytes_to_read), asio::use_awaitable);
  }
  co_return ret;
}

asio::awaitable<void> AsyncSocketReader::read_data_into(void* data, size_t size) {
  if (this->pending_data.size() == size) {
    memcpy(data, this->pending_data.data(), size);
    this->pending_data.clear();
  } else if (this->pending_data.size() > size) {
    memcpy(data, this->pending_data.data(), size);
    this->pending_data = this->pending_data.substr(size);
  } else {
    memcpy(data, this->pending_data.data(), this->pending_data.size());
    size_t bytes_to_read = size - this->pending_data.size();
    this->pending_data.clear();
    void* read_buf = reinterpret_cast<uint8_t*>(data) + size - bytes_to_read;
    co_await asio::async_read(this->sock, asio::buffer(read_buf, bytes_to_read), asio::use_awaitable);
  }
}

void AsyncWriteCollector::add(string&& data) {
  const auto& item = this->owned_data.emplace_back(std::move(data));
  bufs.emplace_back(asio::buffer(item.data(), item.size()));
}

void AsyncWriteCollector::add_reference(const void* data, size_t size) {
  bufs.emplace_back(asio::buffer(data, size));
}

asio::awaitable<void> AsyncWriteCollector::write(asio::ip::tcp::socket& sock) {
  deque<string> local_owned_data;
  local_owned_data.swap(this->owned_data);
  vector<asio::const_buffer> local_bufs;
  local_bufs.swap(this->bufs);
  co_await asio::async_write(sock, local_bufs, asio::use_awaitable);
}

asio::awaitable<void> async_sleep(chrono::steady_clock::duration duration) {
  asio::steady_timer timer(co_await asio::this_coro::executor, duration);
  co_await timer.async_wait(asio::use_awaitable);
}

asio::awaitable<asio::ip::tcp::socket> async_connect_tcp(uint32_t ipv4_addr, uint16_t port) {
  uint8_t octets[4] = {
      static_cast<uint8_t>(ipv4_addr >> 24),
      static_cast<uint8_t>(ipv4_addr >> 16),
      static_cast<uint8_t>(ipv4_addr >> 8),
      static_cast<uint8_t>(ipv4_addr)};
  return async_connect_tcp(std::format("{}.{}.{}.{}", octets[0], octets[1], octets[2], octets[3]), port);
}

asio::awaitable<asio::ip::tcp::socket> async_connect_tcp(const std::string& host, uint16_t port) {
  auto executor = co_await asio::this_coro::executor;

  asio::ip::tcp::resolver resolver(executor);
  auto endpoints = co_await resolver.async_resolve(host, std::format("{}", port), asio::use_awaitable);

  asio::ip::tcp::socket sock(executor);
  co_await asio::async_connect(sock, endpoints, asio::use_awaitable);

  co_return sock;
}

asio::awaitable<asio::ip::tcp::socket> async_connect_tcp(const asio::ip::tcp::endpoint& ep) {
  auto executor = co_await asio::this_coro::executor;
  asio::ip::tcp::socket sock(executor);
  co_await sock.async_connect(ep, asio::use_awaitable);
  co_return sock;
}
