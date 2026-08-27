// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_NETWORKING_ENDPOINT_HPP
#define GREM_NETWORKING_ENDPOINT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/networking/Error.hpp>
#include <GREM/networking/platform.hpp>

#include <compare>      // std::strong_ordering
#include <cstddef>      // std::size_t
#include <functional>   // std::hash
#include <new>          // std::launder
#include <system_error> // std::error_code
#include <utility>      // std::in_place

namespace grem::networking {

namespace detail {

inline constexpr in6_addr IPV6_ADDRESS_ANY = IN6ADDR_ANY_INIT;
inline constexpr in6_addr IPV6_ADDRESS_LOOPBACK = IN6ADDR_LOOPBACK_INIT;

} // namespace detail

using PortNumber = uint16_t;
using ScopeID = uint32_t;

enum class EndpointFamily : short { // NOLINT(performance-enum-size)
	IPv4 = AF_INET,
	IPv6 = AF_INET6,
};

class IPv4Address {
public:
	static const IPv4Address ANY;
	static const IPv4Address BROADCAST;
	static const IPv4Address NONE;
	static const IPv4Address LOOPBACK;

	[[nodiscard]] GREM_API(networking) static Optional<IPv4Address> parse(CStringView string, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Address> resolve(const char* hostName, const char* service, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Address> resolve(CStringView host, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Address> getLocalAddress(std::error_code& errorCode);

	[[nodiscard]] GREM_API(networking) static IPv4Address parse(CStringView string);
	[[nodiscard]] GREM_API(networking) static IPv4Address resolve(const char* hostName, const char* service);
	[[nodiscard]] GREM_API(networking) static IPv4Address resolve(CStringView host);
	[[nodiscard]] GREM_API(networking) static IPv4Address getLocalAddress();

	constexpr explicit IPv4Address(const in_addr& sin_addr) noexcept
		: sin_addr(sin_addr) {}

	constexpr IPv4Address(uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3) noexcept
#ifdef _WIN32
		: sin_addr{.S_un{.S_addr = (static_cast<uint32_t>(byte3) << 24) | //
	                               (static_cast<uint32_t>(byte2) << 16) | //
	                               (static_cast<uint32_t>(byte1) << 8) |  //
	                               (static_cast<uint32_t>(byte0) << 0)}}
#else
		: sin_addr{.s_addr = (static_cast<uint32_t>(byte3) << 24) | //
	                         (static_cast<uint32_t>(byte2) << 16) | //
	                         (static_cast<uint32_t>(byte1) << 8) |  //
	                         (static_cast<uint32_t>(byte0) << 0)}
#endif
	{
	}

	[[nodiscard]] constexpr Array<uint8_t, 4> toBigEndianBytes() const noexcept {
		const uint8_t byte0 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 0) & 0xFF);
		const uint8_t byte1 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 8) & 0xFF);
		const uint8_t byte2 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 16) & 0xFF);
		const uint8_t byte3 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 24) & 0xFF);
		return {byte0, byte1, byte2, byte3};
	}

	[[nodiscard]] constexpr bool isAny() const noexcept {
		return *this == IPv4Address{0, 0, 0, 0};
	}

	[[nodiscard]] constexpr bool isLoopback() const noexcept {
		const uint8_t byte0 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 0) & 0xFF);
		return byte0 == 127;
	}

	[[nodiscard]] constexpr bool isPrivate() const noexcept {
		const uint8_t byte0 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 0) & 0xFF);
		const uint8_t byte1 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 8) & 0xFF);
		return byte0 == 10 || (byte0 == 172 && byte1 >= 16 && byte1 < 32) || (byte0 == 192 && byte1 == 168);
	}

	[[nodiscard]] constexpr in_addr* get() noexcept {
		return &sin_addr;
	}

	[[nodiscard]] constexpr const in_addr* get() const noexcept {
		return &sin_addr;
	}

	[[nodiscard]] constexpr bool operator==(IPv4Address other) const noexcept {
		return toBigEndianBytes() == other.toBigEndianBytes();
	}

	[[nodiscard]] constexpr std::strong_ordering operator<=>(IPv4Address other) const noexcept {
		return toBigEndianBytes() <=> other.toBigEndianBytes();
	}

private:
	constexpr explicit IPv4Address(uint32_t address) noexcept
		: sin_addr{} {
		sin_addr.s_addr = (static_cast<uint32_t>(address << 24) & uint32_t{0xFF000000}) | //
		                  (static_cast<uint32_t>(address << 8) & uint32_t{0x00FF0000}) |  //
		                  (static_cast<uint32_t>(address >> 8) & uint32_t{0x0000FF00}) |  //
		                  (static_cast<uint32_t>(address >> 24) & uint32_t{0x000000FF});
	}

	in_addr sin_addr;
};

inline constexpr IPv4Address IPv4Address::ANY{0, 0, 0, 0};
inline constexpr IPv4Address IPv4Address::BROADCAST{255, 255, 255, 255};
inline constexpr IPv4Address IPv4Address::NONE{255, 255, 255, 255};
inline constexpr IPv4Address IPv4Address::LOOPBACK{127, 0, 0, 1};

class IPv6Address {
public:
	static const IPv6Address ANY;
	static const IPv6Address LOOPBACK;

	[[nodiscard]] GREM_API(networking) static Optional<IPv6Address> parse(CStringView string, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Address> resolve(const char* hostName, const char* service, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Address> resolve(CStringView host, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Address> getLocalAddress(std::error_code& errorCode);

	[[nodiscard]] GREM_API(networking) static IPv6Address parse(CStringView string);
	[[nodiscard]] GREM_API(networking) static IPv6Address resolve(const char* hostName, const char* service);
	[[nodiscard]] GREM_API(networking) static IPv6Address resolve(CStringView host);
	[[nodiscard]] GREM_API(networking) static IPv6Address getLocalAddress();

	constexpr explicit IPv6Address(const in6_addr& sin6_addr) noexcept
		: sin6_addr(sin6_addr) {}

	constexpr IPv6Address(uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, uint8_t byte5, uint8_t byte6, uint8_t byte7, uint8_t byte8, uint8_t byte9,
		uint8_t byte10, uint8_t byte11, uint8_t byte12, uint8_t byte13, uint8_t byte14, uint8_t byte15) noexcept
		: IPv6Address(Array<uint8_t, 16>{byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7, byte8, byte9, byte10, byte11, byte12, byte13, byte14, byte15}) {}

	[[nodiscard]] constexpr Array<uint8_t, 16> toBigEndianBytes() const noexcept {
		return bit_cast<Array<uint8_t, 16>>(sin6_addr);
	}

	[[nodiscard]] constexpr bool isAny() const noexcept {
		return *this == IPv6Address{detail::IPV6_ADDRESS_ANY};
	}

	[[nodiscard]] constexpr bool isLoopback() const noexcept {
		return *this == IPv6Address{detail::IPV6_ADDRESS_LOOPBACK};
	}

	[[nodiscard]] constexpr in6_addr* get() noexcept {
		return &sin6_addr;
	}

	[[nodiscard]] constexpr const in6_addr* get() const noexcept {
		return &sin6_addr;
	}

	[[nodiscard]] constexpr bool operator==(const IPv6Address& other) const noexcept {
		return toBigEndianBytes() == other.toBigEndianBytes();
	}

	[[nodiscard]] constexpr std::strong_ordering operator<=>(const IPv6Address& other) const noexcept {
		return toBigEndianBytes() <=> other.toBigEndianBytes();
	}

private:
	constexpr IPv6Address(const Array<uint8_t, 16>& bytes) noexcept
		: sin6_addr(bit_cast<in6_addr>(bytes)) {}

	in6_addr sin6_addr;
};

inline constexpr IPv6Address IPv6Address::ANY{detail::IPV6_ADDRESS_ANY};
inline constexpr IPv6Address IPv6Address::LOOPBACK{detail::IPV6_ADDRESS_LOOPBACK};

class IPv4Endpoint {
public:
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> parse(CStringView string, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> resolve(const char* hostName, const char* service, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> resolve(CStringView host, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> getLocalEndpoint(std::error_code& errorCode);

	[[nodiscard]] GREM_API(networking) static IPv4Endpoint parse(CStringView addressString, CStringView portNumberString);
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint parse(CStringView string);
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint resolve(const char* hostName, const char* service);
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint resolve(CStringView host);
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint getLocalEndpoint();

	constexpr explicit IPv4Endpoint(IPv4Address address, PortNumber port = 0)
		: addr{} {
		addr.sin_family = AF_INET;
#ifdef __APPLE__
		addr.sin_len = sizeof(addr);
#endif
		addr.sin_addr = *address.get();
		addr.sin_port = convertHostEndianToBigEndian(port);
	}

	constexpr explicit IPv4Endpoint(const sockaddr_in& addr) noexcept
		: addr(addr) {}

	constexpr void setAddress(IPv4Address newAddress) noexcept {
		addr.sin_addr = *newAddress.get();
	}

	constexpr void setPortNumber(PortNumber newPortNumber) noexcept {
		addr.sin_port = convertHostEndianToBigEndian(decltype(addr.sin_port){newPortNumber});
	}

	[[nodiscard]] constexpr IPv4Address getAddress() const noexcept {
		return IPv4Address{addr.sin_addr};
	}

	[[nodiscard]] constexpr PortNumber getPortNumber() const noexcept {
		return PortNumber{convertBigEndianToHostEndian(addr.sin_port)};
	}

	[[nodiscard]] constexpr bool isAddressAny() const noexcept {
		return getAddress().isAny();
	}

	[[nodiscard]] constexpr bool isAddressLoopback() const noexcept {
		return getAddress().isLoopback();
	}

	[[nodiscard]] constexpr bool isAddressPrivate() const noexcept {
		return getAddress().isPrivate();
	}

	[[nodiscard]] constexpr sockaddr_in* get() noexcept {
		return &addr;
	}

	[[nodiscard]] constexpr const sockaddr_in* get() const noexcept {
		return &addr;
	}

	[[nodiscard]] constexpr bool operator==(IPv4Endpoint other) const noexcept {
		return getAddress() == other.getAddress() && getPortNumber() == other.getPortNumber();
	}

	[[nodiscard]] constexpr std::strong_ordering operator<=>(IPv4Endpoint other) const noexcept {
		return (getAddress() == other.getAddress()) ? getPortNumber() <=> other.getPortNumber() : getAddress() <=> other.getAddress();
	}

private:
	sockaddr_in addr;
};

class IPv6Endpoint {
public:
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> parse(CStringView string, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> resolve(const char* hostName, const char* service, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> resolve(CStringView host, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> getLocalEndpoint(std::error_code& errorCode);

	[[nodiscard]] GREM_API(networking) static IPv6Endpoint parse(CStringView addressString, CStringView portNumberString);
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint parse(CStringView string);
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint resolve(const char* hostName, const char* service);
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint resolve(CStringView host);
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint getLocalEndpoint();

	constexpr explicit IPv6Endpoint(const IPv6Address& address, PortNumber port = 0, ScopeID scopeID = 0)
		: addr{} {
		addr.sin6_family = AF_INET6;
#ifdef __APPLE__
		addr.sin_len = sizeof(addr);
#endif
		addr.sin6_addr = *address.get();
		addr.sin6_port = convertHostEndianToBigEndian(port);
		addr.sin6_scope_id = scopeID;
	}

	constexpr explicit IPv6Endpoint(const sockaddr_in6& addr) noexcept
		: addr(addr) {}

	constexpr void setAddress(const IPv6Address& newAddress) noexcept {
		addr.sin6_addr = *newAddress.get();
	}

	constexpr void setPortNumber(PortNumber newPortNumber) noexcept {
		addr.sin6_port = convertHostEndianToBigEndian(decltype(addr.sin6_port){newPortNumber});
	}

	constexpr void setScopeID(ScopeID newScopeID) noexcept {
		addr.sin6_scope_id = newScopeID;
	}

	[[nodiscard]] constexpr IPv6Address getAddress() const noexcept {
		return IPv6Address{addr.sin6_addr};
	}

	[[nodiscard]] constexpr PortNumber getPortNumber() const noexcept {
		return PortNumber{convertBigEndianToHostEndian(addr.sin6_port)};
	}

	[[nodiscard]] constexpr ScopeID getScopeID() const noexcept {
		return addr.sin6_scope_id;
	}

	[[nodiscard]] constexpr bool isAddressAny() const noexcept {
		return getAddress().isAny();
	}

	[[nodiscard]] constexpr bool isAddressLoopback() const noexcept {
		return getAddress().isLoopback();
	}

	[[nodiscard]] constexpr sockaddr_in6* get() noexcept {
		return &addr;
	}

	[[nodiscard]] constexpr const sockaddr_in6* get() const noexcept {
		return &addr;
	}

	[[nodiscard]] constexpr bool operator==(const IPv6Endpoint& other) const noexcept {
		return getAddress() == other.getAddress() && getPortNumber() == other.getPortNumber() && getScopeID() == other.getScopeID();
	}

	[[nodiscard]] constexpr std::strong_ordering operator<=>(const IPv6Endpoint& other) const noexcept {
		return (getAddress() == other.getAddress()) ? (getPortNumber() == other.getPortNumber()) ? getScopeID() <=> other.getScopeID() : getPortNumber() <=> other.getPortNumber()
		                                            : getAddress() <=> other.getAddress();
	}

private:
	sockaddr_in6 addr;
};

class Endpoint {
public:
	[[nodiscard]] static Endpoint any(EndpointFamily family, PortNumber portNumber = 0) noexcept {
		switch (family) {
			case EndpointFamily::IPv4: return Endpoint{IPv4Address::ANY, portNumber};
			case EndpointFamily::IPv6: return Endpoint{IPv6Address::ANY, portNumber};
		}
		unreachable();
	}

	[[nodiscard]] static Endpoint loopback(EndpointFamily family, PortNumber portNumber = 0) noexcept {
		switch (family) {
			case EndpointFamily::IPv4: return Endpoint{IPv4Address::LOOPBACK, portNumber};
			case EndpointFamily::IPv6: return Endpoint{IPv6Address::LOOPBACK, portNumber};
		}
		unreachable();
	}

	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> parse(CStringView string, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> parse(EndpointFamily family, CStringView addressString, CStringView portNumberString,
		std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> parse(EndpointFamily family, CStringView string, std::error_code& errorCode) noexcept;
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> resolve(const char* hostName, const char* service, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> resolve(CStringView host, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> resolve(EndpointFamily family, const char* hostName, const char* service, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> resolve(EndpointFamily family, CStringView host, std::error_code& errorCode);
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> getLocalEndpoint(EndpointFamily family, std::error_code& errorCode);

	[[nodiscard]] GREM_API(networking) static Endpoint parse(CStringView addressString, CStringView portNumberString);
	[[nodiscard]] GREM_API(networking) static Endpoint parse(CStringView string);
	[[nodiscard]] GREM_API(networking) static Endpoint parse(EndpointFamily family, CStringView addressString, CStringView portNumberString);
	[[nodiscard]] GREM_API(networking) static Endpoint parse(EndpointFamily family, CStringView string);
	[[nodiscard]] GREM_API(networking) static Endpoint resolve(const char* hostName, const char* service);
	[[nodiscard]] GREM_API(networking) static Endpoint resolve(CStringView host);
	[[nodiscard]] GREM_API(networking) static Endpoint resolve(EndpointFamily family, const char* hostName, const char* service);
	[[nodiscard]] GREM_API(networking) static Endpoint resolve(EndpointFamily family, CStringView host);
	[[nodiscard]] GREM_API(networking) static Endpoint getLocalEndpoint(EndpointFamily family);

	Endpoint(const sockaddr_storage* otherStorage, socklen_t length) noexcept {
		GREM_ASSERT(length > 0);
		GREM_ASSERT(static_cast<size_t>(length) <= sizeof(sockaddr_storage));
		memcpy(storage, otherStorage, static_cast<size_t>(length));
	}

	Endpoint(const IPv4Endpoint& endpoint) {
		memcpy(storage, endpoint.get(), sizeof(*endpoint.get()));
	}

	Endpoint(const IPv6Endpoint& endpoint) {
		memcpy(storage, endpoint.get(), sizeof(*endpoint.get()));
	}

	explicit Endpoint(const IPv4Address& address, PortNumber port = 0)
		: Endpoint(IPv4Endpoint{address, port}) {}

	explicit Endpoint(const IPv6Address& address, PortNumber port = 0)
		: Endpoint(IPv6Endpoint{address, port}) {}

	explicit Endpoint(const sockaddr_in& addr) noexcept
		: Endpoint(IPv4Endpoint{addr}) {}

	explicit Endpoint(const sockaddr_in6& addr) noexcept
		: Endpoint(IPv6Endpoint{addr}) {}

	~Endpoint() = default;

	Endpoint(const Endpoint& other) noexcept {
		memcpy(storage, other.storage, sizeof(other.storage));
	}

	Endpoint(Endpoint&& other) noexcept
		: Endpoint(other) {} // NOLINT(performance-move-constructor-init)

	Endpoint& operator=(const Endpoint& other) noexcept {
		memcpy(storage, other.storage, sizeof(other.storage));
		return *this;
	}

	Endpoint& operator=(Endpoint&& other) noexcept {
		return *this = other;
	}

	void setPortNumber(PortNumber newPortNumber) noexcept {
		switch (getFamily()) {
			case EndpointFamily::IPv4: *this = Endpoint{getIPv4Endpoint()->getAddress(), newPortNumber}; break;
			case EndpointFamily::IPv6: *this = Endpoint{getIPv6Endpoint()->getAddress(), newPortNumber}; break;
		}
	}

	[[nodiscard]] EndpointFamily getFamily() const noexcept {
		short family{};
		memcpy(&family, storage, sizeof(family));
		return static_cast<EndpointFamily>(family);
	}

	[[nodiscard]] socklen_t getLength() const noexcept {
		switch (getFamily()) {
			case EndpointFamily::IPv4: return static_cast<socklen_t>(sizeof(sockaddr_in));
			case EndpointFamily::IPv6: return static_cast<socklen_t>(sizeof(sockaddr_in6));
		}
		return 0;
	}

	[[nodiscard]] Optional<IPv4Endpoint> getIPv4Endpoint() const noexcept {
		return (getFamily() == EndpointFamily::IPv4) ? Optional<IPv4Endpoint>{std::in_place, *std::launder(reinterpret_cast<const sockaddr_in*>(storage))}
		                                             : Optional<IPv4Endpoint>{};
	}

	[[nodiscard]] Optional<IPv6Endpoint> getIPv6Endpoint() const noexcept {
		return (getFamily() == EndpointFamily::IPv6) ? Optional<IPv6Endpoint>{std::in_place, *std::launder(reinterpret_cast<const sockaddr_in6*>(storage))}
		                                             : Optional<IPv6Endpoint>{};
	}

	[[nodiscard]] PortNumber getPortNumber() const noexcept {
		switch (getFamily()) {
			case EndpointFamily::IPv4: return getIPv4Endpoint()->getPortNumber();
			case EndpointFamily::IPv6: return getIPv6Endpoint()->getPortNumber();
		}
		return 0;
	}

	[[nodiscard]] bool isAddressAny() const noexcept {
		switch (getFamily()) {
			case EndpointFamily::IPv4: return getIPv4Endpoint()->isAddressAny();
			case EndpointFamily::IPv6: return getIPv6Endpoint()->isAddressAny();
		}
		return false;
	}

	[[nodiscard]] bool isAddressLoopback() const noexcept {
		switch (getFamily()) {
			case EndpointFamily::IPv4: return getIPv4Endpoint()->isAddressLoopback();
			case EndpointFamily::IPv6: return getIPv6Endpoint()->isAddressLoopback();
		}
		return false;
	}

	[[nodiscard]] sockaddr* get() noexcept {
		return reinterpret_cast<sockaddr*>(storage);
	}

	[[nodiscard]] const sockaddr* get() const noexcept {
		return reinterpret_cast<const sockaddr*>(storage);
	}

	[[nodiscard]] bool operator==(const Endpoint& other) const noexcept {
		const EndpointFamily family = getFamily();
		const EndpointFamily otherFamily = other.getFamily();
		if (family == otherFamily) {
			switch (family) {
				case EndpointFamily::IPv4: return *getIPv4Endpoint() == *other.getIPv4Endpoint();
				case EndpointFamily::IPv6: return *getIPv6Endpoint() == *other.getIPv6Endpoint();
			}
			return true;
		}
		return false;
	}

	[[nodiscard]] std::strong_ordering operator<=>(const Endpoint& other) const noexcept {
		const EndpointFamily family = getFamily();
		const EndpointFamily otherFamily = other.getFamily();
		if (family == otherFamily) {
			switch (family) {
				case EndpointFamily::IPv4: return *getIPv4Endpoint() <=> *other.getIPv4Endpoint();
				case EndpointFamily::IPv6: return *getIPv6Endpoint() <=> *other.getIPv6Endpoint();
			}
			return std::strong_ordering::equal;
		}
		return family <=> otherFamily;
	}

private:
	alignas(sockaddr_storage) byte storage[sizeof(sockaddr_storage)];
};

} // namespace grem::networking

template <>
struct std::hash<grem::networking::IPv4Address> {
	[[nodiscard]] std::size_t operator()(const grem::networking::IPv4Address& address) const {
		return grem::getHash(grem::convertBigEndianToHostEndian(static_cast<uint32_t>(address.get()->s_addr)));
	}
};

template <>
struct std::hash<grem::networking::IPv6Address> {
	[[nodiscard]] std::size_t operator()(const grem::networking::IPv6Address& address) const {
		const grem::Array<grem::uint8_t, 16> bytes = address.toBigEndianBytes();
		return grem::getRangeHash(bytes);
	}
};

template <>
struct std::hash<grem::networking::IPv4Endpoint> {
	[[nodiscard]] std::size_t operator()(const grem::networking::IPv4Endpoint& endpoint) const {
		return grem::getHash(endpoint.getAddress(), endpoint.getPortNumber());
	}
};

template <>
struct std::hash<grem::networking::IPv6Endpoint> {
	[[nodiscard]] std::size_t operator()(const grem::networking::IPv6Endpoint& endpoint) const {
		return grem::getHash(endpoint.getAddress(), endpoint.getPortNumber());
	}
};

template <>
struct std::hash<grem::networking::Endpoint> {
	[[nodiscard]] std::size_t operator()(const grem::networking::Endpoint& endpoint) const {
		switch (endpoint.getFamily()) {
			case grem::networking::EndpointFamily::IPv4: return grem::getHash(*endpoint.getIPv4Endpoint());
			case grem::networking::EndpointFamily::IPv6: return grem::getHash(*endpoint.getIPv6Endpoint());
		}
		return 0;
	}
};

template <>
struct grem::Formatter<grem::networking::IPv4Address> : Formatter<CStringView> {
	void formatTo(FormatOutput& output, const grem::networking::IPv4Address& value) const {
		std::error_code errorCode{};
		grem::networking::ensurePlatformInitialized(errorCode);
		if (errorCode) {
			throw grem::networking::Error{errorCode};
		}

		grem::Array<char, INET_ADDRSTRLEN> buffer{};
		const char* string = inet_ntop(AF_INET, value.get(), buffer.data(), static_cast<socklen_t>(buffer.size()));
		if (!string) {
			throw grem::networking::Error{grem::networking::Error::getLastErrorCode()};
		}
		Formatter<CStringView>::formatTo(output, CStringView{string});
	}
};

template <>
struct grem::Formatter<grem::networking::IPv6Address> : Formatter<CStringView> {
	void formatTo(FormatOutput& output, const grem::networking::IPv6Address& value) const {
		std::error_code errorCode{};
		grem::networking::ensurePlatformInitialized(errorCode);
		if (errorCode) {
			throw grem::networking::Error{errorCode};
		}

		grem::Array<char, INET6_ADDRSTRLEN> buffer{};
		const char* string = inet_ntop(AF_INET6, value.get(), buffer.data(), static_cast<socklen_t>(buffer.size()));
		if (!string) {
			throw grem::networking::Error{grem::networking::Error::getLastErrorCode()};
		}
		Formatter<CStringView>::formatTo(output, CStringView{string});
	}
};

template <>
struct grem::Formatter<grem::networking::IPv4Endpoint> {
	bool alwaysIncludePort = false;

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == '0') {
			alwaysIncludePort = true;
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const grem::networking::IPv4Endpoint& value) const {
		const grem::networking::IPv4Address address = value.getAddress();
		const grem::networking::PortNumber portNumber = value.getPortNumber();
		Formatter<grem::networking::IPv4Address>{}.formatTo(output, address);
		if (alwaysIncludePort || portNumber != 0) {
			output.append(":");
			Formatter<grem::networking::PortNumber>{}.formatTo(output, portNumber);
		}
	}
};

template <>
struct grem::Formatter<grem::networking::IPv6Endpoint> {
	bool skipBrackets = false;
	bool alwaysIncludePort = false;

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'n') {
			skipBrackets = true;
			++p;
		}
		if (*p == '0') {
			alwaysIncludePort = true;
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const grem::networking::IPv6Endpoint& value) const {
		const grem::networking::IPv6Address address = value.getAddress();
		const grem::networking::PortNumber portNumber = value.getPortNumber();
		if (!skipBrackets || alwaysIncludePort || portNumber != 0) {
			output.append("[");
		}
		Formatter<grem::networking::IPv6Address>{}.formatTo(output, address);
		if (!skipBrackets || alwaysIncludePort || portNumber != 0) {
			output.append("]");
		}
		if (alwaysIncludePort || portNumber != 0) {
			output.append(":");
			Formatter<grem::networking::PortNumber>{}.formatTo(output, portNumber);
		}
	}
};

template <>
struct grem::Formatter<grem::networking::Endpoint> {
	bool skipBrackets = false;
	bool alwaysIncludePort = false;

	[[nodiscard]] constexpr const char* parseFormatSpecification(const char* p) {
		if (*p == 'n') {
			skipBrackets = true;
			++p;
		}
		if (*p == '0') {
			alwaysIncludePort = true;
			++p;
		}
		return p;
	}

	void formatTo(FormatOutput& output, const grem::networking::Endpoint& value) const {
		switch (value.getFamily()) {
			case grem::networking::EndpointFamily::IPv4: {
				Formatter<grem::networking::IPv4Endpoint> formatter{};
				formatter.alwaysIncludePort = alwaysIncludePort;
				formatter.formatTo(output, *value.getIPv4Endpoint());
				break;
			}
			case grem::networking::EndpointFamily::IPv6: {
				Formatter<grem::networking::IPv6Endpoint> formatter{};
				formatter.skipBrackets = skipBrackets;
				formatter.alwaysIncludePort = alwaysIncludePort;
				formatter.formatTo(output, *value.getIPv6Endpoint());
				break;
			}
		}
	}
};

#endif
