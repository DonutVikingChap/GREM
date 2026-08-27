// SPDX-FileCopyrightText: 2026 Ivar Härnqvist
// SPDX-License-Identifier: MIT

#ifndef GREM_EXAMPLES_FPS_CONNECTION_HPP
#define GREM_EXAMPLES_FPS_CONNECTION_HPP

#include <GREM/aliases.hpp>
#include <GREM/core/data/Array.hpp>
#include <GREM/core/data/DoubleEndedQueue.hpp>
#include <GREM/core/data/FunctionView.hpp>
#include <GREM/core/data/RingBuffer.hpp>
#include <GREM/core/data/Span.hpp>
#include <GREM/core/data/Variant.hpp>
#include <GREM/core/fundamentals.hpp>
#include <GREM/core/metaprogramming.hpp>
#include <GREM/core/profiling.hpp>
#include <GREM/core/randomness.hpp>
#include <GREM/core/system/Clock.hpp>
#include <GREM/core/time.hpp>

#include "TimeSampleBuffer.hpp"
#include "serialization.hpp"

#include <system_error> // std::error_code, std::errc, std::make_error_code(std::errc)
#include <utility>      // std::move, std::in_place_type_t, std::declval
#include <zstd.h>       // ZSTD_...

inline constexpr size_t MAX_RELIABLE_PACKET_SIZE = 512;
inline constexpr size_t MAX_UNRELIABLE_PACKET_SIZE = 1024;
inline constexpr size_t MAX_PACKET_SIZE = max(MAX_RELIABLE_PACKET_SIZE, MAX_UNRELIABLE_PACKET_SIZE);
using PacketBuffer = Array<byte, MAX_PACKET_SIZE>;

using SequenceNumber = uint64_t;
using ReliableSequenceNumber = uint64_t;
using UnreliableSequenceNumber = uint64_t;
using EarlyPacketBitmask = uint32_t;
inline constexpr size_t MAX_EARLY_PACKET_COUNT = 32;
inline constexpr size_t MAX_UNACKNOWLEDGED_RELIABLE_PACKET_COUNT = 1024;
inline constexpr size_t MAX_PACKETS_PER_SEND = 128;

inline constexpr Duration MAX_RELIABLE_ACKNOWLEDGEMENT_WAIT_TIME = 1_second;

enum class PacketType : uint8_t {
	CONNECTION_REQUEST,
	CONNECTION_CONFIRMATION,
	DISCONNECT_REQUEST,
	DISCONNECT_CONFIRMATION,
	HEARTBEAT,
	RELIABLE_PAYLOAD_PART,
	RELIABLE_PAYLOAD_LAST_UNCOMPRESSED,
	RELIABLE_PAYLOAD_LAST_COMPRESSED,
	UNRELIABLE_PAYLOAD_FULL_UNCOMPRESSED,
	UNRELIABLE_PAYLOAD_FULL_COMPRESSED,
	UNRELIABLE_PAYLOAD_START,
	UNRELIABLE_PAYLOAD_MIDDLE_PART,
	UNRELIABLE_PAYLOAD_END_UNCOMPRESSED,
	UNRELIABLE_PAYLOAD_END_COMPRESSED,
};

struct PacketHeader {
	uint8_t protocolVersion = 1;
	uint8_t typeIndex;
	int16_t extraUnreliablePayloadSizeNegativeIfCompressed = 0;
	EarlyPacketBitmask maskOfReliablePacketsReceivedEarly{};
	ReliableSequenceNumber latestAcknowledgedReliableSequenceNumber = 0;
	UnreliableSequenceNumber lastValidReceivedUnreliableSequenceNumber = 0;
	ReliableSequenceNumber reliableSequenceNumber = 0;
	UnreliableSequenceNumber unreliableSequenceNumber = 0;
};
static_assert(trivially_serializable<PacketHeader> || !HOST_IS_LITTLE_ENDIAN);

inline constexpr size_t MAX_PAYLOAD_SIZE_PER_RELIABLE_PACKET = MAX_RELIABLE_PACKET_SIZE - sizeof(PacketHeader);
inline constexpr size_t MAX_PAYLOAD_SIZE_PER_UNRELIABLE_PACKET = MAX_UNRELIABLE_PACKET_SIZE - sizeof(PacketHeader);
inline constexpr size_t MAX_RELIABLE_PAYLOAD_SIZE = MAX_PAYLOAD_SIZE_PER_RELIABLE_PACKET * MAX_UNACKNOWLEDGED_RELIABLE_PACKET_COUNT;
inline constexpr size_t MAX_UNRELIABLE_PAYLOAD_SIZE = MAX_PAYLOAD_SIZE_PER_UNRELIABLE_PACKET * 1024;
inline constexpr size_t CONNECTION_REQUEST_PAYLOAD_SIZE = 512 - sizeof(PacketHeader);
inline constexpr size_t MAX_PAYLOAD_SIZE_LEFT_UNCOMPRESSED = 64;
inline constexpr size_t MAX_DECOMPRESSED_PAYLOAD_SIZE = max(MAX_RELIABLE_PAYLOAD_SIZE, MAX_UNRELIABLE_PAYLOAD_SIZE) * 4;

class Channel {
public:
	Channel() {
		outgoingPacketBuffer.reserve(MAX_PACKET_SIZE);
	}

	void enableIncomingFakeLag(Duration mean, Duration standardDeviation = {}) {
		incomingFakeLagBuffer.emplace(duration_cast<FloatSeconds>(mean).count(), duration_cast<FloatSeconds>(standardDeviation).count());
	}

	void disableIncomingFakeLag() noexcept {
		incomingFakeLagBuffer.reset();
	}

	void enableOutgoingFakeLag(Duration mean, Duration standardDeviation = {}) {
		outgoingFakeLagBuffer.emplace(duration_cast<FloatSeconds>(mean).count(), duration_cast<FloatSeconds>(standardDeviation).count());
	}

	void disableOutgoingFakeLag() noexcept {
		outgoingFakeLagBuffer.reset();
	}

	void enableIncomingFakeLoss(float lossProbability) {
		incomingFakeLossDistribution.emplace(static_cast<double>(lossProbability));
	}

	void disableIncomingFakeLoss() noexcept {
		incomingFakeLossDistribution.reset();
	}

	void enableOutgoingFakeLoss(float lossProbability) {
		outgoingFakeLossDistribution.emplace(static_cast<double>(lossProbability));
	}

	void disableOutgoingFakeLoss() noexcept {
		outgoingFakeLossDistribution.reset();
	}

	void close(std::error_code reason) {
		*this = Channel{};
		errorCode = reason;
	}

	bool acceptConnection(Span<const byte> firstPacket) {
		switch (state) {
			case State::CLOSED: break;
			case State::REQUESTING_CONNECTION: [[fallthrough]];
			case State::CONFIRMING_CONNECTION: close(make_error_code(std::errc::connection_already_in_progress)); return false;
			case State::CONNECTED: close(make_error_code(std::errc::already_connected)); return false;
			case State::REQUESTING_DISCONNECT: [[fallthrough]];
			case State::CONFIRMING_DISCONNECT: close(make_error_code(std::errc::resource_unavailable_try_again)); return false;
		}

		Span<const byte> payload = firstPacket;
		PacketHeader header;
		if (!deserialize(header, payload)) {
			close(make_error_code(std::errc::protocol_error));
			return false;
		}

		if (header.protocolVersion != PacketHeader{}.protocolVersion || header.typeIndex != static_cast<uint8_t>(PacketType::CONNECTION_REQUEST) ||
			firstPacket.size() != sizeof(PacketHeader) + CONNECTION_REQUEST_PAYLOAD_SIZE || header.reliableSequenceNumber != 1) {
			close(make_error_code(std::errc::connection_refused));
			return false;
		}

		if (!receiveAcknowledgement(header)) {
			return false;
		}

		lastValidReceivedUnreliableSequenceNumber = header.unreliableSequenceNumber;
		latestAcknowledgedIncomingReliableSequenceNumber = header.reliableSequenceNumber;
		enqueueOutgoingReliablePacket(PacketType::CONNECTION_CONFIRMATION, {});

		const TimePoint currentTime = Clock::now();
		lastValidReceiveTime = currentTime;
		nextReliableRetryTime = currentTime;
		lastRoundTripSampleTime = currentTime;
		roundTripTimeSampleBuffer = {};
		roundTripTimeSampleBuffer.update(INITIAL_ASSUMED_ROUND_TRIP_TIME);
		roundTripTimeStatistics = roundTripTimeSampleBuffer.getStatistics();
		state = State::CONFIRMING_CONNECTION;

		updatePacketLoss(currentTime, 0);

		return true;
	}

	bool requestConnection() {
		switch (state) {
			case State::CLOSED: break;
			case State::REQUESTING_CONNECTION: [[fallthrough]];
			case State::CONFIRMING_CONNECTION: close(make_error_code(std::errc::connection_already_in_progress)); return false;
			case State::CONNECTED: close(make_error_code(std::errc::already_connected)); return false;
			case State::REQUESTING_DISCONNECT: [[fallthrough]];
			case State::CONFIRMING_DISCONNECT: close(make_error_code(std::errc::resource_unavailable_try_again)); return false;
		}

		Array<byte, CONNECTION_REQUEST_PAYLOAD_SIZE> payload{};
		enqueueOutgoingReliablePacket(PacketType::CONNECTION_REQUEST, payload);

		const TimePoint currentTime = Clock::now();
		lastValidReceiveTime = currentTime;
		nextReliableRetryTime = currentTime;
		lastRoundTripSampleTime = currentTime;
		roundTripTimeSampleBuffer = {};
		roundTripTimeSampleBuffer.update(INITIAL_ASSUMED_ROUND_TRIP_TIME);
		roundTripTimeStatistics = roundTripTimeSampleBuffer.getStatistics();
		state = State::REQUESTING_CONNECTION;
		return true;
	}

	bool requestDisconnect() {
		switch (state) {
			case State::CLOSED: close(make_error_code(std::errc::not_connected)); return false;
			case State::REQUESTING_CONNECTION: [[fallthrough]];
			case State::CONFIRMING_CONNECTION: [[fallthrough]];
			case State::CONNECTED: break;
			case State::REQUESTING_DISCONNECT: [[fallthrough]];
			case State::CONFIRMING_DISCONNECT: return true;
		}

		enqueueOutgoingReliablePacket(PacketType::DISCONNECT_REQUEST, {});
		lastValidReceiveTime = Clock::now();
		state = State::REQUESTING_DISCONNECT;
		return true;
	}

	bool receive(Span<const byte> packet, FunctionView<bool(Span<const byte> payload)> handlePayload) {
		GREM_ASSERT(packet.size() <= MAX_PACKET_SIZE);

		if (incomingFakeLossDistribution && (*incomingFakeLossDistribution)(numberGenerator)) {
			return true;
		}
		if (incomingFakeLagBuffer) {
			const TimePoint currentTime = Clock::now();
			const float lagSeconds = incomingFakeLagBuffer->lagDistribution(numberGenerator);
			incomingFakeLagBuffer->packets.push_back_and_overwrite([&](FakeLagBuffer::Packet& laggedPacket) -> void {
				laggedPacket.timePoint = currentTime + duration_cast<Duration>(FloatSeconds{lagSeconds});
				laggedPacket.data.assign_range(packet);
			});
			while (!incomingFakeLagBuffer->packets.empty() && currentTime >= incomingFakeLagBuffer->packets.front().timePoint) {
				if (!doReceive(incomingFakeLagBuffer->packets.front().data, handlePayload)) {
					return false;
				}
				incomingFakeLagBuffer->packets.pop_front();
			}
			return true;
		}
		return doReceive(packet, handlePayload);
	}

	void writeReliablePayload(FunctionView<void(Buffer<byte>& output)> writePayload) {
		outgoingReliablePayloadBuffers.push_back_and_overwrite([&](OutgoingReliablePayload& payload) -> void {
			payload.buffer.clear();
			payload.compressed = false;
			writePayload(payload.buffer);
			if (payload.buffer.size() > MAX_PAYLOAD_SIZE_LEFT_UNCOMPRESSED) {
				compressionBuffer.clear();
				if (compressPayload(compressionBuffer, payload.buffer)) {
					payload.buffer.swap(compressionBuffer);
					payload.compressed = true;
				}
			}
		});
	}

	void writeUnreliablePayload(FunctionView<void(Buffer<byte>& output)> writePayload) {
		outgoingUnreliablePayloads.push_back_and_overwrite([&](OutgoingUnreliablePayload& payload) -> void {
			payload.buffer.clear();
			payload.bytesSent = 0;
			payload.compressed = false;
			writePayload(payload.buffer);
			if (payload.buffer.size() > MAX_PAYLOAD_SIZE_LEFT_UNCOMPRESSED) {
				compressionBuffer.clear();
				if (compressPayload(compressionBuffer, payload.buffer)) {
					payload.buffer.swap(compressionBuffer);
					payload.compressed = true;
				}
			}
		});
	}

	bool send(FunctionView<bool(Span<const byte> packet)> sendPacket) {
		if (state == State::CLOSED) {
			return false;
		}

		const TimePoint currentTime = Clock::now();
		if (currentTime - lastValidReceiveTime > DISCONNECT_WAIT_TIME) {
			if (state == State::REQUESTING_DISCONNECT) {
				close(make_error_code(std::errc::timed_out));
				return false;
			}
			if (state == State::CONFIRMING_DISCONNECT) {
				close({});
				return false;
			}
		}
		if (currentTime - lastValidReceiveTime > CONNECTION_TIMEOUT) {
			close(make_error_code(std::errc::timed_out));
			return false;
		}

		for (const OutgoingReliablePayload& payload : outgoingReliablePayloadBuffers) {
			if (payload.buffer.size() > MAX_RELIABLE_PAYLOAD_SIZE) {
				close(make_error_code(std::errc::no_buffer_space));
				return false;
			}
			Span<const byte> remaining = payload.buffer;
			while (!remaining.empty()) {
				if (unacknowledgedOutgoingReliablePackets.size() >= MAX_UNACKNOWLEDGED_RELIABLE_PACKET_COUNT) {
					close(make_error_code(std::errc::no_buffer_space));
					return false;
				}

				const Span<const byte> chunk = remaining.first(min(remaining.size(), MAX_PAYLOAD_SIZE_PER_RELIABLE_PACKET));
				const PacketType type = (chunk.size() < remaining.size()) ? PacketType::RELIABLE_PAYLOAD_PART
				                        : (payload.compressed)            ? PacketType::RELIABLE_PAYLOAD_LAST_COMPRESSED
				                                                          : PacketType::RELIABLE_PAYLOAD_LAST_UNCOMPRESSED;
				enqueueOutgoingReliablePacket(type, chunk);
				remaining = remaining.subspan(chunk.size());
			}
		}
		outgoingReliablePayloadBuffers.clear();

		const bool retrySendReliablePackets = currentTime >= nextReliableRetryTime;
		if (retrySendReliablePackets) {
			const Duration reliableRetryTimeInterval =
				min(ceil<Duration>(FloatSeconds{roundTripTimeStatistics.mean + 2 * roundTripTimeStatistics.standardDeviation}) + RELIABLE_RETRY_TIME_SAFETY_MARGIN,
					MAX_RELIABLE_RETRY_TIME_INTERVAL);
			nextReliableRetryTime = max(nextReliableRetryTime + reliableRetryTimeInterval, currentTime);
			if (unacknowledgedOutgoingReliablePackets.empty()) {
				enqueueOutgoingReliablePacket(PacketType::HEARTBEAT, {});
			}
		}

		size_t packetsSent = 0;
		for (OutgoingReliablePacket& outgoing : unacknowledgedOutgoingReliablePackets) {
			if (packetsSent >= MAX_PACKETS_PER_SEND) {
				break;
			}
			const size_t outgoingPacketSize = outgoing.packet.size();
			if (outgoingPacketSize > 0 && (!outgoing.sent || retrySendReliablePackets)) {
				PacketHeader header;
				Span<const byte> payload = outgoing.packet;
				[[maybe_unused]] const bool success = deserialize(header, payload);
				GREM_ASSERT(success);

				header.extraUnreliablePayloadSizeNegativeIfCompressed = 0;
				if (!outgoingUnreliablePayloads.empty()) {
					const OutgoingUnreliablePayload& extraUnreliablePayload = outgoingUnreliablePayloads.front();
					if (extraUnreliablePayload.bytesSent == 0 && !extraUnreliablePayload.buffer.empty() &&
						extraUnreliablePayload.buffer.size() <= MAX_RELIABLE_PACKET_SIZE - outgoingPacketSize &&
						extraUnreliablePayload.buffer.size() <= ((extraUnreliablePayload.compressed) ? size_t{Limits<int16_t>::MAX} + 1 : size_t{Limits<int16_t>::MAX})) {
						header.extraUnreliablePayloadSizeNegativeIfCompressed =
							(extraUnreliablePayload.compressed)
								? static_cast<int16_t>(-static_cast<int32_t>(extraUnreliablePayload.buffer.size()))
								: static_cast<int16_t>(extraUnreliablePayload.buffer.size());
						outgoing.packet.append_range(extraUnreliablePayload.buffer);
						outgoingUnreliablePayloads.pop_front();
						header.unreliableSequenceNumber = ++lastSentUnreliableSequenceNumber;
						outgoingUnreliableSendTimes.push_back(Clock::now());
					} else {
						header.unreliableSequenceNumber = {};
					}
				} else {
					header.unreliableSequenceNumber = {};
				}
				header.maskOfReliablePacketsReceivedEarly = maskOfIncomingReliablePacketsReceivedEarly;
				header.latestAcknowledgedReliableSequenceNumber = latestAcknowledgedIncomingReliableSequenceNumber;
				header.lastValidReceivedUnreliableSequenceNumber = lastValidReceivedUnreliableSequenceNumber;

				outgoingPacketBuffer.clear();
				serialize(header, outgoingPacketBuffer);
				GREM_ASSERT(outgoingPacketBuffer.size() == sizeof(PacketHeader));
				GREM_ASSERT(outgoing.packet.size() >= sizeof(PacketHeader));
				memcpy(outgoing.packet.data(), &header, sizeof(PacketHeader));

				outgoing.sent = true;
				if (!doSend(sendPacket, outgoing.packet)) {
					return false;
				}
				outgoing.packet.resize(outgoingPacketSize);
				++packetsSent;
			}
		}

		if (!outgoingReliableEnqueueTimes.empty() && currentTime - outgoingReliableEnqueueTimes.front() > MAX_RELIABLE_ACKNOWLEDGEMENT_WAIT_TIME) {
			outgoingUnreliablePayloads.clear();
		}

		while (!outgoingUnreliablePayloads.empty()) {
			if (packetsSent >= MAX_PACKETS_PER_SEND) {
				break;
			}

			OutgoingUnreliablePayload& payload = outgoingUnreliablePayloads.front();
			if (payload.buffer.size() > MAX_UNRELIABLE_PAYLOAD_SIZE) {
				close(make_error_code(std::errc::no_buffer_space));
				return false;
			}

			if (payload.buffer.size() - payload.bytesSent <= MAX_PAYLOAD_SIZE_PER_UNRELIABLE_PACKET) {
				const PacketType type =
					(payload.bytesSent == 0) ? ((payload.compressed) ? PacketType::UNRELIABLE_PAYLOAD_FULL_COMPRESSED : PacketType::UNRELIABLE_PAYLOAD_FULL_UNCOMPRESSED)
											 : ((payload.compressed) ? PacketType::UNRELIABLE_PAYLOAD_END_COMPRESSED : PacketType::UNRELIABLE_PAYLOAD_END_UNCOMPRESSED);
				if (!sendUnreliablePacket(sendPacket, type, Span{payload.buffer}.subspan(payload.bytesSent))) {
					return false;
				}
				++packetsSent;
			} else {
				Span<const byte> remaining = Span{payload.buffer}.subspan(payload.bytesSent);
				const Span<const byte> firstChunk = remaining.first(min(remaining.size(), MAX_PAYLOAD_SIZE_PER_UNRELIABLE_PACKET));
				const PacketType firstType = (payload.bytesSent == 0) ? PacketType::UNRELIABLE_PAYLOAD_START : PacketType::UNRELIABLE_PAYLOAD_MIDDLE_PART;
				if (!sendUnreliablePacket(sendPacket, firstType, firstChunk)) {
					return false;
				}
				++packetsSent;
				payload.bytesSent += firstChunk.size();
				remaining = remaining.subspan(firstChunk.size());
				while (!remaining.empty()) {
					if (packetsSent >= MAX_PACKETS_PER_SEND) {
						break;
					}
					const Span<const byte> chunk = remaining.first(min(remaining.size(), MAX_PAYLOAD_SIZE_PER_UNRELIABLE_PACKET));
					const PacketType type = (chunk.size() < remaining.size()) ? PacketType::UNRELIABLE_PAYLOAD_MIDDLE_PART
					                        : (payload.compressed)            ? PacketType::UNRELIABLE_PAYLOAD_END_COMPRESSED
					                                                          : PacketType::UNRELIABLE_PAYLOAD_END_UNCOMPRESSED;
					if (!sendUnreliablePacket(sendPacket, type, chunk)) {
						return false;
					}
					++packetsSent;
					payload.bytesSent += chunk.size();
					remaining = remaining.subspan(chunk.size());
				}
				if (packetsSent >= MAX_PACKETS_PER_SEND) {
					break;
				}
			}
			outgoingUnreliablePayloads.pop_front();
		}

		while (outgoingUnreliablePayloads.size() > 1) {
			outgoingUnreliablePayloads.pop_back();
		}

		if (outgoingFakeLagBuffer) {
			const TimePoint newCurrentTime = Clock::now();
			while (!outgoingFakeLagBuffer->packets.empty() && newCurrentTime >= outgoingFakeLagBuffer->packets.front().timePoint) {
				if (!outgoingFakeLossDistribution || !(*outgoingFakeLossDistribution)(numberGenerator)) {
					if (!sendPacket(outgoingFakeLagBuffer->packets.front().data)) {
						return false;
					}
				}
				outgoingFakeLagBuffer->packets.pop_front();
			}
		}
		return true;
	}

	[[nodiscard]] bool isClosed() const noexcept {
		return state == State::CLOSED;
	}

	[[nodiscard]] bool isConnecting() const noexcept {
		return state == State::REQUESTING_CONNECTION || state == State::CONFIRMING_CONNECTION;
	}

	[[nodiscard]] bool isConnected() const noexcept {
		return state == State::CONNECTED;
	}

	[[nodiscard]] bool isDisconnecting() const noexcept {
		return state == State::REQUESTING_DISCONNECT || state == State::CONFIRMING_DISCONNECT;
	}

	[[nodiscard]] std::error_code getErrorCode() const noexcept {
		return errorCode;
	}

	[[nodiscard]] TimeSampleBufferStatistics getRecentRoundTripTimeStatistics() const noexcept {
		return roundTripTimeStatistics;
	}

	[[nodiscard]] float getRecentIncomingPacketLossFraction() const noexcept {
		const size_t lostPacketCount = incomingPacketLossDetectionTimes.size();
		const size_t receivedPacketCount = incomingPacketReceiveTimes.size();
		return (receivedPacketCount == 0) ? 0.0f : static_cast<float>(lostPacketCount) / static_cast<float>(receivedPacketCount + lostPacketCount);
	}

private:
#ifndef NDEBUG
	static constexpr Seconds CONNECTION_TIMEOUT{1000000};
#else
	static constexpr Seconds CONNECTION_TIMEOUT{20};
#endif
	static constexpr Duration DISCONNECT_WAIT_TIME = 3_seconds;
	static constexpr Duration INITIAL_ASSUMED_ROUND_TRIP_TIME = 100_milliseconds;
	static constexpr Duration RELIABLE_RETRY_TIME_SAFETY_MARGIN = 5_milliseconds;
	static constexpr Duration MAX_RELIABLE_RETRY_TIME_INTERVAL = 500_milliseconds;
	static constexpr Duration ROUND_TRIP_TIME_SAMPLE_INTERVAL = 250_milliseconds;
	static constexpr size_t ROUND_TRIP_TIME_SAMPLE_COUNT = 8;
	static constexpr Duration PACKET_LOSS_SAMPLE_DURATION = 3_seconds;

	[[nodiscard]] static bool compressPayload(Buffer<byte>& output, Span<const byte> payload) {
		GREM_PROFILE_FUNCTION();
		output.resize(ZSTD_compressBound(payload.size()));
		const size_t compressedBytes = ZSTD_compress(output.data(), output.size(), payload.data(), payload.size(), ZSTD_CLEVEL_DEFAULT);
		if (ZSTD_isError(compressedBytes) || compressedBytes >= payload.size()) {
			return false;
		}
		output.resize(compressedBytes);
		return true;
	}

	[[nodiscard]] static bool decompressPayload(Buffer<byte>& output, Span<const byte> payload) {
		GREM_PROFILE_FUNCTION();
		output.resize(MAX_DECOMPRESSED_PAYLOAD_SIZE);
		const size_t decompressedBytes = ZSTD_decompress(output.data(), MAX_DECOMPRESSED_PAYLOAD_SIZE, payload.data(), payload.size());
		if (ZSTD_isError(decompressedBytes)) {
			return false;
		}
		output.resize(decompressedBytes);
		return true;
	}

	[[nodiscard]] bool receiveAcknowledgement(const PacketHeader& header) {
		const TimePoint currentTime = Clock::now();

		if (header.lastValidReceivedUnreliableSequenceNumber > lastAcknowledgedOutgoingUnreliableSequenceNumber) {
			const size_t newlyAcknowledgedPacketCount = static_cast<size_t>(header.lastValidReceivedUnreliableSequenceNumber - lastAcknowledgedOutgoingUnreliableSequenceNumber);
			if (newlyAcknowledgedPacketCount > outgoingUnreliableSendTimes.size()) {
				close(make_error_code(std::errc::protocol_error));
				return false;
			}
			do {
				updateRoundTripTime(currentTime - outgoingUnreliableSendTimes.front());
				outgoingUnreliableSendTimes.pop_front();
				++lastAcknowledgedOutgoingUnreliableSequenceNumber;
			} while (header.lastValidReceivedUnreliableSequenceNumber > lastAcknowledgedOutgoingUnreliableSequenceNumber);
		}

		if (header.latestAcknowledgedReliableSequenceNumber > latestAcknowledgedOutgoingReliableSequenceNumber) {
			const size_t newlyAcknowledgedPacketCount = static_cast<size_t>(header.latestAcknowledgedReliableSequenceNumber - latestAcknowledgedOutgoingReliableSequenceNumber);
			GREM_ASSERT(outgoingReliableEnqueueTimes.size() == unacknowledgedOutgoingReliablePackets.size());
			if (newlyAcknowledgedPacketCount > unacknowledgedOutgoingReliablePackets.size()) {
				close(make_error_code(std::errc::protocol_error));
				return false;
			}
			do {
				if (!unacknowledgedOutgoingReliablePackets.front().sent) {
					close(make_error_code(std::errc::protocol_error));
					return false;
				}
				updateRoundTripTime(currentTime - outgoingReliableEnqueueTimes.front());
				outgoingReliableEnqueueTimes.pop_front();
				unacknowledgedOutgoingReliablePackets.pop_front();
				++latestAcknowledgedOutgoingReliableSequenceNumber;
				maskOfOutgoingReliablePacketsReceivedEarly >>= 1;
			} while (header.latestAcknowledgedReliableSequenceNumber > latestAcknowledgedOutgoingReliableSequenceNumber);
		} else if (header.latestAcknowledgedReliableSequenceNumber == latestAcknowledgedOutgoingReliableSequenceNumber &&
				   header.maskOfReliablePacketsReceivedEarly != maskOfOutgoingReliablePacketsReceivedEarly) {
			maskOfOutgoingReliablePacketsReceivedEarly = header.maskOfReliablePacketsReceivedEarly;
			EarlyPacketBitmask mask = header.maskOfReliablePacketsReceivedEarly;
			size_t index = 1;
			while (mask != 0) {
				if ((mask & 1) != 0) {
					if (index < unacknowledgedOutgoingReliablePackets.size()) {
						if (!unacknowledgedOutgoingReliablePackets[index].sent) {
							close(make_error_code(std::errc::protocol_error));
							return false;
						}
						unacknowledgedOutgoingReliablePackets[index].packet.clear();
					} else {
						close(make_error_code(std::errc::protocol_error));
						return false;
					}
				}
				mask >>= 1;
				++index;
			}
		}
		return true;
	}

	void updateRoundTripTime(Duration newRoundTripTime) {
		const TimePoint currentTime = Clock::now();
		if (currentTime >= lastRoundTripSampleTime + ROUND_TRIP_TIME_SAMPLE_INTERVAL) {
			lastRoundTripSampleTime = currentTime;

			roundTripTimeSampleBuffer.update(newRoundTripTime);
			roundTripTimeStatistics = roundTripTimeSampleBuffer.getStatistics();
		}
	}

	void updatePacketLoss(TimePoint receiveTime, size_t lostPacketCount) {
		const TimePoint sampleStartTime = receiveTime - PACKET_LOSS_SAMPLE_DURATION;
		while (!incomingPacketLossDetectionTimes.empty() && incomingPacketLossDetectionTimes.front() < sampleStartTime) {
			incomingPacketLossDetectionTimes.pop_front();
		}
		while (!incomingPacketReceiveTimes.empty() && incomingPacketReceiveTimes.front() < sampleStartTime) {
			incomingPacketReceiveTimes.pop_front();
		}
		GREM_ASSERT(incomingPacketReceiveTimes.empty() || incomingPacketReceiveTimes.back() <= receiveTime);
		incomingPacketReceiveTimes.push_back(receiveTime);
		while (lostPacketCount-- > 0) {
			incomingPacketLossDetectionTimes.push_back(receiveTime);
		}
	}

	[[nodiscard]] bool handleReliablePacket(const PacketHeader& header, Span<const byte> payload, FunctionView<bool(Span<const byte> payload)> handlePayload) {
		const bool extraUnreliablePayloadCompressed = header.extraUnreliablePayloadSizeNegativeIfCompressed < 0;
		const size_t extraUnreliablePayloadSize =
			(extraUnreliablePayloadCompressed) ? static_cast<size_t>(-static_cast<int32_t>(header.extraUnreliablePayloadSizeNegativeIfCompressed))
											   : static_cast<size_t>(header.extraUnreliablePayloadSizeNegativeIfCompressed);
		if (extraUnreliablePayloadSize > payload.size()) {
			close(make_error_code(std::errc::protocol_error));
			return false;
		}
		payload = payload.first(payload.size() - extraUnreliablePayloadSize);

		if (payload.size() > MAX_RELIABLE_PAYLOAD_SIZE - incomingReliablePayloadBuffer.size()) {
			close(make_error_code(std::errc::no_buffer_space));
			return false;
		}
		incomingReliablePayloadBuffer.append_range(payload);

		const PacketType type = static_cast<PacketType>(header.typeIndex);
		switch (type) {
			case PacketType::CONNECTION_REQUEST: close(make_error_code(std::errc::protocol_error)); return false;
			case PacketType::CONNECTION_CONFIRMATION:
				if (!isConnecting()) {
					close(make_error_code(std::errc::protocol_error));
					return false;
				}
				if (state == State::REQUESTING_CONNECTION) {
					enqueueOutgoingReliablePacket(PacketType::CONNECTION_CONFIRMATION, {});
				}
				incomingReliablePayloadBuffer.clear();
				lastValidReceiveTime = Clock::now();
				state = State::CONNECTED;
				break;
			case PacketType::DISCONNECT_REQUEST:
				incomingReliablePayloadBuffer.clear();
				if (state != State::CONFIRMING_DISCONNECT) {
					enqueueOutgoingReliablePacket(PacketType::DISCONNECT_CONFIRMATION, {});
					lastValidReceiveTime = Clock::now();
					state = State::CONFIRMING_DISCONNECT;
				}
				break;
			case PacketType::DISCONNECT_CONFIRMATION:
				incomingReliablePayloadBuffer.clear();
				close({});
				return false;
			case PacketType::RELIABLE_PAYLOAD_PART:
				if (state == State::CONNECTED) {
					lastValidReceiveTime = Clock::now();
				}
				break;
			case PacketType::RELIABLE_PAYLOAD_LAST_UNCOMPRESSED: [[fallthrough]];
			case PacketType::RELIABLE_PAYLOAD_LAST_COMPRESSED:
				if (state == State::CONNECTED) {
					if (type == PacketType::RELIABLE_PAYLOAD_LAST_COMPRESSED) {
						if (!decompressPayload(decompressionBuffer, incomingReliablePayloadBuffer)) {
							close(make_error_code(std::errc::bad_message));
							return false;
						}
						incomingReliablePayloadBuffer.swap(decompressionBuffer);
					}
					if (!handlePayload(incomingReliablePayloadBuffer)) {
						close(make_error_code(std::errc::bad_message));
						return false;
					}
					lastValidReceiveTime = Clock::now();
				}
				incomingReliablePayloadBuffer.clear();
				break;
			case PacketType::HEARTBEAT:
				incomingReliablePayloadBuffer.clear();
				if (state == State::CONNECTED) {
					lastValidReceiveTime = Clock::now();
				}
				break;
			case PacketType::UNRELIABLE_PAYLOAD_FULL_UNCOMPRESSED: [[fallthrough]];
			case PacketType::UNRELIABLE_PAYLOAD_FULL_COMPRESSED: [[fallthrough]];
			case PacketType::UNRELIABLE_PAYLOAD_START: [[fallthrough]];
			case PacketType::UNRELIABLE_PAYLOAD_MIDDLE_PART: [[fallthrough]];
			case PacketType::UNRELIABLE_PAYLOAD_END_UNCOMPRESSED: [[fallthrough]];
			case PacketType::UNRELIABLE_PAYLOAD_END_COMPRESSED: unreachable();
		}
		return true;
	}

	[[nodiscard]] bool sendUnreliablePacket(FunctionView<bool(Span<const byte> packet)> sendPacket, PacketType type, Span<const byte> payload) {
		outgoingPacketBuffer.clear();
		serialize(
			PacketHeader{
				.typeIndex = static_cast<uint8_t>(type),
				.maskOfReliablePacketsReceivedEarly = maskOfIncomingReliablePacketsReceivedEarly,
				.latestAcknowledgedReliableSequenceNumber = latestAcknowledgedIncomingReliableSequenceNumber,
				.lastValidReceivedUnreliableSequenceNumber = lastValidReceivedUnreliableSequenceNumber,
				.reliableSequenceNumber{},
				.unreliableSequenceNumber = ++lastSentUnreliableSequenceNumber,
			},
			outgoingPacketBuffer);
		GREM_ASSERT(outgoingPacketBuffer.size() == sizeof(PacketHeader));
		GREM_ASSERT(sizeof(PacketHeader) + payload.size() <= MAX_UNRELIABLE_PACKET_SIZE);
		outgoingPacketBuffer.resize(sizeof(PacketHeader));
		outgoingPacketBuffer.append_range(payload);
		outgoingUnreliableSendTimes.push_back(Clock::now());
		return doSend(sendPacket, outgoingPacketBuffer);
	}

	void enqueueOutgoingReliablePacket(PacketType type, Span<const byte> payload) {
		unacknowledgedOutgoingReliablePackets.push_back_and_overwrite([&](OutgoingReliablePacket& outgoing) -> void {
			outgoing.packet.clear();
			serialize(
				PacketHeader{
					.typeIndex = static_cast<uint8_t>(type),
					.reliableSequenceNumber = ++lastEnqueuedOutgoingReliableSequenceNumber,
					// Note: The omitted header fields will be filled in just before sending the packet.
				},
				outgoing.packet);
			GREM_ASSERT(outgoing.packet.size() == sizeof(PacketHeader));
			GREM_ASSERT(sizeof(PacketHeader) + payload.size() <= MAX_RELIABLE_PACKET_SIZE);
			outgoing.packet.resize(sizeof(PacketHeader));
			outgoing.packet.append_range(payload);
			outgoing.sent = false;
		});
		outgoingReliableEnqueueTimes.push_back(Clock::now());
	}

	[[nodiscard]] bool doSend(FunctionView<bool(Span<const byte> packet)> sendPacket, Span<const byte> packet) {
		if (outgoingFakeLagBuffer) {
			const TimePoint currentTime = Clock::now();
			const float lagSeconds = outgoingFakeLagBuffer->lagDistribution(numberGenerator);
			outgoingFakeLagBuffer->packets.push_back_and_overwrite([&](FakeLagBuffer::Packet& laggedPacket) -> void {
				laggedPacket.timePoint = currentTime + duration_cast<Duration>(FloatSeconds{lagSeconds});
				laggedPacket.data.assign_range(packet);
			});
			return true;
		}
		if (outgoingFakeLossDistribution && (*outgoingFakeLossDistribution)(numberGenerator)) {
			return true;
		}
		return sendPacket(packet);
	}

	[[nodiscard]] bool doReceive(Span<const byte> packet, FunctionView<bool(Span<const byte> payload)> handlePayload) {
		if (state == State::CLOSED) {
			return false;
		}

		Span<const byte> payload = packet;
		PacketHeader header;
		if (!deserialize(header, payload) || header.protocolVersion != PacketHeader{}.protocolVersion) {
			close(make_error_code(std::errc::protocol_error));
			return false;
		}

		if (!receiveAcknowledgement(header)) {
			return false;
		}

		switch (header.typeIndex) {
			case static_cast<uint8_t>(PacketType::CONNECTION_REQUEST): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::CONNECTION_CONFIRMATION): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::DISCONNECT_REQUEST): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::DISCONNECT_CONFIRMATION): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::HEARTBEAT): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::RELIABLE_PAYLOAD_PART): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::RELIABLE_PAYLOAD_LAST_UNCOMPRESSED): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::RELIABLE_PAYLOAD_LAST_COMPRESSED):
				if (packet.size() > MAX_RELIABLE_PACKET_SIZE) {
					close(make_error_code(std::errc::protocol_error));
					return false;
				}

				if (header.unreliableSequenceNumber > lastValidReceivedUnreliableSequenceNumber) {
					const size_t newlyReceivedPacketCount = static_cast<size_t>(header.unreliableSequenceNumber - lastValidReceivedUnreliableSequenceNumber);
					GREM_ASSERT(newlyReceivedPacketCount > 0);
					updatePacketLoss(Clock::now(), newlyReceivedPacketCount - 1);
					lastValidReceivedUnreliableSequenceNumber = header.unreliableSequenceNumber;
					const bool extraUnreliablePayloadCompressed = header.extraUnreliablePayloadSizeNegativeIfCompressed < 0;
					const size_t extraUnreliablePayloadSize =
						(extraUnreliablePayloadCompressed) ? static_cast<size_t>(-static_cast<int32_t>(header.extraUnreliablePayloadSizeNegativeIfCompressed))
														   : static_cast<size_t>(header.extraUnreliablePayloadSizeNegativeIfCompressed);
					if (extraUnreliablePayloadSize > 0) {
						if (extraUnreliablePayloadSize > payload.size()) {
							close(make_error_code(std::errc::protocol_error));
							return false;
						}

						if (state == State::CONNECTED) {
							Span<const byte> extraUnreliablePayload = payload.last(extraUnreliablePayloadSize);
							if (extraUnreliablePayloadCompressed) {
								if (!decompressPayload(decompressionBuffer, extraUnreliablePayload)) {
									close(make_error_code(std::errc::bad_message));
									return false;
								}
								extraUnreliablePayload = decompressionBuffer;
							}
							if (!handlePayload(extraUnreliablePayload)) {
								close(make_error_code(std::errc::bad_message));
								return false;
							}
							lastValidReceiveTime = Clock::now();
						}
					}
				}

				if (header.reliableSequenceNumber == latestAcknowledgedIncomingReliableSequenceNumber + 1) {
					updatePacketLoss(Clock::now(), 0);
					latestAcknowledgedIncomingReliableSequenceNumber = header.reliableSequenceNumber;
					maskOfIncomingReliablePacketsReceivedEarly >>= 1;
					if (!handleReliablePacket(header, payload, handlePayload)) {
						return false;
					}

					while (!earlyReceivedReliablePackets.empty()) {
						const Span<const byte> earlyPacket = earlyReceivedReliablePackets.front();
						if (earlyPacket.empty()) {
							break;
						}

						Span<const byte> earlyPayload = earlyPacket;
						PacketHeader earlyHeader;
						[[maybe_unused]] const bool success = deserialize(earlyHeader, earlyPayload);
						GREM_ASSERT(success);

						if (earlyHeader.reliableSequenceNumber == latestAcknowledgedIncomingReliableSequenceNumber + 1) {
							latestAcknowledgedIncomingReliableSequenceNumber = earlyHeader.reliableSequenceNumber;
							maskOfIncomingReliablePacketsReceivedEarly >>= 1;
							earlyReceivedReliablePackets.pop_front();
							if (!handleReliablePacket(earlyHeader, earlyPayload, handlePayload)) {
								return false;
							}
						} else {
							break;
						}
					}
				} else if (header.reliableSequenceNumber > latestAcknowledgedIncomingReliableSequenceNumber) {
					const size_t sequenceNumberDistance = static_cast<size_t>(header.reliableSequenceNumber - latestAcknowledgedIncomingReliableSequenceNumber);
					GREM_ASSERT(sequenceNumberDistance >= 2);
					const size_t index = sequenceNumberDistance - 2;
					if (index < MAX_EARLY_PACKET_COUNT) {
						if (index >= earlyReceivedReliablePackets.size()) {
							earlyReceivedReliablePackets.resize(index + 1, {});
							updatePacketLoss(Clock::now(), sequenceNumberDistance - 1);
						} else {
							updatePacketLoss(Clock::now(), 0);
						}
						InplaceBuffer<byte, MAX_RELIABLE_PACKET_SIZE>& earlyPacket = earlyReceivedReliablePackets[index];
						if (earlyPacket.empty()) {
							earlyPacket.assign_range(packet);
						}
						maskOfIncomingReliablePacketsReceivedEarly |= EarlyPacketBitmask{1} << index;
					}
				}
				break;
			case static_cast<uint8_t>(PacketType::UNRELIABLE_PAYLOAD_FULL_UNCOMPRESSED): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::UNRELIABLE_PAYLOAD_FULL_COMPRESSED):
				if (state == State::CONNECTED && header.unreliableSequenceNumber > lastValidReceivedUnreliableSequenceNumber) {
					const size_t newlyReceivedPacketCount = static_cast<size_t>(header.unreliableSequenceNumber - lastValidReceivedUnreliableSequenceNumber);
					GREM_ASSERT(newlyReceivedPacketCount > 0);
					updatePacketLoss(Clock::now(), newlyReceivedPacketCount - 1);
					lastValidReceivedUnreliableSequenceNumber = header.unreliableSequenceNumber;
					if (header.typeIndex == static_cast<uint8_t>(PacketType::UNRELIABLE_PAYLOAD_FULL_COMPRESSED)) {
						if (!decompressPayload(decompressionBuffer, payload)) {
							close(make_error_code(std::errc::bad_message));
							return false;
						}
						payload = decompressionBuffer;
					}
					if (!handlePayload(payload)) {
						close(make_error_code(std::errc::bad_message));
						return false;
					}
					lastValidReceiveTime = Clock::now();
				}
				break;
			case static_cast<uint8_t>(PacketType::UNRELIABLE_PAYLOAD_START):
				if (state == State::CONNECTED && header.unreliableSequenceNumber > lastValidReceivedUnreliableSequenceNumber) {
					const size_t newlyReceivedPacketCount = static_cast<size_t>(header.unreliableSequenceNumber - lastValidReceivedUnreliableSequenceNumber);
					GREM_ASSERT(newlyReceivedPacketCount > 0);
					updatePacketLoss(Clock::now(), newlyReceivedPacketCount - 1);
					lastValidReceivedUnreliableSequenceNumber = header.unreliableSequenceNumber;
					if (payload.size() > MAX_UNRELIABLE_PAYLOAD_SIZE) {
						close(make_error_code(std::errc::no_buffer_space));
						return false;
					}
					incomingUnreliablePayloadBuffer.assign_range(payload);
				}
				break;
			case static_cast<uint8_t>(PacketType::UNRELIABLE_PAYLOAD_MIDDLE_PART):
				if (state == State::CONNECTED && header.unreliableSequenceNumber == lastValidReceivedUnreliableSequenceNumber + 1) {
					updatePacketLoss(Clock::now(), 0);
					lastValidReceivedUnreliableSequenceNumber = header.unreliableSequenceNumber;
					if (payload.size() > MAX_UNRELIABLE_PAYLOAD_SIZE - incomingUnreliablePayloadBuffer.size()) {
						close(make_error_code(std::errc::no_buffer_space));
						return false;
					}
					incomingUnreliablePayloadBuffer.append_range(payload);
				}
				break;
			case static_cast<uint8_t>(PacketType::UNRELIABLE_PAYLOAD_END_UNCOMPRESSED): [[fallthrough]];
			case static_cast<uint8_t>(PacketType::UNRELIABLE_PAYLOAD_END_COMPRESSED):
				if (state == State::CONNECTED && header.unreliableSequenceNumber == lastValidReceivedUnreliableSequenceNumber + 1) {
					updatePacketLoss(Clock::now(), 0);
					lastValidReceivedUnreliableSequenceNumber = header.unreliableSequenceNumber;
					if (payload.size() > MAX_UNRELIABLE_PAYLOAD_SIZE - incomingUnreliablePayloadBuffer.size()) {
						close(make_error_code(std::errc::no_buffer_space));
						return false;
					}
					incomingUnreliablePayloadBuffer.append_range(payload);
					if (header.typeIndex == static_cast<uint8_t>(PacketType::UNRELIABLE_PAYLOAD_END_COMPRESSED)) {
						if (!decompressPayload(decompressionBuffer, incomingUnreliablePayloadBuffer)) {
							close(make_error_code(std::errc::bad_message));
							return false;
						}
						incomingUnreliablePayloadBuffer.swap(decompressionBuffer);
					}
					if (!handlePayload(incomingUnreliablePayloadBuffer)) {
						close(make_error_code(std::errc::bad_message));
						return false;
					}
					lastValidReceiveTime = Clock::now();
				}
				break;
			default: close(make_error_code(std::errc::protocol_error)); return false;
		}
		return true;
	}

	enum class State : uint8_t {
		CLOSED,
		REQUESTING_CONNECTION,
		CONFIRMING_CONNECTION,
		CONNECTED,
		REQUESTING_DISCONNECT,
		CONFIRMING_DISCONNECT,
	};

	struct OutgoingReliablePayload {
		Buffer<byte> buffer{};
		bool compressed = false;
	};

	struct OutgoingUnreliablePayload {
		Buffer<byte> buffer{};
		size_t bytesSent = 0;
		bool compressed = false;
	};

	struct OutgoingReliablePacket {
		InplaceBuffer<byte, MAX_RELIABLE_PACKET_SIZE> packet;
		bool sent;
	};

	struct FakeLagBuffer {
		struct Packet {
			TimePoint timePoint;
			Buffer<byte> data;
		};

		rng::NormalDistribution<float> lagDistribution;
		RingBuffer<Packet> packets{};

		FakeLagBuffer(float mean, float standardDeviation)
			: lagDistribution(mean, standardDeviation) {}
	};

	State state = State::CLOSED;
	std::error_code errorCode{};
	ReliableSequenceNumber lastEnqueuedOutgoingReliableSequenceNumber = 0;
	ReliableSequenceNumber latestAcknowledgedIncomingReliableSequenceNumber = 0;
	ReliableSequenceNumber latestAcknowledgedOutgoingReliableSequenceNumber = 0;
	UnreliableSequenceNumber lastSentUnreliableSequenceNumber = 0;
	UnreliableSequenceNumber lastValidReceivedUnreliableSequenceNumber = 0;
	UnreliableSequenceNumber lastAcknowledgedOutgoingUnreliableSequenceNumber = 0;
	EarlyPacketBitmask maskOfIncomingReliablePacketsReceivedEarly = 0;
	EarlyPacketBitmask maskOfOutgoingReliablePacketsReceivedEarly = 0;
	RingBuffer<InplaceBuffer<byte, MAX_RELIABLE_PACKET_SIZE>> earlyReceivedReliablePackets{};
	RingBuffer<OutgoingReliablePacket> unacknowledgedOutgoingReliablePackets{};
	Buffer<byte> incomingReliablePayloadBuffer{};
	Buffer<byte> incomingUnreliablePayloadBuffer{};
	RingBuffer<OutgoingReliablePayload> outgoingReliablePayloadBuffers{};
	RingBuffer<OutgoingUnreliablePayload> outgoingUnreliablePayloads{};
	Buffer<byte> outgoingPacketBuffer{};
	Buffer<byte> compressionBuffer{};
	Buffer<byte> decompressionBuffer{};
	TimePoint lastValidReceiveTime = Clock::now();
	TimePoint nextReliableRetryTime = lastValidReceiveTime;
	DoubleEndedQueue<TimePoint> outgoingReliableEnqueueTimes{};
	DoubleEndedQueue<TimePoint> outgoingUnreliableSendTimes{};
	TimeSampleBuffer<ROUND_TRIP_TIME_SAMPLE_COUNT> roundTripTimeSampleBuffer{};
	TimeSampleBufferStatistics roundTripTimeStatistics{};
	TimePoint lastRoundTripSampleTime = Clock::now();
	DoubleEndedQueue<TimePoint> incomingPacketLossDetectionTimes{};
	DoubleEndedQueue<TimePoint> incomingPacketReceiveTimes{};
	Optional<FakeLagBuffer> incomingFakeLagBuffer{};
	Optional<FakeLagBuffer> outgoingFakeLagBuffer{};
	Optional<rng::BernoulliDistribution> incomingFakeLossDistribution{};
	Optional<rng::BernoulliDistribution> outgoingFakeLossDistribution{};
	rng::DefaultRandomEngine numberGenerator{rng::NonDeterministicRandomEngine{}()};
};

template <typename IncomingMessage, typename OutgoingMessage>
class Connection : private Channel {
public:
	using Channel::acceptConnection;
	using Channel::Channel;
	using Channel::close;
	using Channel::disableIncomingFakeLag;
	using Channel::disableIncomingFakeLoss;
	using Channel::disableOutgoingFakeLag;
	using Channel::disableOutgoingFakeLoss;
	using Channel::enableIncomingFakeLag;
	using Channel::enableIncomingFakeLoss;
	using Channel::enableOutgoingFakeLag;
	using Channel::enableOutgoingFakeLoss;
	using Channel::getErrorCode;
	using Channel::getRecentIncomingPacketLossFraction;
	using Channel::getRecentRoundTripTimeStatistics;
	using Channel::isClosed;
	using Channel::isConnected;
	using Channel::isConnecting;
	using Channel::isDisconnecting;
	using Channel::requestConnection;
	using Channel::requestDisconnect;

	bool receive(Span<const byte> packet, auto handleMessage) {
		return Channel::receive(packet, [&](Span<const byte> payload) -> bool {
			while (!payload.empty()) {
				IncomingTypeIndex typeIndex;
				if (!deserialize(typeIndex, payload)) {
					return false;
				}

				if (!IncomingMessage::visitIndex(typeIndex,
						Overloaded{
							[&]<typename Message>(std::in_place_type_t<Message>) -> bool {
								Message& message = get<Message>(incomingMessages);
								if (!deserialize(message, payload)) {
									return false;
								}
								handleMessage(std::move(message));
								return true;
							},
							[&]() -> bool { return false; },
						})) {
					return false;
				}
			}
			return true;
		});
	}

	template <typename Message>
	void writeReliableMessage(const Message& message) requires(variant_has_alternative_v<Message, OutgoingMessage>) {
		const OutgoingTypeIndex typeIndex = variant_index_v<Message, OutgoingMessage>;
		const size_t offset = outgoingReliableMessageStream.size();
		serialize(typeIndex, outgoingReliableMessageStream);
		serialize(message, outgoingReliableMessageStream);
		if (offset > 0 && outgoingReliableMessageStream.size() > MAX_PAYLOAD_SIZE_PER_RELIABLE_PACKET) {
			writeReliablePayload([&](Buffer<byte>& payload) -> void {
				payload.assign_range(Span{outgoingReliableMessageStream}.subspan(offset));
				outgoingReliableMessageStream.resize(offset);
				payload.swap(outgoingReliableMessageStream);
			});
		}
	}

	void writeReliableMessage(const OutgoingMessage& message) {
		return match(message)([&](const auto& msg) -> void { return writeReliableMessage(msg); });
	}

	template <typename Message>
	void writeUnreliableMessage(const Message& message) requires(variant_has_alternative_v<Message, OutgoingMessage>) {
		const OutgoingTypeIndex typeIndex = variant_index_v<Message, OutgoingMessage>;
		const size_t offset = outgoingUnreliableMessageStream.size();
		serialize(typeIndex, outgoingUnreliableMessageStream);
		serialize(message, outgoingUnreliableMessageStream);
		if (offset > 0 && outgoingUnreliableMessageStream.size() > MAX_PAYLOAD_SIZE_PER_UNRELIABLE_PACKET) {
			writeUnreliablePayload([&](Buffer<byte>& payload) -> void {
				payload.assign_range(Span{outgoingUnreliableMessageStream}.subspan(offset));
				outgoingUnreliableMessageStream.resize(offset);
				payload.swap(outgoingUnreliableMessageStream);
			});
		}
	}

	void writeUnreliableMessage(const OutgoingMessage& message) {
		return match(message)([&](const auto& msg) -> void { return writeUnreliableMessage(msg); });
	}

	bool send(FunctionView<bool(Span<const byte> packet)> sendPacket) {
		if (!outgoingReliableMessageStream.empty()) {
			writeReliablePayload([&](Buffer<byte>& payload) -> void { payload.swap(outgoingReliableMessageStream); });
			GREM_ASSERT(outgoingReliableMessageStream.empty());
		}
		if (!outgoingUnreliableMessageStream.empty()) {
			writeUnreliablePayload([&](Buffer<byte>& payload) -> void { payload.swap(outgoingUnreliableMessageStream); });
			GREM_ASSERT(outgoingUnreliableMessageStream.empty());
		}
		return Channel::send(sendPacket);
	}

private:
	using IncomingTypeIndex = typename IncomingMessage::index_type;
	using OutgoingTypeIndex = typename OutgoingMessage::index_type;

	Buffer<byte> outgoingReliableMessageStream{};
	Buffer<byte> outgoingUnreliableMessageStream{};
	meta::tuple_of_variant_alternatives_t<IncomingMessage> incomingMessages{};
};

#endif
