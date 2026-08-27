// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_NETWORKING_SOCKET_HPP
#define GREM_NETWORKING_SOCKET_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/UniqueHandle.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/networking/Endpoint.hpp>
#include <GREM/networking/Error.hpp>
#include <GREM/networking/platform.hpp>

#include <system_error> // std::error_code, std::errc, std::make_error_code(std::errc)

namespace grem::networking {

enum class BlockingMode : uint8_t {
	UNSPECIFIED,  ///< Preserve current blocking mode.
	BLOCKING,     ///< Enable blocking.
	NON_BLOCKING, ///< Disable blocking.
};

enum class ProtocolType : int { // NOLINT(performance-enum-size)
	UDP = SOCK_DGRAM,           ///< User Datagram Protocol.
	TCP = SOCK_STREAM,          ///< Transmission Control Protocol.
};

enum class ShutdownType : int { // NOLINT(performance-enum-size)
	RECEIVE = SD_RECEIVE,
	SEND = SD_SEND,
	BOTH = SD_BOTH,
};

class Socket {
public:
	static constexpr size_t MAX_LISTEN_QUEUE_BACKLOG_SIZE = SOMAXCONN;

	Socket() noexcept
		: Socket(INVALID_SOCKET) {}

	explicit Socket(SOCKET handle) noexcept
		: handle(handle) {}

	Socket(EndpointFamily domain, ProtocolType type, std::error_code& errorCode) {
		open(domain, type, errorCode);
	}

	Socket(EndpointFamily domain, ProtocolType type) {
		open(domain, type);
	}

	explicit operator bool() const noexcept {
		return static_cast<bool>(handle);
	}

	GREM_API(networking) void close(std::error_code& errorCode) noexcept;

	void close() noexcept {
		handle.reset();
	}

	SOCKET release() noexcept {
		return handle.release();
	}

	GREM_API(networking) void open(EndpointFamily domain, ProtocolType type, std::error_code& errorCode);
	void open(EndpointFamily domain, ProtocolType type) {
		std::error_code errorCode{};
		open(domain, type, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	GREM_API(networking) void setBlockingMode(BlockingMode mode, std::error_code& errorCode);
	void setBlockingMode(BlockingMode mode) {
		std::error_code errorCode{};
		setBlockingMode(mode, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	GREM_API(networking) void setReceiveTimeout(Duration timeout, std::error_code& errorCode);
	void setReceiveTimeout(Duration timeout) {
		std::error_code errorCode{};
		setReceiveTimeout(timeout, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	GREM_API(networking) void setSendTimeout(Duration timeout, std::error_code& errorCode);
	void setSendTimeout(Duration timeout) {
		std::error_code errorCode{};
		setSendTimeout(timeout, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	GREM_API(networking) void connect(const Endpoint& endpoint, std::error_code& errorCode);
	void connect(const Endpoint& endpoint) {
		std::error_code errorCode{};
		connect(endpoint, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	GREM_API(networking) void bind(const Endpoint& endpoint, std::error_code& errorCode);
	void bind(const Endpoint& endpoint) {
		std::error_code errorCode{};
		bind(endpoint, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	GREM_API(networking) void shutdown(ShutdownType how, std::error_code& errorCode);
	void shutdown(ShutdownType how) {
		std::error_code errorCode{};
		shutdown(how, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	[[nodiscard]] GREM_API(networking) Optional<Endpoint> getLocalEndpoint(std::error_code& errorCode) const;
	[[nodiscard]] Endpoint getLocalEndpoint() const {
		std::error_code errorCode{};
		Optional<Endpoint> result = getLocalEndpoint(errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
		if (!result) {
			throw networking::Error{make_error_code(SocketError::FAILED)};
		}
		return *result;
	}

	[[nodiscard]] GREM_API(networking) Optional<Endpoint> getRemoteEndpoint(std::error_code& errorCode) const;
	[[nodiscard]] Endpoint getRemoteEndpoint() const {
		std::error_code errorCode{};
		Optional<Endpoint> result = getRemoteEndpoint(errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
		if (!result) {
			throw networking::Error{make_error_code(SocketError::FAILED)};
		}
		return *result;
	}

	GREM_API(networking) void listen(size_t listenQueueBacklogSize, std::error_code& errorCode);
	void listen(size_t listenQueueBacklogSize) {
		std::error_code errorCode{};
		listen(listenQueueBacklogSize, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	[[nodiscard]] GREM_API(networking) Optional<Socket> accept(BlockingMode mode, std::error_code& errorCode);
	[[nodiscard]] Optional<Socket> accept(BlockingMode mode) {
		std::error_code errorCode{};
		Optional<Socket> result = accept(mode, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	[[nodiscard]] GREM_API(networking) Optional<Span<byte>> receive(Span<byte> buffer, int flags, std::error_code& errorCode);
	[[nodiscard]] Optional<Span<byte>> receive(Span<byte> buffer, int flags) {
		std::error_code errorCode{};
		Optional<Span<byte>> result = receive(buffer, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	[[nodiscard]] GREM_API(networking) Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer, int flags, std::error_code& errorCode);
	[[nodiscard]] Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer, int flags) {
		std::error_code errorCode{};
		Optional<Pair<Span<byte>, Endpoint>> result = receiveFrom(buffer, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	GREM_API(networking) Optional<size_t> send(Span<const byte> bytes, int flags, std::error_code& errorCode);
	[[nodiscard]] Optional<size_t> send(Span<const byte> bytes, int flags) {
		std::error_code errorCode{};
		Optional<size_t> result = send(bytes, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	GREM_API(networking) Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, int flags, std::error_code& errorCode);
	[[nodiscard]] Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, int flags) {
		std::error_code errorCode{};
		Optional<size_t> result = sendTo(endpoint, bytes, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	GREM_API(networking) void awaitReadable(Duration timeout, std::error_code& errorCode);
	void awaitReadable(Duration timeout) {
		std::error_code errorCode{};
		awaitReadable(timeout, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	GREM_API(networking) void awaitWritable(Duration timeout, std::error_code& errorCode);
	void awaitWritable(Duration timeout) {
		std::error_code errorCode{};
		awaitWritable(timeout, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	[[nodiscard]] SOCKET get() const noexcept {
		return handle.get();
	}

private:
	struct SocketDeleter {
		GREM_API(networking) void operator()(SOCKET handle) const noexcept;
	};

	UniqueHandle<SOCKET, SocketDeleter, INVALID_SOCKET> handle;
};

class UDPSocket : private Socket {
public:
	UDPSocket() noexcept
		: UDPSocket(INVALID_SOCKET) {}

	explicit UDPSocket(SOCKET handle) noexcept
		: Socket(handle) {}

	UDPSocket(BlockingMode mode, const Endpoint& endpoint)
		: UDPSocket() {
		bind(mode, endpoint);
	}

	using Socket::operator bool;
	using Socket::close;
	using Socket::get;
	using Socket::getLocalEndpoint;
	using Socket::release;
	using Socket::setBlockingMode;
	using Socket::setReceiveTimeout;
	using Socket::setSendTimeout;

	void bind(BlockingMode mode, const Endpoint& endpoint, std::error_code& errorCode) {
		Socket::close();
		Socket::open(endpoint.getFamily(), ProtocolType::UDP, errorCode);
		if (errorCode) {
			return;
		}
		Socket::setBlockingMode(BlockingMode::NON_BLOCKING, errorCode);
		if (errorCode) {
			return;
		}
		Socket::bind(endpoint, errorCode);
		if (mode == BlockingMode::BLOCKING) {
			Socket::setBlockingMode(BlockingMode::BLOCKING);
		}
	}

	void bind(BlockingMode mode, const Endpoint& endpoint) {
		std::error_code errorCode{};
		bind(mode, endpoint, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	[[nodiscard]] Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer, std::error_code& errorCode) {
		return Socket::receiveFrom(buffer, UDP_RECEIVE_FLAGS, errorCode);
	}

	[[nodiscard]] Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer) {
		return Socket::receiveFrom(buffer, UDP_RECEIVE_FLAGS);
	}

	Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, std::error_code& errorCode) {
		return Socket::sendTo(endpoint, bytes, UDP_SEND_FLAGS, errorCode);
	}

	[[nodiscard]] Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes) {
		return Socket::sendTo(endpoint, bytes, UDP_SEND_FLAGS);
	}

private:
	static constexpr int UDP_RECEIVE_FLAGS = 0;
	static constexpr int UDP_SEND_FLAGS = 0;
};

class TCPSocket : private Socket {
public:
	TCPSocket() noexcept
		: TCPSocket(INVALID_SOCKET) {}

	explicit TCPSocket(SOCKET handle) noexcept
		: Socket(handle) {}

	TCPSocket(BlockingMode mode, const Endpoint& endpoint, Duration timeout)
		: TCPSocket() {
		connect(mode, endpoint, timeout);
	}

	using Socket::operator bool;
	using Socket::close;
	using Socket::get;
	using Socket::getLocalEndpoint;
	using Socket::getRemoteEndpoint;
	using Socket::release;
	using Socket::setBlockingMode;
	using Socket::setReceiveTimeout;
	using Socket::setSendTimeout;

	GREM_API(networking) void connect(BlockingMode mode, const Endpoint& endpoint, Duration timeout, std::error_code& errorCode);
	void connect(BlockingMode mode, const Endpoint& endpoint, Duration timeout) {
		std::error_code errorCode{};
		connect(mode, endpoint, timeout, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	void disconnect(std::error_code& errorCode) {
		shutdown(ShutdownType::SEND, errorCode);
	}

	void disconnect() {
		shutdown(ShutdownType::SEND);
	}

	[[nodiscard]] Optional<Span<byte>> receive(Span<byte> buffer, std::error_code& errorCode) {
		return Socket::receive(buffer, TCP_RECEIVE_FLAGS, errorCode);
	}

	[[nodiscard]] Optional<Span<byte>> receive(Span<byte> buffer) {
		return Socket::receive(buffer, TCP_RECEIVE_FLAGS);
	}

	[[nodiscard]] Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer, std::error_code& errorCode) {
		return Socket::receiveFrom(buffer, TCP_RECEIVE_FLAGS, errorCode);
	}

	[[nodiscard]] Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer) {
		return Socket::receiveFrom(buffer, TCP_RECEIVE_FLAGS);
	}

	Optional<size_t> send(Span<const byte> bytes, std::error_code& errorCode) {
		size_t bytesSent = 0;
		while (bytesSent < bytes.size()) {
			const Optional<size_t> chunkBytesSent = Socket::send(bytes.subspan(bytesSent), TCP_SEND_FLAGS, errorCode);
			if (errorCode) {
				if (errorCode == SocketError::WAIT && bytesSent > 0) {
					errorCode = make_error_code(SocketError::PARTIAL);
					return bytesSent;
				}
				return {};
			}
			bytesSent += chunkBytesSent.value_or(0);
		}
		errorCode.clear();
		return bytesSent;
	}

	[[nodiscard]] Optional<size_t> send(Span<const byte> bytes) {
		std::error_code errorCode{};
		Optional<size_t> result = send(bytes, errorCode);
		if (errorCode && errorCode != SocketError::WAIT && errorCode != SocketError::PARTIAL) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, std::error_code& errorCode) {
		size_t bytesSent = 0;
		while (bytesSent < bytes.size()) {
			const Optional<size_t> chunkBytesSent = Socket::sendTo(endpoint, bytes.subspan(bytesSent), TCP_SEND_FLAGS, errorCode);
			if (errorCode) {
				if (errorCode == SocketError::WAIT && bytesSent > 0) {
					errorCode = make_error_code(SocketError::PARTIAL);
					return bytesSent;
				}
				return {};
			}
			bytesSent += chunkBytesSent.value_or(0);
		}
		errorCode.clear();
		return bytesSent;
	}

	[[nodiscard]] Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes) {
		std::error_code errorCode{};
		Optional<size_t> result = sendTo(endpoint, bytes, errorCode);
		if (errorCode && errorCode != SocketError::WAIT && errorCode != SocketError::PARTIAL) {
			throw networking::Error{errorCode};
		}
		return result;
	}

private:
	static constexpr int TCP_RECEIVE_FLAGS = 0;
#ifdef _WIN32
	static constexpr int TCP_SEND_FLAGS = 0;
#else
	static constexpr int TCP_SEND_FLAGS = MSG_NOSIGNAL;
#endif
};

class TCPListener : private Socket {
public:
	using Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE;

	TCPListener() noexcept
		: TCPListener(INVALID_SOCKET) {}

	explicit TCPListener(SOCKET handle) noexcept
		: Socket(handle) {}

	TCPListener(BlockingMode mode, size_t listenQueueBacklogSize, const Endpoint& endpoint)
		: TCPListener() {
		listen(mode, listenQueueBacklogSize, endpoint);
	}

	using Socket::operator bool;
	using Socket::close;
	using Socket::get;
	using Socket::getLocalEndpoint;
	using Socket::release;
	using Socket::setBlockingMode;

	void listen(BlockingMode mode, size_t listenQueueBacklogSize, const Endpoint& endpoint, std::error_code& errorCode) {
		Socket::close();
		Socket::open(endpoint.getFamily(), ProtocolType::TCP, errorCode);
		if (errorCode) {
			return;
		}
		Socket::setBlockingMode(mode, errorCode);
		if (errorCode) {
			return;
		}
		Socket::bind(endpoint, errorCode);
		if (errorCode) {
			return;
		}
		Socket::listen(listenQueueBacklogSize, errorCode);
	}

	void listen(BlockingMode mode, size_t listenQueueBacklogSize, const Endpoint& endpoint) {
		std::error_code errorCode{};
		listen(mode, listenQueueBacklogSize, endpoint, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	[[nodiscard]] Optional<TCPSocket> accept(BlockingMode mode, std::error_code& errorCode) {
		return Socket::accept(mode, errorCode).transform([](Socket socket) -> TCPSocket { return TCPSocket{socket.release()}; });
	}

	[[nodiscard]] Optional<TCPSocket> accept(BlockingMode mode) {
		return Socket::accept(mode).transform([](Socket socket) -> TCPSocket { return TCPSocket{socket.release()}; });
	}
};

} // namespace grem::networking

#endif
