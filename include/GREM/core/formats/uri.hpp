// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_URI_HPP
#define GREM_CORE_FORMATS_URI_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>

namespace grem::uri {

/**
 * Encode arbitrary data into a percent-encoded string.
 *
 * \param data input data to encode.
 *
 * \return a percent-encoded string containing the encoded representation of the
 *         input.
 *
 * \throws std::length_error if the length of the encoded string exceeds the
 *         maximum supported string size.
 * \throws std::bad_alloc on allocation failure.
 */
[[nodiscard]] GREM_API(core) String percentEncode(StringView data);

/**
 * Decode the original data from a percent-encoded string.
 *
 * \param string percent-encoded string to decode.
 *
 * \return a byte sequence containing the data decoded from the percent-encoded
 *         string.
 *
 * \throws std::length_error if the length of the decoded byte sequence exceeds
 *         the maximum supported string size.
 * \throws std::bad_alloc on allocation failure.
 */
[[nodiscard]] GREM_API(core) String percentDecode(StringView string);

} // namespace grem::uri

#endif
