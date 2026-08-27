// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/InplaceDoubleEndedQueue.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Pair.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/SharedPointer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Subrange.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/synchronization.hpp>
#include <GREM/core/time.hpp>
#include <GREM/networking/Endpoint.hpp>
#include <GREM/networking/Error.hpp>
#include <GREM/networking/Socket.hpp>
#include <GREM/networking/platform.hpp>

#include <system_error> // std::error_code, std::errc, std::make_error_code(std::errc)
#include <type_traits>  // std::make_unsigned_t
#include <utility>      // std::move

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/system/Thread.hpp>
#endif

namespace grem::networking {

namespace {

class EmulatedSockets {
public:
	[[nodiscard]] static EmulatedSockets& getInstance() noexcept {
		static EmulatedSockets instance{};
		return instance;
	}

	void close(SOCKET handle, std::error_code& errorCode) noexcept {
		if (handle == INVALID_SOCKET) {
			errorCode = make_error_code(std::errc::bad_file_descriptor);
			return;
		}
		SharedPointer<EmulatedSocket> socket{};
		{
			ScopedLock lock{socketsMutex};
			const auto it = sockets.find(handle);
			if (it == sockets.end()) {
				errorCode = make_error_code(std::errc::bad_file_descriptor);
				return;
			}
			socket = it->second;
			sockets.erase(it);
		}
		PortNumber boundPortNumber = 0;
		{
			ScopedLock lock{socket->mutex};
			if (socket->localEndpoint) {
				boundPortNumber = socket->localEndpoint->getPortNumber();
			}
		}
		if (boundPortNumber != 0) {
			ScopedLock lock{socketsMutex};
			if (const auto itPort = ports.find(boundPortNumber); itPort != ports.end()) {
				erase_if(itPort->second.ipv4Bindings, [&](const EmulatedPortIPv4Binding& binding) -> bool { return binding.socketHandle == handle; });
				erase_if(itPort->second.ipv6Bindings, [&](const EmulatedPortIPv6Binding& binding) -> bool { return binding.socketHandle == handle; });
				if (itPort->second.ipv4Bindings.empty() && itPort->second.ipv6Bindings.empty()) {
					ports.erase(itPort);
				}
			}
		}
		errorCode.clear();
	}

	[[nodiscard]] SOCKET open(EndpointFamily domain, ProtocolType type, std::error_code& errorCode) {
		ScopedLock lock{socketsMutex};
		SOCKET newHandle = bit_cast<SOCKET>(nextSocketHandleValue++);
		if (newHandle == INVALID_SOCKET) {
			newHandle = bit_cast<SOCKET>(nextSocketHandleValue++);
		}
		try {
			SharedPointer<EmulatedSocket> newSocket = SharedPointer<EmulatedSocket>::create(domain, type);
			if (!sockets.try_emplace(newHandle, std::move(newSocket)).second) {
				errorCode = make_error_code(std::errc::resource_unavailable_try_again);
				return INVALID_SOCKET;
			}
		} catch (...) {
			errorCode = make_error_code(std::errc::not_enough_memory);
			return INVALID_SOCKET;
		}
		errorCode.clear();
		return newHandle;
	}

	void setBlockingMode(SOCKET handle, BlockingMode mode, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			ScopedLock lock{socket->mutex};
			socket->blockingMode = mode;
		}
	}

	void setReceiveTimeout(SOCKET handle, Duration timeout, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			ScopedLock lock{socket->mutex};
			socket->receiveTimeout = timeout;
		}
	}

	void setSendTimeout(SOCKET handle, Duration timeout, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			ScopedLock lock{socket->mutex};
			socket->sendTimeout = timeout;
		}
	}

	void connect(SOCKET handle, const Endpoint& endpoint, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			socket->connect(lock, handle, endpoint, errorCode);
		}
	}

	void bind(SOCKET handle, const Endpoint& endpoint, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			socket->bind(lock, handle, endpoint, errorCode);
		}
	}

	void shutdown(SOCKET handle, ShutdownType how, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			socket->shutdown(lock, how, errorCode);
		}
	}

	[[nodiscard]] Optional<Endpoint> getLocalEndpoint(SOCKET handle, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			ScopedLock lock{socket->mutex};
			if (socket->localEndpoint) {
				return socket->localEndpoint;
			}
			return Endpoint::any(socket->domain);
		}
		return {};
	}

	[[nodiscard]] Optional<Endpoint> getRemoteEndpoint(SOCKET handle, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			ScopedLock lock{socket->mutex};
			if (socket->remoteEndpoint) {
				return socket->remoteEndpoint;
			}
			return Endpoint::any(socket->domain);
		}
		return {};
	}

	void listen(SOCKET handle, size_t listenQueueBacklogSize, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			socket->listen(lock, handle, listenQueueBacklogSize, errorCode);
		}
	}

	[[nodiscard]] SOCKET accept(SOCKET handle, BlockingMode mode, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			return socket->accept(lock, handle, mode, errorCode);
		}
		return INVALID_SOCKET;
	}

	Optional<Span<byte>> receive(SOCKET handle, Span<byte> buffer, int flags, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			return socket->receive(lock, buffer, flags, errorCode);
		}
		return {};
	}

	Optional<Pair<Span<byte>, Endpoint>> receiveFrom(SOCKET handle, Span<byte> buffer, int flags, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			return socket->receiveFrom(lock, buffer, flags, errorCode);
		}
		return {};
	}

	[[nodiscard]] Optional<size_t> send(SOCKET handle, Span<const byte> bytes, int flags, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			return socket->send(lock, handle, bytes, flags, errorCode);
		}
		return {};
	}

	[[nodiscard]] Optional<size_t> sendTo(SOCKET handle, const Endpoint& endpoint, Span<const byte> bytes, int flags, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			return socket->sendTo(lock, handle, endpoint, bytes, flags, errorCode);
		}
		return {};
	}

	void awaitReadable(SOCKET handle, Duration timeout, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			socket->awaitReadable(lock, timeout, errorCode);
		}
	}

	void awaitWritable(SOCKET handle, Duration timeout, std::error_code& errorCode) {
		if (const SharedPointer<EmulatedSocket> socket = getSocket(handle, errorCode)) {
			UniqueLock lock{socket->mutex};
			socket->awaitWritable(lock, timeout, errorCode);
		}
	}

private:
	static constexpr size_t MAX_RECEIVED_PACKET_SIZE = 1500;
	static constexpr PortNumber LOCAL_PORT_RANGE_MIN = 32768;
	static constexpr PortNumber LOCAL_PORT_RANGE_MAX = 60999;
	static constexpr size_t LOCAL_PORT_COUNT = 1 + LOCAL_PORT_RANGE_MAX - LOCAL_PORT_RANGE_MIN;
	static constexpr int SUPPORTED_RECEIVE_FLAGS = 0;
#ifdef _WIN32
	static constexpr int SUPPORTED_SEND_FLAGS = 0;
#else
	static constexpr int SUPPORTED_SEND_FLAGS = MSG_NOSIGNAL;
#endif

	using SocketHandleValue = std::make_unsigned_t<SOCKET>;

	enum class EmulatedTCPState : uint8_t {
		CLOSED,
		LISTEN,
		SYN_SENT,
		ESTABLISHED,
		TIME_WAIT,
		LAST_ACK,
	};

	struct EmulatedPortIPv4Binding {
		IPv4Address address;
		SOCKET socketHandle;
	};

	struct EmulatedPortIPv6Binding {
		IPv6Address address;
		SOCKET socketHandle;
	};

	struct EmulatedPort {
		ArrayList<EmulatedPortIPv4Binding> ipv4Bindings{};
		ArrayList<EmulatedPortIPv6Binding> ipv6Bindings{};
	};

	struct EmulatedReceivedPacket {
		Optional<Endpoint> sender{};
		size_t readOffset = 0;
		InplaceArrayList<byte, MAX_RECEIVED_PACKET_SIZE> data;
	};

	struct EmulatedSocket {
		const EndpointFamily domain;
		const ProtocolType type;
		BlockingMode blockingMode = BlockingMode::BLOCKING;
		Duration receiveTimeout{};
		Duration sendTimeout{};
		Optional<Endpoint> localEndpoint{};
		Optional<Endpoint> remoteEndpoint{};
		SOCKET remoteSocketHandle = INVALID_SOCKET;
		size_t listenQueueBacklogSize = 0;
		InplaceDoubleEndedQueue<SOCKET, Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE> listenQueue{};
		RingBuffer<EmulatedReceivedPacket> receivedPackets{};
		EmulatedTCPState tcpState = EmulatedTCPState::CLOSED;
		bool shutdownReceptions = false;
		bool shutdownTransmissions = false;
		Mutex mutex{};

		EmulatedSocket(EndpointFamily domain, ProtocolType type)
			: domain(domain)
			, type(type) {}

		void connect(UniqueLock<Mutex>& lock, SOCKET handle, const Endpoint& endpoint, std::error_code& errorCode) {
			if (endpoint.getFamily() != domain) {
				errorCode = make_error_code(std::errc::invalid_argument);
				return;
			}

			if (type != ProtocolType::TCP) {
				remoteEndpoint = endpoint;
				errorCode.clear();
				return;
			}

			switch (tcpState) {
				case EmulatedTCPState::CLOSED: break;
				case EmulatedTCPState::LISTEN:
					listenQueueBacklogSize = 0;
					listenQueue.clear();
					tcpState = EmulatedTCPState::CLOSED;
					break;
				case EmulatedTCPState::SYN_SENT: errorCode = make_error_code(std::errc::connection_already_in_progress); return;
				case EmulatedTCPState::ESTABLISHED: errorCode = make_error_code(std::errc::already_connected); return;
				case EmulatedTCPState::TIME_WAIT: [[fallthrough]];
				case EmulatedTCPState::LAST_ACK:
					remoteSocketHandle = INVALID_SOCKET;
					tcpState = EmulatedTCPState::CLOSED;
					shutdownReceptions = false;
					shutdownTransmissions = false;
					break;
			}

			if (!localEndpoint) {
				bind(lock, handle, Endpoint::any(domain), errorCode);
				if (errorCode) {
					return;
				}
			}

			lock.unlock();

			const Optional<EmulatedRemoteSocket> remote = EmulatedSockets::getInstance().findRemoteSocket(endpoint, errorCode);
			if (errorCode) {
				return;
			}
			if (!remote) {
				errorCode = make_error_code(std::errc::connection_refused);
				return;
			}

			SOCKET remoteHandle = INVALID_SOCKET;
			{
				ScopedLock remoteLock{remote->socket->mutex};
				if (remote->socket->domain != domain || remote->socket->type != ProtocolType::TCP || remote->socket->tcpState != EmulatedTCPState::LISTEN ||
					remote->socket->listenQueue.size() >= remote->socket->listenQueueBacklogSize) {
					errorCode = make_error_code(std::errc::connection_refused);
					return;
				}
				try {
					remote->socket->listenQueue.push_back(handle);
				} catch (...) {
					errorCode = make_error_code(std::errc::not_enough_memory);
					return;
				}
				remoteHandle = remote->socketHandle;
			}
			lock.lock();

			remoteSocketHandle = remoteHandle;
			tcpState = EmulatedTCPState::SYN_SENT;

			remoteEndpoint = endpoint;
		}

		void bind(UniqueLock<Mutex>& lock, SOCKET handle, const Endpoint& endpoint, std::error_code& errorCode) {
			if (endpoint.getFamily() != domain || localEndpoint) {
				errorCode = make_error_code(std::errc::invalid_argument);
				return;
			}
			lock.unlock();
			const Optional<Endpoint> boundEndpoint = EmulatedSockets::getInstance().bindPort(endpoint, handle, errorCode);
			if (!boundEndpoint) {
				return;
			}
			lock.lock();
			localEndpoint = *boundEndpoint;
			errorCode.clear();
		}

		void shutdown(UniqueLock<Mutex>& lock, ShutdownType how, std::error_code& errorCode) {
			if (type == ProtocolType::TCP) {
				listenQueueBacklogSize = 0;
				listenQueue.clear();
				switch (how) {
					case ShutdownType::RECEIVE:
						shutdownReceptions = true;
						receivedPackets.clear();
						tcpState = EmulatedTCPState::CLOSED;
						break;
					case ShutdownType::SEND: {
						if (tcpState == EmulatedTCPState::ESTABLISHED) {
							lock.unlock();
							const SharedPointer<EmulatedSocket> remoteSocket = EmulatedSockets::getInstance().getSocket(remoteSocketHandle, errorCode);
							if (!remoteSocket) {
								errorCode = make_error_code(std::errc::not_connected);
								return;
							}
							{
								ScopedLock remoteLock{remoteSocket->mutex};
								if (remoteSocket->tcpState != EmulatedTCPState::CLOSED) {
									remoteSocket->remoteSocketHandle = INVALID_SOCKET;
									remoteSocket->tcpState = EmulatedTCPState::LAST_ACK;
								}
							}
							lock.lock();
							tcpState = EmulatedTCPState::TIME_WAIT;
						} else {
							tcpState = EmulatedTCPState::CLOSED;
						}
						shutdownTransmissions = true;
						break;
					}
					case ShutdownType::BOTH:
						shutdownReceptions = true;
						shutdownTransmissions = true;
						receivedPackets.clear();
						tcpState = EmulatedTCPState::CLOSED;
						break;
				}
			} else {
				switch (how) {
					case ShutdownType::RECEIVE:
						shutdownReceptions = true;
						receivedPackets.clear();
						break;
					case ShutdownType::SEND: shutdownTransmissions = true; break;
					case ShutdownType::BOTH:
						shutdownReceptions = true;
						shutdownTransmissions = true;
						break;
				}
			}
			errorCode.clear();
		}

		void listen(UniqueLock<Mutex>& lock, SOCKET handle, size_t newListenQueueBacklogSize, std::error_code& errorCode) {
			if (type != ProtocolType::TCP) {
				errorCode = make_error_code(std::errc::operation_not_supported);
				return;
			}
			switch (tcpState) {
				case EmulatedTCPState::CLOSED: break;
				case EmulatedTCPState::LISTEN:
					listenQueueBacklogSize = 0;
					listenQueue.clear();
					tcpState = EmulatedTCPState::CLOSED;
					break;
				case EmulatedTCPState::SYN_SENT: errorCode = make_error_code(std::errc::connection_already_in_progress); return;
				case EmulatedTCPState::ESTABLISHED: errorCode = make_error_code(std::errc::already_connected); return;
				case EmulatedTCPState::TIME_WAIT: [[fallthrough]];
				case EmulatedTCPState::LAST_ACK:
					remoteSocketHandle = INVALID_SOCKET;
					tcpState = EmulatedTCPState::CLOSED;
					break;
			}
			if (!localEndpoint) {
				bind(lock, handle, Endpoint::any(domain), errorCode);
				if (errorCode) {
					return;
				}
			}
			if (newListenQueueBacklogSize == 0) {
				listenQueueBacklogSize = Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE;
			} else {
				listenQueueBacklogSize = min(newListenQueueBacklogSize, Socket::MAX_LISTEN_QUEUE_BACKLOG_SIZE);
			}
			errorCode.clear();
		}

		[[nodiscard]] SOCKET accept(UniqueLock<Mutex>& lock, SOCKET handle, BlockingMode mode, std::error_code& errorCode) {
			if (type != ProtocolType::TCP) {
				errorCode = make_error_code(std::errc::operation_not_supported);
				return INVALID_SOCKET;
			}

			if (!localEndpoint || tcpState != EmulatedTCPState::LISTEN || listenQueueBacklogSize == 0) {
				errorCode = make_error_code(std::errc::invalid_argument);
				return INVALID_SOCKET;
			}

			if (listenQueue.empty()) {
				if (blockingMode == BlockingMode::NON_BLOCKING) {
					errorCode = make_error_code(std::errc::operation_would_block);
					return INVALID_SOCKET;
				}
#ifdef GREM_USE_MULTITHREADING
				const TimePoint startTime = Clock::now();
				Duration sleepInterval = duration_cast<Duration>(Milliseconds{1});
				if (receiveTimeout > Duration{}) {
					sleepInterval = min(sleepInterval, receiveTimeout);
				}
				while (true) {
					lock.unlock();
					sleepFor(sleepInterval);
					lock.lock();
					if (!localEndpoint || tcpState != EmulatedTCPState::LISTEN || listenQueueBacklogSize == 0) {
						errorCode = make_error_code(std::errc::invalid_argument);
						return INVALID_SOCKET;
					}
					if (!listenQueue.empty()) {
						break;
					}
					if (blockingMode == BlockingMode::NON_BLOCKING) {
						errorCode = make_error_code(std::errc::operation_would_block);
						return INVALID_SOCKET;
					}
					if (receiveTimeout > Duration{} && Clock::now() - startTime >= receiveTimeout) {
						errorCode = make_error_code(std::errc::timed_out);
						return INVALID_SOCKET;
					}
				}
#else
				errorCode = make_error_code(std::errc::timed_out);
				return INVALID_SOCKET;
#endif
			}

			const SOCKET remoteSocketHandle = listenQueue.front();
			listenQueue.pop_front();

			const Duration newReceiveTimeout = receiveTimeout;
			const Duration newSendTimeout = sendTimeout;
			lock.unlock();

			const SharedPointer<EmulatedSocket> remoteSocket = EmulatedSockets::getInstance().getSocket(remoteSocketHandle, errorCode);
			if (!remoteSocket) {
				errorCode = make_error_code(std::errc::connection_aborted);
				return INVALID_SOCKET;
			}

			PortNumber remotePortNumber{};
			Optional<Endpoint> newLocalEndpoint{};
			{
				ScopedLock remoteLock{remoteSocket->mutex};
				if (!remoteSocket->localEndpoint || !remoteSocket->remoteEndpoint || remoteSocket->remoteSocketHandle != handle ||
					remoteSocket->tcpState != EmulatedTCPState::SYN_SENT) {
					errorCode = make_error_code(std::errc::connection_aborted);
					return INVALID_SOCKET;
				}
				remotePortNumber = remoteSocket->localEndpoint->getPortNumber();
				newLocalEndpoint = *remoteSocket->remoteEndpoint;
			}

			SOCKET newHandle = INVALID_SOCKET;
			{
				EmulatedSockets& emulatedSockets = EmulatedSockets::getInstance();
				ScopedLock socketsLock{emulatedSockets.socketsMutex};
				const auto itPort = emulatedSockets.ports.try_emplace(newLocalEndpoint->getPortNumber()).first;
				newHandle = bit_cast<SOCKET>(emulatedSockets.nextSocketHandleValue++);
				if (newHandle == INVALID_SOCKET) {
					newHandle = bit_cast<SOCKET>(emulatedSockets.nextSocketHandleValue++);
				}
				try {
					switch (newLocalEndpoint->getFamily()) {
						case EndpointFamily::IPv4:
							itPort->second.ipv4Bindings.push_back(EmulatedPortIPv4Binding{.address = newLocalEndpoint->getIPv4Endpoint()->getAddress(), .socketHandle = newHandle});
							break;
						case EndpointFamily::IPv6:
							itPort->second.ipv6Bindings.push_back(EmulatedPortIPv6Binding{.address = newLocalEndpoint->getIPv6Endpoint()->getAddress(), .socketHandle = newHandle});
							break;
					}
					try {
						SharedPointer<EmulatedSocket> newSocket = SharedPointer<EmulatedSocket>::create(domain, type);
						newSocket->blockingMode = mode;
						newSocket->receiveTimeout = newReceiveTimeout;
						newSocket->sendTimeout = newSendTimeout;
						newSocket->localEndpoint = *newLocalEndpoint;
						switch (domain) {
							case EndpointFamily::IPv4: newSocket->remoteEndpoint = Endpoint{IPv4Address::LOOPBACK, remotePortNumber}; break;
							case EndpointFamily::IPv6: newSocket->remoteEndpoint = Endpoint{IPv6Address::LOOPBACK, remotePortNumber}; break;
						}
						newSocket->remoteSocketHandle = handle;
						newSocket->tcpState = EmulatedTCPState::ESTABLISHED;
						if (!emulatedSockets.sockets.try_emplace(newHandle, std::move(newSocket)).second) {
							switch (newLocalEndpoint->getFamily()) {
								case EndpointFamily::IPv4: itPort->second.ipv4Bindings.pop_back(); break;
								case EndpointFamily::IPv6: itPort->second.ipv6Bindings.pop_back(); break;
							}
							if (itPort->second.ipv4Bindings.empty() && itPort->second.ipv6Bindings.empty()) {
								emulatedSockets.ports.erase(itPort);
							}
							errorCode = make_error_code(std::errc::resource_unavailable_try_again);
							return INVALID_SOCKET;
						}
					} catch (...) {
						switch (newLocalEndpoint->getFamily()) {
							case EndpointFamily::IPv4: itPort->second.ipv4Bindings.pop_back(); break;
							case EndpointFamily::IPv6: itPort->second.ipv6Bindings.pop_back(); break;
						}
						if (itPort->second.ipv4Bindings.empty() && itPort->second.ipv6Bindings.empty()) {
							emulatedSockets.ports.erase(itPort);
						}
						errorCode = make_error_code(std::errc::not_enough_memory);
						return INVALID_SOCKET;
					}
				} catch (...) {
					emulatedSockets.ports.erase(itPort);
					errorCode = make_error_code(std::errc::not_enough_memory);
					return INVALID_SOCKET;
				}
			}

			{
				ScopedLock remoteLock{remoteSocket->mutex};
				remoteSocket->tcpState = EmulatedTCPState::ESTABLISHED;
			}

			errorCode.clear();
			return newHandle;
		}

		[[nodiscard]] Optional<Span<byte>> receive(UniqueLock<Mutex>& lock, Span<byte> buffer, int flags, std::error_code& errorCode) {
			if (const Optional<Pair<Span<byte>, Endpoint>> received = receiveFrom(lock, buffer, flags, errorCode)) {
				return received->first;
			}
			return {};
		}

		[[nodiscard]] Optional<Pair<Span<byte>, Endpoint>> receiveFrom([[maybe_unused]] UniqueLock<Mutex>& lock, Span<byte> buffer, int flags, std::error_code& errorCode) {
			if ((flags & ~SUPPORTED_RECEIVE_FLAGS) != 0) {
				errorCode = make_error_code(std::errc::operation_not_supported);
				return {};
			}

			if (type == ProtocolType::TCP && tcpState != EmulatedTCPState::ESTABLISHED) {
				if (remoteEndpoint && (tcpState == EmulatedTCPState::TIME_WAIT || tcpState == EmulatedTCPState::LAST_ACK)) {
					errorCode.clear();
					return Pair<Span<byte>, Endpoint>{{}, *remoteEndpoint};
				}
				errorCode = make_error_code(std::errc::not_connected);
				return {};
			}

			if (shutdownReceptions) {
				errorCode = make_error_code(std::errc::not_connected);
				return {};
			}

			if (receivedPackets.empty()) {
				if (blockingMode == BlockingMode::NON_BLOCKING) {
					errorCode = make_error_code(std::errc::operation_would_block);
					return {};
				}
#ifdef GREM_USE_MULTITHREADING
				const TimePoint startTime = Clock::now();
				Duration sleepInterval = duration_cast<Duration>(Milliseconds{1});
				if (receiveTimeout > Duration{}) {
					sleepInterval = min(sleepInterval, receiveTimeout);
				}
				while (true) {
					lock.unlock();
					sleepFor(sleepInterval);
					lock.lock();
					if (type == ProtocolType::TCP && tcpState != EmulatedTCPState::ESTABLISHED) {
						if (remoteEndpoint && (tcpState == EmulatedTCPState::TIME_WAIT || tcpState == EmulatedTCPState::LAST_ACK)) {
							errorCode.clear();
							return Pair<Span<byte>, Endpoint>{{}, *remoteEndpoint};
						}
						errorCode = make_error_code(std::errc::not_connected);
						return {};
					}
					if (!receivedPackets.empty()) {
						break;
					}
					if (blockingMode == BlockingMode::NON_BLOCKING) {
						errorCode = make_error_code(std::errc::operation_would_block);
						return {};
					}
					if (receiveTimeout > Duration{} && Clock::now() - startTime >= receiveTimeout) {
						errorCode = make_error_code(std::errc::timed_out);
						return {};
					}
				}
#else
				errorCode = make_error_code(std::errc::timed_out);
				return {};
#endif
			}

			errorCode.clear();

			EmulatedReceivedPacket& receivedPacket = receivedPackets.front();
			const Endpoint sender = *receivedPacket.sender;
			if (type == ProtocolType::UDP) {
				const size_t bytesReceived = min(buffer.size(), receivedPacket.data.size());
				if (bytesReceived > 0) {
					memcpy(buffer.data(), receivedPacket.data.data(), bytesReceived);
				}
				receivedPackets.pop_front();
				return Pair<Span<byte>, Endpoint>{buffer.first(bytesReceived), sender};
			}

			const size_t bytesReceived = min(buffer.size(), receivedPacket.data.size() - receivedPacket.readOffset);
			if (bytesReceived > 0) {
				memcpy(buffer.data(), receivedPacket.data.data() + receivedPacket.readOffset, bytesReceived);
			}
			receivedPacket.readOffset += bytesReceived;
			if (receivedPacket.readOffset >= receivedPacket.data.size()) {
				receivedPackets.pop_front();
			}
			return Pair<Span<byte>, Endpoint>{buffer.first(bytesReceived), sender};
		}

		[[nodiscard]] Optional<size_t> send(UniqueLock<Mutex>& lock, SOCKET handle, Span<const byte> bytes, int flags, std::error_code& errorCode) {
			if (!remoteEndpoint) {
				errorCode = make_error_code(std::errc::destination_address_required);
				return {};
			}
			return sendTo(lock, handle, *remoteEndpoint, bytes, flags, errorCode);
		}

		[[nodiscard]] Optional<size_t> sendTo(UniqueLock<Mutex>& lock, SOCKET handle, const Endpoint& endpoint, Span<const byte> bytes, int flags, std::error_code& errorCode) {
			if ((flags & ~SUPPORTED_SEND_FLAGS) != 0) {
				errorCode = make_error_code(std::errc::operation_not_supported);
				return {};
			}

			if (type == ProtocolType::TCP) {
				if (!localEndpoint || tcpState != EmulatedTCPState::ESTABLISHED) {
					errorCode = make_error_code(std::errc::not_connected);
					return {};
				}
				if (endpoint != remoteEndpoint) {
					errorCode = make_error_code(std::errc::already_connected);
					return {};
				}
			} else {
				if (!localEndpoint) {
					bind(lock, handle, Endpoint::any(domain), errorCode);
					if (errorCode) {
						return {};
					}
				}
			}

			if (shutdownTransmissions) {
				errorCode = make_error_code(std::errc::broken_pipe);
				return {};
			}

			if (!endpoint.isAddressLoopback()) {
				errorCode = make_error_code(std::errc::network_unreachable);
				return {};
			}

			const SOCKET remoteHandle = remoteSocketHandle;
			Endpoint sender = *localEndpoint;
			if (sender.isAddressAny()) {
				sender = Endpoint::loopback(domain, sender.getPortNumber());
			}

			lock.unlock();

			const Optional<EmulatedRemoteSocket> remote = EmulatedSockets::getInstance().findRemoteSocket(endpoint, errorCode);
			if (errorCode) {
				return {};
			}
			if (!remote) {
				if (type == ProtocolType::TCP) {
					errorCode = make_error_code(std::errc::connection_reset);
				}
				return {};
			}

			size_t bytesSent = 0;
			{
				ScopedLock remoteLock{remote->socket->mutex};
				if (remote->socket->domain != domain || remote->socket->type != type) {
					errorCode = make_error_code(std::errc::connection_refused);
					return {};
				}

				if (type == ProtocolType::TCP) {
					if (remote->socketHandle != remoteHandle || remote->socket->remoteSocketHandle != handle || remote->socket->shutdownReceptions) {
						errorCode = make_error_code(std::errc::connection_reset);
						return {};
					}

					while (bytesSent < bytes.size()) {
						const size_t packetSize = min(bytes.size() - bytesSent, MAX_RECEIVED_PACKET_SIZE);
						try {
							remote->socket->receivedPackets.push_back_and_overwrite([&](EmulatedReceivedPacket& sentPacket) -> void {
								sentPacket.sender = sender;
								sentPacket.readOffset = 0;
								sentPacket.data.assign_range(bytes.subspan(bytesSent, packetSize));
							});
						} catch (...) {
							errorCode = make_error_code(std::errc::not_enough_memory);
							return {};
						}
						bytesSent += packetSize;
					}
				} else {
					if (bytes.size() <= MAX_RECEIVED_PACKET_SIZE) {
						try {
							remote->socket->receivedPackets.push_back_and_overwrite([&](EmulatedReceivedPacket& sentPacket) -> void {
								sentPacket.sender = sender;
								sentPacket.readOffset = 0;
								sentPacket.data.assign_range(bytes);
							});
						} catch (...) {
							errorCode = make_error_code(std::errc::not_enough_memory);
							return {};
						}
						bytesSent = bytes.size();
					}
				}
			}

			errorCode.clear();
			return bytesSent;
		}

		void awaitReadable([[maybe_unused]] UniqueLock<Mutex>& lock, [[maybe_unused]] Duration timeout, std::error_code& errorCode) const {
			if (shutdownReceptions) {
				errorCode = make_error_code(std::errc::invalid_argument);
				return;
			}

			if (type == ProtocolType::TCP) {
				switch (tcpState) {
					case EmulatedTCPState::CLOSED: [[fallthrough]];
					case EmulatedTCPState::LISTEN: errorCode = make_error_code(std::errc::not_connected); return;
					case EmulatedTCPState::TIME_WAIT: [[fallthrough]];
					case EmulatedTCPState::LAST_ACK: errorCode.clear(); return;
					case EmulatedTCPState::SYN_SENT: [[fallthrough]];
					case EmulatedTCPState::ESTABLISHED: break;
				}
			}

			if (receivedPackets.empty()) {
#ifdef GREM_USE_MULTITHREADING
				const TimePoint startTime = Clock::now();
				Duration sleepInterval = duration_cast<Duration>(Milliseconds{1});
				if (timeout > Duration{}) {
					sleepInterval = min(sleepInterval, timeout);
				}
				while (true) {
					lock.unlock();
					sleepFor(sleepInterval);
					lock.lock();
					if (type == ProtocolType::TCP) {
						if (tcpState != EmulatedTCPState::SYN_SENT && tcpState != EmulatedTCPState::ESTABLISHED) {
							if (tcpState == EmulatedTCPState::TIME_WAIT || tcpState == EmulatedTCPState::LAST_ACK) {
								break;
							}
							errorCode = make_error_code(std::errc::connection_aborted);
							return;
						}
					}
					if (!receivedPackets.empty()) {
						break;
					}
					if (timeout > Duration{} && Clock::now() - startTime >= timeout) {
						errorCode = make_error_code(std::errc::timed_out);
						return;
					}
				}
#else
				errorCode = make_error_code(std::errc::timed_out);
				return;
#endif
			}

			errorCode.clear();
		}

		void awaitWritable([[maybe_unused]] UniqueLock<Mutex>& lock, [[maybe_unused]] Duration timeout, std::error_code& errorCode) const {
			if (shutdownTransmissions) {
				errorCode = make_error_code(std::errc::invalid_argument);
				return;
			}

			if (type != ProtocolType::TCP) {
				errorCode.clear();
				return;
			}

			switch (tcpState) {
				case EmulatedTCPState::CLOSED: [[fallthrough]];
				case EmulatedTCPState::LISTEN: [[fallthrough]];
				case EmulatedTCPState::TIME_WAIT: [[fallthrough]];
				case EmulatedTCPState::LAST_ACK: errorCode = make_error_code(std::errc::not_connected); return;
				case EmulatedTCPState::SYN_SENT: break;
				case EmulatedTCPState::ESTABLISHED: errorCode.clear(); return;
			}

#ifdef GREM_USE_MULTITHREADING
			const TimePoint startTime = Clock::now();
			Duration sleepInterval = duration_cast<Duration>(Milliseconds{1});
			if (timeout > Duration{}) {
				sleepInterval = min(sleepInterval, timeout);
			}
			while (true) {
				lock.unlock();
				sleepFor(sleepInterval);
				lock.lock();
				if (tcpState != EmulatedTCPState::SYN_SENT) {
					if (tcpState == EmulatedTCPState::ESTABLISHED) {
						break;
					}
					errorCode = make_error_code(std::errc::connection_aborted);
					return;
				}
				if (timeout > Duration{} && Clock::now() - startTime >= timeout) {
					errorCode = make_error_code(std::errc::timed_out);
					return;
				}
			}
			errorCode.clear();
#else
			errorCode = make_error_code(std::errc::timed_out);
#endif
		}
	};

	struct EmulatedRemoteSocket {
		SOCKET socketHandle;
		SharedPointer<EmulatedSocket> socket;
	};

	[[nodiscard]] Optional<Endpoint> bindPort(const Endpoint& endpoint, SOCKET handle, std::error_code& errorCode) {
		ScopedLock socketsLock{socketsMutex};
		Endpoint newEndpoint = endpoint;
		PortNumber portNumber = newEndpoint.getPortNumber();
		if (portNumber == 0) {
			for (size_t i = 0; i < LOCAL_PORT_COUNT; ++i) {
				const PortNumber newPortNumber = static_cast<PortNumber>(static_cast<size_t>(LOCAL_PORT_RANGE_MIN) + nextLocalPortNumberOffset);
				nextLocalPortNumberOffset = (nextLocalPortNumberOffset + 1) % LOCAL_PORT_COUNT;
				if (!ports.contains(newPortNumber)) {
					portNumber = newPortNumber;
					break;
				}
			}
			if (portNumber == 0) {
				errorCode = make_error_code(std::errc::address_not_available);
				return {};
			}
			newEndpoint.setPortNumber(portNumber);
		} else {
			if (portNumber < 1024) {
				errorCode = make_error_code(std::errc::permission_denied);
				return {};
			}
			if (ports.contains(portNumber)) {
				errorCode = make_error_code(std::errc::address_in_use);
				return {};
			}
		}
		const auto itPort = ports.try_emplace(portNumber).first;
		try {
			switch (newEndpoint.getFamily()) {
				case EndpointFamily::IPv4:
					itPort->second.ipv4Bindings.push_back(EmulatedPortIPv4Binding{.address = newEndpoint.getIPv4Endpoint()->getAddress(), .socketHandle = handle});
					break;
				case EndpointFamily::IPv6:
					itPort->second.ipv6Bindings.push_back(EmulatedPortIPv6Binding{.address = newEndpoint.getIPv6Endpoint()->getAddress(), .socketHandle = handle});
					break;
			}
		} catch (...) {
			ports.erase(itPort);
			errorCode = make_error_code(std::errc::not_enough_memory);
			return {};
		}
		errorCode.clear();
		return newEndpoint;
	}

	[[nodiscard]] SharedPointer<EmulatedSocket> getSocket(SOCKET handle, std::error_code& errorCode) {
		if (handle == INVALID_SOCKET) {
			errorCode = make_error_code(std::errc::bad_file_descriptor);
			return {};
		}
		ScopedLock socketsLock{socketsMutex};
		const auto it = sockets.find(handle);
		if (it == sockets.end()) {
			errorCode = make_error_code(std::errc::bad_file_descriptor);
			return {};
		}
		errorCode.clear();
		return it->second;
	}

	[[nodiscard]] Optional<EmulatedRemoteSocket> findRemoteSocket(const Endpoint& endpoint, std::error_code& errorCode) {
		if (!endpoint.isAddressLoopback()) {
			errorCode = make_error_code(std::errc::network_unreachable);
			return {};
		}

		ScopedLock socketsLock{socketsMutex};
		switch (endpoint.getFamily()) {
			case EndpointFamily::IPv4: {
				const IPv4Endpoint ipv4Endpoint = *endpoint.getIPv4Endpoint();
				if (const auto itPort = ports.find(ipv4Endpoint.getPortNumber()); itPort != ports.end()) {
					for (const EmulatedPortIPv4Binding& binding : itPort->second.ipv4Bindings) {
						if (binding.address == IPv4Address::ANY || binding.address == ipv4Endpoint.getAddress()) {
							if (const auto it = sockets.find(binding.socketHandle); it != sockets.end()) {
								errorCode.clear();
								return EmulatedRemoteSocket{.socketHandle = binding.socketHandle, .socket = it->second};
							}
						}
					}
				}
				break;
			}
			case EndpointFamily::IPv6: {
				const IPv6Endpoint ipv6Endpoint = *endpoint.getIPv6Endpoint();
				if (const auto itPort = ports.find(ipv6Endpoint.getPortNumber()); itPort != ports.end()) {
					for (const EmulatedPortIPv6Binding& binding : itPort->second.ipv6Bindings) {
						if (binding.address == IPv6Address::ANY || binding.address == ipv6Endpoint.getAddress()) {
							if (const auto it = sockets.find(binding.socketHandle); it != sockets.end()) {
								errorCode.clear();
								return EmulatedRemoteSocket{.socketHandle = binding.socketHandle, .socket = it->second};
							}
						}
					}
				}
				break;
			}
		}

		errorCode.clear();
		return {};
	}

	EmulatedSockets() noexcept = default;

	HashMap<SOCKET, SharedPointer<EmulatedSocket>> sockets{};
	HashMap<PortNumber, EmulatedPort> ports{};
	SocketHandleValue nextSocketHandleValue = bit_cast<SocketHandleValue>(INVALID_SOCKET) + 1;
	size_t nextLocalPortNumberOffset = 0;
	Mutex socketsMutex{};
};

} // namespace

void Socket::close(std::error_code& errorCode) noexcept {
	if (handle) {
		EmulatedSockets::getInstance().close(handle.get(), errorCode);
		if (errorCode) {
			return;
		}
		handle.release();
	} else {
		errorCode.clear();
	}
}

void Socket::open(EndpointFamily domain, ProtocolType type, std::error_code& errorCode) {
	const SOCKET newHandle = EmulatedSockets::getInstance().open(domain, type, errorCode);
	if (errorCode) {
		return;
	}
	handle.reset(newHandle);
}

void Socket::setBlockingMode(BlockingMode mode, std::error_code& errorCode) {
	EmulatedSockets::getInstance().setBlockingMode(handle.get(), mode, errorCode);
}

void Socket::setReceiveTimeout(Duration timeout, std::error_code& errorCode) {
	EmulatedSockets::getInstance().setReceiveTimeout(handle.get(), timeout, errorCode);
}

void Socket::setSendTimeout(Duration timeout, std::error_code& errorCode) {
	EmulatedSockets::getInstance().setSendTimeout(handle.get(), timeout, errorCode);
}

void Socket::connect(const Endpoint& endpoint, std::error_code& errorCode) {
	EmulatedSockets::getInstance().connect(handle.get(), endpoint, errorCode);
}

void Socket::bind(const Endpoint& endpoint, std::error_code& errorCode) {
	EmulatedSockets::getInstance().bind(handle.get(), endpoint, errorCode);
}

void Socket::shutdown(ShutdownType how, std::error_code& errorCode) {
	EmulatedSockets::getInstance().shutdown(handle.get(), how, errorCode);
}

Optional<Endpoint> Socket::getLocalEndpoint(std::error_code& errorCode) const {
	return EmulatedSockets::getInstance().getLocalEndpoint(handle.get(), errorCode);
}

Optional<Endpoint> Socket::getRemoteEndpoint(std::error_code& errorCode) const {
	return EmulatedSockets::getInstance().getRemoteEndpoint(handle.get(), errorCode);
}

void Socket::listen(size_t listenQueueBacklogSize, std::error_code& errorCode) {
	EmulatedSockets::getInstance().listen(handle.get(), listenQueueBacklogSize, errorCode);
}

Optional<Socket> Socket::accept(BlockingMode mode, std::error_code& errorCode) {
	const SOCKET newHandle = EmulatedSockets::getInstance().accept(handle.get(), mode, errorCode);
	if (newHandle == INVALID_SOCKET) {
		return {};
	}
	return Socket{newHandle};
}

Optional<Span<byte>> Socket::receive(Span<byte> buffer, int flags, std::error_code& errorCode) {
	return EmulatedSockets::getInstance().receive(handle.get(), buffer, flags, errorCode);
}

Optional<Pair<Span<byte>, Endpoint>> Socket::receiveFrom(Span<byte> buffer, int flags, std::error_code& errorCode) {
	return EmulatedSockets::getInstance().receiveFrom(handle.get(), buffer, flags, errorCode);
}

Optional<size_t> Socket::send(Span<const byte> bytes, int flags, std::error_code& errorCode) {
	return EmulatedSockets::getInstance().send(handle.get(), bytes, flags, errorCode);
}

Optional<size_t> Socket::sendTo(const Endpoint& endpoint, Span<const byte> bytes, int flags, std::error_code& errorCode) {
	return EmulatedSockets::getInstance().sendTo(handle.get(), endpoint, bytes, flags, errorCode);
}

void Socket::awaitReadable(Duration timeout, std::error_code& errorCode) {
	EmulatedSockets::getInstance().awaitReadable(handle.get(), timeout, errorCode);
}

void Socket::awaitWritable(Duration timeout, std::error_code& errorCode) {
	EmulatedSockets::getInstance().awaitWritable(handle.get(), timeout, errorCode);
}

void Socket::SocketDeleter::operator()(SOCKET handle) const noexcept {
	if (handle != INVALID_SOCKET) {
		std::error_code errorCode{};
		EmulatedSockets::getInstance().close(handle, errorCode);
	}
}

void TCPSocket::connect(BlockingMode mode, const Endpoint& endpoint, Duration timeout, std::error_code& errorCode) {
	Socket::close();
	Socket::open(endpoint.getFamily(), ProtocolType::TCP, errorCode);
	if (errorCode) {
		return;
	}
	if (timeout <= Duration{}) {
		Socket::setBlockingMode(mode, errorCode);
		if (errorCode) {
			return;
		}
		Socket::connect(endpoint, errorCode);
		return;
	}
	Socket::setBlockingMode(BlockingMode::NON_BLOCKING, errorCode);
	if (errorCode) {
		return;
	}
	Socket::connect(endpoint, errorCode);
	if (errorCode) {
#ifdef GREM_USE_MULTITHREADING
		if (mode != BlockingMode::BLOCKING || errorCode != SocketError::WAIT) {
			return;
		}
		Socket::awaitWritable(timeout, errorCode);
		if (errorCode) {
			return;
		}
#else
		if (errorCode == SocketError::WAIT) {
			std::error_code setBlockingModeErrorCode{};
			Socket::setBlockingMode(BlockingMode::BLOCKING, setBlockingModeErrorCode);
			if (setBlockingModeErrorCode) {
				errorCode = setBlockingModeErrorCode;
				return;
			}
		}
		return;
#endif
	}
	if (mode == BlockingMode::BLOCKING) {
		Socket::setBlockingMode(BlockingMode::BLOCKING, errorCode);
	}
}

} // namespace grem::networking
