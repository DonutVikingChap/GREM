// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_RESOURCE_FORMATS_HDR_HPP
#define GREM_RESOURCE_FORMATS_HDR_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/data/Allocation.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/Reader.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/Writer.hpp>
#include <GREM/core/extents.hpp>
#include <GREM/core/formatting.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/math.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/resource/Error.hpp>
#include <GREM/resource/Image.hpp>

#include <cstdlib>   // std::strtoull
#include <cstring>   // std::strncmp
#include <stdexcept> // std::length_error

namespace grem::resource {

inline constexpr Array<char, 11> HDR_RADIANCE_IDENTIFIER{'#', '?', 'R', 'A', 'D', 'I', 'A', 'N', 'C', 'E', '\n'};
inline constexpr Array<char, 7> HDR_RGBE_IDENTIFIER{'#', '?', 'R', 'G', 'B', 'E', '\n'};

[[nodiscard]] inline Array<uint8_t, 4> convertLinearColorToRGBE(vec3 linearColor) {
	const float maxComponent = max(max(linearColor.x, linearColor.y), linearColor.z);
	if (maxComponent < 1e-32f) {
		return {};
	}
	int exponent{};
	const float fraction = frexp(maxComponent, &exponent) * 256.0f / maxComponent;
	const u8vec3 rgb{linearColor * fraction};
	const uint8_t e = static_cast<uint8_t>(128 + exponent);
	return {rgb.x, rgb.y, rgb.z, e};
}

[[nodiscard]] inline vec3 convertRGBEToLinearColor(Span<const uint8_t, 4> rgbe) {
	if (rgbe[3] == 0) {
		return {};
	}
	const float scale = ldexp(1.0f, static_cast<int>(rgbe[3]) - (128 + 8));
	return vec3{u8vec3{rgbe[0], rgbe[1], rgbe[2]}} * scale;
}

[[nodiscard]] inline Image loadHDRImage(Reader reader, const ImageOptions& options) {
	GREM_PROFILE_BLOCK("Load HDR image");

	if (options.requiredType && *options.requiredType != ImageType::IMAGE_2D) {
		throw resource::Error{"Unexpected image type."};
	}

	ImageFormat format = ImageFormat::R32G32B32A32_FLOAT;
	size_t componentCount = 4;
	if (options.requiredFormat) {
		switch (*options.requiredFormat) {
			case ImageFormat::R32_FLOAT: componentCount = 1; break;
			case ImageFormat::R32G32_FLOAT: componentCount = 2; break;
			case ImageFormat::R32G32B32_FLOAT: componentCount = 3; break;
			case ImageFormat::R32G32B32A32_FLOAT: componentCount = 4; break;
			default: throw resource::Error{"Incompatible required format specified for HDR image."};
		}
		format = *options.requiredFormat;
	}

	const String identifier = reader.readLine();
	if (identifier != "#?RADIANCE" && identifier != "#?RGBE") {
		throw resource::Error{"Invalid HDR identifier."};
	}

	bool foundExpectedFormatVariable = false;
	while (true) {
		const String headerVariable = reader.readLine();
		if (headerVariable.empty()) {
			break;
		}
		if (headerVariable == "FORMAT=32-bit_rle_rgbe") {
			foundExpectedFormatVariable = true;
		}
	}
	if (!foundExpectedFormatVariable) {
		throw resource::Error{"Unsupported image format."};
	}

	String resolutionString = reader.readLine();
	char* p = resolutionString.data();
	bool flipVertically = false;
	if (std::strncmp(p, "+Y ", 3) == 0) {
		flipVertically = true;
	} else if (std::strncmp(p, "-Y ", 3) != 0) {
		throw resource::Error{"Unsupported image layout."};
	}
	p += 3;
	const unsigned long long heightULL = std::strtoull(p, &p, 10);
	while (*p == ' ') {
		++p;
	}
	if (std::strncmp(p, "+X ", 3) != 0) {
		throw resource::Error{"Unsupported image layout."};
	}
	p += 3;
	const unsigned long long widthULL = std::strtoull(p, &p, 10);
	if (widthULL > static_cast<unsigned long long>(Limits<uint32_t>::MAX) || heightULL > static_cast<unsigned long long>(Limits<uint32_t>::MAX)) {
		throw std::length_error{"Image size overflow."};
	}
	const size_t width = static_cast<size_t>(widthULL);
	const size_t height = static_cast<size_t>(heightULL);
	const Extent2D size2D{.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};
	if (size2D.width > options.maxImageDimensions.width || size2D.height > options.maxImageDimensions.height || options.maxImageDimensions.depth == 0) {
		throw std::length_error{"Maximum image dimensions exceeded."};
	}
	if (Image::getSizeInBytes(format, size2D, 1) > options.maxImageSizeInBytes) {
		throw std::length_error{"Maximum image size exceeded."};
	}
	Image result{ImageType::IMAGE_2D, format, size2D, 1};
	const auto setPixel = [&](size_t x, size_t y, Span<const uint8_t, 4> rgbe) -> void {
		constexpr float32_t ONE = 1.0f;
		const vec3 linearColor = convertRGBEToLinearColor(rgbe);
		byte* const pixelData = result.data() + (y * width + x) * componentCount * sizeof(float32_t);
		switch (componentCount) {
			case 2: memcpy(pixelData + sizeof(float32_t), &ONE, sizeof(float32_t)); [[fallthrough]];
			case 1: {
				const float32_t value = (linearColor.x + linearColor.y + linearColor.z) / 3.0f;
				memcpy(pixelData, &value, sizeof(float32_t));
				break;
			}
			case 4: memcpy(pixelData + sizeof(float32_t) * 3, &ONE, sizeof(float32_t)); [[fallthrough]];
			case 3: {
				const float32_t r = linearColor.x;
				const float32_t g = linearColor.y;
				const float32_t b = linearColor.z;
				memcpy(pixelData + sizeof(float32_t) * 0, &r, sizeof(float32_t));
				memcpy(pixelData + sizeof(float32_t) * 1, &g, sizeof(float32_t));
				memcpy(pixelData + sizeof(float32_t) * 2, &b, sizeof(float32_t));
				break;
			}
			default: unreachable();
		}
	};

	if (width < 8 || width > size_t{Limits<int16_t>::MAX}) {
		const auto readUncompressedScanline = [&](size_t y) -> void {
			for (size_t x = 0; x < width; ++x) {
				Array<uint8_t, 4> rgbe{};
				reader.read(asWritableBytes(Span{rgbe}));
				setPixel(x, y, rgbe);
			}
		};

		if (flipVertically) {
			for (size_t y = height; y-- > 0;) {
				readUncompressedScanline(y);
			}
		} else {
			for (size_t y = 0; y < height; ++y) {
				readUncompressedScanline(y);
			}
		}
	} else {
		Allocation<uint8_t> scanlineRGBE(width * 4);
		const auto readNewRunLengthEncodedScanline = [&](size_t y) -> void {
			const Array<uint8_t, 4> scanlineHeader{reader.readUInt8(), reader.readUInt8(), reader.readUInt8(), reader.readUInt8()};
			if (scanlineHeader[0] != 2 || scanlineHeader[1] != 2 || (scanlineHeader[2] & uint8_t{0b1000'0000}) != 0) {
				setPixel(0, y, scanlineHeader);
				for (size_t x = 1; x < width; ++x) {
					Array<uint8_t, 4> rgbe{};
					reader.read(asWritableBytes(Span{rgbe}));
					setPixel(x, y, rgbe);
				}
			} else {
				if (((static_cast<size_t>(scanlineHeader[2]) << 8) | static_cast<size_t>(scanlineHeader[3])) != width) {
					throw resource::Error{"Invalid scanline length."};
				}

				scanlineRGBE.resize(width * 4);
				for (size_t componentIndex = 0; componentIndex < 4; ++componentIndex) {
					size_t x = 0;
					while (x < width) {
						const uint8_t lengthByte = reader.readUInt8();
						if (lengthByte > uint8_t{0b1000'0000}) {
							const uint8_t value = reader.readUInt8();
							uint8_t runLength = static_cast<uint8_t>(lengthByte & uint8_t{0b0111'1111});
							if (runLength == 0 || runLength > width - x) {
								throw resource::Error{"Invalid run length."};
							}
							while (runLength-- > 0) {
								scanlineRGBE[x * 4 + componentIndex] = value;
								++x;
							}
						} else {
							uint8_t nonRunLength = lengthByte;
							if (nonRunLength == 0 || nonRunLength > width - x) {
								throw resource::Error{"Invalid non-run length."};
							}
							while (nonRunLength-- > 0) {
								scanlineRGBE[x * 4 + componentIndex] = reader.readUInt8();
								++x;
							}
						}
					}
				}

				for (size_t x = 0; x < width; ++x) {
					setPixel(x, y, Span{scanlineRGBE}.subspan(x * 4).first<4>());
				}
			}
		};

		if (flipVertically) {
			for (size_t y = height; y-- > 0;) {
				readNewRunLengthEncodedScanline(y);
			}
		} else {
			for (size_t y = 0; y < height; ++y) {
				readNewRunLengthEncodedScanline(y);
			}
		}
	}
	return result;
}

inline void saveHDRImage(const ImageView& image, const ImageSaveHDROptions& options, Writer writer) {
	GREM_PROFILE_BLOCK("Save HDR image");

	if (options.subresource.layer >= image.getDepth()) {
		throw resource::Error{formatString("Invalid layer index {} (image has {} layers).", options.subresource.layer, image.getDepth())};
	}
	if (options.subresource.mipLevel >= image.getMipLevelCount()) {
		throw resource::Error{formatString("Invalid mip level index {} (image has {} mip levels).", options.subresource.mipLevel, image.getMipLevelCount())};
	}
	const ImageView layer = image.getLayer(options.subresource.layer, options.subresource.mipLevel);
	const size_t width = static_cast<size_t>(layer.getWidth());
	const size_t height = static_cast<size_t>(layer.getHeight());
	if (width == 0 || height == 0) {
		throw resource::Error{"Cannot save an empty image layer."};
	}

	size_t componentCount{};
	switch (image.getFormat()) {
		case ImageFormat::R32_FLOAT: componentCount = 1; break;
		case ImageFormat::R32G32_FLOAT: componentCount = 2; break;
		case ImageFormat::R32G32B32_FLOAT: componentCount = 3; break;
		case ImageFormat::R32G32B32A32_FLOAT: componentCount = 4; break;
		default: throw resource::Error{"Cannot save as HDR since the image is not stored in a raw 32-bit floating-point format."};
	}

	constexpr StringView HEADER_VARIABLES_AND_EMPTY_LINE =
		"FORMAT=32-bit_rle_rgbe\n"
		"EXPOSURE=1.0\n"
		"\n";

	writer.write(asBytes(Span{HDR_RADIANCE_IDENTIFIER}));
	writer.write(asBytes(Span{HEADER_VARIABLES_AND_EMPTY_LINE}));

	// Always use -Y +X layout for maximum compatibility, even if we have to flip the image.
	const String resolutionString = formatString("-Y {} +X {}\n", height, width);
	writer.write(asBytes(Span{resolutionString}));

	if (width < 8 || width > size_t{Limits<int16_t>::MAX}) {
		const auto writeUncompressedScanline = [&](size_t y) -> void {
			for (size_t x = 0; x < width; ++x) {
				const float32_t* const pixel = std::launder(reinterpret_cast<const float32_t*>(layer.data() + (y * width + x) * componentCount * sizeof(float32_t)));
				const vec3 linearColor = (componentCount >= 3) ? vec3{pixel[0], pixel[1], pixel[2]} : vec3{pixel[0]};
				const Array<uint8_t, 4> rgbe = convertLinearColorToRGBE(linearColor);
				writer.write(asBytes(Span{rgbe}));
			}
		};

		for (size_t y = 0; y < height; ++y) {
			writeUncompressedScanline(y);
		}
	} else {
		Allocation<uint8_t> perComponentRowRGBE(width * 4);
		const auto writeNewRunLengthEncodedScanline = [&](size_t y) -> void {
			const Array<uint8_t, 4> scanlineHeader{2, 2, static_cast<uint8_t>((width >> 8) & 0xFF), static_cast<uint8_t>(width & 0xFF)};
			writer.write(asBytes(Span{scanlineHeader}));

			for (size_t x = 0; x < width; ++x) {
				const float32_t* const pixel = std::launder(reinterpret_cast<const float32_t*>(layer.data() + (y * width + x) * componentCount * sizeof(float32_t)));
				const vec3 linearColor = (componentCount >= 3) ? vec3{pixel[0], pixel[1], pixel[2]} : vec3{pixel[0]};
				const Array<uint8_t, 4> rgbe = convertLinearColorToRGBE(linearColor);
				perComponentRowRGBE[width * 0 + x] = rgbe[0];
				perComponentRowRGBE[width * 1 + x] = rgbe[1];
				perComponentRowRGBE[width * 2 + x] = rgbe[2];
				perComponentRowRGBE[width * 3 + x] = rgbe[3];
			}

			for (size_t componentIndex = 0; componentIndex < 4; ++componentIndex) {
				const uint8_t* componentRow = perComponentRowRGBE.data() + width * componentIndex;
				size_t x = 0;
				while (x < width) {
					size_t runBegin = x;
					while (runBegin + 2 < width && (componentRow[runBegin + 1] != componentRow[runBegin] || componentRow[runBegin + 2] != componentRow[runBegin])) {
						++runBegin;
					}
					if (runBegin + 2 >= width) {
						runBegin = width;
					}

					while (x < runBegin) {
						const size_t nonRunLength = min(runBegin - x, size_t{128});
						const uint8_t nonRunLengthByte = static_cast<uint8_t>(nonRunLength);
						writer.write(asBytes(Span{&nonRunLengthByte, 1}));
						writer.write(asBytes(Span{componentRow + x, nonRunLength}));
						x += nonRunLength;
					}

					if (runBegin + 2 < width) {
						size_t runEnd = runBegin;
						while (runEnd < width && componentRow[runEnd] == componentRow[runBegin]) {
							++runEnd;
						}

						while (x < runEnd) {
							const size_t runLength = min(runEnd - x, size_t{127});
							const uint8_t runLengthByte = static_cast<uint8_t>(0b1000'0000 | runLength);
							writer.write(asBytes(Span{&runLengthByte, 1}));
							writer.write(asBytes(Span{componentRow + runBegin, 1}));
							x += runLength;
						}
					}
				}
			}
		};

		for (size_t y = 0; y < height; ++y) {
			writeNewRunLengthEncodedScanline(y);
		}
	}
}

} // namespace grem::resource

#endif
