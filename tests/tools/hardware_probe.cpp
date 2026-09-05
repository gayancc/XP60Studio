#include "midi/LibremidiTransport.h"
#include "protocol/RolandRequestTracker.h"
#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "roland/RolandSysExMessage.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/MemoryImage.h"
#include "xpmodel/Xp60PatchCodec.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
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
        // The documented Patch span: Common plus the four padded Tone blocks.
        address = roland::RolandAddress(0x03, 0x00, 0x00, 0x00);
        size = roland::RolandSize::fromValue(xpmodel::Xp60PatchLayout::patchSpan()).value();
        label = "temporary Patch, whole (span " + std::to_string(xpmodel::Xp60PatchLayout::patchSpan()) + ")";
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
        } else if (arg == "--patch") {
            options.decodePatch = true;
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
