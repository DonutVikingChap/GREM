// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/Optional.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/networking/Endpoint.hpp>
#include <GREM/networking/Error.hpp>
#include <GREM/networking/Socket.hpp>
#include <GREM/networking/platform.hpp>

#include <charconv>     // std::from_chars_result, std::from_chars
#include <new>          // std::launder
#include <system_error> // std::error_code, std::generic_category, std::errc, std::make_error_code(std::errc)

#ifndef _WIN32
#include <cerrno> // errno
#endif

namespace grem::networking {

namespace {

Optional<PortNumber> parsePortNumber(CStringView string, std::error_code& errorCode) noexcept {
	PortNumber result{};
	const char* const begin = string.c_str();
	const char* end = begin;
	while (*end >= '0' && *end <= '9') {
		++end;
	}
	if (const std::from_chars_result parseResult = std::from_chars(begin, end, result); parseResult.ec != std::errc{}) {
		errorCode = make_error_code(parseResult.ec);
		return {};
	}
	errorCode.clear();
	return result;
}

} // namespace

Optional<IPv4Address> IPv4Address::parse(CStringView string, std::error_code& errorCode) noexcept {
	ensurePlatformInitialized(errorCode);
	if (errorCode) {
		return {};
	}

	in_addr addr{};
	switch (inet_pton(AF_INET, string.c_str(), &addr)) {
		case 0: errorCode = make_error_code(std::errc::invalid_argument); return {};
		case 1: break;
		default:
			errorCode = Error::getLastErrorCode();
			if (!errorCode) {
				errorCode = make_error_code(std::errc::address_family_not_supported);
			}
			return {};
	}
	errorCode.clear();
	return IPv4Address{addr};
}

Optional<IPv4Address> IPv4Address::resolve(const char* hostName, const char* service, std::error_code& errorCode) {
	return IPv4Endpoint::resolve(hostName, service, errorCode).transform(&IPv4Endpoint::getAddress);
}

Optional<IPv4Address> IPv4Address::resolve(CStringView host, std::error_code& errorCode) {
	return IPv4Endpoint::resolve(host, errorCode).transform(&IPv4Endpoint::getAddress);
}

Optional<IPv4Address> IPv4Address::getLocalAddress(std::error_code& errorCode) {
	return IPv4Endpoint::getLocalEndpoint(errorCode).transform(&IPv4Endpoint::getAddress);
}

IPv4Address IPv4Address::parse(CStringView string) {
	std::error_code errorCode{};
	Optional<IPv4Address> result = parse(string, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

IPv4Address IPv4Address::resolve(const char* hostName, const char* service) {
	std::error_code errorCode{};
	Optional<IPv4Address> result = resolve(hostName, service, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

IPv4Address IPv4Address::resolve(CStringView host) {
	std::error_code errorCode{};
	Optional<IPv4Address> result = resolve(host, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

IPv4Address IPv4Address::getLocalAddress() {
	std::error_code errorCode{};
	Optional<IPv4Address> result = getLocalAddress(errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

Optional<IPv6Address> IPv6Address::parse(CStringView string, std::error_code& errorCode) noexcept {
	ensurePlatformInitialized(errorCode);
	if (errorCode) {
		return {};
	}

	in6_addr addr{};
	switch (inet_pton(AF_INET6, string.c_str(), &addr)) {
		case 0: errorCode = make_error_code(std::errc::invalid_argument); return {};
		case 1: break;
		default:
			errorCode = Error::getLastErrorCode();
			if (!errorCode) {
				errorCode = make_error_code(std::errc::address_family_not_supported);
			}
			return {};
	}
	errorCode.clear();
	return IPv6Address{addr};
}

Optional<IPv6Address> IPv6Address::resolve(const char* hostName, const char* service, std::error_code& errorCode) {
	return IPv6Endpoint::resolve(hostName, service, errorCode).transform(&IPv6Endpoint::getAddress);
}

Optional<IPv6Address> IPv6Address::resolve(CStringView host, std::error_code& errorCode) {
	return IPv6Endpoint::resolve(host, errorCode).transform(&IPv6Endpoint::getAddress);
}

Optional<IPv6Address> IPv6Address::getLocalAddress(std::error_code& errorCode) {
	return IPv6Endpoint::getLocalEndpoint(errorCode).transform(&IPv6Endpoint::getAddress);
}

IPv6Address IPv6Address::parse(CStringView string) {
	std::error_code errorCode{};
	Optional<IPv6Address> result = parse(string, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

IPv6Address IPv6Address::resolve(const char* hostName, const char* service) {
	std::error_code errorCode{};
	Optional<IPv6Address> result = resolve(hostName, service, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

IPv6Address IPv6Address::resolve(CStringView host) {
	std::error_code errorCode{};
	Optional<IPv6Address> result = resolve(host, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

IPv6Address IPv6Address::getLocalAddress() {
	std::error_code errorCode{};
	Optional<IPv6Address> result = getLocalAddress(errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

Optional<IPv4Endpoint> IPv4Endpoint::parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept {
	const Optional<IPv4Address> address = IPv4Address::parse(addressString, errorCode);
	if (!address) {
		return {};
	}
	const Optional<PortNumber> portNumber = parsePortNumber(portNumberString, errorCode);
	if (!portNumber) {
		return {};
	}
	return IPv4Endpoint{*address, *portNumber};
}

Optional<IPv4Endpoint> IPv4Endpoint::parse(CStringView string, std::error_code& errorCode) noexcept {
	if (const size_t lastColonOffset = string.rfind(':'); lastColonOffset != CStringView::npos && lastColonOffset < INET_ADDRSTRLEN) {
		Array<char, INET_ADDRSTRLEN> addressStringBuffer{};
		if (lastColonOffset > 0) {
			memcpy(&addressStringBuffer, string.data(), lastColonOffset);
		}
		return IPv4Endpoint::parse(CStringView{addressStringBuffer.data()}, string.substr(lastColonOffset + 1), errorCode);
	}
	const Optional<IPv4Address> address = IPv4Address::parse(string, errorCode);
	if (!address) {
		return {};
	}
	return IPv4Endpoint{*address, 0};
}

Optional<IPv4Endpoint> IPv4Endpoint::resolve(const char* hostName, const char* service, std::error_code& errorCode) {
	return Endpoint::resolve(EndpointFamily::IPv4, hostName, service, errorCode).and_then(&Endpoint::getIPv4Endpoint);
}

Optional<IPv4Endpoint> IPv4Endpoint::resolve(CStringView host, std::error_code& errorCode) {
	if (const size_t lastColonOffset = host.rfind(':'); lastColonOffset != CStringView::npos) {
		return IPv4Endpoint::resolve(String{host.c_str(), lastColonOffset}.c_str(), host.substr(lastColonOffset + 1).c_str(), errorCode);
	}
	return IPv4Endpoint::resolve(host.c_str(), nullptr, errorCode);
}

Optional<IPv4Endpoint> IPv4Endpoint::getLocalEndpoint(std::error_code& errorCode) {
	return Endpoint::getLocalEndpoint(EndpointFamily::IPv4, errorCode).and_then(&Endpoint::getIPv4Endpoint);
}

IPv4Endpoint IPv4Endpoint::parse(CStringView addressString, CStringView portNumberString) {
	std::error_code errorCode{};
	Optional<IPv4Endpoint> result = parse(addressString, portNumberString, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

IPv4Endpoint IPv4Endpoint::parse(CStringView string) {
	std::error_code errorCode{};
	Optional<IPv4Endpoint> result = parse(string, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

IPv4Endpoint IPv4Endpoint::resolve(const char* hostName, const char* service) {
	std::error_code errorCode{};
	Optional<IPv4Endpoint> result = resolve(hostName, service, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

IPv4Endpoint IPv4Endpoint::resolve(CStringView host) {
	std::error_code errorCode{};
	Optional<IPv4Endpoint> result = resolve(host, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

IPv4Endpoint IPv4Endpoint::getLocalEndpoint() {
	std::error_code errorCode{};
	Optional<IPv4Endpoint> result = getLocalEndpoint(errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

Optional<IPv6Endpoint> IPv6Endpoint::parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept {
	const Optional<IPv6Address> address = IPv6Address::parse(addressString, errorCode);
	if (!address) {
		return {};
	}
	const Optional<PortNumber> portNumber = parsePortNumber(portNumberString, errorCode);
	if (!portNumber) {
		return {};
	}
	return IPv6Endpoint{*address, *portNumber};
}

Optional<IPv6Endpoint> IPv6Endpoint::parse(CStringView string, std::error_code& errorCode) noexcept {
	if (const size_t lastColonOffset = string.rfind(':'); lastColonOffset != CStringView::npos) {
		if (lastColonOffset > 1 && string[lastColonOffset - 1] == ']' && string[0] == '[' && lastColonOffset - 2 < INET6_ADDRSTRLEN) {
			Array<char, INET6_ADDRSTRLEN> addressStringBuffer{};
			if (lastColonOffset > 2) {
				memcpy(&addressStringBuffer, string.data() + 1, lastColonOffset - 2);
			}
			return IPv6Endpoint::parse(CStringView{addressStringBuffer.data()}, string.substr(lastColonOffset + 1), errorCode);
		}
	}
	const Optional<IPv6Address> address = IPv6Address::parse(string, errorCode);
	if (!address) {
		return {};
	}
	return IPv6Endpoint{*address, 0};
}

Optional<IPv6Endpoint> IPv6Endpoint::resolve(const char* hostName, const char* service, std::error_code& errorCode) {
	return Endpoint::resolve(EndpointFamily::IPv6, hostName, service, errorCode).and_then(&Endpoint::getIPv6Endpoint);
}

Optional<IPv6Endpoint> IPv6Endpoint::resolve(CStringView host, std::error_code& errorCode) {
	if (const size_t lastColonOffset = host.rfind(':'); lastColonOffset != CStringView::npos) {
		if (lastColonOffset > 0 && host[lastColonOffset - 1] == ']' && host[0] == '[') {
			return IPv6Endpoint::resolve(String{host.data() + 1, lastColonOffset - 2}.c_str(), host.substr(lastColonOffset + 1).c_str(), errorCode);
		}
	}
	return IPv6Endpoint::resolve(host.c_str(), nullptr, errorCode);
}

Optional<IPv6Endpoint> IPv6Endpoint::getLocalEndpoint(std::error_code& errorCode) {
	return Endpoint::getLocalEndpoint(EndpointFamily::IPv6, errorCode).and_then(&Endpoint::getIPv6Endpoint);
}

IPv6Endpoint IPv6Endpoint::parse(CStringView addressString, CStringView portNumberString) {
	std::error_code errorCode{};
	Optional<IPv6Endpoint> result = parse(addressString, portNumberString, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

IPv6Endpoint IPv6Endpoint::parse(CStringView string) {
	std::error_code errorCode{};
	Optional<IPv6Endpoint> result = parse(string, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

IPv6Endpoint IPv6Endpoint::resolve(const char* hostName, const char* service) {
	std::error_code errorCode{};
	Optional<IPv6Endpoint> result = resolve(hostName, service, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

IPv6Endpoint IPv6Endpoint::resolve(CStringView host) {
	std::error_code errorCode{};
	Optional<IPv6Endpoint> result = resolve(host, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

IPv6Endpoint IPv6Endpoint::getLocalEndpoint() {
	std::error_code errorCode{};
	Optional<IPv6Endpoint> result = getLocalEndpoint(errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

Optional<Endpoint> Endpoint::parse(CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept {
	if (addressString.contains(':')) {
		return IPv6Endpoint::parse(addressString, portNumberString, errorCode);
	}
	return IPv4Endpoint::parse(addressString, portNumberString, errorCode);
}

Optional<Endpoint> Endpoint::parse(CStringView string, std::error_code& errorCode) noexcept {
	if (const size_t lastColonOffset = string.rfind(':'); lastColonOffset != StringView::npos) {
		if (lastColonOffset > 1 && string[lastColonOffset - 1] == ']' && string[0] == '[' && lastColonOffset - 2 < INET6_ADDRSTRLEN) {
			Array<char, INET6_ADDRSTRLEN> addressStringBuffer{};
			if (lastColonOffset > 2) {
				memcpy(&addressStringBuffer, string.data() + 1, lastColonOffset - 2);
			}
			return IPv6Endpoint::parse(CStringView{addressStringBuffer.data()}, string.substr(lastColonOffset + 1), errorCode);
		}
		if (lastColonOffset < INET_ADDRSTRLEN && StringView{string.data(), lastColonOffset}.find(':') == StringView::npos) {
			Array<char, INET_ADDRSTRLEN> addressStringBuffer{};
			if (lastColonOffset > 0) {
				memcpy(&addressStringBuffer, string.data(), lastColonOffset);
			}
			return IPv4Endpoint::parse(CStringView{addressStringBuffer.data()}, string.substr(lastColonOffset + 1), errorCode);
		}
		const Optional<IPv6Address> address = IPv6Address::parse(string, errorCode);
		if (!address) {
			return {};
		}
		return IPv6Endpoint{*address, 0};
	}
	const Optional<IPv4Address> address = IPv4Address::parse(string, errorCode);
	if (!address) {
		return {};
	}
	return IPv4Endpoint{*address, 0};
}

Optional<Endpoint> Endpoint::parse(EndpointFamily family, CStringView addressString, CStringView portNumberString, std::error_code& errorCode) noexcept {
	switch (family) {
		case EndpointFamily::IPv4: return IPv4Endpoint::parse(addressString, portNumberString, errorCode);
		case EndpointFamily::IPv6: return IPv6Endpoint::parse(addressString, portNumberString, errorCode);
	}
	errorCode = make_error_code(std::errc::address_family_not_supported);
	return {};
}

Optional<Endpoint> Endpoint::parse(EndpointFamily family, CStringView string, std::error_code& errorCode) noexcept {
	switch (family) {
		case EndpointFamily::IPv4: return IPv4Endpoint::parse(string, errorCode);
		case EndpointFamily::IPv6: return IPv6Endpoint::parse(string, errorCode);
	}
	errorCode = make_error_code(std::errc::address_family_not_supported);
	return {};
}

Optional<Endpoint> Endpoint::resolve(const char* hostName, const char* service, std::error_code& errorCode) {
	ensurePlatformInitialized(errorCode);
	if (errorCode) {
		return {};
	}

	addrinfo hints{};
	hints.ai_family = AF_UNSPEC;
	addrinfo* info = nullptr;
	const int dnsResult = getaddrinfo(hostName, service, &hints, &info);
	if (dnsResult != 0) {
#ifdef _WIN32
		errorCode = make_error_code(static_cast<EndpointError>(dnsResult));
#else
		if (dnsResult == EAI_SYSTEM && errno != 0) {
			errorCode = std::error_code{errno, std::generic_category()};
		} else {
			errorCode = make_error_code(static_cast<EndpointError>(dnsResult));
		}
#endif
		return {};
	}
	if (!info) {
		errorCode = make_error_code(EndpointError::FAIL);
		return {};
	}
	Optional<Endpoint> result{};
	switch (info->ai_family) {
		case static_cast<int>(static_cast<short>(EndpointFamily::IPv4)):
			if (info->ai_addrlen == static_cast<socklen_t>(sizeof(sockaddr_in))) {
				result.emplace(IPv4Endpoint{*std::launder(reinterpret_cast<const sockaddr_in*>(info->ai_addr))});
			}
			break;
		case static_cast<int>(static_cast<short>(EndpointFamily::IPv6)):
			if (info->ai_addrlen == static_cast<socklen_t>(sizeof(sockaddr_in6))) {
				result.emplace(IPv6Endpoint{*std::launder(reinterpret_cast<const sockaddr_in6*>(info->ai_addr))});
			}
			break;
		default: errorCode = make_error_code(EndpointError::FAMILY); break;
	}
	if (result) {
		errorCode.clear();
	} else if (!errorCode) {
		errorCode = make_error_code(EndpointError::FAIL);
	}
	freeaddrinfo(info);
	return result;
}

Optional<Endpoint> Endpoint::resolve(CStringView host, std::error_code& errorCode) {
	if (const size_t lastColonOffset = host.rfind(':'); lastColonOffset != CStringView::npos) {
		if (lastColonOffset > 0 && host[lastColonOffset - 1] == ']' && host[0] == '[') {
			return Endpoint::resolve(String{host.data() + 1, lastColonOffset - 2}.c_str(), host.substr(lastColonOffset + 1).c_str(), errorCode);
		}
		if (StringView{host.data(), lastColonOffset}.find(':') == StringView::npos) {
			return Endpoint::resolve(String{host.data(), lastColonOffset}.c_str(), host.substr(lastColonOffset + 1).c_str(), errorCode);
		}
	}
	return Endpoint::resolve(host.c_str(), nullptr, errorCode);
}

Optional<Endpoint> Endpoint::resolve(EndpointFamily family, const char* hostName, const char* service, std::error_code& errorCode) {
	ensurePlatformInitialized(errorCode);
	if (errorCode) {
		return {};
	}

	addrinfo hints{};
	hints.ai_family = static_cast<short>(family);
	addrinfo* info = nullptr;
	const int dnsResult = getaddrinfo(hostName, service, &hints, &info);
	if (dnsResult != 0) {
#ifdef _WIN32
		errorCode = make_error_code(static_cast<EndpointError>(dnsResult));
#else
		if (dnsResult == EAI_SYSTEM && errno != 0) {
			errorCode = std::error_code{errno, std::generic_category()};
		} else {
			errorCode = make_error_code(static_cast<EndpointError>(dnsResult));
		}
#endif
		return {};
	}
	if (!info) {
		errorCode = make_error_code(EndpointError::FAIL);
		return {};
	}
	if (info->ai_family != static_cast<int>(static_cast<short>(family))) {
		errorCode = make_error_code(EndpointError::FAMILY);
		freeaddrinfo(info);
		return {};
	}
	Optional<Endpoint> result{};
	switch (family) {
		case EndpointFamily::IPv4:
			if (info->ai_addrlen == static_cast<socklen_t>(sizeof(sockaddr_in))) {
				result.emplace(IPv4Endpoint{*std::launder(reinterpret_cast<const sockaddr_in*>(info->ai_addr))});
			}
			break;
		case EndpointFamily::IPv6:
			if (info->ai_addrlen == static_cast<socklen_t>(sizeof(sockaddr_in6))) {
				result.emplace(IPv6Endpoint{*std::launder(reinterpret_cast<const sockaddr_in6*>(info->ai_addr))});
			}
			break;
	}
	if (result) {
		errorCode.clear();
	} else {
		errorCode = make_error_code(EndpointError::FAIL);
	}
	freeaddrinfo(info);
	return result;
}

Optional<Endpoint> Endpoint::resolve(EndpointFamily family, CStringView host, std::error_code& errorCode) {
	if (const size_t lastColonOffset = host.rfind(':'); lastColonOffset != CStringView::npos) {
		if (lastColonOffset > 0 && host[lastColonOffset - 1] == ']' && host[0] == '[') {
			return Endpoint::resolve(family, String{host.data() + 1, lastColonOffset - 2}.c_str(), host.substr(lastColonOffset + 1).c_str(), errorCode);
		}
		if (StringView{host.data(), lastColonOffset}.find(':') == StringView::npos) {
			return Endpoint::resolve(family, String{host.data(), lastColonOffset}.c_str(), host.substr(lastColonOffset + 1).c_str(), errorCode);
		}
	}
	return Endpoint::resolve(family, host.c_str(), nullptr, errorCode);
}

Optional<Endpoint> Endpoint::getLocalEndpoint(EndpointFamily family, std::error_code& errorCode) {
	Socket socket{family, ProtocolType::UDP, errorCode};
	if (errorCode) {
		return {};
	}
	const Endpoint dummyEndpoint =
		(family == EndpointFamily::IPv6) //
			? Endpoint{IPv6Endpoint{IPv6Address::LOOPBACK, 9}}
			: Endpoint{IPv4Endpoint{IPv4Address::LOOPBACK, 9}};
	socket.connect(dummyEndpoint, errorCode);
	if (errorCode) {
		return {};
	}
	return socket.getLocalEndpoint(errorCode);
}

Endpoint Endpoint::parse(CStringView addressString, CStringView portNumberString) {
	std::error_code errorCode{};
	Optional<Endpoint> result = parse(addressString, portNumberString, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

Endpoint Endpoint::parse(CStringView string) {
	std::error_code errorCode{};
	Optional<Endpoint> result = parse(string, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

Endpoint Endpoint::parse(EndpointFamily family, CStringView addressString, CStringView portNumberString) {
	std::error_code errorCode{};
	Optional<Endpoint> result = parse(family, addressString, portNumberString, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

Endpoint Endpoint::parse(EndpointFamily family, CStringView string) {
	std::error_code errorCode{};
	Optional<Endpoint> result = parse(family, string, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(std::errc::invalid_argument)};
	}
	return *result;
}

Endpoint Endpoint::resolve(const char* hostName, const char* service) {
	std::error_code errorCode{};
	Optional<Endpoint> result = resolve(hostName, service, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

Endpoint Endpoint::resolve(CStringView host) {
	std::error_code errorCode{};
	Optional<Endpoint> result = resolve(host, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

Endpoint Endpoint::resolve(EndpointFamily family, const char* hostName, const char* service) {
	std::error_code errorCode{};
	Optional<Endpoint> result = resolve(family, hostName, service, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

Endpoint Endpoint::resolve(EndpointFamily family, CStringView host) {
	std::error_code errorCode{};
	Optional<Endpoint> result = resolve(family, host, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

Endpoint Endpoint::getLocalEndpoint(EndpointFamily family) {
	std::error_code errorCode{};
	Optional<Endpoint> result = getLocalEndpoint(family, errorCode);
	if (errorCode) {
		throw networking::Error{errorCode};
	}
	if (!result) {
		throw networking::Error{make_error_code(EndpointError::FAIL)};
	}
	return *result;
}

} // namespace grem::networking
