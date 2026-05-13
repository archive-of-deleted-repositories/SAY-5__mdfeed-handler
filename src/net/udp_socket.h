#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mdfeed::net {

// Thin RAII wrapper around a POSIX UDP socket. No kernel-bypass; the point
// of this study is to measure the cost of the standard recvfrom path and
// have a portable CI story over loopback.
class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // Bind to receive on host:port. Returns 0 on success, -errno on failure.
    int bind(std::string_view host, uint16_t port);

    // Connect for send-only use. Same return convention.
    int connect(std::string_view host, uint16_t port);

    // Set SO_RCVBUF. Best-effort; kernel may clamp.
    int set_recv_buffer(int bytes);

    // Blocking receive. Returns number of bytes read, or -errno on error.
    // SIGTERM during recvfrom will return -EINTR which the caller must treat
    // as a graceful shutdown signal rather than an error to log+continue.
    ssize_t recv(uint8_t* buf, std::size_t cap);

    // Send to the connected peer.
    ssize_t send(const uint8_t* buf, std::size_t len);

    int fd() const { return fd_; }

    void close();

private:
    int fd_ = -1;
};

}  // namespace mdfeed::net
