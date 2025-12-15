#pragma once

// =============================================================
//  UDPClient Stub Implementation (ASIO removed)
//  This bypasses ASIO issues in newer compilers.
//  Mako does NOT need real UDP for anything.
// =============================================================

namespace erpc {

template <class T>
class UDPClient {
 public:
  UDPClient() {}

  UDPClient(const UDPClient &) = delete;

  ~UDPClient() {}

  /**
   * Dummy send() — always returns sizeof(T) to indicate success.
   *
   * @param rem_hostname ignored
   * @param rem_port ignored
   * @param msg ignored (not actually transmitted)
   */
  size_t send(const std::string /*rem_hostname*/, uint16_t /*rem_port*/, const T & /*msg*/) {
    return sizeof(T);   // Pretend successful send
  }

  void enable_recording() {}

 private:
};

}  // namespace erpc

