#include "midi/LibremidiTransport.h"
#include "protocol/RolandRequestTracker.h"
#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "roland/RolandSysExMessage.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchDiff.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <algorithm>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// Read-only hardware acceptance probe for a physical Roland XP-60.
//
// This tool sends RQ1 (Data Request) messages only. RQ1 never modifies device
// memory, so the probe cannot alter the temporary area or permanent User
// memory. Constructing a DT1 is deliberately not implemented here: writes
// belong to the application's armed write flow, which carries the safety
// snapshot described in docs/HARDWARE_VALIDATION_XP60.md step 8.
//
// It exists so that steps 1-7 of that procedure produce recordable raw hex
// rather than screenshots. See docs/DEVICE_ACCEPTANCE.md areas 1-3.

namespace {

using namespace xp60studio;
using Clock = std::chrono::steady_clock;

struct Reply
{
    midi::MidiBytes bytes;
    std::chrono::milliseconds sinceRequest{0};
};

// Shared with the transport's receive handler, which runs on a backend thread.
struct ReceiveState
{
    std::mutex mutex;
    std::condition_variable arrived;
    std::vector<Reply> replies;
    Clock::time_point requestSentAt = Clock::now();
};

struct Options
{
    std::string inputMatch;
    std::string outputMatch;
    std::string presetId = "temporary-patch-name";
    // Set together by --address/--size, which override the preset.
    std::string addressHex;
    std::string sizeHex;
    int deviceIdDisplay = 17;
    int repeat = 1;
    std::chrono::milliseconds firstTimeout = xp60::transferDefaults().firstResponseTimeout;
    std::chrono::milliseconds quietTimeout = xp60::transferDefaults().betweenChunkTimeout;
    std::string savePath;
    bool decodePatch = false;
    bool verifyParameters = false;
    bool listParameters = false;
    int watchIntervalMs = 0;   // >0 enables watch mode
    int watchSeconds = 0;      // 0 = until interrupted
    bool surveyWaves = false;
};

void usage()
{
    std::cout << "Usage: xp60studio_hardware_probe [options]\n"
              << "  --in <text>         substring of the MIDI IN port name (required)\n"
              << "  --out <text>        substring of the MIDI OUT port name (required)\n"
              << "  --preset <id>       safe read preset id (default temporary-patch-name)\n"
              << "  --address <hex>     RQ1 address, e.g. \"03 00 00 00\" (with --size, overrides --preset)\n"
              << "  --size <hex>        RQ1 size quad, e.g. \"00 00 01 00\"\n"
              << "  --device-id <n>     XP-60 display device ID 17..32 (default 17)\n"
              << "  --repeat <n>        repeat the request n times (default 1)\n"
              << "  --timeout <ms>      wait for the first reply (default 1500)\n"
              << "  --quiet <ms>        wait for further packets after one arrives (default 1000)\n"
              << "  --save <file.syx>   append every received reply to a binary .syx capture\n"
              << "  --patch             fetch the whole temporary Patch and decode it\n"
              << "  --verify            verify every parameter and round-trip the bytes (implies --patch)\n"
              << "  --list-parameters   with --verify, print every parameter, not only the failures\n"
              << "  --watch [ms]        poll the Patch and name every parameter that changes (default 700)\n"
              << "  --survey-waves      tabulate wave references across all 128 User Patches\n"
              << "  --list-presets      print the available safe read presets and exit\n"
              << "\nOnly RQ1 (read) messages are ever transmitted.\n";
}

// Printable-ASCII interpretation of a payload, as the Devices screen renders
// it, so a decoded Patch name can be compared with the XP-60 display.
std::string asAsciiText(roland::ByteSpan data)
{
    std::string text;
    text.reserve(data.size());
    for (const auto byte : data)
        text.push_back(byte >= 0x20 && byte < 0x7F ? static_cast<char>(byte) : '.');
    return text;
}

const midi::MidiEndpointInfo* findPort(const std::vector<midi::MidiEndpointInfo>& ports, std::string_view match)
{
    // First enumerated match wins, so repeated runs choose the same endpoint.
    for (const auto& port : ports) {
        if (port.displayName.find(match) != std::string::npos)
            return &port;
    }
    return nullptr;
}

// Decodes the collected DT1s as a Patch through the application's own codec, so
// the probe reports what XP60Studio would actually show for this instrument
// state. Structure only: whether the values are *right* is settled by comparing
// them against the XP-60's own edit pages (DEVICE_ACCEPTANCE.md area 4).
void reportDecodedPatch(const std::vector<Reply>& replies, const roland::RolandAddress& patchBase)
{
    xpmodel::MemoryImage image;
    for (const auto& reply : replies) {
        const auto decoded =
            roland::decodeRolandSysEx(roland::ByteSpan(reply.bytes.data(), reply.bytes.size()), xp60::modelId());
        if (decoded.ok())
            image.addDataSet(*decoded.message);
    }

    const auto result = xpmodel::Xp60PatchCodec::decode(image, patchBase);
    if (!result.ok()) {
        std::cout << "    patch decode FAILED:\n";
        std::cout << "      " << result.describe() << "\n";
        return;
    }

    const auto& patch = *result.patch;
    std::cout << "    patch decoded: \"" << patch.name().displayText() << "\", " << patch.enabledToneCount()
              << " of 4 Tones enabled\n";
    for (const auto tone : xpmodel::ToneIndex::all()) {
        const auto wave = patch.wave(tone);
        std::cout << "      Tone " << tone.number() << ": " << (patch.toneEnabled(tone) ? "ON " : "off")
                  << "  wave " << wave.groupTypeLabel << " group " << wave.groupId << " #" << wave.numberDisplay
                  << "  cutoff " << patch.display(tone, xpmodel::ToneParameter::CutoffFrequency) << "\n";
    }
    if (!result.issues.empty() || !result.missing.empty())
        std::cout << "      notes: " << result.describe() << "\n";
}

// Full parameter verification against the bytes the instrument sent.
//
// Two independent checks, neither of which needs the front panel:
//   1. every documented parameter decodes inside its documented range;
//   2. re-encoding the decoded Patch reproduces the device's bytes exactly.
// Check 2 is the strong one: it proves no parameter was dropped, truncated,
// mis-ordered or silently normalised anywhere in the decode. What it cannot
// prove is that a parameter *means* what the table says; that still requires
// comparing values with the XP-60's own edit pages.
bool verifyAllParameters(const std::vector<Reply>& replies, const roland::RolandAddress& patchBase, bool listAll)
{
    xpmodel::MemoryImage image;
    for (const auto& reply : replies) {
        const auto decoded =
            roland::decodeRolandSysEx(roland::ByteSpan(reply.bytes.data(), reply.bytes.size()), xp60::modelId());
        if (decoded.ok())
            image.addDataSet(*decoded.message);
    }

    const auto result = xpmodel::Xp60PatchCodec::decode(image, patchBase);
    if (!result.ok()) {
        std::cout << "  parameter verification FAILED: patch did not decode\n    " << result.describe() << "\n";
        return false;
    }
    const auto& patch = *result.patch;

    struct BlockRef
    {
        std::string name;
        const xpmodel::BlockValues* values;
    };
    std::vector<BlockRef> blocks{{"Patch Common", &patch.common()}};
    for (const auto tone : xpmodel::ToneIndex::all())
        blocks.push_back({"Tone " + std::to_string(tone.number()), &patch.tone(tone)});

    std::size_t total = 0;
    std::size_t outOfRange = 0;
    for (const auto& block : blocks) {
        const auto& table = block.values->table();
        const auto parameters = table.parameters();
        std::cout << "  " << block.name << ": " << parameters.size() << " parameters, "
                  << table.describedByteCount() << " of " << table.blockSize() << " bytes described ("
                  << xpmodel::tableCompletenessName(table.completeness()) << ")\n";
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            const auto& descriptor = parameters[i];
            const auto raw = block.values->rawAt(i);
            ++total;
            const bool inRange = descriptor.isRawInRange(raw);
            if (!inRange)
                ++outOfRange;
            if (listAll || !inRange) {
                std::cout << "    " << (inRange ? "  " : "!!") << " " << descriptor.id << "  raw=" << raw;
                const auto label = block.values->label(descriptor.id);
                if (label)
                    std::cout << "  \"" << *label << "\"";
                else if (const auto shown = block.values->display(descriptor.id))
                    std::cout << "  display=" << *shown;
                if (!inRange)
                    std::cout << "   OUTSIDE documented range " << descriptor.rawMin << ".." << descriptor.rawMax;
                std::cout << "\n";
            }
        }
    }

    // Byte-exact round trip against what the device actually sent.
    const auto reencoded = xpmodel::Xp60PatchCodec::blockBytes(patch);
    const std::array<std::uint32_t, 5> offsets{0, xpmodel::Xp60PatchLayout::toneOffset(xpmodel::ToneIndex::tone1()),
        xpmodel::Xp60PatchLayout::toneOffset(xpmodel::ToneIndex::tone2()),
        xpmodel::Xp60PatchLayout::toneOffset(xpmodel::ToneIndex::tone3()),
        xpmodel::Xp60PatchLayout::toneOffset(xpmodel::ToneIndex::tone4())};

    std::size_t mismatches = 0;
    for (std::size_t block = 0; block < reencoded.size(); ++block) {
        const auto blockAddress = patchBase.plus(offsets[block]);
        if (!blockAddress)
            continue;
        const auto original = image.read(*blockAddress, static_cast<std::uint32_t>(reencoded[block].size()));
        if (!original) {
            std::cout << "  round trip: " << blocks[block].name << " not fully present in the capture\n";
            ++mismatches;
            continue;
        }
        for (std::size_t i = 0; i < original->size(); ++i) {
            if ((*original)[i] != reencoded[block][i]) {
                ++mismatches;
                std::cout << "  round trip MISMATCH in " << blocks[block].name << " at byte " << i << ": device sent "
                          << roland::toHex((*original)[i]) << ", re-encode produced "
                          << roland::toHex(reencoded[block][i]) << "\n";
            }
        }
    }

    std::cout << "\n  parameters decoded: " << total << "\n";
    std::cout << "  outside documented range: " << outOfRange << "\n";
    std::cout << "  re-encode reproduces the device's bytes: " << (mismatches == 0 ? "YES, exactly" : "NO") << "\n";
    return mismatches == 0 && outOfRange == 0;
}

// Decodes the collected replies into a Patch, or nullopt when the capture is
// incomplete. Shared by the reporting and watch paths.
std::optional<xpmodel::Xp60Patch> decodePatchFrom(const std::vector<Reply>& replies,
    const roland::RolandAddress& patchBase)
{
    xpmodel::MemoryImage image;
    for (const auto& reply : replies) {
        const auto decoded =
            roland::decodeRolandSysEx(roland::ByteSpan(reply.bytes.data(), reply.bytes.size()), xp60::modelId());
        if (decoded.ok())
            image.addDataSet(*decoded.message);
    }
    auto result = xpmodel::Xp60PatchCodec::decode(image, patchBase);
    if (!result.ok())
        return std::nullopt;
    return std::move(result.patch);
}

const xp60::SafeReadPreset* findPreset(std::string_view id)
{
    for (const auto& candidate : xp60::safeReadPresets()) {
        if (candidate.id == id)
            return &candidate;
    }
    return nullptr;
}

int runProbe(const Options& options)
{
    // An explicit --address/--size pair addresses blocks that no preset covers,
    // which the acceptance session needs for the Patch Tone blocks. It is still
    // read-only: the result is an RQ1 either way.
    roland::RolandAddress address;
    roland::RolandSize size;
    std::string label;
    if (options.decodePatch) {
        // A whole Patch: Common plus the four padded Tone blocks. The base
        // defaults to the temporary area; an explicit --address reads a Patch
        // elsewhere in memory, such as a permanent User Patch. Read-only either
        // way, so the size is always the documented span.
        const auto base = options.addressHex.empty() ? std::optional(roland::RolandAddress(0x03, 0x00, 0x00, 0x00))
                                                     : roland::RolandAddress::parseHex(options.addressHex);
        if (!base) {
            std::cerr << "Cannot parse address '" << options.addressHex << "' as four 7-bit bytes.\n";
            return 2;
        }
        address = *base;
        size = roland::RolandSize::fromValue(xpmodel::Xp60PatchLayout::patchSpan()).value();
        label = "whole Patch at " + address.toHexString() + " (span "
            + std::to_string(xpmodel::Xp60PatchLayout::patchSpan()) + ")";
    } else if (!options.addressHex.empty() || !options.sizeHex.empty()) {
        if (options.addressHex.empty() || options.sizeHex.empty()) {
            std::cerr << "--address and --size must be given together.\n";
            return 2;
        }
        const auto parsedAddress = roland::RolandAddress::parseHex(options.addressHex);
        const auto parsedSize = roland::RolandSize::parseHex(options.sizeHex);
        if (!parsedAddress) {
            std::cerr << "Cannot parse address '" << options.addressHex << "' as four 7-bit bytes.\n";
            return 2;
        }
        if (!parsedSize) {
            std::cerr << "Cannot parse size '" << options.sizeHex << "' as four 7-bit bytes.\n";
            return 2;
        }
        address = *parsedAddress;
        size = *parsedSize;
        label = "address " + address.toHexString() + ", size " + size.toHexString();
    } else {
        const auto* preset = findPreset(options.presetId);
        if (preset == nullptr) {
            std::cerr << "Unknown preset '" << options.presetId << "'. Use --list-presets.\n";
            return 2;
        }
        address = preset->address;
        size = preset->size;
        label = std::string(preset->name);
    }

    const auto deviceId = roland::RolandDeviceId::fromDisplayNumber(options.deviceIdDisplay);
    if (!deviceId) {
        std::cerr << "Device ID " << options.deviceIdDisplay << " is outside the XP-60 range 17..32.\n";
        return 2;
    }

    midi::LibremidiTransport transport;
    std::cout << "Backend: " << transport.backendName() << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(2)); // WinRT discovery is asynchronous.

    const auto inputs = transport.enumerateInputs();
    const auto outputs = transport.enumerateOutputs();
    const auto* input = findPort(inputs, options.inputMatch);
    const auto* output = findPort(outputs, options.outputMatch);
    if (input == nullptr) {
        std::cerr << "No MIDI IN port matching '" << options.inputMatch << "'.\n";
        return 3;
    }
    if (output == nullptr) {
        std::cerr << "No MIDI OUT port matching '" << options.outputMatch << "'.\n";
        return 3;
    }

    // Held by shared_ptr and captured by value, not by reference. The transport
    // invokes the receive handler on a backend-owned thread and releases its own
    // lock before calling, so a copy of the handler can still run while this
    // function is unwinding. Sharing ownership keeps the state alive for that
    // call instead of depending on declaration or destruction order.
    const auto state = std::make_shared<ReceiveState>();

    transport.setErrorHandler([](const midi::TransportError& error) {
        std::cerr << "  transport error: " << error.message << "\n";
    });
    transport.setReceiveHandler([state](const midi::MidiEvent& event) {
        if (!event.isCompleteSysEx())
            return; // Channel traffic and partial buffers are not this probe's concern.
        const auto now = Clock::now();
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->replies.push_back(
                Reply{event.bytes, std::chrono::duration_cast<std::chrono::milliseconds>(now - state->requestSentAt)});
        }
        state->arrived.notify_all();
    });

    if (const auto error = transport.openInput(input->id); error.failed()) {
        std::cerr << "Opening MIDI IN failed: " << error.message << "\n";
        return 4;
    }
    if (const auto error = transport.openOutput(output->id); error.failed()) {
        std::cerr << "Opening MIDI OUT failed: " << error.message << "\n";
        return 4;
    }
    std::cout << "MIDI IN : " << input->displayName << " [" << input->backendName << "]\n";
    std::cout << "MIDI OUT: " << output->displayName << " [" << output->backendName << "]\n";
    std::cout << "Request : " << label << "\n";
    std::cout << "Device  : ID " << deviceId->displayNumber() << " (byte " << roland::toHex(deviceId->byte())
              << "), model " << xp60::modelId().toHexString() << "\n\n";

    std::ofstream capture;
    if (!options.savePath.empty()) {
        capture.open(options.savePath, std::ios::binary | std::ios::app);
        if (!capture) {
            std::cerr << "Cannot open capture file '" << options.savePath << "'.\n";
            return 5;
        }
    }

    const auto request =
        roland::RolandSysExMessage::dataRequest(*deviceId, xp60::modelId(), address, size);
    const auto requestBytes = request.encode();

    // Sends the RQ1 once and collects replies until the device goes quiet.
    const auto exchangeOnce = [&]() -> std::vector<Reply> {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->replies.clear();
            state->requestSentAt = Clock::now();
        }
        if (transport.sendSysEx(midi::MidiByteSpan(requestBytes.data(), requestBytes.size())).failed())
            return {};
        std::unique_lock<std::mutex> lock(state->mutex);
        if (state->arrived.wait_for(lock, options.firstTimeout, [&] { return !state->replies.empty(); })) {
            while (true) {
                const auto seen = state->replies.size();
                if (!state->arrived.wait_for(lock, options.quietTimeout, [&] { return state->replies.size() > seen; }))
                    break;
            }
        }
        return state->replies;
    };

    // Wave reference survey across the permanent User Patch bank.
    //
    // Evidence for DEVICE_ACCEPTANCE.md area 9 that needs no front panel. Every
    // Tone carries a (group type, group ID, number) triple, so the 128 User
    // Patches hold 512 wave references the instrument itself wrote. If the
    // catalog's bank structure is right, group IDs and number ranges must fall
    // inside it; a reference outside it disproves the mapping outright.
    //
    // What this cannot do is confirm a *name*: that needs the wave name on the
    // instrument's display, which is why area 9 still has panel steps.
    if (options.surveyWaves) {
        std::cout << "Surveying wave references across USER:001..128 (read-only)\n\n";
        struct GroupStats
        {
            std::size_t count = 0;
            int minNumber = 1 << 30;
            int maxNumber = -1;
        };
        std::map<std::pair<int, int>, GroupStats> byGroup; // (groupType, groupId)
        std::map<std::string, std::size_t> byTypeLabel;
        std::size_t patchesRead = 0;
        std::size_t patchesFailed = 0;

        const auto userBase = roland::RolandAddress(0x11, 0x00, 0x00, 0x00);
        for (int patchNumber = 0; patchNumber < 128; ++patchNumber) {
            const auto base = userBase.plus(static_cast<std::uint64_t>(patchNumber)
                * xpmodel::Xp60PatchLayout::kUserPatchStride);
            if (!base)
                break;
            const auto patchRequest =
                roland::RolandSysExMessage::dataRequest(*deviceId, xp60::modelId(), *base, size);
            const auto patchRequestBytes = patchRequest.encode();
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->replies.clear();
                state->requestSentAt = Clock::now();
            }
            if (transport.sendSysEx(midi::MidiByteSpan(patchRequestBytes.data(), patchRequestBytes.size())).failed()) {
                ++patchesFailed;
                continue;
            }
            std::vector<Reply> collected;
            {
                std::unique_lock<std::mutex> lock(state->mutex);
                if (state->arrived.wait_for(lock, options.firstTimeout, [&] { return !state->replies.empty(); })) {
                    while (true) {
                        const auto seen = state->replies.size();
                        if (!state->arrived.wait_for(
                                lock, options.quietTimeout, [&] { return state->replies.size() > seen; }))
                            break;
                    }
                }
                collected = state->replies;
            }
            const auto patch = decodePatchFrom(collected, *base);
            if (!patch) {
                ++patchesFailed;
                std::cout << "  USER:" << (patchNumber + 1) << " did not decode\n";
                continue;
            }
            ++patchesRead;
            for (const auto tone : xpmodel::ToneIndex::all()) {
                const auto wave = patch->wave(tone);
                auto& stats = byGroup[{wave.groupTypeRaw, wave.groupId}];
                ++stats.count;
                stats.minNumber = std::min(stats.minNumber, wave.numberDisplay);
                stats.maxNumber = std::max(stats.maxNumber, wave.numberDisplay);
                ++byTypeLabel[std::string(wave.groupTypeLabel)];
            }
        }

        std::cout << "Patches read: " << patchesRead << ", failed: " << patchesFailed << "\n";
        std::cout << "Wave references: " << (patchesRead * 4) << "\n\n";
        std::cout << "  groupType  groupId  references  number range (display)\n";
        for (const auto& [key, stats] : byGroup) {
            std::cout << "  " << key.first << "          " << key.second << "        " << stats.count << "         "
                      << stats.minNumber << " .. " << stats.maxNumber << "\n";
        }
        std::cout << "\n  group type labels seen:";
        for (const auto& [label, count] : byTypeLabel)
            std::cout << " " << label << "(" << count << ")";
        std::cout << "\n\nCatalog for comparison: INT-A holds 255 waves, INT-B holds 193.\n"
                  << "A number above a bank's size, or a group ID the catalog has no bank for,\n"
                  << "would disprove the assumed mapping. Names still need the instrument display.\n";
        transport.closeAll();
        return 0;
    }

    // Watch mode: poll the Patch and name every parameter that moves.
    //
    // This is the capture-pair workflow of DEVICE_ACCEPTANCE.md areas 8 and 9
    // turned into one continuous run. Instead of taking a before capture,
    // changing a value, taking an after capture and diffing the two by hand for
    // every parameter under test, the instrument is polled and each change is
    // reported as it happens, named by the same parameter tables that generate
    // the C++ code. Read-only: every poll is an RQ1.
    if (options.watchIntervalMs > 0) {
        // Unbuffered: the operator reads this live while turning knobs, and it
        // is usually redirected to a log at the same time.
        std::cout << std::unitbuf;
        std::cout << "Watching " << label << " every " << options.watchIntervalMs
                  << " ms. Change values on the XP-60; each change is named below.\n"
                  << "Read-only (RQ1 only). Press Ctrl+C to stop.\n\n";
        std::optional<xpmodel::Xp60Patch> previous;
        const auto watchDeadline = options.watchSeconds > 0
            ? std::optional(Clock::now() + std::chrono::seconds(options.watchSeconds))
            : std::nullopt;
        while (!watchDeadline || Clock::now() < *watchDeadline) {
            const auto replies = exchangeOnce();
            auto current = decodePatchFrom(replies, address);
            if (!current) {
                std::cout << "  (incomplete capture, retrying)\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(options.watchIntervalMs));
                continue;
            }
            if (!previous) {
                std::cout << "baseline: \"" << current->name().displayText() << "\"\n";
            } else {
                const auto diff = xpmodel::Xp60PatchDiff::compare(*previous, *current);
                if (!diff.identical()) {
                    std::cout << "--- " << diff.summary() << "\n";
                    for (const auto& d : diff.differences()) {
                        std::cout << "  " << d.block << "  " << d.parameterName << " (" << d.parameterId << ")\n"
                                  << "      raw " << d.leftRaw << " -> " << d.rightRaw << "   display \"" << d.leftText
                                  << "\" -> \"" << d.rightText << "\"\n"
                                  << "      block offset " << d.blockOffset << ", patch offset " << d.patchOffset
                                  << "\n";
                    }
                    std::cout << std::flush;
                }
            }
            previous = std::move(current);
            std::this_thread::sleep_for(std::chrono::milliseconds(options.watchIntervalMs));
        }
        std::cout << "\nWatch finished.\n";
        transport.closeAll();
        return 0;
    }

    int failures = 0;
    for (int attempt = 1; attempt <= options.repeat; ++attempt) {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->replies.clear();
            state->requestSentAt = Clock::now();
        }
        std::cout << "--- attempt " << attempt << " of " << options.repeat << " ---\n";
        std::cout << "OUT " << request.summary() << "\n";
        std::cout << "    " << roland::toHex(roland::ByteSpan(requestBytes.data(), requestBytes.size())) << "\n";

        if (const auto error = transport.sendSysEx(midi::MidiByteSpan(requestBytes.data(), requestBytes.size()));
            error.failed()) {
            std::cerr << "Send failed: " << error.message << "\n";
            return 6;
        }

        // Wait for the first reply, then keep collecting until the device goes
        // quiet, so a multi-packet answer is not reported as a single chunk.
        std::vector<Reply> collected;
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            if (state->arrived.wait_for(lock, options.firstTimeout, [&] { return !state->replies.empty(); })) {
                while (true) {
                    const auto seen = state->replies.size();
                    if (!state->arrived.wait_for(lock, options.quietTimeout,
                            [&] { return state->replies.size() > seen; }))
                        break;
                }
            }
            collected = state->replies;
        }

        if (collected.empty()) {
            std::cout << "IN  (nothing within " << options.firstTimeout.count() << " ms)\n"
                      << "    Check: Rx Exclusive ON, the device ID, and that the interface MIDI OUT reaches\n"
                      << "    XP-60 MIDI IN while XP-60 MIDI OUT reaches the interface MIDI IN.\n\n";
            ++failures;
            continue;
        }

        std::size_t totalPayload = 0;
        for (const auto& reply : collected) {
            const roland::ByteSpan span(reply.bytes.data(), reply.bytes.size());
            std::cout << "IN  +" << reply.sinceRequest.count() << " ms, " << reply.bytes.size() << " raw bytes\n";
            std::cout << "    " << roland::toHex(span) << "\n";
            const auto decoded = roland::decodeRolandSysEx(span, xp60::modelId());
            if (decoded.ok()) {
                const auto& message = *decoded.message;
                std::cout << "    " << message.summary() << ", checksum OK\n";
                if (message.isDataSet()) {
                    totalPayload += message.data().size();
                    std::cout << "    text: \"" << asAsciiText(message.data()) << "\"\n";
                }
            } else {
                ++failures;
                std::cout << "    REJECTED: " << roland::describe(*decoded.failure) << "\n";
            }
            if (capture) {
                capture.write(reinterpret_cast<const char*>(reply.bytes.data()),
                    static_cast<std::streamsize>(reply.bytes.size()));
            }
        }
        std::cout << "    packets: " << collected.size() << ", payload bytes: " << totalPayload << " of "
                  << size.value() << " address units requested\n";

        // Replay the exchange through the application's own correlation logic,
        // so the probe reports what XP60Studio would conclude rather than only
        // what the wire carried.
        protocol::RolandRequestTracker tracker;
        const auto trackedId = tracker.enqueue(request, protocol::Clock::now());
        tracker.markSent(trackedId, protocol::Clock::now());
        for (const auto& reply : collected) {
            const auto decoded =
                roland::decodeRolandSysEx(roland::ByteSpan(reply.bytes.data(), reply.bytes.size()), xp60::modelId());
            if (decoded.ok())
                tracker.onDataSet(*decoded.message, protocol::Clock::now());
        }
        if (const auto* op = tracker.find(trackedId)) {
            std::cout << "    tracker: " << protocol::requestStateName(op->state) << ", " << op->receivedBytes
                      << " bytes in " << op->chunkCount << " chunk(s), covered through " << op->coveredThroughBytes
                      << " of " << op->expectedBytes;
            if (op->notes.empty()) {
                std::cout << ", no notes\n";
            } else {
                std::cout << "\n";
                for (const auto& note : op->notes)
                    std::cout << "      note: " << note << "\n";
            }
            if (op->state != protocol::RequestState::Completed)
                ++failures;
        }
        if (options.decodePatch)
            reportDecodedPatch(collected, address);
        if (options.verifyParameters && !verifyAllParameters(collected, address, options.listParameters))
            ++failures;
        std::cout << "\n";
    }

    transport.closeAll();
    if (failures > 0) {
        std::cout << failures << " attempt(s) did not produce a valid reply.\n";
        return 1;
    }
    std::cout << "All " << options.repeat << " attempt(s) answered and validated.\n";
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << name << " needs a value.\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            usage();
            return 0;
        } else if (arg == "--list-presets") {
            for (const auto& preset : xp60studio::xp60::safeReadPresets())
                std::cout << preset.id << "  -  " << preset.name << "\n";
            return 0;
        } else if (arg == "--in") {
            options.inputMatch = next("--in");
        } else if (arg == "--out") {
            options.outputMatch = next("--out");
        } else if (arg == "--preset") {
            options.presetId = next("--preset");
        } else if (arg == "--address") {
            options.addressHex = next("--address");
        } else if (arg == "--size") {
            options.sizeHex = next("--size");
        } else if (arg == "--device-id") {
            options.deviceIdDisplay = std::stoi(next("--device-id"));
        } else if (arg == "--repeat") {
            options.repeat = std::stoi(next("--repeat"));
        } else if (arg == "--timeout") {
            options.firstTimeout = std::chrono::milliseconds(std::stoi(next("--timeout")));
        } else if (arg == "--quiet") {
            options.quietTimeout = std::chrono::milliseconds(std::stoi(next("--quiet")));
        } else if (arg == "--survey-waves") {
            options.decodePatch = true;
            options.surveyWaves = true;
        } else if (arg == "--watch-for") {
            options.watchSeconds = std::stoi(next("--watch-for"));
        } else if (arg == "--watch") {
            options.decodePatch = true;
            options.watchIntervalMs = (i + 1 < argc && argv[i + 1][0] != '-') ? std::stoi(next("--watch")) : 700;
        } else if (arg == "--patch") {
            options.decodePatch = true;
        } else if (arg == "--verify") {
            options.decodePatch = true;
            options.verifyParameters = true;
        } else if (arg == "--list-parameters") {
            options.listParameters = true;
        } else if (arg == "--save") {
            options.savePath = next("--save");
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            usage();
            return 2;
        }
    }
    if (options.inputMatch.empty() || options.outputMatch.empty()) {
        std::cerr << "--in and --out are required.\n\n";
        usage();
        return 2;
    }
    try {
        return runProbe(options);
    } catch (const std::exception& error) {
        std::cerr << "Hardware probe failed: " << error.what() << "\n";
        return 1;
    }
}
