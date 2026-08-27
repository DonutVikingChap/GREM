#include <GREM/core/data/Span.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>

#include <ostream> // std::ostream, std::streamsize

namespace grem {

namespace detail {

void StreamFormatWriter::append(StringView string) {
	Span<const char> remaining{string};
	while (!remaining.empty()) {
		const size_t size = min(remaining.size(), size_t{Limits<std::streamsize>::MAX});
		output.write(remaining.data(), static_cast<std::streamsize>(size));
		remaining = remaining.subspan(size);
	}
}

} // namespace detail

} // namespace grem
