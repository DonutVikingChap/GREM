// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_BASE64_HPP
#define GREM_CORE_FORMATS_BASE64_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>

namespace grem::base64 {

/**
 * Encode arbitrary data into a Base64 string.
 *
 * \param data input data to encode.
 *
 * \return a Base64 string containing the encoded representation of the input.
 *
 * \throws std::length_error if the length of the encoded string exceeds the
 *         maximum supported string size.
 * \throws std::bad_alloc on allocation failure.
 */
GREM_API(core) String encode(StringView data);

/**
 * Decode the original data from a Base64 string.
 *
 * \param string Base64 string to decode.
 *
 * \return a byte sequence containing the data decoded from the Base64 string.
 *
 * \throws std::invalid_argument if the length of the Base64 string is not
 *         divisible by 4.
 * \throws std::length_error if the length of the decoded byte sequence exceeds
 *         the maximum supported string size.
 * \throws std::bad_alloc on allocation failure.
 */
GREM_API(core) String decode(StringView string);

} // namespace grem::base64

#endif
