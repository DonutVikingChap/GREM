// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_FORMATS_DEFLATE_HPP
#define GREM_CORE_FORMATS_DEFLATE_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/fundamentals.hpp>

#include <stdexcept> // std::invalid_argument
#include <utility>   // std::move

namespace grem::deflate {

/**
 * Deflate compression options.
 */
struct CompressionOptions {
	/**
	 * Compression level.
	 *
	 * Higher values yield smaller compressed sizes at the cost of potentially
	 * slower compression/decompression speed and higher memory usage during
	 * compression.
	 */
	size_t compressionLevel = 8;
};

/**
 * Compress arbitrary data into a deflate-compressed data set.
 *
 * \param writer writer to write the compressed data to.
 * \param data input data to compress.
 * \param options compression options, see CompressionOptions.
 *
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 * \throws any exception thrown by the writer implementation.
 */
GREM_API(core) void compress(Writer writer, Span<const byte> data, const CompressionOptions& options = {});

/**
 * Compress arbitrary data into a deflate-compressed data set.
 *
 * \param data input data to compress.
 * \param options compression options, see CompressionOptions.
 *
 * \return a buffer containing the deflate-compressed data set.
 *
 * \throws std::length_error if an internal size limit was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
[[nodiscard]] GREM_API(core) Buffer<byte> compress(Span<const byte> data, const CompressionOptions& options = {});

/**
 * Deflate decompression options.
 */
struct DecompressionOptions {
	/**
	 * Estimated size of the decompressed data, in bytes.
	 *
	 * This number of bytes will be reserved in the output buffer initially. The
	 * closer it is to the final size of the decompressed data, the fewer
	 * unnecessary reallocations will need to be performed while decompressing.
	 */
	size_t estimatedDecompressedSizeInBytes = 1024;

	/**
	 * Maximum size of the decompressed data.
	 *
	 * If the output buffer would exceed this size (plus its initial size) while
	 * decompressing, an exception is thrown.
	 */
	size_t maxDecompressedSizeInBytes = (uint64_t{8'589'934'592ull} <= uint64_t{Limits<size_t>::MAX}) ? static_cast<size_t>(uint64_t{8'589'934'592ull}) : size_t{2'147'483'648ull};
};

/**
 * Decompress the original data from a deflate-compressed data set.
 *
 * \param output buffer to write the data decompressed from the
 *        deflate-compressed data set to.
 * \param reader reader over the deflate-compressed data set to decompress.
 *        After successful decompression, the reader's next read position will
 *        be the next full byte after the compressed data.
 * \param options deompression options, see DecompressionOptions.
 *
 * \throws std::invalid_argument if the read data is not a valid
 *         deflate-compressed data set.
 * \throws std::length_error if an internal size limit was exceeded, if the end
 *         of the reader was reached unexpectedly, or if the specified maximum
 *         decompressed size was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
GREM_API(core) void decompress(Buffer<byte>& output, SpanReader reader, const DecompressionOptions& options = {});

/**
 * Decompress the original data from a deflate-compressed data set.
 *
 * \param reader reader over the deflate-compressed data set to decompress.
 *        After successful decompression, the reader's next read position will
 *        be the next full byte after the compressed data.
 * \param options deompression options, see DecompressionOptions.
 *
 * \return a buffer containing the data decompressed from the deflate-compressed
 *         data set.
 *
 * \throws std::invalid_argument if the read data is not a valid
 *         deflate-compressed data set.
 * \throws std::length_error if an internal size limit was exceeded, if the end
 *         of the reader was reached unexpectedly, or if the specified maximum
 *         decompressed size was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
[[nodiscard]] inline Buffer<byte> decompress(SpanReader reader, const DecompressionOptions& options = {}) {
	Buffer<byte> result{};
	decompress(result, reader, options);
	return result;
}

/**
 * Decompress the original data from a deflate-compressed data set.
 *
 * \param output buffer to write the data decompressed from the
 *        deflate-compressed data set to.
 * \param compressedData deflate-compressed data set to decompress.
 * \param options decompression options, see DecompressionOptions.
 *
 * \throws std::invalid_argument if the given data is not a valid
 *         deflate-compressed data set, or contains data after the end of the
 *         compressed data set.
 * \throws std::length_error if an internal size limit was exceeded, if the end
 *         of the compressed data was reached unexpectedly, or if the specified
 *         maximum decompressed size was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
inline void decompress(Buffer<byte>& output, Span<const byte>&& compressedData, // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	const DecompressionOptions& options = {}) {
	SpanReader reader{compressedData};
	decompress(output, reader, options);
	if (!reader.eof()) {
		throw std::invalid_argument{"Deflate decompression terminated earlier than the specified end of compressed data."};
	}
}

/**
 * Decompress the original data from a deflate-compressed data set.
 *
 * \param compressedData deflate-compressed data set to decompress.
 * \param options decompression options, see DecompressionOptions.
 *
 * \return a buffer containing the data decompressed from the deflate-compressed
 *         data set.
 *
 * \throws std::invalid_argument if the given data is not a valid
 *         deflate-compressed data set, or contains data after the end of the
 *         compressed data set.
 * \throws std::length_error if an internal size limit was exceeded, if the end
 *         of the compressed data was reached unexpectedly, or if the specified
 *         maximum decompressed size was exceeded.
 * \throws std::bad_array_new_length if an internal size limit was exceeded.
 * \throws std::bad_alloc on allocation failure.
 */
[[nodiscard]] inline Buffer<byte> decompress(Span<const byte>&& compressedData, // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
	const DecompressionOptions& options = {}) {
	Buffer<byte> result{};
	decompress(result, std::move(compressedData), options); // NOLINT(performance-move-const-arg)
	return result;
}

} // namespace grem::deflate

#endif
