// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>
#include <GREM/networking/Error.hpp>
#include <GREM/networking/Socket.hpp>
#include <GREM/networking/platform.hpp>

#include <system_error> // std::error_code, std::errc, std::make_error_code(std::errc)

namespace grem::networking {

void Socket::close(std::error_code& errorCode) noexcept {
	if (closesocket(handle.get()) != 0) {
		errorCode = Error::getLastErrorCode();
		return;
	}

	handle.release();
	errorCode.clear();
}

void Socket::open(EndpointFamily domain, ProtocolType type, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	ensurePlatformInitialized(errorCode);
	if (errorCode) {
		return;
	}

	SOCKET newHandle = socket(static_cast<int>(static_cast<short>(domain)), static_cast<int>(type), 0);
	if (newHandle == INVALID_SOCKET) {
		errorCode = Error::getLastErrorCode();
		return;
	}
#ifdef _WIN32
	const DWORD reuseAddress = 1;
#else
	const int reuseAddress = 1;
#endif
	setsockopt(newHandle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

	handle.reset(newHandle);
	errorCode.clear();
}

void Socket::setBlockingMode(BlockingMode mode, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}
	if (mode == BlockingMode::UNSPECIFIED) {
		return;
	}

#ifdef _WIN32
	u_long nonBlock = (mode == BlockingMode::NON_BLOCKING) ? 1 : 0;
	ioctlsocket(handle.get(), FIONBIO, &nonBlock);
#else
	int flags = fcntl(handle.get(), F_GETFL);
	if (mode == BlockingMode::NON_BLOCKING) {
		flags |= O_NONBLOCK;
	} else {
		flags &= ~O_NONBLOCK;
	}
	if (fcntl(handle.get(), F_SETFL, flags) == -1) {
		errorCode = Error::getLastErrorCode();
		return;
	}
#endif
	errorCode.clear();
}

void Socket::setReceiveTimeout(Duration timeout, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}

#ifdef _WIN32
	using DWORDMilliseconds = DurationBase<DWORD, Ratio<1, 1'000>>;
	const DWORD receiveTimeout = duration_cast<DWORDMilliseconds>(timeout).count();
#else
	timeval receiveTimeout{};
	using TimevalSeconds = DurationBase<decltype(receiveTimeout.tv_sec), Ratio<1, 1>>;
	using TimevalMicroseconds = DurationBase<decltype(receiveTimeout.tv_usec), Ratio<1, 1'000'000>>;
	const TimevalSeconds timeoutSeconds = floor<TimevalSeconds>(timeout);
	const TimevalMicroseconds timeoutMicroseconds = duration_cast<TimevalMicroseconds>(timeout - timeoutSeconds);
	receiveTimeout.tv_sec = timeoutSeconds.count();
	receiveTimeout.tv_usec = timeoutMicroseconds.count();
#endif
	if (setsockopt(handle.get(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&receiveTimeout), sizeof(receiveTimeout)) != 0) {
		errorCode = Error::getLastErrorCode();
		return;
	}
	errorCode.clear();
}

void Socket::setSendTimeout(Duration timeout, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}

#ifdef _WIN32
	using DWORDMilliseconds = DurationBase<DWORD, Ratio<1, 1'000>>;
	const DWORD sendTimeout = duration_cast<DWORDMilliseconds>(timeout).count();
#else
	timeval sendTimeout{};
	using TimevalSeconds = DurationBase<decltype(sendTimeout.tv_sec), Ratio<1, 1>>;
	using TimevalMicroseconds = DurationBase<decltype(sendTimeout.tv_usec), Ratio<1, 1'000'000>>;
	const TimevalSeconds timeoutSeconds = floor<TimevalSeconds>(timeout);
	const TimevalMicroseconds timeoutMicroseconds = duration_cast<TimevalMicroseconds>(timeout - timeoutSeconds);
	sendTimeout.tv_sec = timeoutSeconds.count();
	sendTimeout.tv_usec = timeoutMicroseconds.count();
#endif
	if (setsockopt(handle.get(), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sendTimeout), sizeof(sendTimeout)) != 0) {
		errorCode = Error::getLastErrorCode();
		return;
	}
	errorCode.clear();
}

void Socket::connect(const Endpoint& endpoint, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}

	if (::connect(handle.get(), endpoint.get(), endpoint.getLength()) < 0) {
		errorCode = Error::getLastErrorCode();
		return;
	}
	errorCode.clear();
}

void Socket::bind(const Endpoint& endpoint, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}

	if (::bind(handle.get(), endpoint.get(), endpoint.getLength()) != 0) {
		errorCode = Error::getLastErrorCode();
		return;
	}
	errorCode.clear();
}

void Socket::shutdown(ShutdownType how, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}

	if (::shutdown(handle.get(), static_cast<int>(how)) != 0) {
		errorCode = Error::getLastErrorCode();
		return;
	}
	errorCode.clear();
}

Optional<Endpoint> Socket::getLocalEndpoint(std::error_code& errorCode) const {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return {};
	}

	sockaddr_storage storage{};
	socklen_t length = static_cast<socklen_t>(sizeof(storage));
	if (getsockname(handle.get(), reinterpret_cast<sockaddr*>(&storage), &length) != 0) {
		errorCode = Error::getLastErrorCode();
		return {};
	}
	if (length <= 0 || length > sizeof(storage)) {
		errorCode = make_error_code(SocketError::FAILED);
		return {};
	}
	errorCode.clear();
	return Endpoint{&storage, length};
}

Optional<Endpoint> Socket::getRemoteEndpoint(std::error_code& errorCode) const {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return {};
	}

	sockaddr_storage storage{};
	socklen_t length = static_cast<socklen_t>(sizeof(storage));
	if (getpeername(handle.get(), reinterpret_cast<sockaddr*>(&storage), &length) != 0) {
		errorCode = Error::getLastErrorCode();
		return {};
	}
	if (length <= 0 || length > sizeof(storage)) {
		errorCode = make_error_code(SocketError::FAILED);
		return {};
	}
	errorCode.clear();
	return Endpoint{&storage, length};
}

void Socket::listen(size_t listenQueueBacklogSize, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}

	if (listenQueueBacklogSize == 0) {
		listenQueueBacklogSize = MAX_LISTEN_QUEUE_BACKLOG_SIZE;
	} else {
		listenQueueBacklogSize = min(listenQueueBacklogSize, MAX_LISTEN_QUEUE_BACKLOG_SIZE);
	}
	if (::listen(handle.get(), static_cast<int>(listenQueueBacklogSize)) != 0) {
		errorCode = Error::getLastErrorCode();
		return;
	}
	errorCode.clear();
}

Optional<Socket> Socket::accept(BlockingMode mode, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return {};
	}

	sockaddr_storage storage{};
	socklen_t length = static_cast<socklen_t>(sizeof(storage));
	const SOCKET remote = ::accept(handle.get(), reinterpret_cast<sockaddr*>(&storage), &length);
	if (remote == INVALID_SOCKET) {
		errorCode = Error::getLastErrorCode();
		return {};
	}
	Socket result{remote};
	result.setBlockingMode(mode, errorCode);
	if (errorCode) {
		return {};
	}
	errorCode.clear();
	return result;
}

Optional<Span<byte>> Socket::receive(Span<byte> buffer, int flags, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return {};
	}

#ifdef _WIN32
	if (buffer.size() > size_t{Limits<int>::MAX}) {
		errorCode = make_error_code(std::errc::invalid_argument);
		return {};
	}
	const int size = static_cast<int>(buffer.size());
#else
	const size_t size = buffer.size();
#endif

	const auto bytesReceived = recv(handle.get(), reinterpret_cast<char*>(buffer.data()), size, flags);
	if (bytesReceived < 0) {
		errorCode = Error::getLastErrorCode();
		return {};
	}
	errorCode.clear();
	return buffer.first(static_cast<size_t>(bytesReceived));
}

Optional<Pair<Span<byte>, Endpoint>> Socket::receiveFrom(Span<byte> buffer, int flags, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return {};
	}

#ifdef _WIN32
	if (buffer.size() > size_t{Limits<int>::MAX}) {
		errorCode = make_error_code(std::errc::invalid_argument);
		return {};
	}
	const int size = static_cast<int>(buffer.size());
#else
	const size_t size = buffer.size();
#endif

	sockaddr_storage storage{};
	socklen_t length = static_cast<socklen_t>(sizeof(storage));
	const auto bytesReceived = recvfrom(handle.get(), reinterpret_cast<char*>(buffer.data()), size, flags, reinterpret_cast<sockaddr*>(&storage), &length);
	if (bytesReceived < 0) {
		errorCode = Error::getLastErrorCode();
		return {};
	}
	if (length <= 0 || length > sizeof(storage)) {
		errorCode = make_error_code(SocketError::FAILED);
		return {};
	}
	errorCode.clear();
	return Pair<Span<byte>, Endpoint>{buffer.first(static_cast<size_t>(bytesReceived)), Endpoint{&storage, length}};
}

Optional<size_t> Socket::send(Span<const byte> bytes, int flags, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return {};
	}

#ifdef _WIN32
	if (bytes.size() > size_t{Limits<int>::MAX}) {
		errorCode = make_error_code(std::errc::invalid_argument);
		return {};
	}
	const int size = static_cast<int>(bytes.size());
#else
	const size_t size = bytes.size();
#endif

	const auto bytesSent = ::send(handle.get(), reinterpret_cast<const char*>(bytes.data()), size, flags);
	if (bytesSent < 0) {
		errorCode = Error::getLastErrorCode();
		return {};
	}
	errorCode.clear();
	return static_cast<size_t>(bytesSent);
}

Optional<size_t> Socket::sendTo(const Endpoint& endpoint, Span<const byte> bytes, int flags, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return {};
	}

#ifdef _WIN32
	if (bytes.size() > size_t{Limits<int>::MAX}) {
		errorCode = make_error_code(std::errc::invalid_argument);
		return {};
	}
	const int size = static_cast<int>(bytes.size());
#else
	const size_t size = bytes.size();
#endif

	if (sendto(handle.get(), reinterpret_cast<const char*>(bytes.data()), size, flags, reinterpret_cast<const sockaddr*>(endpoint.get()), endpoint.getLength()) < 0) {
		errorCode = Error::getLastErrorCode();
		return {};
	}
	errorCode.clear();
	return bytes.size();
}

void Socket::awaitReadable(Duration timeout, std::error_code& errorCode) { // NOLINT(readability-make-member-function-const)
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}

#ifdef _WIN32
	fd_set readfs{};
	FD_ZERO(&readfs);
	FD_SET(Socket::get(), &readfs);
	timeval tv{};
	using TimevalSeconds = DurationBase<decltype(tv.tv_sec), Ratio<1, 1>>;
	using TimevalMicroseconds = DurationBase<decltype(tv.tv_usec), Ratio<1, 1'000'000>>;
	const TimevalSeconds timeoutSeconds = floor<TimevalSeconds>(timeout);
	const TimevalMicroseconds timeoutMicroseconds = duration_cast<TimevalMicroseconds>(timeout - timeoutSeconds);
	tv.tv_sec = timeoutSeconds.count();
	tv.tv_usec = timeoutMicroseconds.count();
	if (select(static_cast<int>(Socket::get() + 1), &readfs, nullptr, nullptr, &tv) < 1) {
		errorCode = Error::getLastErrorCode();
		return;
	}
#else
	Array fds{pollfd{.fd = Socket::get(), .events = POLLIN, .revents = 0}};
	using IntMilliseconds = DurationBase<int, Ratio<1, 1000>>;
	if (poll(fds.data(), static_cast<nfds_t>(fds.size()), ceil<IntMilliseconds>(timeout).count()) < 1) {
		errorCode = Error::getLastErrorCode();
		return;
	}
#endif
	errorCode.clear();
}

void Socket::awaitWritable(Duration timeout, std::error_code& errorCode) { // NOLINT(readability-make-member-function-const)
	GREM_PROFILE_FUNCTION();

	if (!handle) {
		errorCode = make_error_code(std::errc::bad_file_descriptor);
		return;
	}

#ifdef _WIN32
	fd_set writefs{};
	FD_ZERO(&writefs);
	FD_SET(Socket::get(), &writefs);
	timeval tv{};
	using TimevalSeconds = DurationBase<decltype(tv.tv_sec), Ratio<1, 1>>;
	using TimevalMicroseconds = DurationBase<decltype(tv.tv_usec), Ratio<1, 1'000'000>>;
	const TimevalSeconds timeoutSeconds = floor<TimevalSeconds>(timeout);
	const TimevalMicroseconds timeoutMicroseconds = duration_cast<TimevalMicroseconds>(timeout - timeoutSeconds);
	tv.tv_sec = timeoutSeconds.count();
	tv.tv_usec = timeoutMicroseconds.count();
	if (select(static_cast<int>(Socket::get() + 1), nullptr, &writefs, nullptr, &tv) < 1) {
		errorCode = Error::getLastErrorCode();
		return;
	}
#else
	Array fds{pollfd{.fd = Socket::get(), .events = POLLOUT, .revents = 0}};
	using IntMilliseconds = DurationBase<int, Ratio<1, 1000>>;
	if (poll(fds.data(), static_cast<nfds_t>(fds.size()), ceil<IntMilliseconds>(timeout).count()) < 1) {
		errorCode = Error::getLastErrorCode();
		return;
	}
#endif
	errorCode.clear();
}

void Socket::SocketDeleter::operator()(SOCKET handle) const noexcept {
	if (handle != INVALID_SOCKET) {
		closesocket(handle);
	}
}

void TCPSocket::connect(BlockingMode mode, const Endpoint& endpoint, Duration timeout, std::error_code& errorCode) {
	GREM_PROFILE_FUNCTION();

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
		if (mode != BlockingMode::BLOCKING || errorCode != SocketError::WAIT) {
			return;
		}
		Socket::awaitWritable(timeout, errorCode);
		if (errorCode) {
			return;
		}
		sockaddr_storage storage{};
		socklen_t length = static_cast<socklen_t>(sizeof(storage));
		if (getpeername(Socket::get(), reinterpret_cast<sockaddr*>(&storage), &length) != 0) {
			errorCode = Error::getLastErrorCode();
			return;
		}
		errorCode.clear();
	}
	if (mode == BlockingMode::BLOCKING) {
		Socket::setBlockingMode(BlockingMode::BLOCKING, errorCode);
	}
}

} // namespace grem::networking
