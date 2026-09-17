// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_NETWORKING_ENDPOINT_HPP
#define GREM_NETWORKING_ENDPOINT_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/concepts.hpp>
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

/**
 * IP port number (0-65535).
 *
 * \note A value of 0 usually means "any port".
 */
using PortNumber = uint16_t;

/**
 * IPv6 scope/zone ID.
 */
using ScopeID = uint32_t;

/**
 * Address family of an IP endpoint.
 */
enum class AddressFamily : short { // NOLINT(performance-enum-size)
	IPv4 = AF_INET,                ///< IPv4.
	IPv6 = AF_INET6,               ///< IPv6.
};

/**
 * 32-bit IP address of an IPv4 endpoint.
 */
class IPv4Address {
public:
	static const IPv4Address ANY;       ///< Value representing any IPv4 address (0.0.0.0).
	static const IPv4Address BROADCAST; ///< Value of the IPv4 broadcast address (255.255.255.255).
	static const IPv4Address NONE;      ///< Value representing no IPv4 address (255.255.255.255).
	static const IPv4Address LOOPBACK;  ///< Value of the standard IPv4 loopback address (127.0.0.1).

	/**
	 * Parse an IPv4 address from an ASCII string in dot-decimal notation.
	 *
	 * \param string string containing the four-decimal address to parse.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        IPv4 address, or cleared on success.
	 *
	 * \return the parsed IPv4 address, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Address> parse(CStringView string, std::error_code& errorCode) noexcept;

	/**
	 * Parse an IPv4 address from an ASCII string in dot-decimal notation,
	 * throwing an exception on failure.
	 *
	 * \param string string containing the four-decimal address to parse.
	 *
	 * \return the parsed IPv4 address.
	 *
	 * \throws networking::Error on failure to parse a valid IPv4 address.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Address parse(CStringView string);

	/**
	 * Resolve the IPv4 address of a host through the Domain Name System (DNS)
	 * using a `getaddrinfo()` lookup.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com" or "127.0.0.1", or nullptr to look
	 *        up by service only. Must not be nullptr if service is nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved IPv4 address, or an empty optional if resolution
	 *         failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Address> resolve(const char* hostName, const char* service, std::error_code& errorCode);

	/**
	 * Resolve the IPv4 address of a host through the Domain Name System (DNS)
	 * using a `getaddrinfo()` lookup, throwing an exception on failure.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com" or "127.0.0.1", or nullptr to look
	 *        up by service only. Must not be nullptr if service is nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 *
	 * \return the resolved IPv4 address.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Address resolve(const char* hostName, const char* service);

	/**
	 * Resolve the IPv4 address of a host through the Domain Name System (DNS)
	 * using a `getaddrinfo()` lookup.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http" or "127.0.0.1:80", or a host name-only
	 *        string with no colon, e.g. "example.com" or "127.0.0.1".
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved IPv4 address, or an empty optional if resolution
	 *         failed.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Address> resolve(CStringView host, std::error_code& errorCode);

	/**
	 * Resolve the IPv4 address of a host through the Domain Name System (DNS)
	 * using a `getaddrinfo()` lookup, throwing an exception on failure.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http" or "127.0.0.1:80", or a host name-only
	 *        string with no colon, e.g. "example.com" or "127.0.0.1".
	 *
	 * \return the resolved IPv4 address.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Address resolve(CStringView host);

	/**
	 * Get the IPv4 address of the local host on the local network by calling
	 * `connect()` on a temporary connectionless socket to the loopback address
	 * on port 9 (the "Discard" service) and querying its local address with
	 * `getsockname()`.
	 *
	 * \param errorCode error code that is filled in on failure to get the local
	 *        address, or cleared on success.
	 *
	 * \return the local IPv4 address (typically a private Class C address like
	 *         "192.168.1.2"), or an empty optional on failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Address> getLocalAddress(std::error_code& errorCode);

	/**
	 * Get the IPv4 address of the local host on the local network by calling
	 * `connect()` on a temporary connectionless socket to the loopback address
	 * on port 9 (the "Discard" service) and querying its local address with
	 * `getsockname()`, throwing an exception on failure.
	 *
	 * \return the local IPv4 address (typically a private Class C address like
	 *         "192.168.1.2").
	 *
	 * \throws networking::Error on failure to get the local address.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Address getLocalAddress();

	/**
	 * Construct an IPv4 address from a native in_addr structure.
	 *
	 * \param sin_addr in_addr structure to construct the IPv4 address from.
	 */
	constexpr explicit IPv4Address(const in_addr& sin_addr) noexcept
		: sin_addr(sin_addr) {}

	/**
	 * Construct an IPv4 address from its 4 constituent bytes, specified in the
	 * same order as in a dot-decimal notation string.
	 *
	 * \param byte0 first byte value.
	 * \param byte1 second byte value.
	 * \param byte2 third byte value.
	 * \param byte3 fourth byte value.
	 */
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

	/**
	 * Convert this IPv4 address to an array of 4 bytes in network byte order.
	 *
	 * \return a big-endian array of the 4 byte values of the address.
	 */
	[[nodiscard]] constexpr Array<uint8_t, 4> toBigEndianBytes() const noexcept {
		const uint8_t byte0 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 0) & 0xFF);
		const uint8_t byte1 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 8) & 0xFF);
		const uint8_t byte2 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 16) & 0xFF);
		const uint8_t byte3 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 24) & 0xFF);
		return {byte0, byte1, byte2, byte3};
	}

	/**
	 * Check if the value of this IPv4 address is equal to the ANY address
	 * (0.0.0.0).
	 *
	 * \return `*this == IPv4Address::ANY`.
	 */
	[[nodiscard]] constexpr bool isAny() const noexcept {
		// Note: We can't use IPv4Address::ANY directly here while remaining constexpr, since its constexpr value is not defined yet.
		// (Its value has to be defined after the class definition since IPv4Address is an incomplete type until then.)
		return *this == IPv4Address{0, 0, 0, 0};
	}

	/**
	 * Check if this IPv4 address is a loopback address (starts with 127).
	 *
	 * \return true if this is a loopback address, false otherwise.
	 */
	[[nodiscard]] constexpr bool isLoopback() const noexcept {
		const uint8_t byte0 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 0) & 0xFF);
		return byte0 == 127;
	}

	/**
	 * Check if this IPv4 address is in a private address range.
	 *
	 * The address ranges reserved for private networks are:
	 * - 10.0.0.0 - 10.255.255.255
	 * - 172.16.0.0 - 172.31.255.255
	 * - 192.168.0.0 - 192.168.255.255
	 *
	 * \return true if this is a private address, false otherwise.
	 */
	[[nodiscard]] constexpr bool isPrivate() const noexcept {
		const uint8_t byte0 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 0) & 0xFF);
		const uint8_t byte1 = static_cast<uint8_t>((static_cast<uint32_t>(sin_addr.s_addr) >> 8) & 0xFF);
		return byte0 == 10 || (byte0 == 172 && byte1 >= 16 && byte1 < 32) || (byte0 == 192 && byte1 == 168);
	}

	/**
	 * Get a pointer to the underlying native in_addr structure of the IPv4
	 * address.
	 *
	 * \return a non-owning pointer to the underlying in_addr structure.
	 */
	[[nodiscard]] constexpr in_addr* get() noexcept {
		return &sin_addr;
	}

	/**
	 * Get a pointer to the underlying native in_addr structure of the IPv4
	 * address.
	 *
	 * \return a non-owning read-only pointer to the underlying in_addr
	 *         structure.
	 */
	[[nodiscard]] constexpr const in_addr* get() const noexcept {
		return &sin_addr;
	}

	/**
	 * Compare this address to another for equality.
	 *
	 * \param other the address to compare this one to.
	 *
	 * \return true if the addresses are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(IPv4Address other) const noexcept {
		return toBigEndianBytes() == other.toBigEndianBytes();
	}

	/**
	 * Compare this address to another.
	 *
	 * \param other the address to compare this one to.
	 *
	 * \return a strong ordering between the two addresses, based on the
	 *         lexicographical ordering of their big-endian byte value arrays.
	 */
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

/**
 * 128-bit IP address of an IPv6 endpoint.
 */
class IPv6Address {
public:
	static const IPv6Address ANY;      ///< Value representing any IPv6 address (::).
	static const IPv6Address LOOPBACK; ///< Value of the IPv6 loopback address (::1).

	/**
	 * Parse an IPv6 address from an ASCII string representation.
	 *
	 * \param string string containing the IPv6 address representation to parse.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        IPv6 address, or cleared on success.
	 *
	 * \return the parsed IPv6 address, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Address> parse(CStringView string, std::error_code& errorCode) noexcept;

	/**
	 * Parse an IPv6 address from an ASCII string representation, throwing an
	 * exception on failure.
	 *
	 * \param string string containing the IPv6 address representation to parse.
	 *
	 * \return the parsed IPv6 address.
	 *
	 * \throws networking::Error on failure to parse a valid IPv6 address.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Address parse(CStringView string);

	/**
	 * Resolve the IPv6 address of a host through the Domain Name System (DNS)
	 * using a `getaddrinfo()` lookup.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com" or "::1", or nullptr to look up by
	 *        service only. Must not be nullptr if service is nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved IPv6 address, or an empty optional if resolution
	 *         failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Address> resolve(const char* hostName, const char* service, std::error_code& errorCode);

	/**
	 * Resolve the IPv6 address of a host through the Domain Name System (DNS)
	 * using a `getaddrinfo()` lookup, throwing an exception on failure.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com" or "::1", or nullptr to look up by
	 *        service only. Must not be nullptr if service is nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 *
	 * \return the resolved IPv6 address.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Address resolve(const char* hostName, const char* service);

	/**
	 * Resolve the IPv6 address of a host through the Domain Name System (DNS)
	 * using a `getaddrinfo()` lookup.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        with the host enclosed in square brackets if it's an IPv6 address,
	 *        e.g. "example.com:http" or "[::1]:80", or a host name-only string
	 *        with no colon or no square brackets, e.g. "example.com" or "::1".
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved IPv6 address, or an empty optional if resolution
	 *         failed.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Address> resolve(CStringView host, std::error_code& errorCode);

	/**
	 * Resolve the IPv6 address of a host through the Domain Name System (DNS)
	 * using a `getaddrinfo()` lookup, throwing an exception on failure.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        with the host enclosed in square brackets if it's an IPv6 address,
	 *        e.g. "example.com:http" or "[::1]:80", or a host name-only string
	 *        with no colon or no square brackets, e.g. "example.com" or "::1".
	 *
	 * \return the resolved IPv6 address.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Address resolve(CStringView host);

	/**
	 * Get the IPv6 address of the local host by calling `connect()` on a
	 * temporary connectionless socket to the loopback address on port 9 (the
	 * "Discard" service) and querying its local address with `getsockname()`.
	 *
	 * \param errorCode error code that is filled in on failure to get the local
	 *        address, or cleared on success.
	 *
	 * \return the local IPv6 address, or an empty optional on failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Address> getLocalAddress(std::error_code& errorCode);

	/**
	 * Get the IPv6 address of the local host by calling `connect()` on a
	 * temporary connectionless socket to the loopback address on port 9 (the
	 * "Discard" service) and querying its local address with `getsockname()`,
	 * throwing an exception on failure.
	 *
	 * \return the local IPv6 address.
	 *
	 * \throws networking::Error on failure to get the local address.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Address getLocalAddress();

	/**
	 * Construct an IPv6 address from a native in6_addr structure.
	 *
	 * \param sin6_addr in6_addr structure to construct the IPv6 address from.
	 */
	constexpr explicit IPv6Address(const in6_addr& sin6_addr) noexcept
		: sin6_addr(sin6_addr) {}

	/**
	 * Construct an IPv6 address from its 16 constituent bytes, specified in the
	 * same order as in a string representation.
	 *
	 * \param byte0 first byte value.
	 * \param byte1 second byte value.
	 * \param byte2 third byte value.
	 * \param byte3 fourth byte value.
	 * \param byte4 fifth byte value.
	 * \param byte5 sixth byte value.
	 * \param byte6 seventh byte value.
	 * \param byte7 eighth byte value.
	 * \param byte8 ninth byte value.
	 * \param byte9 tenth byte value.
	 * \param byte10 eleventh byte value.
	 * \param byte11 twelvth byte value.
	 * \param byte12 thirteenth byte value.
	 * \param byte13 fourteenth byte value.
	 * \param byte14 fifteenth byte value.
	 * \param byte15 sixteenth byte value.
	 */
	constexpr IPv6Address(uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4, uint8_t byte5, uint8_t byte6, uint8_t byte7, uint8_t byte8, uint8_t byte9,
		uint8_t byte10, uint8_t byte11, uint8_t byte12, uint8_t byte13, uint8_t byte14, uint8_t byte15) noexcept
		: sin6_addr(bit_cast<in6_addr>(Array<uint8_t, 16>{byte0, byte1, byte2, byte3, byte4, byte5, byte6, byte7, byte8, byte9, byte10, byte11, byte12, byte13, byte14, byte15})) {}

	/**
	 * Convert this IPv6 address to an array of 16 bytes in network byte order.
	 *
	 * \return a big-endian array of the 16 byte values of the address.
	 */
	[[nodiscard]] constexpr Array<uint8_t, 16> toBigEndianBytes() const noexcept {
		return bit_cast<Array<uint8_t, 16>>(sin6_addr);
	}

	/**
	 * Check if the value of this IPv4 address is equal to the ANY address (::).
	 *
	 * \return `*this == IPv6Address::ANY`.
	 */
	[[nodiscard]] constexpr bool isAny() const noexcept {
		// Note: We can't use IPv6Address::ANY directly here while remaining constexpr, since its constexpr value is not defined yet.
		// (Its value has to be defined after the class definition since IPv6Address is an incomplete type until then.)
		return *this == IPv6Address{detail::IPV6_ADDRESS_ANY};
	}

	/**
	 * Check if this IPv6 address is equal to the LOOPBACK address (::1).
	 *
	 * \return `*this == IPv6Address::LOOPBACK`.
	 */
	[[nodiscard]] constexpr bool isLoopback() const noexcept {
		// Note: We can't use IPv6Address::LOOPBACK directly here while remaining constexpr, since its constexpr value is not defined yet.
		// (Its value has to be defined after the class definition since IPv6Address is an incomplete type until then.)
		return *this == IPv6Address{detail::IPV6_ADDRESS_LOOPBACK};
	}

	/**
	 * Get a pointer to the underlying native in6_addr structure of the IPv6
	 * address.
	 *
	 * \return a non-owning pointer to the underlying in6_addr structure.
	 */
	[[nodiscard]] constexpr in6_addr* get() noexcept {
		return &sin6_addr;
	}

	/**
	 * Get a pointer to the underlying native in6_addr structure of the IPv6
	 * address.
	 *
	 * \return a non-owning read-only pointer to the underlying in6_addr
	 *         structure.
	 */
	[[nodiscard]] constexpr const in6_addr* get() const noexcept {
		return &sin6_addr;
	}

	/**
	 * Compare this address to another for equality.
	 *
	 * \param other the address to compare this one to.
	 *
	 * \return true if the addresses are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const IPv6Address& other) const noexcept {
		return toBigEndianBytes() == other.toBigEndianBytes();
	}

	/**
	 * Compare this address to another.
	 *
	 * \param other the address to compare this one to.
	 *
	 * \return a strong ordering between the two addresses, based on the
	 *         lexicographical ordering of their big-endian byte value arrays.
	 */
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const IPv6Address& other) const noexcept {
		return toBigEndianBytes() <=> other.toBigEndianBytes();
	}

private:
	in6_addr sin6_addr;
};

inline constexpr IPv6Address IPv6Address::ANY{detail::IPV6_ADDRESS_ANY};
inline constexpr IPv6Address IPv6Address::LOOPBACK{detail::IPV6_ADDRESS_LOOPBACK};

/**
 * Combined IPv4Address and PortNumber.
 */
class IPv4Endpoint {
public:
	/**
	 * Parse an IPv4 address from an ASCII string in dot-decimal notation, and a
	 * port number from a decimal ASCII string.
	 *
	 * \param addressString string containing the four-decimal address to parse.
	 * \param portNumberString string containing the decimal port number to
	 *        parse.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        IPv4 address or port number, or cleared on success.
	 *
	 * \return the parsed IPv4 endpoint, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept;

	/**
	 * Parse an IPv4 address from an ASCII string in dot-decimal notation, and a
	 * port number from a decimal ASCII string, throwing an exception on
	 * failure.
	 *
	 * \param addressString string containing the four-decimal address to parse.
	 * \param portNumberString string containing the decimal port number to
	 *        parse.
	 *
	 * \return the parsed IPv4 endpoint.
	 *
	 * \throws networking::Error on failure to parse a valid IPv4 address or
	 *         port number.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint parse(CStringView addressString, CStringView portNumberString);

	/**
	 * Parse an IPv4 address from an ASCII string in dot-decimal notation with
	 * an optional decimal port number.
	 *
	 * \param string string containing the four-decimal address to parse, and
	 *        potentially a decimal port number separated by a colon.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        IPv4 address or port number, or cleared on success.
	 *
	 * \return the parsed IPv4 endpoint, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> parse(CStringView string, std::error_code& errorCode) noexcept;

	/**
	 * Parse an IPv4 address from an ASCII string in dot-decimal notation with
	 * an optional decimal port number, throwing an exception on failure.
	 *
	 * \param string string containing the four-decimal address to parse, and
	 *        potentially a decimal port number separated by a colon.
	 *
	 * \return the parsed IPv4 endpoint.
	 *
	 * \throws networking::Error on failure to parse a valid IPv4 address or
	 *         port number.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint parse(CStringView string);

	/**
	 * Resolve the IPv4 address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com" or "127.0.0.1", or nullptr to look
	 *        up by service only. Must not be nullptr if service is nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved IPv4 endpoint, or an empty optional if resolution
	 *         failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> resolve(const char* hostName, const char* service, std::error_code& errorCode);

	/**
	 * Resolve the IPv4 address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup, throwing an exception
	 * on failure.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com" or "127.0.0.1", or nullptr to look
	 *        up by service only. Must not be nullptr if service is nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 *
	 * \return the resolved IPv4 endpoint.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint resolve(const char* hostName, const char* service);

	/**
	 * Resolve the IPv4 address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http" or "127.0.0.1:80", or a host name-only
	 *        string with no colon, e.g. "example.com" or "127.0.0.1".
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved IPv4 endpoint, or an empty optional if resolution
	 *         failed.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> resolve(CStringView host, std::error_code& errorCode);

	/**
	 * Resolve the IPv4 address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup, throwing an exception
	 * on failure.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http" or "127.0.0.1:80", or a host name-only
	 *        string with no colon, e.g. "example.com" or "127.0.0.1".
	 *
	 * \return the resolved IPv4 endpoint.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint resolve(CStringView host);

	/**
	 * Get the IPv4 address and port of the local host on the local network by
	 * calling `connect()` on a temporary connectionless socket to the loopback
	 * address on port 9 (the "Discard" service) and querying its local address
	 * with `getsockname()`.
	 *
	 * \param errorCode error code that is filled in on failure to get the local
	 *        address, or cleared on success.
	 *
	 * \return the local IPv4 endpoint (typically with a private Class C address
	 *         like "192.168.1.2"), or an empty optional on failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv4Endpoint> getLocalEndpoint(std::error_code& errorCode);

	/**
	 * Get the IPv4 address and port of the local host on the local network by
	 * calling `connect()` on a temporary connectionless socket to the loopback
	 * address on port 9 (the "Discard" service) and querying its local address
	 * with `getsockname()`, throwing an exception on failure.
	 *
	 * \return the local IPv4 endpoint (typically with a private Class C address
	 *         like "192.168.1.2").
	 *
	 * \throws networking::Error on failure to get the local address.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv4Endpoint getLocalEndpoint();

	/**
	 * Construct an IPv4 endpoint from an IPv4 address and port number.
	 *
	 * \param address IPv4 address of the endpoint.
	 * \param port port number of the endpoint.
	 */
	constexpr explicit IPv4Endpoint(IPv4Address address, PortNumber port = 0) noexcept
		: addr{} {
		addr.sin_family = AF_INET;
#ifdef __APPLE__
		addr.sin_len = sizeof(addr);
#endif
		addr.sin_addr = *address.get();
		addr.sin_port = convertHostEndianToBigEndian(port);
	}

	/**
	 * Construct an IPv4 endpoint from a native sockaddr_in structure.
	 *
	 * \param addr sockaddr_in structure to construct the IPv4 endpoint from.
	 */
	constexpr explicit IPv4Endpoint(const sockaddr_in& addr) noexcept
		: addr(addr) {}

	/**
	 * Set the address of the endpoint.
	 *
	 * \param newAddress new address to set.
	 */
	constexpr void setAddress(IPv4Address newAddress) noexcept {
		addr.sin_addr = *newAddress.get();
	}

	/**
	 * Set the port number of the endpoint.
	 *
	 * \param newPortNumber new port number to set.
	 */
	constexpr void setPortNumber(PortNumber newPortNumber) noexcept {
		addr.sin_port = convertHostEndianToBigEndian(decltype(addr.sin_port){newPortNumber});
	}

	/**
	 * Get the address of the endpoint.
	 *
	 * \return the endpoint's address.
	 */
	[[nodiscard]] constexpr IPv4Address getAddress() const noexcept {
		return IPv4Address{addr.sin_addr};
	}

	/**
	 * Get the port number of the endpoint.
	 *
	 * \return the endpoint's port number.
	 */
	[[nodiscard]] constexpr PortNumber getPortNumber() const noexcept {
		return PortNumber{convertBigEndianToHostEndian(addr.sin_port)};
	}

	/**
	 * Check if the value of this endpoint's IPv4 address is equal to the ANY
	 * address (0.0.0.0).
	 *
	 * \return `getAddress().isAny()`.
	 */
	[[nodiscard]] constexpr bool isAddressAny() const noexcept {
		return getAddress().isAny();
	}

	/**
	 * Check if this endpoint's IPv4 address is a loopback address (starts with
	 * 127).
	 *
	 * \return true if the endpoint's address is a loopback address, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool isAddressLoopback() const noexcept {
		return getAddress().isLoopback();
	}

	/**
	 * Check if this endpoint's IPv4 address is in a private address range.
	 *
	 * The address ranges reserved for private networks are:
	 * - 10.0.0.0 - 10.255.255.255
	 * - 172.16.0.0 - 172.31.255.255
	 * - 192.168.0.0 - 192.168.255.255
	 *
	 * \return true if the endpoint's address is a private address, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool isAddressPrivate() const noexcept {
		return getAddress().isPrivate();
	}

	/**
	 * Get a pointer to the underlying native sockaddr_in structure of the IPv4
	 * endpoint.
	 *
	 * \return a non-owning pointer to the underlying sockaddr_in structure.
	 */
	[[nodiscard]] constexpr sockaddr_in* get() noexcept {
		return &addr;
	}

	/**
	 * Get a pointer to the underlying native sockaddr_in structure of the IPv4
	 * endpoint.
	 *
	 * \return a non-owning read-only pointer to the underlying sockaddr_in
	 *         structure.
	 */
	[[nodiscard]] constexpr const sockaddr_in* get() const noexcept {
		return &addr;
	}

	/**
	 * Compare this endpoint to another for equality.
	 *
	 * \param other the endpoint to compare this one to.
	 *
	 * \return true if the addresses and ports are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(IPv4Endpoint other) const noexcept {
		return getAddress() == other.getAddress() && getPortNumber() == other.getPortNumber();
	}

	/**
	 * Compare this endpoint to another.
	 *
	 * \param other the endpoint to compare this one to.
	 *
	 * \return a strong ordering between the two endpoints, based on the
	 *         lexicographical ordering of their addresses' big-endian byte
	 *         value arrays, or their port numbers if the addresses are equal.
	 */
	[[nodiscard]] constexpr std::strong_ordering operator<=>(IPv4Endpoint other) const noexcept {
		return (getAddress() == other.getAddress()) ? getPortNumber() <=> other.getPortNumber() : getAddress() <=> other.getAddress();
	}

private:
	sockaddr_in addr;
};

/**
 * Combined IPv6Address, PortNumber and ScopeID.
 */
class IPv6Endpoint {
public:
	/**
	 * Parse an IPv6 address from an ASCII string representation, and a port
	 * number from a decimal ASCII string.
	 *
	 * \param addressString string containing the IPv6 address representation to
	 *        parse.
	 * \param portNumberString string containing the decimal port number to
	 *        parse.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        IPv6 address or port number, or cleared on success.
	 *
	 * \return the parsed IPv6 endpoint, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept;

	/**
	 * Parse an IPv6 address from an ASCII string representation, and a port
	 * number from a decimal ASCII string, throwing an exception on failure.
	 *
	 * \param addressString string containing the IPv6 address representation to
	 *        parse.
	 * \param portNumberString string containing the decimal port number to
	 *        parse.
	 *
	 * \return the parsed IPv6 endpoint.
	 *
	 * \throws networking::Error on failure to parse a valid IPv6 address or
	 *         port number.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint parse(CStringView addressString, CStringView portNumberString);

	/**
	 * Parse an IPv6 address from an ASCII string representation with an
	 * optional decimal port number.
	 *
	 * \param string string containing the IPv6 address representation to parse,
	 *        and potentially a decimal port number separated by a colon if the
	 *        address is enclosed in square brackets.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        IPv6 address or port number, or cleared on success.
	 *
	 * \return the parsed IPv6 endpoint, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> parse(CStringView string, std::error_code& errorCode) noexcept;

	/**
	 * Parse an IPv6 address from an ASCII string representation with an
	 * optional decimal port number, throwing an exception on failure.
	 *
	 * \param string string containing the IPv6 address representation to parse,
	 *        and potentially a decimal port number separated by a colon if the
	 *        address is enclosed in square brackets.
	 *
	 * \return the parsed IPv6 endpoint.
	 *
	 * \throws networking::Error on failure to parse a valid IPv6 address or
	 *         port number.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint parse(CStringView string);

	/**
	 * Resolve the IPv6 address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com" or "::1", or nullptr to look up by
	 *        service only. Must not be nullptr if service is nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved IPv6 endpoint, or an empty optional if resolution
	 *         failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> resolve(const char* hostName, const char* service, std::error_code& errorCode);

	/**
	 * Resolve the IPv6 address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup, throwing an exception
	 * on failure.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com" or "::1", or nullptr to look up by
	 *        service only. Must not be nullptr if service is nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 *
	 * \return the resolved IPv6 endpoint.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint resolve(const char* hostName, const char* service);

	/**
	 * Resolve the IPv6 address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http" or "[::1]:80", or a host name-only string,
	 *        e.g. "example.com" or "::1".
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved IPv6 endpoint, or an empty optional if resolution
	 *         failed.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> resolve(CStringView host, std::error_code& errorCode);

	/**
	 * Resolve the IPv6 address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup, throwing an exception
	 * on failure.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http" or "[::1]:80", or a host name-only string,
	 *        e.g. "example.com" or "::1".
	 *
	 * \return the resolved IPv6 endpoint.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint resolve(CStringView host);

	/**
	 * Get the IPv6 address and port of the local host by calling `connect()` on
	 * a temporary connectionless socket to the loopback address on port 9 (the
	 * "Discard" service) and querying its local address with `getsockname()`.
	 *
	 * \param errorCode error code that is filled in on failure to get the local
	 *        address, or cleared on success.
	 *
	 * \return the local IPv6 endpoint, or an empty optional on failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<IPv6Endpoint> getLocalEndpoint(std::error_code& errorCode);

	/**
	 * Get the IPv6 address and port of the local host by calling `connect()` on
	 * a temporary connectionless socket to the loopback address on port 9 (the
	 * "Discard" service) and querying its local address with `getsockname()`,
	 * throwing an exception on failure.
	 *
	 * \return the local IPv6 endpoint.
	 *
	 * \throws networking::Error on failure to get the local address.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static IPv6Endpoint getLocalEndpoint();

	/**
	 * Construct an IPv6 endpoint from an IPv6 address, port number and
	 * scope/zone ID.
	 *
	 * \param address IPv6 address of the endpoint.
	 * \param port port number of the endpoint.
	 * \param scopeID scope/zone ID of the endpoint.
	 */
	constexpr explicit IPv6Endpoint(const IPv6Address& address, PortNumber port = 0, ScopeID scopeID = 0) noexcept
		: addr{} {
		addr.sin6_family = AF_INET6;
#ifdef __APPLE__
		addr.sin_len = sizeof(addr);
#endif
		addr.sin6_addr = *address.get();
		addr.sin6_port = convertHostEndianToBigEndian(port);
		addr.sin6_scope_id = scopeID;
	}

	/**
	 * Construct an IPv6 endpoint from a native sockaddr_in6 structure.
	 *
	 * \param addr sockaddr_in6 structure to construct the IPv6 endpoint from.
	 */
	constexpr explicit IPv6Endpoint(const sockaddr_in6& addr) noexcept
		: addr(addr) {}

	/**
	 * Set the address of the endpoint.
	 *
	 * \param newAddress new address to set.
	 */
	constexpr void setAddress(const IPv6Address& newAddress) noexcept {
		addr.sin6_addr = *newAddress.get();
	}

	/**
	 * Set the port number of the endpoint.
	 *
	 * \param newPortNumber new port number to set.
	 */
	constexpr void setPortNumber(PortNumber newPortNumber) noexcept {
		addr.sin6_port = convertHostEndianToBigEndian(decltype(addr.sin6_port){newPortNumber});
	}

	/**
	 * Set the scope/zone ID of the endpoint.
	 *
	 * \param newScopeID new scope/zone ID to set.
	 */
	constexpr void setScopeID(ScopeID newScopeID) noexcept {
		addr.sin6_scope_id = newScopeID;
	}

	/**
	 * Get the address of the endpoint.
	 *
	 * \return the endpoint's address.
	 */
	[[nodiscard]] constexpr IPv6Address getAddress() const noexcept {
		return IPv6Address{addr.sin6_addr};
	}

	/**
	 * Get the port number of the endpoint.
	 *
	 * \return the endpoint's port number.
	 */
	[[nodiscard]] constexpr PortNumber getPortNumber() const noexcept {
		return PortNumber{convertBigEndianToHostEndian(addr.sin6_port)};
	}

	/**
	 * Get the scope/zone ID of the endpoint.
	 *
	 * \return the endpoint's scope/zone ID.
	 */
	[[nodiscard]] constexpr ScopeID getScopeID() const noexcept {
		return addr.sin6_scope_id;
	}

	/**
	 * Check if the value of this endpoint's IPv6 address is equal to the ANY
	 * address (::).
	 *
	 * \return `getAddress().isAny()`.
	 */
	[[nodiscard]] constexpr bool isAddressAny() const noexcept {
		return getAddress().isAny();
	}

	/**
	 * Check if the value of this endpoint's IPv6 address is equal to the
	 * LOOPBACK address (::1).
	 *
	 * \return `getAddress().isLoopback()`.
	 */
	[[nodiscard]] constexpr bool isAddressLoopback() const noexcept {
		return getAddress().isLoopback();
	}

	/**
	 * Get a pointer to the underlying native sockaddr_in6 structure of the IPv6
	 * endpoint.
	 *
	 * \return a non-owning pointer to the underlying sockaddr_in6 structure.
	 */
	[[nodiscard]] constexpr sockaddr_in6* get() noexcept {
		return &addr;
	}

	/**
	 * Get a pointer to the underlying native sockaddr_in6 structure of the IPv6
	 * endpoint.
	 *
	 * \return a non-owning read-only pointer to the underlying sockaddr_in6
	 *         structure.
	 */
	[[nodiscard]] constexpr const sockaddr_in6* get() const noexcept {
		return &addr;
	}

	/**
	 * Compare this endpoint to another for equality.
	 *
	 * \param other the endpoint to compare this one to.
	 *
	 * \return true if the addresses, ports and scope/zone IDs are equal, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const IPv6Endpoint& other) const noexcept {
		return getAddress() == other.getAddress() && getPortNumber() == other.getPortNumber() && getScopeID() == other.getScopeID();
	}

	/**
	 * Compare this endpoint to another.
	 *
	 * \param other the endpoint to compare this one to.
	 *
	 * \return a strong ordering between the two endpoints, based on the
	 *         lexicographical ordering of their addresses' big-endian byte
	 *         value arrays, or their port numbers if the addresses are equal,
	 *         or their scope/zone IDs if the port numbers are also equal.
	 */
	[[nodiscard]] constexpr std::strong_ordering operator<=>(const IPv6Endpoint& other) const noexcept {
		return (getAddress() == other.getAddress()) ? (getPortNumber() == other.getPortNumber()) ? getScopeID() <=> other.getScopeID() : getPortNumber() <=> other.getPortNumber()
		                                            : getAddress() <=> other.getAddress();
	}

private:
	sockaddr_in6 addr;
};

/**
 * Combined network address and port number (and potentially other info) that
 * identifies a host, or set of hosts, on the network.
 */
class Endpoint {
public:
	/**
	 * Create an endpoint with the ANY address (0.0.0.0 or ::) for a given
	 * address family.
	 *
	 * \param family address family of the endpoint to create.
	 * \param portNumber port number of the endpoint to create.
	 *
	 * \return `Endpoint{XXXXAddress::ANY, portNumber}`, where `XXXXAddress` is
	 *         the address type corresponding to the specified family.
	 */
	[[nodiscard]] static Endpoint any(AddressFamily family, PortNumber portNumber = 0) noexcept {
		switch (family) {
			case AddressFamily::IPv4: return Endpoint{IPv4Address::ANY, portNumber};
			case AddressFamily::IPv6: return Endpoint{IPv6Address::ANY, portNumber};
		}
		unreachable();
	}

	/**
	 * Create an endpoint with the LOOPBACK address (0.0.0.0 or ::) for a given
	 * address family.
	 *
	 * \param family address family of the endpoint to create.
	 * \param portNumber port number of the endpoint to create.
	 *
	 * \return `Endpoint{XXXXAddress::LOOPBACK, portNumber}`, where
	 *         `XXXXAddress` is the address type corresponding to the specified
	 *         family.
	 */
	[[nodiscard]] static Endpoint loopback(AddressFamily family, PortNumber portNumber = 0) noexcept {
		switch (family) {
			case AddressFamily::IPv4: return Endpoint{IPv4Address::LOOPBACK, portNumber};
			case AddressFamily::IPv6: return Endpoint{IPv6Address::LOOPBACK, portNumber};
		}
		unreachable();
	}

	/**
	 * Parse an endpoint address from an ASCII string representation, and a port
	 * number from a decimal ASCII string.
	 *
	 * \param addressString string containing the address representation to
	 *        parse.
	 * \param portNumberString string containing the decimal port number to
	 *        parse.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        address or port number, or cleared on success.
	 *
	 * \return the parsed endpoint, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept;

	/**
	 * Parse an endpoint address from an ASCII string representation, and a port
	 * number from a decimal ASCII string, throwing an exception on failure.
	 *
	 * \param addressString string containing the address representation to
	 *        parse.
	 * \param portNumberString string containing the decimal port number to
	 *        parse.
	 *
	 * \return the parsed endpoint.
	 *
	 * \throws networking::Error on failure to parse a valid address or port
	 *         number.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint parse(CStringView addressString, CStringView portNumberString);

	/**
	 * Parse an endpoint address of a specific address family type from an ASCII
	 * string representation, and a port number from a decimal ASCII string.
	 *
	 * \param family address family of the address to parse.
	 * \param addressString string containing the address representation to
	 *        parse.
	 * \param portNumberString string containing the decimal port number to
	 *        parse.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        address or port number, or cleared on success.
	 *
	 * \return the parsed endpoint, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> parse(AddressFamily family, CStringView addressString, CStringView portNumberString,
		std::error_code& errorCode) noexcept;

	/**
	 * Parse an endpoint address of a specific address family type from an ASCII
	 * string representation, and a port number from a decimal ASCII string,
	 * throwing an exception on failure.
	 *
	 * \param family address family of the address to parse.
	 * \param addressString string containing the address representation to
	 *        parse.
	 * \param portNumberString string containing the decimal port number to
	 *        parse.
	 *
	 * \return the parsed endpoint.
	 *
	 * \throws networking::Error on failure to parse a valid address or port
	 *         number.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint parse(AddressFamily family, CStringView addressString, CStringView portNumberString);

	/**
	 * Parse an endpoint address from an ASCII string representation with an
	 * optional decimal port number.
	 *
	 * \param string string containing the address representation to parse, and
	 *        potentially a decimal port number separated by a colon if the
	 *        address is enclosed in square brackets or does not contain another
	 *        colon.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        address or port number, or cleared on success.
	 *
	 * \return the parsed endpoint, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> parse(CStringView string, std::error_code& errorCode) noexcept;

	/**
	 * Parse an endpoint address from an ASCII string representation with an
	 * optional decimal port number, throwing an exception on failure.
	 *
	 * \param string string containing the address representation to parse, and
	 *        potentially a decimal port number separated by a colon if the
	 *        address is enclosed in square brackets or does not contain another
	 *        colon.
	 *
	 * \return the parsed endpoint.
	 *
	 * \throws networking::Error on failure to parse a valid address or port
	 *         number.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint parse(CStringView string);

	/**
	 * Parse an endpoint address of a specific address family type from an ASCII
	 * string representation with an optional decimal port number.
	 *
	 * \param family address family of the address to parse.
	 * \param string string containing the address representation to parse, and
	 *        potentially a decimal port number separated by a colon if the
	 *        address is enclosed in square brackets or does not contain another
	 *        colon.
	 * \param errorCode error code that is filled in on failure to parse a valid
	 *        address or port number, or cleared on success.
	 *
	 * \return the parsed endpoint, or an empty optional if parsing failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> parse(AddressFamily family, CStringView string, std::error_code& errorCode) noexcept;

	/**
	 * Parse an endpoint address of a specific address family type from an ASCII
	 * string representation with an optional decimal port number, throwing an
	 * exception on failure.
	 *
	 * \param family address family of the address to parse.
	 * \param string string containing the address representation to parse, and
	 *        potentially a decimal port number separated by a colon if the
	 *        address is enclosed in square brackets or does not contain another
	 *        colon.
	 *
	 * \return the parsed endpoint.
	 *
	 * \throws networking::Error on failure to parse a valid address or port
	 *         number.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint parse(AddressFamily family, CStringView string);

	/**
	 * Resolve the endpoint address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com", "127.0.0.1" or "::1", or nullptr
	 *        to look up by service only. Must not be nullptr if service is
	 *        nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved endpoint, or an empty optional if resolution failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> resolve(const char* hostName, const char* service, std::error_code& errorCode);

	/**
	 * Resolve the endpoint address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup. throwing an exception on
	 * failure.
	 *
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com", "127.0.0.1" or "::1", or nullptr
	 *        to look up by service only. Must not be nullptr if service is
	 *        nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 *
	 * \return the resolved endpoint.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint resolve(const char* hostName, const char* service);

	/**
	 * Resolve the endpoint address and port number of a host with a specific
	 * address family type through the Domain Name System (DNS) using a
	 * `getaddrinfo()` lookup.
	 *
	 * \param family address family of the endpoint to resolve.
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com", "127.0.0.1" or "::1", or nullptr
	 *        to look up by service only. Must not be nullptr if service is
	 *        nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved endpoint, or an empty optional if resolution failed.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> resolve(AddressFamily family, const char* hostName, const char* service, std::error_code& errorCode);

	/**
	 * Resolve the endpoint address and port number of a host with a specific
	 * address family type through the Domain Name System (DNS) using a
	 * `getaddrinfo()` lookup. throwing an exception on failure.
	 *
	 * \param family address family of the endpoint to resolve.
	 * \param hostName null-terminated domain name or address string of the host
	 *        to look up, e.g. "example.com", "127.0.0.1" or "::1", or nullptr
	 *        to look up by service only. Must not be nullptr if service is
	 *        nullptr.
	 * \param service null-terminated service or port string of the host to look
	 *        up, e.g. "http" or "80", or nullptr to look up by host name only.
	 *        Must not be nullptr if hostName is nullptr.
	 *
	 * \return the resolved endpoint.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint resolve(AddressFamily family, const char* hostName, const char* service);

	/**
	 * Resolve the endpoint address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http", "127.0.0.1:80" or "[::1]:80", or a host
	 *        name-only string, e.g. "example.com", "127.0.0.1" or "::1".
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved endpoint, or an empty optional if resolution failed.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> resolve(CStringView host, std::error_code& errorCode);

	/**
	 * Resolve the endpoint address and port number of a host through the Domain
	 * Name System (DNS) using a `getaddrinfo()` lookup, throwing an exception
	 * on failure.
	 *
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http", "127.0.0.1:80" or "[::1]:80", or a host
	 *        name-only string, e.g. "example.com", "127.0.0.1" or "::1".
	 *
	 * \return the resolved endpoint.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint resolve(CStringView host);

	/**
	 * Resolve the endpoint address and port number of a host with a specific
	 * address family type through the Domain Name System (DNS) using a
	 * `getaddrinfo()` lookup.
	 *
	 * \param family address family of the endpoint to resolve.
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http", "127.0.0.1:80" or "[::1]:80", or a host
	 *        name-only string, e.g. "example.com", "127.0.0.1" or "::1".
	 * \param errorCode error code that is filled in on failure to resolve the
	 *        host, or cleared on success.
	 *
	 * \return the resolved endpoint, or an empty optional if resolution failed.
	 *
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> resolve(AddressFamily family, CStringView host, std::error_code& errorCode);

	/**
	 * Resolve the endpoint address and port number of a host with a specific
	 * address family type through the Domain Name System (DNS) using a
	 * `getaddrinfo()` lookup, throwing an exception on failure.
	 *
	 * \param family address family of the endpoint to resolve.
	 * \param host combined host name and service string separated by a colon,
	 *        e.g. "example.com:http", "127.0.0.1:80" or "[::1]:80", or a host
	 *        name-only string, e.g. "example.com", "127.0.0.1" or "::1".
	 *
	 * \return the resolved endpoint.
	 *
	 * \throws networking::Error on failure to resolve the host.
	 * \throws std::length_error if an internal size limit was exceeded.
	 * \throws std::bad_array_new_length if an internal size limit was exceeded.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint resolve(AddressFamily family, CStringView host);

	/**
	 * Get the endpoint address and port of the local host by calling
	 * `connect()` on a temporary connectionless socket to the loopback address
	 * on port 9 (the "Discard" service) and querying its local address with
	 * `getsockname()`.
	 *
	 * \param family address family of the endpoint address to get.
	 * \param errorCode error code that is filled in on failure to get the local
	 *        address, or cleared on success.
	 *
	 * \return the local endpoint, or an empty optional on failure.
	 */
	[[nodiscard]] GREM_API(networking) static Optional<Endpoint> getLocalEndpoint(AddressFamily family, std::error_code& errorCode);

	/**
	 * Get the endpoint address and port of the local host by calling
	 * `connect()` on a temporary connectionless socket to the loopback address
	 * on port 9 (the "Discard" service) and querying its local address with
	 * `getsockname()`, throwing an exception on failure.
	 *
	 * \param family address family of the endpoint address to get.
	 *
	 * \return the local endpoint.
	 *
	 * \throws networking::Error on failure to get the local address.
	 * \throws std::bad_alloc on allocation failure.
	 */
	[[nodiscard]] GREM_API(networking) static Endpoint getLocalEndpoint(AddressFamily family);

	/**
	 * Construct an endpoint from a native sockaddr_storage buffer.
	 *
	 * \param addrStorage non-owning read-only pointer to the address structure
	 *        or sockaddr_storage to read the endpoint data from. Must point to
	 *        a valid native address structure of `length` bytes in size, and
	 *        must not be nullptr.
	 * \param length size of the concrete sockaddr type pointed to by
	 *        addrStorage, in bytes. Must be positive.
	 */
	template <typename SockAddr>
	Endpoint(const SockAddr* addrStorage, socklen_t length)
		requires(same_as<SockAddr, sockaddr_storage> || same_as<SockAddr, sockaddr_in> || same_as<SockAddr, sockaddr_in6> || convertible_to<const SockAddr*, const sockaddr*>) {
		GREM_ASSERT(addrStorage);
		GREM_ASSERT(length > 0);
		GREM_ASSERT(static_cast<size_t>(length) <= sizeof(sockaddr_storage));
		memcpy(storage, addrStorage, static_cast<size_t>(length));
	}

	/**
	 * Implicitly convert or construct a generic endpoint from an IPv4 endpoint.
	 *
	 * \param endpoint IPv4 endpoint to construct the endpoint from.
	 */
	Endpoint(const IPv4Endpoint& endpoint) noexcept {
		memcpy(storage, endpoint.get(), sizeof(*endpoint.get()));
	}

	/**
	 * Implicitly convert or construct a generic endpoint from an IPv6 endpoint.
	 *
	 * \param endpoint IPv6 endpoint to construct the endpoint from.
	 */
	Endpoint(const IPv6Endpoint& endpoint) noexcept {
		memcpy(storage, endpoint.get(), sizeof(*endpoint.get()));
	}

	/**
	 * Construct an endpoint from an IPv4 address and port number.
	 *
	 * \param address IPv4 address of the endpoint to construct.
	 * \param port port number of the endpoint to construct.
	 */
	explicit Endpoint(const IPv4Address& address, PortNumber port = 0) noexcept
		: Endpoint(IPv4Endpoint{address, port}) {}

	/**
	 * Construct an endpoint from an IPv4 address and port number.
	 *
	 * \param address IPv4 address of the endpoint to construct.
	 * \param port port number of the endpoint to construct.
	 * \param scopeID scope/zone ID of the endpoint to construct.
	 */
	explicit Endpoint(const IPv6Address& address, PortNumber port = 0, ScopeID scopeID = 0) noexcept
		: Endpoint(IPv6Endpoint{address, port, scopeID}) {}

	/**
	 * Construct an IPv4 endpoint from a native sockaddr_in structure.
	 *
	 * \param addr sockaddr_in structure to construct the endpoint from.
	 */
	explicit Endpoint(const sockaddr_in& addr) noexcept
		: Endpoint(IPv4Endpoint{addr}) {}

	/**
	 * Construct an IPv6 endpoint from a native sockaddr_in6 structure.
	 *
	 * \param addr sockaddr_in6 structure to construct the endpoint from.
	 */
	explicit Endpoint(const sockaddr_in6& addr) noexcept
		: Endpoint(IPv6Endpoint{addr}) {}

	/** Destructor. */
	~Endpoint() = default;

	/** Copy constructor. */
	Endpoint(const Endpoint& other) noexcept {
		memcpy(storage, other.storage, sizeof(other.storage));
	}

	/** Move constructor. */
	Endpoint(Endpoint&& other) noexcept
		: Endpoint(other) {} // NOLINT(performance-move-constructor-init)

	/** Copy assignment. */
	Endpoint& operator=(const Endpoint& other) noexcept {
		memcpy(storage, other.storage, sizeof(other.storage));
		return *this;
	}

	/** Move assignment. */
	Endpoint& operator=(Endpoint&& other) noexcept {
		return *this = other;
	}

	/**
	 * Set the port number of the endpoint.
	 *
	 * \param newPortNumber new port number to set.
	 */
	void setPortNumber(PortNumber newPortNumber) noexcept {
		switch (getAddressFamily()) {
			case AddressFamily::IPv4: *this = Endpoint{*getIPv4Address(), newPortNumber}; break;
			case AddressFamily::IPv6: *this = Endpoint{*getIPv6Address(), newPortNumber}; break;
		}
	}

	/**
	 * Get the address family of the endpoint.
	 *
	 * \return the endpoint's address family.
	 */
	[[nodiscard]] AddressFamily getAddressFamily() const noexcept {
		short family{};
		memcpy(&family, storage, sizeof(family));
		return static_cast<AddressFamily>(family);
	}

	/**
	 * Get the size of the endpoint's underlying native address structure.
	 *
	 * \return the endpoint's length, in bytes.
	 */
	[[nodiscard]] socklen_t getLength() const noexcept {
		switch (getAddressFamily()) {
			case AddressFamily::IPv4: return static_cast<socklen_t>(sizeof(sockaddr_in));
			case AddressFamily::IPv6: return static_cast<socklen_t>(sizeof(sockaddr_in6));
		}
		return 0;
	}

	/**
	 * Get the IPv4 endpoint stored by this endpoint.
	 *
	 * \return the IPv4 endpoint if the address family is IPv4, or an empty
	 *         optional otherwise.
	 */
	[[nodiscard]] Optional<IPv4Endpoint> getIPv4Endpoint() const noexcept {
		return (getAddressFamily() == AddressFamily::IPv4)
		           ? Optional<IPv4Endpoint>{std::in_place, *std::launder(reinterpret_cast<const sockaddr_in*>(storage))}
		           : Optional<IPv4Endpoint>{};
	}

	/**
	 * Get the IPv6 endpoint stored by this endpoint.
	 *
	 * \return the IPv6 endpoint if the address family is IPv6, or an empty
	 *         optional otherwise.
	 */
	[[nodiscard]] Optional<IPv6Endpoint> getIPv6Endpoint() const noexcept {
		return (getAddressFamily() == AddressFamily::IPv6)
		           ? Optional<IPv6Endpoint>{std::in_place, *std::launder(reinterpret_cast<const sockaddr_in6*>(storage))}
		           : Optional<IPv6Endpoint>{};
	}

	/**
	 * Get the IPv4 address of this endpoint.
	 *
	 * \return the endpoint's IPv4 if its address family is IPv4, or an empty
	 *         optional otherwise.
	 */
	[[nodiscard]] Optional<IPv4Address> getIPv4Address() const noexcept {
		return (getAddressFamily() == AddressFamily::IPv4) ? Optional<IPv4Address>{std::in_place, getIPv4Endpoint()->getAddress()} : Optional<IPv4Address>{};
	}

	/**
	 * Get the IPv6 address of this endpoint.
	 *
	 * \return the endpoint's IPv6 if its address family is IPv6, or an empty
	 *         optional otherwise.
	 */
	[[nodiscard]] Optional<IPv6Address> getIPv6Address() const noexcept {
		return (getAddressFamily() == AddressFamily::IPv6) ? Optional<IPv6Address>{std::in_place, getIPv6Endpoint()->getAddress()} : Optional<IPv6Address>{};
	}

	/**
	 * Get the port number of the endpoint.
	 *
	 * \return the endpoint's port number, or 0 if it does not have a port
	 *         number.
	 */
	[[nodiscard]] PortNumber getPortNumber() const noexcept {
		switch (getAddressFamily()) {
			case AddressFamily::IPv4: return getIPv4Endpoint()->getPortNumber();
			case AddressFamily::IPv6: return getIPv6Endpoint()->getPortNumber();
		}
		return 0;
	}

	/**
	 * Get the scope/zone ID of the endpoint.
	 *
	 * \return the endpoint's scope/zone ID, or 0 if it does not have a
	 *         scope/zone ID.
	 */
	[[nodiscard]] ScopeID getScopeID() const noexcept {
		switch (getAddressFamily()) {
			case AddressFamily::IPv4: break;
			case AddressFamily::IPv6: return getIPv6Endpoint()->getScopeID();
		}
		return 0;
	}

	/**
	 * Check if the value of this endpoint's address is equal to the ANY address
	 * (0.0.0.0 or ::).
	 *
	 * \return true if this endpoint has the ANY address, false otherwise.
	 */
	[[nodiscard]] bool isAddressAny() const noexcept {
		switch (getAddressFamily()) {
			case AddressFamily::IPv4: return getIPv4Endpoint()->isAddressAny();
			case AddressFamily::IPv6: return getIPv6Endpoint()->isAddressAny();
		}
		return false;
	}

	/**
	 * Check if the value of this endpoint's address is a loopback address
	 * (127.X.X.X or ::1).
	 *
	 * \return true if this endpoint has a loopback address, false otherwise.
	 */
	[[nodiscard]] bool isAddressLoopback() const noexcept {
		switch (getAddressFamily()) {
			case AddressFamily::IPv4: return getIPv4Endpoint()->isAddressLoopback();
			case AddressFamily::IPv6: return getIPv6Endpoint()->isAddressLoopback();
		}
		return false;
	}

	/**
	 * Get a pointer to the underlying native address structure of the endpoint.
	 *
	 * \return a non-owning pointer to the underlying address structure.
	 */
	[[nodiscard]] sockaddr* get() noexcept {
		return reinterpret_cast<sockaddr*>(storage);
	}

	/**
	 * Get a pointer to the underlying native address structure of the endpoint.
	 *
	 * \return a non-owning read-only pointer to the underlying address
	 *         structure.
	 */
	[[nodiscard]] const sockaddr* get() const noexcept {
		return reinterpret_cast<const sockaddr*>(storage);
	}

	/**
	 * Compare this endpoint to another for equality.
	 *
	 * \param other the endpoint to compare this one to.
	 *
	 * \return true if the endpoints are equal, false otherwise.
	 */
	[[nodiscard]] bool operator==(const Endpoint& other) const noexcept {
		const AddressFamily family = getAddressFamily();
		const AddressFamily otherFamily = other.getAddressFamily();
		if (family == otherFamily) {
			switch (family) {
				case AddressFamily::IPv4: return *getIPv4Endpoint() == *other.getIPv4Endpoint();
				case AddressFamily::IPv6: return *getIPv6Endpoint() == *other.getIPv6Endpoint();
			}
			return true;
		}
		return false;
	}

	/**
	 * Compare this endpoint to another.
	 *
	 * \param other the endpoint to compare this one to.
	 *
	 * \return a strong ordering between the two endpoints.
	 */
	[[nodiscard]] std::strong_ordering operator<=>(const Endpoint& other) const noexcept {
		const AddressFamily family = getAddressFamily();
		const AddressFamily otherFamily = other.getAddressFamily();
		if (family == otherFamily) {
			switch (family) {
				case AddressFamily::IPv4: return *getIPv4Endpoint() <=> *other.getIPv4Endpoint();
				case AddressFamily::IPv6: return *getIPv6Endpoint() <=> *other.getIPv6Endpoint();
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
		switch (endpoint.getAddressFamily()) {
			case grem::networking::AddressFamily::IPv4: return grem::getHash(*endpoint.getIPv4Endpoint());
			case grem::networking::AddressFamily::IPv6: return grem::getHash(*endpoint.getIPv6Endpoint());
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
		switch (value.getAddressFamily()) {
			case grem::networking::AddressFamily::IPv4: {
				Formatter<grem::networking::IPv4Endpoint> formatter{};
				formatter.alwaysIncludePort = alwaysIncludePort;
				formatter.formatTo(output, *value.getIPv4Endpoint());
				break;
			}
			case grem::networking::AddressFamily::IPv6: {
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
