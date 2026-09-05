#include "library/LibraryEntry.h"
#include "library/SyxExport.h"
#include "midi/LibremidiTransport.h"
#include "roland/HexFormat.h"
#include "roland/RolandCodec.h"
#include "services/DeviceSession.h"
#include "xp60/Xp60Device.h"
#include "xpmodel/Xp60PatchDiff.h"
#include "xpmodel/Xp60PatchLayout.h"

#include <QCoreApplication>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Area 10 of docs/DEVICE_ACCEPTANCE.md, run against a physical XP-60:
// confirm that a `.syx` XP60Studio exported is accepted by the instrument and
// reproduces the Patch it was exported from.
//
//   fetch -> build a library entry -> export .syx -> validate -> send ->
//   fetch again -> compare
//
// The export is produced by library::exportEntries, the same code the Library
// screen will call; nothing about the file is assembled here.
//
// Safety. The export targets the temporary Patch area, and before a byte is
// transmitted every message in the file is decoded and checked to lie inside
// that area. Anything else -- a stray address, a message that is not a DT1,
// bytes that do not decode -- aborts the run without sending. Permanent User
// memory is therefore unreachable even if the export were wrong, which is the
// point of validating the file rather than trusting the exporter that wrote it.

namespace {

using namespace xp60studio;

const midi::MidiEndpointInfo* findPort(const std::vector<midi::MidiEndpointInfo>& ports, const std::string& match)
{
    for (const auto& port : ports) {
        if (port.displayName.find(match) != std::string::npos)
            return &port;
    }
    return nullptr;
}

template <typename Predicate>
bool pumpUntil(Predicate done, std::chrono::milliseconds limit)
{
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

// Splits a byte stream into complete F0..F7 messages.
std::vector<roland::ByteVector> splitSysEx(const roland::ByteVector& stream)
{
    std::vector<roland::ByteVector> messages;
    std::size_t i = 0;
    while (i < stream.size()) {
        if (stream[i] != 0xF0) {
            ++i;
            continue;
        }
        std::size_t end = i + 1;
        while (end < stream.size() && stream[end] != 0xF7)
            ++end;
        if (end >= stream.size())
            break;
        messages.emplace_back(stream.begin() + static_cast<long>(i), stream.begin() + static_cast<long>(end) + 1);
        i = end + 1;
    }
    return messages;
}

// The gate. Every message must be a DT1 whose whole payload lies inside the
// temporary Patch span. Returns the decoded messages, or nothing on refusal.
std::vector<roland::RolandSysExMessage> validateForTemporaryPatch(const roland::ByteVector& stream)
{
    const auto base = roland::RolandAddress(0x03, 0x00, 0x00, 0x00);
    const auto limit = base.plus(xpmodel::Xp60PatchLayout::patchSpan());
    std::vector<roland::RolandSysExMessage> accepted;
    for (const auto& raw : splitSysEx(stream)) {
        const auto decoded = roland::decodeRolandSysEx(roland::ByteSpan(raw.data(), raw.size()), xp60::modelId());
        if (!decoded.ok()) {
            std::cerr << "   REFUSED: a message did not decode: " << describe(*decoded.failure) << "\n";
            return {};
        }
        const auto& message = *decoded.message;
        if (!message.isDataSet()) {
            std::cerr << "   REFUSED: the export contains a " << message.summary() << "; only DT1 may be sent\n";
            return {};
        }
        const auto end = message.endAddress();
        if (message.address() < base || !end || !limit || *end > *limit) {
            std::cerr << "   REFUSED: " << message.summary() << " falls outside the temporary Patch area "
                      << base.toHexString() << " .. " << (limit ? limit->toHexString() : std::string("?")) << "\n";
            return {};
        }
        accepted.push_back(message);
    }
    if (accepted.empty())
        std::cerr << "   REFUSED: the export contained no DT1 messages\n";
    return accepted;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    std::string portMatch = "U2MIDI";
    int deviceIdDisplay = 17;
    std::string savePath;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc)
            portMatch = argv[++i];
        else if (arg == "--device-id" && i + 1 < argc)
            deviceIdDisplay = std::stoi(argv[++i]);
        else if (arg == "--save" && i + 1 < argc)
            savePath = argv[++i];
        else if (arg == "--help") {
            std::cout << "Usage: xp60studio_export_roundtrip [--port <text>] [--device-id <n>] [--save <file.syx>]\n"
                      << "Exports the temporary Patch as .syx, sends it back and verifies the result.\n"
                      << "Only the temporary Patch area is ever written.\n";
            return 0;
        }
    }

    auto transport = std::make_unique<midi::LibremidiTransport>();
    std::cout << "Backend: " << transport->backendName() << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    const auto inputs = transport->enumerateInputs();
    const auto outputs = transport->enumerateOutputs();
    const auto* input = findPort(inputs, portMatch);
    const auto* output = findPort(outputs, portMatch);
    if (!input || !output) {
        std::cerr << "No MIDI port matching '" << portMatch << "'.\n";
        return 3;
    }
    const auto inputId = input->id;
    const auto outputId = output->id;

    services::DeviceSession session(std::move(transport));
    const auto deviceId = roland::RolandDeviceId::fromDisplayNumber(deviceIdDisplay);
    if (!deviceId) {
        std::cerr << "Device ID " << deviceIdDisplay << " is outside 17..32.\n";
        return 2;
    }
    session.setDeviceId(*deviceId);
    if (!session.connectEndpoints(inputId, outputId)) {
        std::cerr << "Connect failed: " << session.lastError() << "\n";
        return 4;
    }
    std::cout << "Connected to " << input->displayName << " / " << output->displayName << ", device ID "
              << deviceId->displayNumber() << "\n\n";

    const auto fetchTemporary = [&](std::optional<xpmodel::Xp60Patch>& out, roland::ByteVector* rawOut) -> bool {
        session.clearLog();
        if (!session.fetchTemporaryPatch(services::DeviceSession::PatchFetchPurpose::Transfer))
            return false;
        const auto done = [&] {
            const auto s = session.patchFetch().state;
            return s == services::DeviceSession::PatchFetchState::Completed
                || s == services::DeviceSession::PatchFetchState::Failed;
        };
        if (!pumpUntil(done, std::chrono::seconds(20)))
            return false;
        if (session.patchFetch().state != services::DeviceSession::PatchFetchState::Completed) {
            std::cerr << "   fetch failed: " << session.patchFetch().message << "\n";
            return false;
        }
        out = session.patchFetch().patch;
        if (rawOut) {
            // The exact bytes the instrument sent, so the library entry's
            // originalSysEx is what arrived rather than a re-encoding of it.
            rawOut->clear();
            for (const auto& entry : session.log()) {
                if (entry.direction != diagnostics::LogDirection::In
                    || entry.kind != diagnostics::LogKind::RolandDataSet)
                    continue;
                if (const auto bytes = roland::parseHexBytes(entry.rawHex))
                    rawOut->insert(rawOut->end(), bytes->begin(), bytes->end());
            }
        }
        return true;
    };

    // 1. FETCH -------------------------------------------------------------
    std::cout << "1. Fetching the temporary Patch\n";
    std::optional<xpmodel::Xp60Patch> original;
    roland::ByteVector originalSysEx;
    if (!fetchTemporary(original, &originalSysEx) || !original)
        return 5;
    std::cout << "   \"" << original->name().displayText() << "\", " << originalSysEx.size()
              << " bytes of original SysEx captured\n";

    // 2. EXPORT ------------------------------------------------------------
    std::cout << "\n2. Exporting as .syx through library::exportEntries\n";
    library::PatchProvenance provenance;
    provenance.origin = library::PatchOrigin::FetchedFromDevice;
    provenance.sourceName = "XP-60 temporary Patch";
    provenance.address = roland::RolandAddress(0x03, 0x00, 0x00, 0x00);
    provenance.deviceId = *deviceId;
    provenance.modelId = xp60::modelId();
    const library::LibraryEntry entry(*original, originalSysEx, provenance);

    library::SyxExportOptions options;
    options.source = library::SyxExportSource::ReencodedFromModel;
    options.target.kind = library::SyxExportTarget::Kind::TemporaryPatch;
    options.deviceId = *deviceId;
    const auto exported = library::exportEntry(entry, options);
    if (!exported.ok) {
        std::cerr << "   export failed: " << exported.error << "\n";
        return 6;
    }
    std::cout << "   " << exported.summary() << "\n";
    for (const auto& note : exported.notes)
        std::cout << "   note: " << note << "\n";
    if (!savePath.empty()) {
        std::ofstream file(savePath, std::ios::binary);
        file.write(reinterpret_cast<const char*>(exported.bytes.data()),
            static_cast<std::streamsize>(exported.bytes.size()));
        std::cout << "   written to " << savePath << "\n";
    }

    // 3. VALIDATE ----------------------------------------------------------
    std::cout << "\n3. Validating every message in the export before sending\n";
    const auto messages = validateForTemporaryPatch(exported.bytes);
    if (messages.empty())
        return 7;
    std::cout << "   " << messages.size() << " DT1 message(s), all inside the temporary Patch area\n";

    // 4. SEND --------------------------------------------------------------
    std::cout << "\n4. Sending the exported file to the instrument\n";
    bool batchDone = false;
    bool batchOk = false;
    QString batchError;
    QObject::connect(&session, &services::DeviceSession::dataSetBatchFinished,
        [&](quint64, bool ok, const QString& error) {
            batchDone = true;
            batchOk = ok;
            batchError = error;
        });
    const auto batch = session.sendDataSets(messages);
    if (!batch.isValid()) {
        std::cerr << "   send refused: " << session.lastError() << "\n";
        return 8;
    }
    if (!pumpUntil([&] { return batchDone; }, std::chrono::seconds(30))) {
        std::cerr << "   send did not finish within 30 s\n";
        return 8;
    }
    if (!batchOk) {
        std::cerr << "   send failed: " << batchError.toStdString() << "\n";
        return 8;
    }
    std::cout << "   sent\n";

    // 5. FETCH AGAIN AND COMPARE -------------------------------------------
    std::cout << "\n5. Reading the Patch back and comparing\n";
    std::optional<xpmodel::Xp60Patch> readBack;
    if (!fetchTemporary(readBack, nullptr) || !readBack)
        return 9;
    std::cout << "   \"" << readBack->name().displayText() << "\"\n";

    const auto diff = xpmodel::Xp60PatchDiff::compare(*original, *readBack);
    if (diff.identical()) {
        std::cout << "\nEXPORT ROUND TRIP VERIFIED: the instrument accepted the exported .syx and the\n"
                     "Patch read back equals the one it was exported from, parameter for parameter.\n";
        session.disconnectEndpoints();
        return 0;
    }
    std::cout << "\n   " << diff.summary() << "\n" << diff.describe(40) << "\n";
    std::cout << "EXPORT ROUND TRIP NOT VERIFIED\n";
    session.disconnectEndpoints();
    return 1;
}
