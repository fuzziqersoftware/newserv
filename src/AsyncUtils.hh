#pragma once

#include <asio.hpp>
#include <asio/experimental/parallel_group.hpp>
#include <asio/experimental/promise.hpp>
#include <deque>
#include <exception>
#include <functional>
#include <optional>
#include <phosg/Strings.hh>

template <typename T>
class AsyncPromise {
public:
  AsyncPromise() = default;

  asio::awaitable<T> get() {
    if (!this->exc && !this->val.has_value()) {
      auto executor = co_await asio::this_coro::executor;
      co_await asio::async_initiate<decltype(asio::use_awaitable), void(std::error_code)>(
          [this, &executor](auto&& new_handler) {
            this->resolver_ref.emplace(ResolverRef{.resolve = std::move(new_handler), .executor = &executor});
          },
          asio::use_awaitable);
    }

    if (this->exc) {
      std::rethrow_exception(this->exc);
    } else if (this->val.has_value()) {
      co_return *this->val;
    } else {
      throw std::logic_error("AsyncPromise await resolved but did not have a value or exception");
    }
  }

  void set_value(T&& result) {
    if (this->exc || this->val.has_value()) {
      throw std::logic_error("attempted to set value on completed promise");
    }
    this->val = result;
    this->resolve();
  }

  void set_exception(std::exception_ptr ex) {
    if (this->exc || this->val.has_value()) {
      throw std::logic_error("attempted to set value on completed promise");
    }
    this->exc = ex;
    this->resolve();
  }

  void cancel() {
    this->set_exception(std::make_exception_ptr(std::runtime_error("AsyncPromise cancelled")));
  }

  bool done() const {
    return this->exc || this->val.has_value();
  }

private:
  struct ResolverRef {
    asio::detail::awaitable_handler<asio::any_io_executor, std::error_code> resolve;
    asio::any_io_executor* executor;
  };
  std::optional<T> val;
  std::exception_ptr exc;
  std::optional<ResolverRef> resolver_ref;

  void resolve() {
    if (this->resolver_ref.has_value()) {
      auto* executor = this->resolver_ref->executor;
      asio::post(*executor, [ref = std::move(this->resolver_ref)]() mutable -> void {
        ref->resolve(std::error_code{});
      });
      this->resolver_ref.reset();
    }
  }
};

template <>
class AsyncPromise<void> {
public:
  AsyncPromise() = default;

  asio::awaitable<void> get() {
    if (!this->exc && !this->returned) {
      auto executor = co_await asio::this_coro::executor;
      co_await asio::async_initiate<decltype(asio::use_awaitable), void(std::error_code)>(
          [this, &executor](auto&& new_handler) {
            this->resolver_ref.emplace(ResolverRef{.resolve = std::move(new_handler), .executor = &executor});
          },
          asio::use_awaitable);
    }

    if (this->exc) {
      std::rethrow_exception(this->exc);
    } else if (this->returned) {
      co_return;
    } else {
      throw std::logic_error("AsyncPromise await resolved but did not have a value or exception");
    }
  }

  void set_value() {
    if (this->exc || this->returned) {
      throw std::logic_error("attempted to set value on completed promise");
    }
    this->returned = true;
    this->resolve();
  }

  void set_exception(std::exception_ptr ex) {
    if (this->exc || this->returned) {
      throw std::logic_error("attempted to set value on completed promise");
    }
    this->exc = ex;
    this->resolve();
  }

  void cancel() {
    this->set_exception(std::make_exception_ptr(std::runtime_error("AsyncPromise cancelled")));
  }

  bool done() const {
    return this->exc || this->returned;
  }

private:
  struct ResolverRef {
    asio::detail::awaitable_handler<asio::any_io_executor, std::error_code> resolve;
    asio::any_io_executor* executor;
  };
  bool returned;
  std::exception_ptr exc;
  std::optional<ResolverRef> resolver_ref;

  void resolve() {
    if (this->resolver_ref.has_value()) {
      auto* executor = this->resolver_ref->executor;
      asio::post(*executor, [ref = std::move(this->resolver_ref)]() mutable -> void {
        ref->resolve(std::error_code{});
      });
      this->resolver_ref.reset();
    }
  }
};

class AsyncEvent {
public:
  AsyncEvent(asio::any_io_executor ex);
  AsyncEvent(const AsyncEvent&) = delete;
  AsyncEvent(AsyncEvent&&) = delete;
  AsyncEvent& operator=(const AsyncEvent&) = delete;
  AsyncEvent& operator=(AsyncEvent&&) = delete;

  void set();
  void clear();
  asio::awaitable<void> wait();

private:
  asio::any_io_executor executor;
  bool is_set;
  std::mutex lock;
  std::vector<std::unique_ptr<asio::detail::awaitable_handler<asio::any_io_executor>>> waiters;
};

class AsyncSocketReader {
public:
  explicit AsyncSocketReader(asio::ip::tcp::socket&& sock);
  AsyncSocketReader(const AsyncSocketReader&) = delete;
  AsyncSocketReader(AsyncSocketReader&&) = delete;
  AsyncSocketReader& operator=(const AsyncSocketReader&) = delete;
  AsyncSocketReader& operator=(AsyncSocketReader&&) = delete;
  ~AsyncSocketReader() = default;

  // Reads one line from the socket, buffering any extra data read. The
  // delimiter is not included in the returned line. max_length = 0 means no
  // maximum length is enforced.
  asio::awaitable<std::string> read_line(
      const char* delimiter = "\n", size_t max_length = 0);
  asio::awaitable<std::string> read_data(size_t size);
  asio::awaitable<void> read_data_into(void* data, size_t size);

  // The caller cannot know what the socket's read state is, so this should
  // only be used when the caller intends to write to the socket, not read
  inline asio::ip::tcp::socket& get_socket() {
    return this->sock;
  }

  inline void close() {
    this->sock.close();
  }

private:
  std::string pending_data; // Data read but not yet returned to the caller
  asio::ip::tcp::socket sock;
};

class AsyncWriteCollector {
public:
  AsyncWriteCollector() = default;
  AsyncWriteCollector(const AsyncWriteCollector&) = delete;
  AsyncWriteCollector(AsyncWriteCollector&&) = delete;
  AsyncWriteCollector& operator=(const AsyncWriteCollector&) = delete;
  AsyncWriteCollector& operator=(AsyncWriteCollector&&) = delete;
  ~AsyncWriteCollector() = default;

  void add(std::string&& data);

  // When using add_reference, it is the caller's responsibility to ensure that
  // the buffer is valid until *this is destroyed or write() returns.
  void add_reference(const void* data, size_t size);

  asio::awaitable<void> write(asio::ip::tcp::socket& sock);

private:
  std::deque<std::string> owned_data;
  std::vector<asio::const_buffer> bufs;
};

asio::awaitable<void> async_sleep(std::chrono::steady_clock::duration duration);

inline asio::ip::tcp::endpoint make_endpoint_ipv4(uint32_t addr, uint16_t port) {
  return asio::ip::tcp::endpoint(asio::ip::address_v4(addr), port);
}

inline asio::ip::tcp::endpoint make_endpoint_ipv6(const void* addr, uint16_t port) {
  std::array<uint8_t, 0x10> bytes;
  for (size_t z = 0; z < 0x10; z++) {
    bytes[z] = reinterpret_cast<const uint8_t*>(addr)[z];
  }
  return asio::ip::tcp::endpoint(asio::ip::address_v6(bytes), port);
}

inline std::string str_for_endpoint(const asio::ip::tcp::endpoint& ep) {
  return ep.address().to_string() + std::format(":{}", ep.port());
}

inline uint32_t ipv4_addr_for_asio_addr(const asio::ip::address& addr) {
  if (!addr.is_v4()) {
    throw std::runtime_error("Address is not IPv4");
  }
  return addr.to_v4().to_uint();
}

asio::awaitable<asio::ip::tcp::socket> async_connect_tcp(uint32_t ipv4_addr, uint16_t port);
asio::awaitable<asio::ip::tcp::socket> async_connect_tcp(const std::string& host, uint16_t port);
asio::awaitable<asio::ip::tcp::socket> async_connect_tcp(const asio::ip::tcp::endpoint& ep);

template <typename FnT, typename... ArgTs>
asio::awaitable<std::invoke_result_t<FnT, ArgTs...>> call_on_thread_pool(asio::thread_pool& pool, FnT&& f, ArgTs&&... args) {
  using ReturnT = std::invoke_result_t<FnT, ArgTs...>;
  auto bound = std::bind(std::forward<FnT>(f), std::forward<ArgTs>(args)...);
  AsyncPromise<ReturnT> promise;

  asio::post(pool, [&promise, &bound]() -> void {
    promise.set_value(bound());
  });
  co_return co_await promise.get();
}
