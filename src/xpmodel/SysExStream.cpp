#include "xpmodel/SysExStream.h"

#include "midi/SysExAssembler.h"
#include "roland/RolandCodec.h"

namespace xp60studio::xpmodel {

SysExStreamResult parseSysExStream(roland::ByteSpan bytes, std::span<const roland::RolandModelId> knownModelIds)
{
    SysExStreamResult result;
    midi::SysExAssembler assembler;

    // The assembler reports message boundaries; offsets are recovered by
    // tracking how many bytes each emitted message consumed. Realtime bytes
    // interleaved inside a SysEx are emitted first, so offsets are best effort
    // for those and exact for everything else.
    std::size_t consumed = 0;
    std::size_t pendingSysExStart = 0;
    bool inSysEx = false;
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const roland::Byte b = bytes[i];
        if (!inSysEx && b == 0xF0) {
            inSysEx = true;
            pendingSysExStart = i;
        }
        assembler.feed(midi::MidiByteSpan(&b, 1), [&](midi::MidiBytes message) {
            SysExStreamItem item;
            item.raw = roland::ByteVector(message.begin(), message.end());
            item.isSysEx = !message.empty() && message.front() == 0xF0;
            if (item.isSysEx) {
                item.offset = pendingSysExStart;
                inSysEx = false;
                const auto decoded = roland::decodeRolandSysEx(item.raw, knownModelIds);
                if (decoded.ok()) {
                    item.roland = decoded.message;
                    if (decoded.message->isDataSet()) {
                        ++result.dataSetCount;
                    } else {
                        ++result.requestCount;
                    }
                } else {
                    item.failure = decoded.failure;
                    if (roland::isRolandSysEx(item.raw)) {
                        ++result.rejectedRolandCount;
                    } else {
                        ++result.nonRolandSysExCount;
                    }
                }
            } else {
                item.offset = i + 1 - message.size();
                ++result.otherMidiCount;
            }
            consumed += message.size();
            result.items.push_back(std::move(item));
        });
        if (inSysEx && b >= 0x80 && b != 0xF0 && b < 0xF8 && b != 0xF7) {
            // A status byte interrupted the SysEx; the assembler counted it.
            inSysEx = false;
        }
    }
    (void)consumed;
    result.strayBytes = assembler.strayByteCount();
    result.abortedSysEx = assembler.abortedSysExCount() + assembler.overflowSysExCount() + (assembler.sysExInProgress() ? 1 : 0);
    return result;
}

MemoryImage imageFromStream(const SysExStreamResult& stream)
{
    MemoryImage image;
    for (const auto& item : stream.items) {
        if (item.roland && item.roland->isDataSet()) {
            image.addDataSet(*item.roland);
        }
    }
    return image;
}

} // namespace xp60studio::xpmodel
