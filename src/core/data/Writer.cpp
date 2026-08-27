// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#include <GREM/build_config.hpp>

#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/fundamentals.hpp>

#include <cstdio>    // std::FILE, std::fwrite, std::fflush
#include <ostream>   // std::streambuf, std::ostream, std::streamsize
#include <stdexcept> // std::runtime_error

namespace grem {

Writer::Writer(std::FILE* output)
	: Writer(output, [](void* context, Span<const byte> data, bool thenFlush) -> size_t {
		std::FILE* const output = static_cast<std::FILE*>(context);
		if (std::fwrite(data.data(), sizeof(byte), data.size(), output) != data.size()) {
			throw std::runtime_error{"Failed to write to file."};
		}
		if (thenFlush) {
			if (std::fflush(output) != 0) {
				throw std::runtime_error{"Failed to flush to file."};
			}
		}
		return data.size();
	}) {
	GREM_ASSERT(output);
}

Writer::Writer(std::streambuf* output)
	: Writer(output, [](void* context, Span<const byte> data, bool thenFlush) -> size_t {
		std::streambuf& output = *static_cast<std::streambuf*>(context);
		const size_t size = min(data.size(), size_t{Limits<std::streamsize>::MAX});
		const std::streamsize bytesWritten = output.sputn(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(size));
		if (thenFlush) {
			output.pubsync();
		}
		return static_cast<size_t>(max(bytesWritten, std::streamsize{0}));
	}) {
	GREM_ASSERT(output);
}

Writer::Writer(std::ostream& output)
	: Writer(output.rdbuf()) {}

} // namespace grem
