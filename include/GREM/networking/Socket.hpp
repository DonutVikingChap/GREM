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

/**
 * Blocking mode of a Socket.
 */
enum class BlockingMode : uint8_t {
	UNSPECIFIED,  ///< Preserve current blocking mode.
	BLOCKING,     ///< Enable blocking.
	NON_BLOCKING, ///< Disable blocking.
};

/**
 * Communication semantics of a Socket.
 */
enum class ProtocolType : int { // NOLINT(performance-enum-size)
	UDP = SOCK_DGRAM,           ///< User Datagram Protocol (connectionless, unreliable messages of a fixed maximum length).
	TCP = SOCK_STREAM,          ///< Transmission Control Protocol (connection-based, sequenced, reliable two-way data stream).
};

/**
 * Direction(s) in which to shut down the connection of a Socket.
 */
enum class ShutdownType : int { // NOLINT(performance-enum-size)
	RECEIVE = SD_RECEIVE,       ///< Shut down further receptions.
	SEND = SD_SEND,             ///< Shut down further transmissions.
	BOTH = SD_BOTH,             ///< Shut down both further receptions and transmissions.
};

/**
 * Strong type for a bitmask of native `MSG_...` flags.
 */
enum class MessageFlags : int {};

/**
 * Generic socket handle/file descriptor for receiving and transmitting data
 * across the network.
 */
class Socket {
public:
	/**
	 * Maximum `listenQueueBacklogSize` value that can be passed to listen().
	 */
	static constexpr size_t MAX_LISTEN_QUEUE_BACKLOG_SIZE = SOMAXCONN;

	/**
	 * Construct a closed socket with an invalid socket handle.
	 */
	Socket() noexcept
		: Socket(INVALID_SOCKET) {}

	/**
	 * Construct a socket from a native socket handle.
	 *
	 * \param handle native socket handle for this socket to adopt. Must either
	 *        be a valid socket handle or be equal to INVALID_SOCKET.
	 */
	explicit Socket(SOCKET handle) noexcept
		: handle(handle) {}

	/**
	 * Construct a new open socket.
	 *
	 * \param domain address family of the communication domain to open the
	 *        socket for.
	 * \param type communication semantics of the socket.
	 * \param errorCode error code that is filled in on failure to create the
	 *        socket, or cleared on success.
	 */
	Socket(AddressFamily domain, ProtocolType type, std::error_code& errorCode)
		: Socket() {
		open(domain, type, errorCode);
	}

	/**
	 * Construct a new open socket, throwing an exception on failure.
	 *
	 * \param domain address family of the communication domain to open the
	 *        socket for.
	 * \param type communication semantics of the socket.
	 *
	 * \throws networking::Error on failure to create the socket.
	 * \throws std::bad_alloc on allocation failure.
	 */
	Socket(AddressFamily domain, ProtocolType type)
		: Socket() {
		open(domain, type);
	}

	/**
	 * Check if this is a valid open (though not necessarily bound or connected)
	 * socket, i.e. if its handle is not equal to INVALID_SOCKET.
	 *
	 * \return `get() != INVALID_SOCKET`.
	 */
	explicit operator bool() const noexcept {
		return static_cast<bool>(handle);
	}

	/**
	 * Close the socket and set its handle to INVALID_SOCKET.
	 *
	 * \param errorCode error code that is filled in on failure to close the
	 *        socket, or cleared on success.
	 */
	GREM_API(networking) void close(std::error_code& errorCode) noexcept;

	/**
	 * Close the socket and set its handle to INVALID_SOCKET, ignoring failure.
	 */
	void close() noexcept {
		handle.reset();
	}

	/**
	 * Relinquish ownership of the socket's native handle.
	 *
	 * This socket will have its handle reset to INVALID_SOCKET without
	 * destroying/closing the socket associated with the returned handle.
	 *
	 * \return the socket handle that was released, or INVALID_SOCKET if the
	 *         socket was already closed.
	 *
	 * \warning After calling this function, the associated socket will no
	 *          longer be closed automatically along with this object. It
	 *          instead becomes the responsibility of the caller to ensure that
	 *          the socket is properly cleaned up. If the intent is to reset the
	 *          handle to INVALID_SOCKET while closing the associated socket in
	 *          the process, use close() instead.
	 *
	 * \sa close()
	 * \sa get()
	 */
	SOCKET release() noexcept {
		return handle.release();
	}

	/**
	 * Replace this socket with a newly created open socket.
	 *
	 * \param domain address family of the communication domain to open the
	 *        socket for.
	 * \param type communication semantics of the socket.
	 * \param errorCode error code that is filled in on failure to close the old
	 *        socket or create the new socket, or cleared on success.
	 *
	 * \note If the socket is already open, it will be closed before creating
	 *       the new socket. To avoid losing the old socket on failure, create a
	 *       new Socket object and move-assign it to this one instead of using
	 *       this function.
	 */
	GREM_API(networking) void open(AddressFamily domain, ProtocolType type, std::error_code& errorCode);

	/**
	 * Replace this socket with a newly created open socket, throwing an
	 * exception on failure.
	 *
	 * \param domain address family of the communication domain to open the
	 *        socket for.
	 * \param type communication semantics of the socket.
	 *
	 * \throws networking::Error on failure to close the old socket or create
	 *         the new socket.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is already open, it will be closed before creating
	 *       the new socket. To avoid losing the old socket on failure, create a
	 *       new Socket object and move-assign it to this one instead of using
	 *       this function.
	 */
	void open(AddressFamily domain, ProtocolType type) {
		std::error_code errorCode{};
		open(domain, type, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Set the blocking mode of the socket.
	 *
	 * \param newMode new blocking mode to set, or BlockingMode::UNSPECIFIED to
	 *        leave the blocking mode unchanged.
	 * \param errorCode error code that is filled in on failure to set the
	 *        blocking mode, or cleared on success.
	 *
	 * \note This function requires the socket to be in an open state (though
	 *       not necessarily bound or connected).
	 */
	GREM_API(networking) void setBlockingMode(BlockingMode newMode, std::error_code& errorCode);

	/**
	 * Set the blocking mode of the socket, throwing an exception on failure.
	 *
	 * \param newMode new blocking mode to set, or BlockingMode::UNSPECIFIED to
	 *        leave the blocking mode unchanged.
	 *
	 * \throws networking::Error on failure to set the blocking mode.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in an open state (though
	 *       not necessarily bound or connected).
	 */
	void setBlockingMode(BlockingMode newMode) {
		std::error_code errorCode{};
		setBlockingMode(newMode, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Set the receive timeout of the socket.
	 *
	 * \param newTimeout new receive timeout to set.
	 * \param errorCode error code that is filled in on failure to set the
	 *        receive timeout, or cleared on success.
	 *
	 * \note This function requires the socket to be in an open state (though
	 *       not necessarily bound or connected).
	 * \note If the socket's blocking mode is BlockingMode::NON_BLOCKING, the
	 *       receive timeout is not used.
	 */
	GREM_API(networking) void setReceiveTimeout(Duration newTimeout, std::error_code& errorCode);

	/**
	 * Set the receive timeout of the socket, throwing an exception on failure.
	 *
	 * \param newTimeout new receive timeout to set.
	 *
	 * \throws networking::Error on failure to set the receive timeout.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in an open state (though
	 *       not necessarily bound or connected).
	 * \note If the socket's blocking mode is BlockingMode::NON_BLOCKING, the
	 *       receive timeout is not used.
	 */
	void setReceiveTimeout(Duration newTimeout) {
		std::error_code errorCode{};
		setReceiveTimeout(newTimeout, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Set the send timeout of the socket.
	 *
	 * \param newTimeout new send timeout to set.
	 * \param errorCode error code that is filled in on failure to set the send
	 *        timeout, or cleared on success.
	 *
	 * \note This function requires the socket to be in an open state (though
	 *       not necessarily bound or connected).
	 * \note If the socket's blocking mode is BlockingMode::NON_BLOCKING, the
	 *       send timeout is not used.
	 */
	GREM_API(networking) void setSendTimeout(Duration newTimeout, std::error_code& errorCode);

	/**
	 * Set the send timeout of the socket, throwing an exception on failure.
	 *
	 * \param newTimeout new send timeout to set.
	 *
	 * \throws networking::Error on failure to set the send timeout.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in an open state (though
	 *       not necessarily bound or connected).
	 * \note If the socket's blocking mode is BlockingMode::NON_BLOCKING, the
	 *       send timeout is not used.
	 */
	void setSendTimeout(Duration newTimeout) {
		std::error_code errorCode{};
		setSendTimeout(newTimeout, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Initiate a connection on the socket.
	 *
	 * If the socket is of type ProtocolType::UDP, this sets the address to
	 * which packets are sent by default.
	 *
	 * If the socket is of type ProtocolType::TCP, it attempts to establish a
	 * TCP connection to the remote socket at the specified address.
	 *
	 * \param endpoint remote address, port, etc. to connect to.
	 * \param errorCode error code that is filled in on failure to connect the
	 *        socket, or cleared on success.
	 *
	 * \note This function requires the socket to be in an open state.
	 */
	GREM_API(networking) void connect(const Endpoint& endpoint, std::error_code& errorCode);

	/**
	 * Initiate a connection on the socket, throwing an exception on failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this sets the address and
	 * port to which packets are sent by default.
	 *
	 * If the socket is of type ProtocolType::TCP, it attempts to establish a
	 * TCP connection to the remote socket at the specified address and port.
	 *
	 * \param endpoint remote address, port, etc. to connect to.
	 *
	 * \throws networking::Error on failure to connect the socket.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in an open state.
	 */
	void connect(const Endpoint& endpoint) {
		std::error_code errorCode{};
		connect(endpoint, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Assign an address to the socket.
	 *
	 * If the socket is of type ProtocolType::UDP, this binds the socket to the
	 * specified local port and sets the remote address(es) to receive packets
	 * from on that port.
	 *
	 * If the socket is of type ProtocolType::TCP, this binds the socket to the
	 * specified local port and makes it possible to listen() for incoming
	 * connections from the specified remote address(es) on that port.
	 *
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to.
	 * \param errorCode error code that is filled in on failure to bind the
	 *        socket, or cleared on success.
	 *
	 * \note This function requires the socket to be in an open state.
	 */
	GREM_API(networking) void bind(const Endpoint& endpoint, std::error_code& errorCode);

	/**
	 * Assign an address to the socket, throwing an exception on failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this binds the socket to the
	 * specified local port and sets the remote address(es) to receive packets
	 * from on that port.
	 *
	 * If the socket is of type ProtocolType::TCP, this binds the socket to the
	 * specified local port and makes it possible to listen() for incoming
	 * connections from the specified remote address(es) on that port.
	 *
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to.
	 *
	 * \throws networking::Error on failure to bind the socket.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in an open state.
	 */
	void bind(const Endpoint& endpoint) {
		std::error_code errorCode{};
		bind(endpoint, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Shut down the connection on the socket in the specified direction(s),
	 * disallowing further receptions and/or transmissions.
	 *
	 * \param how direction(s) in which to shut down the connection.
	 * \param errorCode error code that is filled in on failure to shut down the
	 *        connection, or cleared on success.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	GREM_API(networking) void shutdown(ShutdownType how, std::error_code& errorCode);

	/**
	 * Shut down the connection on the socket in the specified direction(s),
	 * disallowing further receptions and/or transmissions, throwing an
	 * exception on failure.
	 *
	 * \param how direction(s) in which to shut down the connection.
	 *
	 * \throws networking::Error on failure to shut down the connection.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	void shutdown(ShutdownType how) {
		std::error_code errorCode{};
		shutdown(how, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Get the local endpoint address and port of the socket using
	 * `getsockname()`.
	 *
	 * \param errorCode error code that is filled in on failure to get the local
	 *        endpoint, or cleared on success.
	 *
	 * \return the local endpoint, or an empty optional on failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	[[nodiscard]] GREM_API(networking) Optional<Endpoint> getLocalEndpoint(std::error_code& errorCode) const;

	/**
	 * Get the local endpoint address and port of the socket using
	 * `getsockname()`, throwing an exception on failure.
	 *
	 * \return the local endpoint.
	 *
	 * \throws networking::Error on failure to get the local endpoint.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
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

	/**
	 * Get the remote endpoint address and port of the socket using
	 * `getpeername()`.
	 *
	 * \param errorCode error code that is filled in on failure to get the
	 *        remote endpoint, or cleared on success.
	 *
	 * \return the remote endpoint, or an empty optional on failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	[[nodiscard]] GREM_API(networking) Optional<Endpoint> getRemoteEndpoint(std::error_code& errorCode) const;

	/**
	 * Get the remote endpoint address and port of the socket using
	 * `getpeername()`, throwing an exception on failure.
	 *
	 * \return the remote endpoint.
	 *
	 * \throws networking::Error on failure to get the remote endpoint.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
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

	/**
	 * Start accepting incoming TCP connections on the port that the socket is
	 * currently bound to, from the remote address(es) that were specified
	 * through a previous call to bind().
	 *
	 * \param listenQueueBacklogSize maximum number of incoming connections to
	 *        allow to be queued up between calls to accept(). The value will be
	 *        clamped between 0 and Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE
	 *        (inclusive). A value of 0 is interpreted as a shorthand for
	 *        Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE.
	 * \param errorCode error code that is filled in on failure to start
	 *        listening, or cleared on success.
	 *
	 * \note This function requires the socket to be of type ProtocolType::TCP
	 *       and be in a bound state.
	 */
	GREM_API(networking) void listen(size_t listenQueueBacklogSize, std::error_code& errorCode);

	/**
	 * Start accepting incoming TCP connections on the port that the socket is
	 * currently bound to, from the remote address(es) that were specified
	 * through a previous call to bind(), throwing an exception on failure.
	 *
	 * \param listenQueueBacklogSize maximum number of incoming connections to
	 *        allow to be queued up between calls to accept(). The value will be
	 *        clamped between 0 and Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE
	 *        (inclusive). A value of 0 is interpreted as a shorthand for
	 *        Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE.
	 *
	 * \throws networking::Error on failure to start listening.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be of type ProtocolType::TCP
	 *       and be in a bound state.
	 */
	void listen(size_t listenQueueBacklogSize) {
		std::error_code errorCode{};
		listen(listenQueueBacklogSize, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Accept a single incoming TCP connection from the socket's listen queue
	 * that was previously set up by a call to listen().
	 *
	 * \param errorCode error code that is filled in on failure to accept a
	 *        connection, or cleared on success.
	 *
	 * \return the newly created TCP socket in a connected state, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note This function requires the socket to be of type ProtocolType::TCP
	 *       and be in a listening state.
	 * \note The send/receive timeout, blocking mode, and other parameters of
	 *       the new socket are unspecified. They may for example be inherited
	 *       from the accepting socket, or have default values, depending on the
	 *       underlying sockets backend. To ensure consistent behavior, the
	 *       relevant parameters should be explicitly set on the returned
	 *       socket.
	 */
	[[nodiscard]] GREM_API(networking) Optional<Socket> accept(std::error_code& errorCode);

	/**
	 * Accept a single incoming TCP connection from the socket's listen queue
	 * that was previously set up by a call to listen(), throwing an exception
	 * on failure.
	 *
	 * \return the newly created TCP socket in a connected state, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to accept a connection.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be of type ProtocolType::TCP
	 *       and be in a listening state.
	 * \note The send/receive timeout, blocking mode, and other parameters of
	 *       the new socket are unspecified. They may for example be inherited
	 *       from the accepting socket, or have default values, depending on the
	 *       underlying sockets backend. To ensure consistent behavior, the
	 *       relevant parameters should be explicitly set on the returned
	 *       socket.
	 */
	[[nodiscard]] Optional<Socket> accept() {
		std::error_code errorCode{};
		Optional<Socket> result = accept(errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	/**
	 * Get some data from the socket's incoming data queue, received from the
	 * remote address(es) specified through a previous call to bind() or
	 * connect().
	 *
	 * If the socket is of type ProtocolType::UDP, this will consume one
	 * datagram from the incoming packet queue. If the full packet does not fit
	 * in the given buffer, the remaining data in the consumed packet is
	 * discarded.
	 *
	 * If the socket is of type ProtocolType::TCP, this will consume available
	 * data from the incoming TCP stream, up to the requested number of bytes in
	 * the given buffer.
	 *
	 * \param buffer writable span to read received data into. The size of the
	 *        span determines the maximum number of bytes that are attempted to
	 *        be read.
	 * \param flags bitset of `MSG_...` flags to pass to `recv()`, or 0 for no
	 *        flags.
	 * \param errorCode error code that is filled in on failure to receive data,
	 *        or cleared on success.
	 *
	 * \return a subspan of the start of the given buffer containing the data
	 *         that was successfully read, or an empty optional on failure or
	 *         timeout.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] GREM_API(networking) Optional<Span<byte>> receive(Span<byte> buffer, MessageFlags flags, std::error_code& errorCode);

	/**
	 * Get some data from the socket's incoming data queue, received from the
	 * remote address(es) specified through a previous call to bind() or
	 * connect(), throwing an exception on failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this will consume one
	 * datagram from the incoming packet queue. If the full packet does not fit
	 * in the given buffer, the remaining data in the consumed packet is
	 * discarded.
	 *
	 * If the socket is of type ProtocolType::TCP, this will consume available
	 * data from the incoming TCP stream, up to the requested number of bytes in
	 * the given buffer.
	 *
	 * \param buffer writable span to read received data into. The size of the
	 *        span determines the maximum number of bytes that are attempted to
	 *        be read.
	 * \param flags bitset of `MSG_...` flags to pass to `recv()`, or 0 for no
	 *        flags.
	 *
	 * \return a subspan of the start of the given buffer containing the data
	 *         that was successfully read, or an empty optional on timeout.
	 *
	 * \throws networking::Error on failure to receive data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] Optional<Span<byte>> receive(Span<byte> buffer, MessageFlags flags) {
		std::error_code errorCode{};
		Optional<Span<byte>> result = receive(buffer, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	/**
	 * Get some data from the socket's incoming data queue, received from the
	 * remote address(es) specified through a previous call to bind() or
	 * connect(), using the default `recv()` flags.
	 *
	 * If the socket is of type ProtocolType::UDP, this will consume one
	 * datagram from the incoming packet queue. If the full packet does not fit
	 * in the given buffer, the remaining data in the consumed packet is
	 * discarded.
	 *
	 * If the socket is of type ProtocolType::TCP, this will consume available
	 * data from the incoming TCP stream, up to the requested number of bytes in
	 * the given buffer.
	 *
	 * \param buffer writable span to read received data into. The size of the
	 *        span determines the maximum number of bytes that are attempted to
	 *        be read.
	 * \param errorCode error code that is filled in on failure to receive data,
	 *        or cleared on success.
	 *
	 * \return a subspan of the start of the given buffer containing the data
	 *         that was successfully read, or an empty optional on failure or
	 *         timeout.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Span<byte>> receive(Span<byte> buffer, std::error_code& errorCode) {
		return receive(buffer, MessageFlags{}, errorCode);
	}

	/**
	 * Get some data from the socket's incoming data queue, received from the
	 * remote address(es) specified through a previous call to bind() or
	 * connect(), using the default `recv()` flags, throwing an exception on
	 * failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this will consume one
	 * datagram from the incoming packet queue. If the full packet does not fit
	 * in the given buffer, the remaining data in the consumed packet is
	 * discarded.
	 *
	 * If the socket is of type ProtocolType::TCP, this will consume available
	 * data from the incoming TCP stream, up to the requested number of bytes in
	 * the given buffer.
	 *
	 * \param buffer writable span to read received data into. The size of the
	 *        span determines the maximum number of bytes that are attempted to
	 *        be read.
	 *
	 * \return a subspan of the start of the given buffer containing the data
	 *         that was successfully read, or an empty optional on timeout.
	 *
	 * \throws networking::Error on failure to receive data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Span<byte>> receive(Span<byte> buffer) {
		return receive(buffer, MessageFlags{});
	}

	/**
	 * Get some data and its associated source address from the socket's
	 * incoming data queue, received from the remote address(es) specified
	 * through a previous call to bind() or connect().
	 *
	 * If the socket is of type ProtocolType::UDP, this will consume one
	 * datagram from the incoming packet queue. If the full packet does not fit
	 * in the given buffer, the remaining data in the consumed packet is
	 * discarded.
	 *
	 * If the socket is of type ProtocolType::TCP, this will consume available
	 * data from the incoming TCP stream, up to the requested number of bytes in
	 * the given buffer.
	 *
	 * \param buffer writable span to read received data into. The size of the
	 *        span determines the maximum number of bytes that are attempted to
	 *        be read.
	 * \param flags bitset of `MSG_...` flags to pass to `recvfrom()`, or 0 for
	 *        no flags.
	 * \param errorCode error code that is filled in on failure to receive data,
	 *        or cleared on success.
	 *
	 * \return an empty optional on failure or timeout, or a pair of:
	 *         - a subspan of the start of the given buffer containing the data
	 *           that was successfully read.
	 *         - the source address and port that the data was received from.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] GREM_API(networking) Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer, MessageFlags flags, std::error_code& errorCode);

	/**
	 * Get some data and its associated source address from the socket's
	 * incoming data queue, received from the remote address(es) specified
	 * through a previous call to bind() or connect(), throwing an exception on
	 * failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this will consume one
	 * datagram from the incoming packet queue. If the full packet does not fit
	 * in the given buffer, the remaining data in the consumed packet is
	 * discarded.
	 *
	 * If the socket is of type ProtocolType::TCP, this will consume available
	 * data from the incoming TCP stream, up to the requested number of bytes in
	 * the given buffer.
	 *
	 * \param buffer writable span to read received data into. The size of the
	 *        span determines the maximum number of bytes that are attempted to
	 *        be read.
	 * \param flags bitset of `MSG_...` flags to pass to `recvfrom()`, or 0 for
	 *        no flags.
	 *
	 * \return an empty optional on timeout, or a pair of:
	 *         - a subspan of the start of the given buffer containing the data
	 *           that was successfully read.
	 *         - the source address and port that the data was received from.
	 *
	 * \throws networking::Error on failure to receive data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer, MessageFlags flags) {
		std::error_code errorCode{};
		Optional<Pair<Span<byte>, Endpoint>> result = receiveFrom(buffer, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	/**
	 * Get some data and its associated source address from the socket's
	 * incoming data queue, received from the remote address(es) specified
	 * through a previous call to bind() or connect(), using the default
	 * `recvfrom()` flags.
	 *
	 * If the socket is of type ProtocolType::UDP, this will consume one
	 * datagram from the incoming packet queue. If the full packet does not fit
	 * in the given buffer, the remaining data in the consumed packet is
	 * discarded.
	 *
	 * If the socket is of type ProtocolType::TCP, this will consume available
	 * data from the incoming TCP stream, up to the requested number of bytes in
	 * the given buffer.
	 *
	 * \param buffer writable span to read received data into. The size of the
	 *        span determines the maximum number of bytes that are attempted to
	 *        be read.
	 * \param errorCode error code that is filled in on failure to receive data,
	 *        or cleared on success.
	 *
	 * \return an empty optional on failure or timeout, or a pair of:
	 *         - a subspan of the start of the given buffer containing the data
	 *           that was successfully read.
	 *         - the source address and port that the data was received from.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer, std::error_code& errorCode) {
		return receiveFrom(buffer, MessageFlags{}, errorCode);
	}

	/**
	 * Get some data and its associated source address from the socket's
	 * incoming data queue, received from the remote address(es) specified
	 * through a previous call to bind() or connect(), using the default
	 * `recvfrom()` flags, throwing an exception on failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this will consume one
	 * datagram from the incoming packet queue. If the full packet does not fit
	 * in the given buffer, the remaining data in the consumed packet is
	 * discarded.
	 *
	 * If the socket is of type ProtocolType::TCP, this will consume available
	 * data from the incoming TCP stream, up to the requested number of bytes in
	 * the given buffer.
	 *
	 * \param buffer writable span to read received data into. The size of the
	 *        span determines the maximum number of bytes that are attempted to
	 *        be read.
	 *
	 * \return an empty optional on timeout, or a pair of:
	 *         - a subspan of the start of the given buffer containing the data
	 *           that was successfully read.
	 *         - the source address and port that the data was received from.
	 *
	 * \throws networking::Error on failure to receive data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<Pair<Span<byte>, Endpoint>> receiveFrom(Span<byte> buffer) {
		return receiveFrom(buffer, MessageFlags{});
	}

	/**
	 * Write some data to the socket's outgoing data queue, to be sent to the
	 * remote address specified through a previous call to connect().
	 *
	 * If the socket is of type ProtocolType::UDP, this will produce one
	 * datagram for the outgoing packet queue.
	 *
	 * If the socket is of type ProtocolType::TCP, this will append data to the
	 * outgoing TCP stream.
	 *
	 * \param bytes data to write.
	 * \param flags bitset of `MSG_...` flags to pass to `send()`, or 0 for no
	 *        flags.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	GREM_API(networking) Optional<size_t> send(Span<const byte> bytes, MessageFlags flags, std::error_code& errorCode);

	/**
	 * Write some data to the socket's outgoing data queue, to be sent to the
	 * remote address specified through a previous call to connect(), throwing
	 * an exception on failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this will produce one
	 * datagram for the outgoing packet queue.
	 *
	 * If the socket is of type ProtocolType::TCP, this will append data to the
	 * outgoing TCP stream.
	 *
	 * \param bytes data to write.
	 * \param flags bitset of `MSG_...` flags to pass to `send()`, or 0 for no
	 *        flags.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to send data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	[[nodiscard]] Optional<size_t> send(Span<const byte> bytes, MessageFlags flags) {
		std::error_code errorCode{};
		Optional<size_t> result = send(bytes, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	/**
	 * Write some data to the socket's outgoing data queue, to be sent to the
	 * remote address specified through a previous call to connect(), using the
	 * default `send()` flags.
	 *
	 * If the socket is of type ProtocolType::UDP, this will produce one
	 * datagram for the outgoing packet queue.
	 *
	 * If the socket is of type ProtocolType::TCP, this will append data to the
	 * outgoing TCP stream.
	 *
	 * \param bytes data to write.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	GREM_ALWAYS_INLINE Optional<size_t> send(Span<const byte> bytes, std::error_code& errorCode) {
		return send(bytes, MessageFlags{}, errorCode);
	}

	/**
	 * Write some data to the socket's outgoing data queue, to be sent to the
	 * remote address specified through a previous call to connect(), using the
	 * default `send()` flags, throwing an exception on failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this will produce one
	 * datagram for the outgoing packet queue.
	 *
	 * If the socket is of type ProtocolType::TCP, this will append data to the
	 * outgoing TCP stream.
	 *
	 * \param bytes data to write.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to send data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<size_t> send(Span<const byte> bytes) {
		return send(bytes, MessageFlags{});
	}

	/**
	 * Write some data to the socket's outgoing data queue, to be sent to a
	 * given remote address.
	 *
	 * If the socket is of type ProtocolType::UDP, this will produce one
	 * datagram for the outgoing packet queue.
	 *
	 * If the socket is of type ProtocolType::TCP, this will append data to the
	 * outgoing TCP stream.
	 *
	 * \param endpoint destination address to send the data to.
	 * \param bytes data to write.
	 * \param flags bitset of `MSG_...` flags to pass to `sendto()`, or 0 for no
	 *        flags.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	GREM_API(networking) Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, MessageFlags flags, std::error_code& errorCode);

	/**
	 * Write some data to the socket's outgoing data queue, to be sent to a
	 * given remote address, throwing an exception on failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this will produce one
	 * datagram for the outgoing packet queue.
	 *
	 * If the socket is of type ProtocolType::TCP, this will append data to the
	 * outgoing TCP stream.
	 *
	 * \param endpoint destination address to send the data to.
	 * \param bytes data to write.
	 * \param flags bitset of `MSG_...` flags to pass to `sendto()`, or 0 for no
	 *        flags.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to send data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, MessageFlags flags) {
		std::error_code errorCode{};
		Optional<size_t> result = sendTo(endpoint, bytes, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	/**
	 * Write some data to the socket's outgoing data queue, to be sent to a
	 * given remote address, using the default `sendto()` flags.
	 *
	 * If the socket is of type ProtocolType::UDP, this will produce one
	 * datagram for the outgoing packet queue.
	 *
	 * If the socket is of type ProtocolType::TCP, this will append data to the
	 * outgoing TCP stream.
	 *
	 * \param endpoint destination address to send the data to.
	 * \param bytes data to write.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	GREM_ALWAYS_INLINE Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, std::error_code& errorCode) {
		return sendTo(endpoint, bytes, MessageFlags{}, errorCode);
	}

	/**
	 * Write some data to the socket's outgoing data queue, to be sent to a
	 * given remote address, using the default `sendto()` flags, throwing an
	 * exception on failure.
	 *
	 * If the socket is of type ProtocolType::UDP, this will produce one
	 * datagram for the outgoing packet queue.
	 *
	 * If the socket is of type ProtocolType::TCP, this will append data to the
	 * outgoing TCP stream.
	 *
	 * \param endpoint destination address to send the data to.
	 * \param bytes data to write.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to send data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is of type ProtocolType::UDP, this function requires
	 *       the socket to be in a bound state. If the socket is of type
	 *       ProtocolType::TCP, it is required to be in a connected state.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes) {
		return sendTo(endpoint, bytes, MessageFlags{});
	}

	/**
	 * Wait for the socket to have incoming data available to be read.
	 *
	 * \param timeout maximum duration of time to wait for. Must be
	 *        non-negative.
	 * \param errorCode error code that is filled in on failure, or cleared on
	 *        success.
	 *
	 * \note This function requires the socket to be in a bound state.
	 */
	GREM_API(networking) void awaitReadable(Duration timeout, std::error_code& errorCode);

	/**
	 * Wait for the socket to have incoming data available to be read, throwing
	 * an exception on failure or timeout.
	 *
	 * \param timeout maximum duration of time to wait for. Must be
	 *        non-negative.
	 *
	 * \throws networking::Error on failure to await, or timeout.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a bound state.
	 */
	void awaitReadable(Duration timeout) {
		std::error_code errorCode{};
		awaitReadable(timeout, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Wait for the socket to become ready to write outgoing data to.
	 *
	 * \param timeout maximum duration of time to wait for. Must be
	 *        non-negative.
	 * \param errorCode error code that is filled in on failure, or cleared on
	 *        success.
	 *
	 * \note This function requires the socket to be in a bound state.
	 */
	GREM_API(networking) void awaitWritable(Duration timeout, std::error_code& errorCode);

	/**
	 * Wait for the socket to become ready to write outgoing data to, throwing
	 * an exception on failure or timeout.
	 *
	 * \param timeout maximum duration of time to wait for. Must be
	 *        non-negative.
	 *
	 * \throws networking::Error on failure to await, or timeout.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a bound state.
	 */
	void awaitWritable(Duration timeout) {
		std::error_code errorCode{};
		awaitWritable(timeout, errorCode);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Get the native handle of the socket.
	 *
	 * \return the native socket handle, or INVALID_SOCKET if the socket is in a
	 *         closed state.
	 *
	 * \note This function does NOT relinquish ownership of the socket handle.
	 *       To acquire ownership of the handle, use release() instead.
	 */
	[[nodiscard]] SOCKET get() const noexcept {
		return handle.get();
	}

private:
	struct SocketDeleter {
		GREM_API(networking) void operator()(SOCKET handle) const noexcept;
	};

	UniqueHandle<SOCKET, SocketDeleter, INVALID_SOCKET> handle;
};

/**
 * Configuration options for a UDPSocket.
 */
struct UDPSocketOptions {};

/**
 * Connectionless socket for receiving and transmitting unordered, unreliable
 * datagrams across the network over ProtocolType::UDP.
 */
class UDPSocket : private Socket {
public:
	/**
	 * Construct a closed UDP socket with an invalid socket handle.
	 */
	UDPSocket() noexcept
		: UDPSocket(INVALID_SOCKET) {}

	/**
	 * Construct a UDP socket from a native socket handle.
	 *
	 * \param handle native socket handle for this socket to adopt. Must either
	 *        be a valid socket handle of type ProtocolType::UDP (SOCK_DGRAM) or
	 *        equal to INVALID_SOCKET.
	 */
	explicit UDPSocket(SOCKET handle) noexcept
		: Socket(handle) {}

	/**
	 * Construct a new open UDP socket bound to a specific local port, and set
	 * the remote address(es) to receive packets from on that port.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to. The address family of the endpoint determines the
	 *        communication domain to open the socket for.
	 * \param errorCode error code that is filled in on failure to create/bind
	 *        the socket, or cleared on success.
	 * \param options socket options, see UDPSocketOptions.
	 */
	UDPSocket(BlockingMode mode, const Endpoint& endpoint, std::error_code& errorCode, const UDPSocketOptions& options = {})
		: UDPSocket() {
		bind(mode, endpoint, errorCode, options);
	}

	/**
	 * Construct a new open UDP socket bound to a specific local port, and set
	 * the remote address(es) to receive packets from on that port, throwing an
	 * exception on failure.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to. The address family of the endpoint determines the
	 *        communication domain to open the socket for.
	 * \param options socket options, see UDPSocketOptions.
	 *
	 * \throws networking::Error on failure to create/bind the socket.
	 * \throws std::bad_alloc on allocation failure.
	 */
	UDPSocket(BlockingMode mode, const Endpoint& endpoint, const UDPSocketOptions& options = {})
		: UDPSocket() {
		bind(mode, endpoint, options);
	}

	using Socket::operator bool;
	using Socket::close;
	using Socket::get;
	using Socket::getLocalEndpoint;
	using Socket::receiveFrom;
	using Socket::release;
	using Socket::sendTo;
	using Socket::setBlockingMode;
	using Socket::setReceiveTimeout;
	using Socket::setSendTimeout;

	/**
	 * Replace this socket with a newly created open socket bound to a specific
	 * local port, and set the remote address(es) to receive packets from on
	 * that port.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to. The address family of the endpoint determines the
	 *        communication domain to open the socket for.
	 * \param errorCode error code that is filled in on failure to close the old
	 *        socket or create/bind the new socket, or cleared on success.
	 * \param options socket options, see UDPSocketOptions.
	 *
	 * \note If the socket is already open, it will be closed before creating
	 *       the new socket. To avoid losing the old socket on failure, create a
	 *       new Socket object and move-assign it to this one instead of using
	 *       this function.
	 */
	void bind(BlockingMode mode, const Endpoint& endpoint, std::error_code& errorCode, const UDPSocketOptions& options = {}) {
		(void)options;
		if (*this) {
			Socket::close(errorCode);
			if (errorCode) {
				return;
			}
		}
		Socket::open(endpoint.getAddressFamily(), ProtocolType::UDP, errorCode);
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

	/**
	 * Replace this socket with a newly created open socket bound to a specific
	 * local port, and set the remote address(es) to receive packets from on
	 * that port, throwing an exception on failure.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to. The address family of the endpoint determines the
	 *        communication domain to open the socket for.
	 * \param options socket options, see UDPSocketOptions.
	 *
	 * \throws networking::Error on failure to close the old socket or
	 *         create/bind the new socket.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is already open, it will be closed before creating
	 *       the new socket. To avoid losing the old socket on failure, create a
	 *       new Socket object and move-assign it to this one instead of using
	 *       this function.
	 */
	void bind(BlockingMode mode, const Endpoint& endpoint, const UDPSocketOptions& options = {}) {
		std::error_code errorCode{};
		bind(mode, endpoint, errorCode, options);
		if (errorCode) {
			throw networking::Error{errorCode};
		}
	}
};

/**
 * Configuration options for a TCPSocket.
 */
struct TCPSocketOptions {
	/**
	 * How long to wait for the connection to be established when the socket is
	 * created.
	 *
	 * \warning Must be non-negative.
	 */
	Duration connectionTimeout{};
};

/**
 * Connection-oriented socket for receiving and transmitting ordered, reliable
 * streams of data across the network over ProtocolType::TCP.
 */
class TCPSocket : private Socket {
public:
	/**
	 * Construct a closed TCP socket with an invalid socket handle.
	 */
	TCPSocket() noexcept
		: TCPSocket(INVALID_SOCKET) {}

	/**
	 * Construct a TCP socket from a native socket handle.
	 *
	 * \param handle native socket handle for this socket to adopt. Must either
	 *        be a valid socket handle of type ProtocolType::TCP (SOCK_STREAM)
	 *        or be equal to INVALID_SOCKET.
	 */
	explicit TCPSocket(SOCKET handle) noexcept
		: Socket(handle) {}

	/**
	 * Construct a new open TCP socket and establish a TCP connection to the
	 * remote socket at the specified address and port.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address, port, etc. to connect to. The address
	 *        family of the endpoint determines the communication domain to open
	 *        the socket for.
	 * \param errorCode error code that is filled in on failure to
	 *        create/connect the socket, or cleared on success.
	 * \param options socket options, see TCPSocketOptions.
	 */
	TCPSocket(BlockingMode mode, const Endpoint& endpoint, std::error_code& errorCode, const TCPSocketOptions& options = {})
		: TCPSocket() {
		connect(mode, endpoint, errorCode, options);
	}

	/**
	 * Construct a new open TCP socket and establish a TCP connection to the
	 * remote socket at the specified address and port, throwing an exception on
	 * failure.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address, port, etc. to connect to. The address
	 *        family of the endpoint determines the communication domain to open
	 *        the socket for.
	 * \param options socket options, see TCPSocketOptions.
	 *
	 * \throws networking::Error on failure to create/connect the socket.
	 * \throws std::bad_alloc on allocation failure.
	 */
	TCPSocket(BlockingMode mode, const Endpoint& endpoint, const TCPSocketOptions& options = {})
		: TCPSocket() {
		connect(mode, endpoint, options);
	}

	using Socket::operator bool;
	using Socket::close;
	using Socket::get;
	using Socket::getLocalEndpoint;
	using Socket::getRemoteEndpoint;
	using Socket::receive;
	using Socket::receiveFrom;
	using Socket::release;
	using Socket::setBlockingMode;
	using Socket::setReceiveTimeout;
	using Socket::setSendTimeout;

	/**
	 * Replace this socket with a newly created open socket and establish a TCP
	 * connection to the remote socket at the specified address and port.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address, port, etc. to connect to. The address
	 *        family of the endpoint determines the communication domain to open
	 *        the socket for.
	 * \param errorCode error code that is filled in on failure to close the old
	 *        socket or create/connect the new socket, or cleared on success.
	 * \param options socket options, see TCPSocketOptions.
	 *
	 * \note If the socket is already open, it will be closed before creating
	 *       the new socket. To avoid losing the old socket on failure, create a
	 *       new Socket object and move-assign it to this one instead of using
	 *       this function.
	 */
	GREM_API(networking) void connect(BlockingMode mode, const Endpoint& endpoint, std::error_code& errorCode, const TCPSocketOptions& options = {});

	/**
	 * Replace this socket with a newly created open socket and establish a TCP
	 * connection to the remote socket at the specified address and port,
	 * throwing an exception on failure.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address, port, etc. to connect to. The address
	 *        family of the endpoint determines the communication domain to open
	 *        the socket for.
	 * \param options socket options, see TCPSocketOptions.
	 *
	 * \throws networking::Error on failure to close the old socket or
	 *         create/connect the new socket.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is already open, it will be closed before creating
	 *       the new socket. To avoid losing the old socket on failure, create a
	 *       new Socket object and move-assign it to this one instead of using
	 *       this function.
	 */
	void connect(BlockingMode mode, const Endpoint& endpoint, const TCPSocketOptions& options = {}) {
		std::error_code errorCode{};
		connect(mode, endpoint, errorCode, options);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Shut down the connection on the socket in the outgoing direction,
	 * disallowing further transmissions, and initiate a graceful shutdown of
	 * the TCP connection.
	 *
	 * \param errorCode error code that is filled in on failure to shut down the
	 *        connection, or cleared on success.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	void disconnect(std::error_code& errorCode) {
		shutdown(ShutdownType::SEND, errorCode);
	}

	/**
	 * Shut down the connection on the socket in the outgoing direction,
	 * disallowing further transmissions, and initiate a graceful shutdown of
	 * the TCP connection, throwing an exception on failure.
	 *
	 * \throws networking::Error on failure to shut down the connection.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 */
	void disconnect() {
		shutdown(ShutdownType::SEND);
	}

	/**
	 * Appends some data to the socket's outgoing TCP stream, to be sent to the
	 * remote address specified through a previous call to connect().
	 *
	 * \param bytes data to write.
	 * \param flags bitset of `MSG_...` flags to pass to `send()`, or 0 for no
	 *        flags.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note This function requires the socket to be in a connected state.
	 * \note Unlike the regular Socket::send(), this TCPSocket version ensures
	 *       that all of the specified data is written before returning, unless
	 *       there is an error.
	 */
	Optional<size_t> send(Span<const byte> bytes, MessageFlags flags, std::error_code& errorCode) {
		size_t bytesSent = 0;
		while (bytesSent < bytes.size()) {
			const Optional<size_t> chunkBytesSent = Socket::send(bytes.subspan(bytesSent), flags, errorCode);
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

	/**
	 * Append some data to the socket's outgoing TCP stream, to be sent to the
	 * remote address specified through a previous call to connect(), throwing
	 * an exception on failure.
	 *
	 * \param bytes data to write.
	 * \param flags bitset of `MSG_...` flags to pass to `send()`, or 0 for no
	 *        flags.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to send data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 * \note Unlike the regular Socket::send(), this TCPSocket version ensures
	 *       that all of the specified data is written before returning, unless
	 *       there is an error.
	 */
	[[nodiscard]] Optional<size_t> send(Span<const byte> bytes, MessageFlags flags) {
		std::error_code errorCode{};
		Optional<size_t> result = send(bytes, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT && errorCode != SocketError::PARTIAL) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	/**
	 * Append some data to the socket's outgoing TCP stream, to be sent to the
	 * remote address specified through a previous call to connect(), using the
	 * default `send()` flags.
	 *
	 * \param bytes data to write.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note This function requires the socket to be in a connected state.
	 * \note Unlike the regular Socket::send(), this TCPSocket version ensures
	 *       that all of the specified data is written before returning, unless
	 *       there is an error.
	 */
	GREM_ALWAYS_INLINE Optional<size_t> send(Span<const byte> bytes, std::error_code& errorCode) {
		return send(bytes, DEFAULT_TCP_SEND_FLAGS, errorCode);
	}

	/**
	 * Append some data to the socket's outgoing TCP stream, to be sent to the
	 * remote address specified through a previous call to connect(), using the
	 * default `send()` flags, throwing an exception on failure.
	 *
	 * \param bytes data to write.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to send data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 * \note Unlike the regular Socket::send(), this TCPSocket version ensures
	 *       that all of the specified data is written before returning, unless
	 *       there is an error.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<size_t> send(Span<const byte> bytes) {
		return send(bytes, DEFAULT_TCP_SEND_FLAGS);
	}

	/**
	 * Append some data to the socket's outgoing TCP stream, to be sent to a
	 * given remote address.
	 *
	 * \param endpoint destination address to send the data to.
	 * \param bytes data to write.
	 * \param flags bitset of `MSG_...` flags to pass to `sendto()`, or 0 for no
	 *        flags.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note This function requires the socket to be in a connected state.
	 * \note Unlike the regular Socket::sendTo(), this TCPSocket version ensures
	 *       that all of the specified data is written before returning, unless
	 *       there is an error.
	 */
	Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, MessageFlags flags, std::error_code& errorCode) {
		size_t bytesSent = 0;
		while (bytesSent < bytes.size()) {
			const Optional<size_t> chunkBytesSent = Socket::sendTo(endpoint, bytes.subspan(bytesSent), flags, errorCode);
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

	/**
	 * Append some data to the socket's outgoing TCP stream, to be sent to a
	 * given remote address, throwing an exception on failure.
	 *
	 * \param endpoint destination address to send the data to.
	 * \param bytes data to write.
	 * \param flags bitset of `MSG_...` flags to pass to `sendto()`, or 0 for no
	 *        flags.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to send data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 * \note Unlike the regular Socket::sendTo(), this TCPSocket version ensures
	 *       that all of the specified data is written before returning, unless
	 *       there is an error.
	 */
	[[nodiscard]] Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, MessageFlags flags) {
		std::error_code errorCode{};
		Optional<size_t> result = sendTo(endpoint, bytes, flags, errorCode);
		if (errorCode && errorCode != SocketError::WAIT && errorCode != SocketError::PARTIAL) {
			throw networking::Error{errorCode};
		}
		return result;
	}

	/**
	 * Append some data to the socket's outgoing TCP stream, to be sent to a
	 * given remote address, using the default `sendto()` flags.
	 *
	 * \param endpoint destination address to send the data to.
	 * \param bytes data to write.
	 * \param errorCode error code that is filled in on failure to send data, or
	 *        cleared on success.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note This function requires the socket to be in a connected state.
	 * \note Unlike the regular Socket::sendTo(), this TCPSocket version ensures
	 *       that all of the specified data is written before returning, unless
	 *       there is an error.
	 */
	GREM_ALWAYS_INLINE Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes, std::error_code& errorCode) {
		return sendTo(endpoint, bytes, DEFAULT_TCP_SEND_FLAGS, errorCode);
	}

	/**
	 * Append some data to the socket's outgoing TCP stream, to be sent to a
	 * given remote address, using the default `sendto()` flags, throwing an
	 * exception on failure.
	 *
	 * \param endpoint destination address to send the data to.
	 * \param bytes data to write.
	 *
	 * \return the number of bytes that were successfully written, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to send data.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a connected state.
	 * \note Unlike the regular Socket::sendTo(), this TCPSocket version ensures
	 *       that all of the specified data is written before returning, unless
	 *       there is an error.
	 */
	[[nodiscard]] GREM_ALWAYS_INLINE Optional<size_t> sendTo(const Endpoint& endpoint, Span<const byte> bytes) {
		return sendTo(endpoint, bytes, DEFAULT_TCP_SEND_FLAGS);
	}

private:
#ifdef _WIN32
	static constexpr MessageFlags DEFAULT_TCP_SEND_FLAGS{};
#else
	static constexpr MessageFlags DEFAULT_TCP_SEND_FLAGS{MSG_NOSIGNAL};
#endif
};

/**
 * Configuration options for a TCPListener.
 */
struct TCPListenerOptions {
	/**
	 * Maximum number of incoming connections to allow to be queued up between
	 * calls to accept().
	 *
	 * The value will be clamped between 0 and
	 * TCPListener::MAX_LISTEN_QUEUE_BACKLOG_SIZE (inclusive).
	 *
	 * A value of 0 is interpreted as a shorthand for
	 * TCPListener::MAX_LISTEN_QUEUE_BACKLOG_SIZE.
	 */
	size_t listenQueueBacklogSize = 0;
};

/**
 * Listener socket for accepting incoming connections across the network over
 * ProtocolType::TCP.
 */
class TCPListener : private Socket {
public:
	using Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE;

	/**
	 * Construct a closed TCP listener with an invalid socket handle.
	 */
	TCPListener() noexcept
		: TCPListener(INVALID_SOCKET) {}

	/**
	 * Construct a TCP listener from a native socket handle.
	 *
	 * \param handle native socket handle for this socket to adopt. Must either
	 *        be a valid socket handle of type ProtocolType::TCP (SOCK_STREAM)
	 *        or be equal to INVALID_SOCKET.
	 */
	explicit TCPListener(SOCKET handle) noexcept
		: Socket(handle) {}

	/**
	 * Construct a new open TCP listener bound to a specific local port, and
	 * start accepting incoming TCP connections from the specified remote
	 * address(es) on that port.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to. The address family of the endpoint determines the
	 *        communication domain to open the socket for.
	 * \param errorCode error code that is filled in on failure to
	 *        create/bind the socket or start listening, or cleared on success.
	 * \param options listener options, see TCPListenerOptions.
	 */
	TCPListener(BlockingMode mode, const Endpoint& endpoint, std::error_code& errorCode, const TCPListenerOptions& options = {})
		: TCPListener() {
		listen(mode, endpoint, errorCode, options);
	}

	/**
	 * Construct a new open TCP listener bound to a specific local port, and
	 * start accepting incoming TCP connections from the specified remote
	 * address(es) on that port, throwing an exception on failure.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to. The address family of the endpoint determines the
	 *        communication domain to open the socket for.
	 * \param options listener options, see TCPListenerOptions.
	 *
	 * \throws networking::Error on failure to create/bind the socket or start
	 *         listening.
	 * \throws std::bad_alloc on allocation failure.
	 */
	TCPListener(BlockingMode mode, const Endpoint& endpoint, const TCPListenerOptions& options = {})
		: TCPListener() {
		listen(mode, endpoint, options);
	}

	using Socket::operator bool;
	using Socket::close;
	using Socket::get;
	using Socket::getLocalEndpoint;
	using Socket::release;
	using Socket::setBlockingMode;

	/**
	 * Replace this socket with a newly created open socket bound to a specific
	 * local port, and start accepting incoming TCP connections from the
	 * specified remote address(es) on that port.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to. The address family of the endpoint determines the
	 *        communication domain to open the socket for.
	 * \param errorCode error code that is filled in on failure to close the old
	 *        socket or create/bind the new socket, or cleared on success.
	 * \param options listener options, see TCPListenerOptions.
	 *
	 * \note If the socket is already open, it will be closed before creating
	 *       the new socket. To avoid losing the old socket on failure, create a
	 *       new Socket object and move-assign it to this one instead of using
	 *       this function.
	 */
	void listen(BlockingMode mode, const Endpoint& endpoint, std::error_code& errorCode, const TCPListenerOptions& options = {}) {
		if (*this) {
			Socket::close(errorCode);
			if (errorCode) {
				return;
			}
		}
		Socket::open(endpoint.getAddressFamily(), ProtocolType::TCP, errorCode);
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
		Socket::listen(options.listenQueueBacklogSize, errorCode);
	}

	/**
	 * Replace this socket with a newly created open socket bound to a specific
	 * local port, and start accepting incoming TCP connections from the
	 * specified remote address(es) on that port, throwing an exception on
	 * failure.
	 *
	 * \param mode blocking mode of the new socket.
	 * \param endpoint remote address(es) to receive from and local port to bind
	 *        the socket to. The address family of the endpoint determines the
	 *        communication domain to open the socket for.
	 * \param options listener options, see TCPListenerOptions.
	 *
	 * \throws networking::Error on failure to close the old socket or
	 *         create/bind the new socket or start listening.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note If the socket is already open, it will be closed before creating
	 *       the new socket. To avoid losing the old socket on failure, create a
	 *       new Socket object and move-assign it to this one instead of using
	 *       this function.
	 */
	void listen(BlockingMode mode, const Endpoint& endpoint, const TCPListenerOptions& options = {}) {
		std::error_code errorCode{};
		listen(mode, endpoint, errorCode, options);
		if (errorCode && errorCode != SocketError::WAIT) {
			throw networking::Error{errorCode};
		}
	}

	/**
	 * Accept a single incoming TCP connection from the socket's listen queue
	 * that was previously set up by a call to listen().
	 *
	 * \param errorCode error code that is filled in on failure to accept a
	 *        connection, or cleared on success.
	 *
	 * \return the newly created TCP socket in a connected state, or an empty
	 *         optional on failure or timeout.
	 *
	 * \note This function requires the socket to be in a listening state.
	 * \note The send/receive timeout, blocking mode, and other parameters of
	 *       the new socket are unspecified. They may for example be inherited
	 *       from the accepting socket, or have default values, depending on the
	 *       underlying sockets backend. To ensure consistent behavior, the
	 *       relevant parameters should be explicitly set on the returned
	 *       socket.
	 */
	[[nodiscard]] Optional<TCPSocket> accept(std::error_code& errorCode) {
		return Socket::accept(errorCode).transform([](Socket socket) -> TCPSocket { return TCPSocket{socket.release()}; });
	}

	/**
	 * Accept a single incoming TCP connection from the socket's listen queue
	 * that was previously set up by a call to listen(), throwing an exception
	 * on failure.
	 *
	 * \return the newly created TCP socket in a connected state, or an empty
	 *         optional on timeout.
	 *
	 * \throws networking::Error on failure to accept a connection.
	 * \throws std::bad_alloc on allocation failure.
	 *
	 * \note This function requires the socket to be in a listening state.
	 * \note The send/receive timeout, blocking mode, and other parameters of
	 *       the new socket are unspecified. They may for example be inherited
	 *       from the accepting socket, or have default values, depending on the
	 *       underlying sockets backend. To ensure consistent behavior, the
	 *       relevant parameters should be explicitly set on the returned
	 *       socket.
	 */
	[[nodiscard]] Optional<TCPSocket> accept() {
		return Socket::accept().transform([](Socket socket) -> TCPSocket { return TCPSocket{socket.release()}; });
	}
};

} // namespace grem::networking

#endif
