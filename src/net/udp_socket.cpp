#include "net/udp_socket.h"

#include <arpa/inet.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>

namespace mdfeed::net {

UdpSocket::UdpSocket() {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
}

UdpSocket::~UdpSocket() {
    close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void UdpSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

int UdpSocket::bind(std::string_view host, uint16_t port) {
    if (fd_ < 0) return -EBADF;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    std::string host_z(host);
    if (::inet_pton(AF_INET, host_z.c_str(), &addr.sin_addr) != 1) {
        return -EINVAL;
    }
    int one = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        return -errno;
    }
    return 0;
}

int UdpSocket::connect(std::string_view host, uint16_t port) {
    if (fd_ < 0) return -EBADF;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    std::string host_z(host);
    if (::inet_pton(AF_INET, host_z.c_str(), &addr.sin_addr) != 1) {
        return -EINVAL;
    }
    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        return -errno;
    }
    return 0;
}

int UdpSocket::set_recv_buffer(int bytes) {
    if (fd_ < 0) return -EBADF;
    if (::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof(bytes)) != 0) {
        return -errno;
    }
    return 0;
}

ssize_t UdpSocket::recv(uint8_t* buf, std::size_t cap) {
    if (fd_ < 0) return -EBADF;
    ssize_t n = ::recv(fd_, buf, cap, 0);
    if (n < 0) return -errno;
    return n;
}

ssize_t UdpSocket::send(const uint8_t* buf, std::size_t len) {
    if (fd_ < 0) return -EBADF;
    ssize_t n = ::send(fd_, buf, len, 0);
    if (n < 0) return -errno;
    return n;
}

}  // namespace mdfeed::net
