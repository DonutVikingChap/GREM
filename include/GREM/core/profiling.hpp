// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_CORE_PROFILING_HPP
#define GREM_CORE_PROFILING_HPP

#include <GREM/build_config.hpp>

#include <GREM/core/fundamentals.hpp>

#ifdef GREM_USE_PROFILING
#include <GREM/core/Error.hpp>
#include <GREM/core/algorithms.hpp>
#include <GREM/core/assertions.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/ArrayList.hpp>
#include <GREM/core/data/BitBuffer.hpp>
#include <GREM/core/data/Buffer.hpp>
#include <GREM/core/data/CStringView.hpp>
#include <GREM/core/data/HashMap.hpp>
#include <GREM/core/data/InplaceArrayList.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/String.hpp>
#include <GREM/core/data/StringView.hpp>
#include <GREM/core/data/UniquePointer.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/formats/json.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/system/File.hpp>
#include <GREM/core/system/NativeFilesystem.hpp>
#include <GREM/core/system/NativeOutputFile.hpp>
#include <GREM/core/system/synchronization.hpp>
#include <GREM/core/time.hpp>

#include <cstring>         // std::strcpy
#include <source_location> // IWYU pragma: keep // std::source_location
#include <utility>         // std::move

#ifdef GREM_USE_MULTITHREADING
#include <GREM/core/system/Thread.hpp>
#ifdef __linux__
#include <cerrno>    // ERANGE
#include <pthread.h> // pthread_t, pthread_self, pthread_setname_np
#endif
#endif
#endif

namespace grem {

/**
 * Format to save profile data in.
 */
enum class ProfileFormat : uint8_t {
	BINARY = 1 << 0,             ///< Generic binary format.
	JSON = 1 << 1,               ///< Generic JSON format.
	TRACE_EVENT_FORMAT = 1 << 2, ///< Trace Event Format compatible with Perfetto and Chromium's about:tracing.
};

/**
 * Set of formats to save profile data in.
 */
class ProfileFormats {
public:
	/**
	 * Set containing all possible object formats.
	 */
	static const ProfileFormats ALL;

	/**
	 * Construct an empty format set.
	 */
	constexpr ProfileFormats() noexcept = default;

	/**
	 * Construct a format set containing only one specific format.
	 *
	 * \param format format identifier to include.
	 *
	 * \note Format sets can be combined using
	 *       operator|(ProfileFormats, ProfileFormats).
	 */
	constexpr ProfileFormats(ProfileFormat format) noexcept
		: bits(static_cast<uint8_t>(format)) {}

	/**
	 * Compare this format set to another for equality.
	 *
	 * \param other the format set to compare this one to.
	 *
	 * \return true if the format sets are equal, false otherwise.
	 */
	[[nodiscard]] constexpr bool operator==(const ProfileFormats& other) const noexcept = default;

	/**
	 * Check if the format set is empty.
	 *
	 * \return true if the set contains no formats, false otherwise.
	 */
	[[nodiscard]] constexpr bool empty() const noexcept {
		return bits == 0;
	}

	/**
	 * Check if the format set contains the given format.
	 *
	 * \param format format identifier to check for.
	 *
	 * \return true if the set contains the given format, false otherwise.
	 */
	[[nodiscard]] constexpr bool contains(ProfileFormat format) const noexcept {
		return (bits & ProfileFormats{format}.bits) != 0;
	}

	/**
	 * Check if the format set contains at least one of the given formats.
	 *
	 * \param formats format set to check for.
	 *
	 * \return true if the set contains at least one of the given formats, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool containsAnyOf(ProfileFormats formats) const noexcept {
		return (bits & formats.bits) != 0;
	}

	/**
	 * Check if the format set contains all of the given formats.
	 *
	 * \param formats format set to check for.
	 *
	 * \return true if the set contains all of the given formats, false
	 *         otherwise.
	 */
	[[nodiscard]] constexpr bool containsAllOf(ProfileFormats formats) const noexcept {
		return (bits & formats.bits) == formats.bits;
	}

	/**
	 * Get the complement of a format set.
	 *
	 * \param a the set to invert.
	 *
	 * \return a set containing all possible formats except those in the given
	 *         set.
	 */
	[[nodiscard]] friend constexpr ProfileFormats operator~(ProfileFormats a) noexcept {
		return ProfileFormats{static_cast<uint8_t>(~a.bits)};
	}

	/**
	 * Get the intersection of two format sets.
	 *
	 * \param a first format set.
	 * \param b second format set.
	 *
	 * \return a set containing all formats contained in both a and b.
	 */
	[[nodiscard]] friend constexpr ProfileFormats operator&(ProfileFormats a, ProfileFormats b) noexcept {
		return ProfileFormats{static_cast<uint8_t>(a.bits & b.bits)};
	}

	/**
	 * Get the union of two format sets.
	 *
	 * \param a first format set.
	 * \param b second format set.
	 *
	 * \return a set containing all formats contained in a or b or both.
	 */
	[[nodiscard]] friend constexpr ProfileFormats operator|(ProfileFormats a, ProfileFormats b) noexcept {
		return ProfileFormats{static_cast<uint8_t>(a.bits | b.bits)};
	}

	/**
	 * Get the symmetric difference of two format sets.
	 *
	 * \param a first format set.
	 * \param b second format set.
	 *
	 * \return a set containing all formats contained in either a or b, but not
	 *         both.
	 */
	[[nodiscard]] friend constexpr ProfileFormats operator^(ProfileFormats a, ProfileFormats b) noexcept {
		return ProfileFormats{static_cast<uint8_t>(a.bits ^ b.bits)};
	}

	/**
	 * Assign the intersection of two format sets to the first set.
	 *
	 * \param a first format set.
	 * \param b second format set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr ProfileFormats& operator&=(ProfileFormats& a, ProfileFormats b) noexcept {
		return a = a & b;
	}

	/**
	 * Assign the union of two format sets to the first set.
	 *
	 * \param a first format set.
	 * \param b second format set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr ProfileFormats& operator|=(ProfileFormats& a, ProfileFormats b) noexcept {
		return a = a | b;
	}

	/**
	 * Assign the symmetric difference of two format sets to the first set.
	 *
	 * \param a first format set.
	 * \param b second format set.
	 *
	 * \return a reference to the first set, which was assigned to.
	 */
	friend constexpr ProfileFormats& operator^=(ProfileFormats& a, ProfileFormats b) noexcept {
		return a = a ^ b;
	}

private:
	constexpr explicit ProfileFormats(uint8_t bits) noexcept
		: bits(bits) {}

	uint8_t bits = 0;
};

/**
 * Get the complement of a profile format.
 *
 * \param a the format to invert.
 *
 * \return a set containing all possible formats except the given format.
 */
constexpr ProfileFormats operator~(ProfileFormat a) noexcept {
	return ~ProfileFormats{a};
}

/**
 * Get the union of two profile formats.
 *
 * \param a first format.
 * \param b second format.
 *
 * \return a set containing both a and b.
 */
constexpr ProfileFormats operator|(ProfileFormat a, ProfileFormat b) noexcept {
	return ProfileFormats{a} | ProfileFormats{b};
}

inline constexpr ProfileFormats ProfileFormats::ALL = ~ProfileFormats{};

} // namespace grem

#ifdef GREM_USE_PROFILING

namespace grem {

namespace detail {

class StaticOrDynamicString {
public:
	template <size_t N>
	[[nodiscard]] static constexpr StaticOrDynamicString createStatic(const char (&string)[N]) noexcept {
		StaticOrDynamicString result{};
		result.string.emplace<StringView>(string);
		return result;
	}

	[[nodiscard]] static constexpr StaticOrDynamicString createStaticFromStringView(StringView string) noexcept {
		StaticOrDynamicString result{};
		result.string.emplace<StringView>(string);
		return result;
	}

	[[nodiscard]] static constexpr StaticOrDynamicString createDynamic(StringView string) {
		StaticOrDynamicString result{};
		if (string.size() <= SMALL_STRING_LENGTH) {
			result.string.emplace<InplaceArrayList<char, SMALL_STRING_LENGTH>>(string.begin(), string.end());
		} else {
			result.string.emplace<String>(string);
		}
		return result;
	}

	constexpr StaticOrDynamicString() noexcept = default;

	constexpr operator StringView() const noexcept {
		GREM_MATCH(string) {
			GREM_CASE(StringView staticString) {
				return staticString;
			}
			GREM_CASE(const InplaceArrayList<char, SMALL_STRING_LENGTH>& smallString) {
				return StringView{smallString.data(), smallString.size()};
			}
			GREM_CASE(const String& largeString) {
				return largeString;
			}
		}
		return {};
	}

private:
	static constexpr size_t SMALL_STRING_LENGTH = 256;

	Variant<StringView, InplaceArrayList<char, SMALL_STRING_LENGTH>, String> string{};
};

struct ProfilerBlock {
	StaticOrDynamicString string{};
	StaticOrDynamicString shortString{};
	size_t level = 0;
	TimePoint startTime{};
	TimePoint endTime{};
};

class ProfilerThread {
public:
	struct CaptureLock {
		UniqueLock<Mutex> captureLock{};
#ifdef GREM_USE_MULTITHREADING
		ThreadID threadID{};
		StringView threadName{};
		size_t threadIndex = 0;
		ThreadID parentThreadID{};
#endif
		Span<const ProfilerBlock> blocks{};
	};

	[[nodiscard]] static ProfilerThread& getInstance();

	size_t blockLevel = 0;

#ifdef GREM_USE_MULTITHREADING
	void setThreadInfo(String newThreadName, size_t newThreadIndex, ThreadID newParentThreadID) {
#ifdef __linux__
		if (!newThreadName.empty()) {
			if (pthread_setname_np(pthread_self(), newThreadName.c_str()) == ERANGE) {
				pthread_setname_np(pthread_self(), newThreadName.substr(0, 15).c_str());
			}
		}
#endif
		ScopedLock lock{mutex};
		threadID = ThreadID::getCurrent();
		threadName = std::move(newThreadName);
		threadIndex = newThreadIndex;
		parentThreadID = newParentThreadID;
	}
#endif

	void submitBlock(ProfilerBlock block) {
		ScopedLock lock{mutex};
		blocks.push_back(std::move(block));
	}

	void clearCapture() {
		ScopedLock lock{mutex};
		blocks.clear();
	}

	[[nodiscard]] CaptureLock lockCapture() {
		UniqueLock lock{mutex};
#ifdef GREM_USE_MULTITHREADING
		return CaptureLock{
			.captureLock = std::move(lock),
			.threadID = threadID,
			.threadName = threadName,
			.threadIndex = threadIndex,
			.parentThreadID = parentThreadID,
			.blocks = blocks,
		};
#else
		return CaptureLock{
			.captureLock = std::move(lock),
			.blocks = blocks,
		};
#endif
	}

private:
	ArrayList<ProfilerBlock> blocks{};
	Mutex mutex{};
#ifdef GREM_USE_MULTITHREADING
	ThreadID threadID{};
	String threadName{};
	std::size_t threadIndex = 0;
	ThreadID parentThreadID{};
#endif
};

class Profiler {
public:
	[[nodiscard]] static Profiler& getInstance() {
		static Profiler instance{};
		return instance;
	}

	void beginFrame() {
		ScopedLock lock{mutex};
		if (framesToSaveCount == 0) {
#ifdef GREM_USE_MULTITHREADING
			for (const UniquePointer<ProfilerThread>& profilerThread : profilerThreads) {
				GREM_ASSERT(profilerThread);
				profilerThread->clearCapture();
			}
#else
			profilerThread.clearCapture();
#endif
		}

		if (frameInProgress) {
			frameStartTimes.push_back(Clock::now());
		} else {
			frameInProgress = true;
			captureStartTime = UTCTimestamp::now();
			frameStartTimes = {Clock::now()};
			frameEndTimes.clear();
		}
	}

	void endFrame() {
		ScopedLock lock{mutex};
		if (framesToSaveCount > 0) {
			frameEndTimes.push_back(Clock::now());
			if (--framesToSaveCount == 0 && frameInProgress) {
				GREM_ASSERT(!nextFrameOutputFilepathPrefix.empty());

				{
#ifdef GREM_USE_MULTITHREADING
					ArrayList<ProfilerThread::CaptureLock> captures{};
					for (const UniquePointer<ProfilerThread>& profilerThread : profilerThreads) {
						GREM_ASSERT(profilerThread);
						captures.push_back(profilerThread->lockCapture());
					}

					stableSortByAscending<&ProfilerThread::CaptureLock::threadIndex>(captures);

					HashMap<ThreadID, Buffer<size_t>> children{};

					Buffer<size_t> traversalStack{};
					BitBuffer traversed(profilerThreads.size(), false);

					for (size_t i = profilerThreads.size(); i-- > 0;) {
						ProfilerThread::CaptureLock& capture = captures[i];
						capture.threadIndex = 0;
						if (capture.parentThreadID) {
							children[capture.parentThreadID].push_back(i);
						} else {
							traversalStack.push_back(i);
						}
					}

					size_t newThreadIndex = 0;
					while (!traversalStack.empty()) {
						const size_t captureIndex = traversalStack.back();
						traversalStack.pop_back();

						ProfilerThread::CaptureLock& capture = captures[captureIndex];
						capture.threadIndex = newThreadIndex++;

						if (const auto it = children.find(capture.threadID); it != children.end()) {
							for (const size_t childIndex : it->second) {
								if (BitBuffer::reference visited = traversed[childIndex]; !visited) {
									visited = true;
									traversalStack.push_back(childIndex);
								}
							}
						}
					}

					stableSortByAscending<&ProfilerThread::CaptureLock::threadIndex>(captures);
#else
					const Array captures{profilerThread.lockCapture()};
#endif

					NativeFilesystem filesystem{};
					if (nextFrameOutputFormats.contains(ProfileFormat::BINARY)) {
						saveAsBinary(filesystem, captures);
					}
					if (nextFrameOutputFormats.contains(ProfileFormat::JSON)) {
						saveAsJSON(filesystem, captures);
					}
					if (nextFrameOutputFormats.contains(ProfileFormat::TRACE_EVENT_FORMAT)) {
						saveAsTraceEventFormat(filesystem, captures);
					}
				}

#ifdef GREM_USE_MULTITHREADING
				for (const UniquePointer<ProfilerThread>& profilerThread : profilerThreads) {
					GREM_ASSERT(profilerThread);
					profilerThread->clearCapture();
				}
#else
				profilerThread.clearCapture();
#endif

				frameInProgress = false;
			}
		} else {
			frameInProgress = false;
		}
	}

	void saveFrames(size_t frameCount, String outputFilepathPrefix, ProfileFormats outputFormats) {
		GREM_ASSERT(!outputFilepathPrefix.empty());
		ScopedLock lock{mutex};
		nextFrameOutputFilepathPrefix = std::move(outputFilepathPrefix);
		nextFrameOutputFormats = outputFormats;
		framesToSaveCount = frameCount;
	}

private:
	friend ProfilerThread;

	Profiler() = default;

	void saveAsBinary(NativeFilesystem& filesystem, Span<const ProfilerThread::CaptureLock> captures) const {
		OutputFileHandle file = filesystem.openEmptyOutputFile(nextFrameOutputFilepathPrefix + captureStartTime.toISO8601({.timeSeparator = '.'}) + ".bin");

		static constexpr Array<char, 6> FORMAT_IDENTIFIER{'\x67', '\0', '\x69', '\xDD', 'P', 'F'};
		static constexpr Pair<uint8_t> CURRENT_FORMAT_VERSION{uint8_t{0}, uint8_t{0}};

		struct Header {
			Array<char, FORMAT_IDENTIFIER.size()> formatIdentifier;
			uint8_t formatBackwardsCompatibilityBreakingVersion;
			uint8_t formatExtensionVersion;
			uint16_t timestampUTCYear;
			uint8_t timestampUTCMonth;
			uint8_t timestampUTCDay;
			uint8_t timestampUTCHour;
			uint8_t timestampUTCMinute;
			uint8_t timestampUTCSecond;
			uint8_t reserved0 = 0;
			uint64_t reserved1 = 0;
			uint64_t startTimeNanoseconds;
			uint64_t endTimeNanoseconds;
			Array<uint64_t, 2> reserved2{};
			uint32_t threadCount;
			Array<uint32_t, 5> reserved3{};
			uint64_t threadDataOffset;
			Array<uint64_t, 5> reserved4{};
		};

		const Header header{
			.formatIdentifier = FORMAT_IDENTIFIER,
			.formatBackwardsCompatibilityBreakingVersion = convertHostEndianToLittleEndian(CURRENT_FORMAT_VERSION.first),
			.formatExtensionVersion = convertHostEndianToLittleEndian(CURRENT_FORMAT_VERSION.second),
			.timestampUTCYear = convertHostEndianToLittleEndian(static_cast<uint16_t>(captureStartTime.year)),
			.timestampUTCMonth = convertHostEndianToLittleEndian(static_cast<uint8_t>(captureStartTime.month)),
			.timestampUTCDay = convertHostEndianToLittleEndian(static_cast<uint8_t>(captureStartTime.day)),
			.timestampUTCHour = convertHostEndianToLittleEndian(static_cast<uint8_t>(captureStartTime.hour)),
			.timestampUTCMinute = convertHostEndianToLittleEndian(static_cast<uint8_t>(captureStartTime.minute)),
			.timestampUTCSecond = convertHostEndianToLittleEndian(static_cast<uint8_t>(captureStartTime.second)),
			.startTimeNanoseconds = convertHostEndianToLittleEndian(uint64_t{0}),
			.endTimeNanoseconds = convertHostEndianToLittleEndian(static_cast<uint64_t>(duration_cast<Nanoseconds>(frameEndTimes.back() - frameStartTimes.front()).count())),
			.threadCount = convertHostEndianToLittleEndian(static_cast<uint32_t>(captures.size())),
			.threadDataOffset = convertHostEndianToLittleEndian(static_cast<uint64_t>(sizeof(Header))),
		};
		file.write(asBytes(Span{&header, 1}));

		size_t byteOffset = sizeof(Header);
		static_assert(sizeof(Header) % alignof(uint64_t) == 0);

		const Array<byte, alignof(uint64_t) - 1> padding{};

		for (const ProfilerThread::CaptureLock& capture : captures) {
#ifdef GREM_USE_MULTITHREADING
			const StringView threadName = capture.threadName;
			const uint32_t threadIndex = convertHostEndianToLittleEndian(static_cast<uint32_t>(capture.threadIndex + 1));
			const uint32_t blockCount = convertHostEndianToLittleEndian(static_cast<uint32_t>(capture.blocks.size()));
			const uint32_t threadNameLength = convertHostEndianToLittleEndian(static_cast<uint32_t>(capture.threadName.size()));
#else
			const StringView threadName = "Main thread";
			const uint32_t threadIndex = convertHostEndianToLittleEndian(uint32_t{1});
			const uint32_t blockCount = convertHostEndianToLittleEndian(static_cast<uint32_t>(capture.blocks.size()));
			const uint32_t threadNameLength = convertHostEndianToLittleEndian(static_cast<uint32_t>(threadName.size()));
#endif
			file.write(asBytes(Span{&threadIndex, 1}));
			file.write(asBytes(Span{&blockCount, 1}));
			file.write(asBytes(Span{&threadNameLength, 1}));
			file.write(asBytes(Span{threadName}));

			byteOffset += sizeof(uint32_t) * 3 + threadName.size();
			const size_t captureHeaderEnd = byteOffset;
			byteOffset = roundUpToMultiple(byteOffset, alignof(uint64_t));
			const size_t captureHeaderAlignmentBytes = byteOffset - captureHeaderEnd;
			file.write(Span{padding}.first(captureHeaderAlignmentBytes));

			for (const ProfilerBlock& block : capture.blocks) {
				const StringView string{block.string};
				const StringView shortString{block.shortString};
				const uint64_t startTimeNanoseconds =
					convertHostEndianToLittleEndian(static_cast<uint64_t>(duration_cast<Nanoseconds>(block.startTime - frameStartTimes.front()).count()));
				const uint64_t endTimeNanoseconds =
					convertHostEndianToLittleEndian(static_cast<uint64_t>(duration_cast<Nanoseconds>(block.endTime - frameStartTimes.front()).count()));
				const uint32_t level = convertHostEndianToLittleEndian(static_cast<uint32_t>(block.level));
				const uint32_t stringLength = convertHostEndianToLittleEndian(static_cast<uint32_t>(string.size()));
				const uint32_t shortStringLength = convertHostEndianToLittleEndian(static_cast<uint32_t>(shortString.size()));
				file.write(asBytes(Span{&startTimeNanoseconds, 1}));
				file.write(asBytes(Span{&endTimeNanoseconds, 1}));
				file.write(asBytes(Span{&level, 1}));
				file.write(asBytes(Span{&stringLength, 1}));
				file.write(asBytes(Span{&shortStringLength, 1}));
				file.write(asBytes(Span{string}));
				file.write(asBytes(Span{shortString}));

				byteOffset += sizeof(uint64_t) * 2 + sizeof(uint32_t) * 3 + string.size() + shortString.size();
				const size_t blockEnd = byteOffset;
				byteOffset = roundUpToMultiple(byteOffset, alignof(uint64_t));
				const size_t blockAlignmentBytes = byteOffset - blockEnd;
				file.write(Span{padding}.first(blockAlignmentBytes));
			}
		}
	}

	void saveAsJSON(NativeFilesystem& filesystem, Span<const ProfilerThread::CaptureLock> captures) const {
		json::Array jsonThreads{};
		for (const ProfilerThread::CaptureLock& capture : captures) {
			json::Array jsonBlocks{};
			for (const ProfilerBlock& block : capture.blocks) {
				jsonBlocks.emplace_back(json::Object{
					{"string", StringView{block.string}},
					{"shortString", StringView{block.shortString}},
					{"level", block.level},
					{"startTimeNanoseconds", duration_cast<Nanoseconds>(block.startTime - frameStartTimes.front()).count()},
					{"endTimeNanoseconds", duration_cast<Nanoseconds>(block.endTime - frameStartTimes.front()).count()},
				});
			}

			jsonThreads.emplace_back(json::Object{
#ifdef GREM_USE_MULTITHREADING
				{"threadName", capture.threadName},
				{"threadIndex", capture.threadIndex + 1},
#else
				{"threadName", "Main thread"},
				{"threadIndex", 1},
#endif
				{"blocks", std::move(jsonBlocks)},
			});
		}

		OutputFileHandle file = filesystem.openEmptyOutputFile(nextFrameOutputFilepathPrefix + captureStartTime.toISO8601({.timeSeparator = '.'}) + ".json");
		*static_cast<NativeOutputFile*>(file.get())
			<< json::Object{
				   {"timestamp", captureStartTime.toISO8601()},
				   {"startTimeNanoseconds", 0},
				   {"endTimeNanoseconds", duration_cast<Nanoseconds>(frameEndTimes.back() - frameStartTimes.front()).count()},
				   {"threads", std::move(jsonThreads)},
			   }
			<< "\r\n";
	}

	void saveAsTraceEventFormat(Filesystem& filesystem, Span<const ProfilerThread::CaptureLock> captures) const {
		struct Event {
			StringView string;
			StringView shortString;
			char phase;
			size_t threadID;
			TimePoint timestamp;
		};

		json::Array jsonTraceEvents{};

		Buffer<Event> events{};
		size_t threadID = 1;
		for (const ProfilerThread::CaptureLock& capture : captures) {
#ifdef GREM_USE_MULTITHREADING
			if (!capture.threadName.empty()) {
				jsonTraceEvents.emplace_back(json::Object{
					{"name", "thread_name"},
					{"cat", "PERF"},
					{"ph", "M"},
					{"pid", 1},
					{"tid", threadID},
					{"args", json::Object{{"name", capture.threadName}}},
				});
			}
			jsonTraceEvents.emplace_back(json::Object{
				{"name", "thread_sort_index"},
				{"cat", "PERF"},
				{"ph", "M"},
				{"pid", 1},
				{"tid", threadID},
				{"args", json::Object{{"sort_index", threadID}}},
			});
#endif
			for (const ProfilerBlock& block : capture.blocks) {
				events.push_back(Event{.string = block.string, .shortString = block.shortString, .phase = 'B', .threadID = threadID, .timestamp = block.startTime});
				events.push_back(Event{.string = block.string, .shortString = block.shortString, .phase = 'E', .threadID = threadID, .timestamp = block.endTime});
			}
			++threadID;
		}
		for (const TimePoint startTime : frameStartTimes) {
			events.push_back(Event{.string = "Frame Start", .shortString{}, .phase = 'i', .threadID = 1, .timestamp = startTime});
		}
		for (const TimePoint endTime : frameEndTimes) {
			events.push_back(Event{.string = "Frame End", .shortString{}, .phase = 'i', .threadID = 1, .timestamp = endTime});
		}

		stableSortByAscending<&Event::timestamp>(events);

		for (const Event& event : events) {
			json::Object jsonEvent{
				{"name", (event.shortString.empty()) ? event.string : event.shortString},
				{"cat", "PERF"},
				{"ph", String{event.phase}},
				{"ts", duration_cast<DurationBase<double, Ratio<1, 1'000'000>>>(event.timestamp - frameStartTimes.front()).count()},
				{"pid", 1},
				{"tid", event.threadID},
			};
			if (!event.shortString.empty()) {
				jsonEvent["args"] = json::Object{{"fullName", event.string}};
			}
			if (event.phase == 'i') {
				jsonEvent["s"] = "p";
			}
			jsonTraceEvents.emplace_back(std::move(jsonEvent));
		}

		OutputFileHandle file = filesystem.openEmptyOutputFile(nextFrameOutputFilepathPrefix + captureStartTime.toISO8601({.timeSeparator = '.'}) + "_trace_event_format.json");
		*static_cast<NativeOutputFile*>(file.get()) << json::Object{{"traceEvents", std::move(jsonTraceEvents)}} << "\r\n";
	}

#ifdef GREM_USE_MULTITHREADING
	[[nodiscard]] ProfilerThread& addThread() {
		ScopedLock lock{mutex};
		return *profilerThreads.emplace_back(UniquePointer<ProfilerThread>::create());
	}

	ArrayList<UniquePointer<ProfilerThread>> profilerThreads{};
#else
	ProfilerThread profilerThread{};
#endif
	Mutex mutex{};
	bool frameInProgress = false;
	UTCTimestamp captureStartTime{};
	Buffer<TimePoint> frameStartTimes{};
	Buffer<TimePoint> frameEndTimes{};
	String nextFrameOutputFilepathPrefix{};
	ProfileFormats nextFrameOutputFormats{};
	size_t framesToSaveCount = 0;
};

inline ProfilerThread& ProfilerThread::getInstance() {
#ifdef GREM_USE_MULTITHREADING
	thread_local ProfilerThread& instance = Profiler::getInstance().addThread();
	return instance;
#else
	return Profiler::getInstance().profilerThread;
#endif
}

class BlockProfiler {
public:
	explicit BlockProfiler(StaticOrDynamicString string, StaticOrDynamicString shortString)
		: string(std::move(string))
		, shortString(std::move(shortString)) {
		++ProfilerThread::getInstance().blockLevel;
	}

	~BlockProfiler() {
		try {
			stop();
		} catch (...) {
		}
	}

	BlockProfiler(const BlockProfiler&) = delete;
	BlockProfiler(BlockProfiler&&) = delete;
	BlockProfiler& operator=(const BlockProfiler&) = delete;
	BlockProfiler& operator=(BlockProfiler&&) = delete;

	void stop() {
		if (stopped) {
			[[unlikely]] return;
		}
		stopped = true;
		const TimePoint endTime = Clock::now();
		ProfilerThread& profilerThread = ProfilerThread::getInstance();
		const size_t level = --profilerThread.blockLevel;
		profilerThread.submitBlock(ProfilerBlock{
			.string = std::move(string),
			.shortString = std::move(shortString),
			.level = level,
			.startTime = startTime,
			.endTime = endTime,
		});
	}

private:
	StaticOrDynamicString string;
	StaticOrDynamicString shortString;
	TimePoint startTime = Clock::now();
	bool stopped = false;
};

class BlockProfilerStopper {
public:
	explicit BlockProfilerStopper(BlockProfiler& blockProfiler) {
		blockProfiler.stop();
	}
};

[[nodiscard]] constexpr StringView getUnqualifiedFunctionName(StringView fullFunctionName) {
	StringView unqualifiedFunctionName = fullFunctionName;
	if (unqualifiedFunctionName.ends_with(')')) {
		size_t level = 1;
		unqualifiedFunctionName.remove_suffix(1);
		while (!unqualifiedFunctionName.empty()) {
			if (unqualifiedFunctionName.back() == ')') {
				++level;
			} else if (unqualifiedFunctionName.back() == '(') {
				if (--level == 0) {
					unqualifiedFunctionName.remove_suffix(1);
					break;
				}
			}
			unqualifiedFunctionName.remove_suffix(1);
		}
	}
	if (const size_t lastNameSpecifierPosition = unqualifiedFunctionName.rfind("::"); lastNameSpecifierPosition != StringView::npos) {
		unqualifiedFunctionName = unqualifiedFunctionName.substr(lastNameSpecifierPosition + 2);
	}
	if (!unqualifiedFunctionName.empty() &&
		allOf(unqualifiedFunctionName, [](const char ch) -> bool { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_'; })) {
		return unqualifiedFunctionName;
	}
	return fullFunctionName;
}

[[nodiscard]] constexpr StringView getQualifiedShortFunctionName(StringView fullFunctionName, StringView shortFunctionName) {
	if (const size_t functionNameOffset = fullFunctionName.find(shortFunctionName); functionNameOffset != StringView::npos) {
		fullFunctionName = fullFunctionName.substr(0, functionNameOffset + shortFunctionName.size());
		if (functionNameOffset > 2 && fullFunctionName[functionNameOffset - 1] == ':' && fullFunctionName[functionNameOffset - 2] == ':') {
			size_t typeNameOffset = functionNameOffset - 2;
			size_t level = 0;
			while (typeNameOffset > 0) {
				const char previousCharacter = fullFunctionName[typeNameOffset - 1];
				if (level == 0 && contains(" \t(){}[]'`:", previousCharacter)) {
					break;
				}
				if (previousCharacter == '>') {
					++level;
				} else if (previousCharacter == '<') {
					if (level == 0) {
						typeNameOffset = 0;
						break;
					}
					--level;
				}
				--typeNameOffset;
			}
			if (typeNameOffset == functionNameOffset - 2) {
				typeNameOffset = functionNameOffset;
			}
			return fullFunctionName.substr(typeNameOffset);
		}
	}
	return shortFunctionName;
}

} // namespace detail

} // namespace grem

#ifdef GREM_USE_MULTITHREADING
#define GREM_PROFILER_SET_THREAD_INFO(threadName, threadIndex, parentThreadID) (grem::detail::ProfilerThread::getInstance().setThreadInfo(threadName, threadIndex, parentThreadID))
#else
#define GREM_PROFILER_SET_THREAD_INFO(threadName, threadIndex, parentThreadID) ((void)0)
#endif
#define GREM_PRIVATE_PROFILING_CONCAT(a, b)          a##b
#define GREM_PRIVATE_PROFILING_CONCAT_INDIRECT(a, b) GREM_PRIVATE_PROFILING_CONCAT(a, b)
#define GREM_PROFILER_BEGIN_FRAME()                  (grem::detail::Profiler::getInstance().beginFrame())
#define GREM_PROFILER_END_FRAME()                    (grem::detail::Profiler::getInstance().endFrame())
#define GREM_PROFILER_SAVE_NEXT_N_FRAMES(frameCount, outputFilepathPrefix, outputFormats) \
	(grem::detail::Profiler::getInstance().saveFrames(frameCount, outputFilepathPrefix, outputFormats))
#define GREM_PROFILER_SAVE_NEXT_FRAME(outputFilepathPrefix, outputFormats) GREM_PROFILER_SAVE_NEXT_N_FRAMES(1, outputFilepathPrefix, outputFormats)
#define GREM_PROFILE_BLOCK(staticString) \
	grem::detail::BlockProfiler GREM_PRIVATE_PROFILING_CONCAT_INDIRECT(GREM_private_blockProfiler, __LINE__) { \
		grem::detail::StaticOrDynamicString::createStaticFromStringView(std::source_location::current().function_name()), \
			grem::detail::StaticOrDynamicString::createStatic(staticString) \
	}
#define GREM_PROFILE_BLOCK_DYNAMIC(dynamicString) \
	grem::detail::BlockProfiler GREM_PRIVATE_PROFILING_CONCAT_INDIRECT(GREM_private_blockProfiler, __LINE__) { \
		grem::detail::StaticOrDynamicString::createStaticFromStringView(std::source_location::current().function_name()), \
			grem::detail::StaticOrDynamicString::createDynamic(dynamicString) \
	}
#define GREM_PROFILE_FUNCTION() \
	grem::detail::BlockProfiler GREM_PRIVATE_PROFILING_CONCAT_INDIRECT(GREM_private_blockProfiler, __LINE__) { \
		grem::detail::StaticOrDynamicString::createStaticFromStringView(std::source_location::current().function_name()), \
			grem::detail::StaticOrDynamicString::createStaticFromStringView( \
				grem::detail::getQualifiedShortFunctionName(std::source_location::current().function_name(), __func__)) \
	}
#define GREM_PROFILE_CONSTRUCTOR_BEGIN() \
	grem::detail::BlockProfiler GREM_private_constructorProfiler { \
		grem::detail::StaticOrDynamicString::createStaticFromStringView(std::source_location::current().function_name()), \
			grem::detail::StaticOrDynamicString::createStaticFromStringView(grem::detail::getUnqualifiedFunctionName(std::source_location::current().function_name())) \
	}
#define GREM_PROFILE_CONSTRUCTOR_END() \
	grem::detail::BlockProfilerStopper GREM_private_constructorProfilerStopper { \
		GREM_private_constructorProfiler \
	}
#else
#define GREM_PROFILER_SET_THREAD_INFO(threadName, threadIndex, parentThreadID)            ((void)0)
#define GREM_PROFILER_BEGIN_FRAME()                                                       ((void)0)
#define GREM_PROFILER_END_FRAME()                                                         ((void)0)
#define GREM_PROFILER_SAVE_NEXT_N_FRAMES(frameCount, outputFilepathPrefix, outputFormats) ((void)0)
#define GREM_PROFILER_SAVE_NEXT_FRAME(outputFilepathPrefix, outputFormats)                ((void)0)
#define GREM_PROFILE_BLOCK(staticString)                                                  ((void)0)
#define GREM_PROFILE_BLOCK_DYNAMIC(dynamicString)                                         ((void)0)
#define GREM_PROFILE_FUNCTION()                                                           ((void)0)
#define GREM_PROFILE_CONSTRUCTOR_BEGIN() \
	struct GREM_private_constructorProfiler {}
#define GREM_PROFILE_CONSTRUCTOR_END() \
	struct GREM_private_constructorProfilerStopper {}
#endif

#endif
