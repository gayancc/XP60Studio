#include "roland/RolandCodec.h"

#include "roland/HexFormat.h"
#include "roland/RolandChecksum.h"

#include <algorithm>

namespace xp60studio::roland {

namespace {

RolandDecodeResult fail(RolandParseError error, std::size_t offset, std::string detail = {})
{
    RolandDecodeResult result;
    result.failure = RolandParseFailure{error, offset, std::move(detail)};
    return result;
}

// F0 41 dev model cmd a a a a sum F7 with a one-byte model: 10 bytes.
constexpr std::size_t kMinimumLength = 10;

} // namespace

bool isSysEx(ByteSpan bytes) noexcept
{
    return !bytes.empty() && bytes[0] == kSysExStart;
}

bool isRolandSysEx(ByteSpan bytes) noexcept
{
    return bytes.size() >= 2 && bytes[0] == kSysExStart && bytes[1] == kRolandManufacturerId;
}

RolandDecodeResult decodeRolandSysEx(ByteSpan bytes, const RolandModelId& knownModelId)
{
    return decodeRolandSysEx(bytes, std::span<const RolandModelId>(&knownModelId, 1));
}

RolandDecodeResult decodeRolandSysEx(ByteSpan bytes, std::span<const RolandModelId> knownModelIds)
{
    if (!isSysEx(bytes)) {
        return fail(RolandParseError::NotSysEx, 0);
    }
    if (bytes.size() < kMinimumLength) {
        return fail(RolandParseError::Truncated, bytes.size(),
                    "only " + std::to_string(bytes.size()) + " bytes; a Roland RQ1/DT1 needs at least "
                        + std::to_string(kMinimumLength));
    }
    if (bytes.back() != kSysExEnd) {
        return fail(RolandParseError::MissingEnd, bytes.size() - 1,
                    "last byte is " + toHex(bytes.back()) + ", expected F7");
    }
    if (bytes[1] != kRolandManufacturerId) {
        return fail(RolandParseError::NotRoland, 1, "manufacturer ID " + toHex(bytes[1]));
    }

    const auto deviceId = RolandDeviceId::fromByte(bytes[2]);
    if (!deviceId) {
        return fail(RolandParseError::InvalidDeviceId, 2, "device ID byte " + toHex(bytes[2]));
    }

    // Locate the model ID: longest match first so a 00 6A model is not
    // mistaken for a hypothetical single-byte 00 model.
    std::size_t modelOffset = 3;
    std::optional<RolandModelId> modelId;
    std::size_t bestLength = 0;
    for (const auto& candidate : knownModelIds) {
        const auto candidateBytes = candidate.bytes();
        if (candidateBytes.size() <= bestLength) {
            continue;
        }
        if (modelOffset + candidateBytes.size() > bytes.size()) {
            continue;
        }
        if (std::equal(candidateBytes.begin(), candidateBytes.end(), bytes.begin() + static_cast<std::ptrdiff_t>(modelOffset))) {
            modelId = candidate;
            bestLength = candidateBytes.size();
        }
    }
    if (!modelId) {
        // Report the bytes that were found so the diagnostics stay useful.
        const std::size_t preview = std::min<std::size_t>(2, bytes.size() - modelOffset);
        return fail(RolandParseError::UnsupportedModel, modelOffset,
                    "model bytes start with " + toHex(bytes.subspan(modelOffset, preview)));
    }

    const std::size_t commandOffset = modelOffset + modelId->size();
    // command + 4 address + checksum + F7 (the body is validated per command)
    if (commandOffset + 1 + RolandAddress::kByteCount + 2 > bytes.size()) {
        return fail(RolandParseError::Truncated, bytes.size(), "no room for command, address and checksum");
    }

    const auto command = commandFromByte(bytes[commandOffset]);
    if (!command) {
        return fail(RolandParseError::UnsupportedCommand, commandOffset,
                    "command byte " + toHex(bytes[commandOffset]));
    }

    const std::size_t addressOffset = commandOffset + 1;
    const ByteSpan addressBytes = bytes.subspan(addressOffset, RolandAddress::kByteCount);
    for (std::size_t i = 0; i < addressBytes.size(); ++i) {
        if (!isDataByte(addressBytes[i])) {
            return fail(RolandParseError::InvalidAddressByte, addressOffset + i,
                        "address byte " + toHex(addressBytes[i]));
        }
    }
    const auto address = RolandAddress::fromBytes(addressBytes);

    const std::size_t bodyOffset = addressOffset + RolandAddress::kByteCount;
    const std::size_t checksumOffset = bytes.size() - 2;
    const ByteSpan body = bytes.subspan(bodyOffset, checksumOffset - bodyOffset);
    const Byte checksum = bytes[checksumOffset];

    RolandDecodeResult result;
    if (*command == RolandCommand::DataRequest1) {
        if (body.size() != RolandSize::kByteCount) {
            return fail(RolandParseError::InvalidSize, bodyOffset,
                        "RQ1 body has " + std::to_string(body.size()) + " bytes, expected 4");
        }
        for (std::size_t i = 0; i < body.size(); ++i) {
            if (!isDataByte(body[i])) {
                return fail(RolandParseError::InvalidSizeByte, bodyOffset + i, "size byte " + toHex(body[i]));
            }
        }
        const auto size = RolandSize::fromBytes(body);
        if (size->isZero()) {
            return fail(RolandParseError::InvalidSize, bodyOffset, "RQ1 size is zero");
        }
        if (!RolandChecksum::verify(addressBytes, body, checksum)) {
            return fail(RolandParseError::InvalidChecksum, checksumOffset,
                        "checksum " + toHex(checksum) + ", expected " + toHex(RolandChecksum::compute(addressBytes, body)));
        }
        result.message = RolandSysExMessage::dataRequest(*deviceId, *modelId, *address, *size);
        return result;
    }

    // DT1
    if (body.empty()) {
        return fail(RolandParseError::EmptyData, bodyOffset);
    }
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (!isDataByte(body[i])) {
            return fail(RolandParseError::InvalidDataByte, bodyOffset + i, "data byte " + toHex(body[i]));
        }
    }
    if (!RolandChecksum::verify(addressBytes, body, checksum)) {
        return fail(RolandParseError::InvalidChecksum, checksumOffset,
                    "checksum " + toHex(checksum) + ", expected " + toHex(RolandChecksum::compute(addressBytes, body)));
    }
    result.message = RolandSysExMessage::dataSet(*deviceId, *modelId, *address, ByteVector(body.begin(), body.end()));
    return result;
}

} // namespace xp60studio::roland
