#include "midi/LibremidiTransport.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"
#include "xpmodel/Xp60PatchDiff.h"

#include <QCoreApplication>
#include <QObject>
#include <QTimer>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

// Phase 3 write round trip against a physical XP-60, run headlessly.
//
// This harness does not implement a write. It drives services::PatchTransfer,
// the same class the Devices screen drives, so every safety property of the
// application's armed write flow is in force here exactly as it is in the UI:
//
//   * the only writable target is the temporary Patch area (03 00 00 00);
//     permanent User memory is not reachable through PatchTransfer at all;
//   * arming is refused until a temporary-Patch read has succeeded;
//   * one arming permits one write;
//   * the Patch present beforehand is captured as a safety snapshot first;
//   * success requires the read-back to equal what was sent, parameter for
//     parameter. A completed send is never reported as a verified state.
//
// By default it writes back the Patch it just read, unchanged. That is the
// identity round trip of docs/HARDWARE_VALIDATION_XP60.md step 8: it proves
// FETCH -> DECODE -> ENCODE -> SEND -> FETCH AGAIN -> COMPARE without altering
// how the instrument sounds.

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

// Spins the Qt event loop until `done` or the deadline passes.
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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    std::string portMatch = "U2MIDI";
    int deviceIdDisplay = 17;
    bool restoreAfter = false;
    int pacingMs = -1;      // -1: leave the session default
    int firstTimeoutMs = -1;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc)
            portMatch = argv[++i];
        else if (arg == "--device-id" && i + 1 < argc)
            deviceIdDisplay = std::stoi(argv[++i]);
        else if (arg == "--restore")
            restoreAfter = true;
        else if (arg == "--pacing" && i + 1 < argc)
            pacingMs = std::stoi(argv[++i]);
        else if (arg == "--first-timeout" && i + 1 < argc)
            firstTimeoutMs = std::stoi(argv[++i]);
        else if (arg == "--help") {
            std::cout << "Usage: xp60studio_write_roundtrip [--port <text>] [--device-id <n>] [--restore]\n"
                      << "                                   [--pacing <ms>] [--first-timeout <ms>]\n"
                      << "Writes the temporary Patch back to itself and verifies the read-back.\n"
                      << "Permanent User memory is never a target.\n";
            return 0;
        }
    }

    auto transport = std::make_unique<midi::LibremidiTransport>();
    std::cout << "Backend: " << transport->backendName() << "\n";
    std::this_thread::sleep_for(std::chrono::seconds(2)); // WinRT discovery is asynchronous.

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
    const auto inputName = input->displayName;
    const auto outputName = output->displayName;

    services::DeviceSession session(std::move(transport));
    const auto deviceId = roland::RolandDeviceId::fromDisplayNumber(deviceIdDisplay);
    if (!deviceId) {
        std::cerr << "Device ID " << deviceIdDisplay << " is outside 17..32.\n";
        return 2;
    }
    session.setDeviceId(*deviceId);
    if (pacingMs >= 0 || firstTimeoutMs > 0) {
        auto pacing = session.pacing();
        if (pacingMs >= 0)
            pacing.interMessageDelay = std::chrono::milliseconds(pacingMs);
        if (firstTimeoutMs > 0)
            pacing.timeouts.firstResponse = std::chrono::milliseconds(firstTimeoutMs);
        session.setPacing(pacing);
        std::cout << "Pacing: " << pacing.interMessageDelay.count() << " ms between messages, "
                  << pacing.timeouts.firstResponse.count() << " ms first-response timeout\n";
    }

    if (!session.connectEndpoints(inputId, outputId)) {
        std::cerr << "Connect failed: " << session.lastError() << "\n";
        return 4;
    }
    std::cout << "Connected. IN: " << inputName << "  OUT: " << outputName << "\n";
    std::cout << "Device ID " << deviceId->displayNumber() << "\n\n";

    services::PatchTransfer transfer(session);

    // 1. FETCH -------------------------------------------------------------
    std::cout << "1. Fetching the temporary Patch\n";
    if (!session.fetchTemporaryPatch(services::DeviceSession::PatchFetchPurpose::Transfer)) {
        std::cerr << "   fetch refused\n";
        return 5;
    }
    const auto fetchDone = [&] {
        const auto state = session.patchFetch().state;
        return state == services::DeviceSession::PatchFetchState::Completed
            || state == services::DeviceSession::PatchFetchState::Failed;
    };
    if (!pumpUntil(fetchDone, std::chrono::seconds(20))) {
        std::cerr << "   fetch did not finish within 20 s\n";
        return 5;
    }
    if (session.patchFetch().state != services::DeviceSession::PatchFetchState::Completed) {
        std::cerr << "   fetch failed: " << session.patchFetch().message << "\n";
        return 5;
    }
    const auto original = *session.patchFetch().patch;
    std::cout << "   \"" << original.name().displayText() << "\", " << session.patchFetch().completedBlocks << " of "
              << session.patchFetch().totalBlocks << " blocks\n";
    if (!session.patchFetch().decodeReport.empty())
        std::cout << "   decode notes: " << session.patchFetch().decodeReport << "\n";

    // 2. ARM ---------------------------------------------------------------
    std::cout << "\n2. Arming\n";
    std::cout << "   plan: " << transfer.writePlanDescription() << "\n";
    std::cout << "   read verified: " << (transfer.readVerified() ? "yes" : "no") << "\n";
    if (!transfer.arm()) {
        std::cerr << "   arming refused (a successful temporary-Patch read is required first)\n";
        return 6;
    }
    std::cout << "   armed: " << (transfer.isArmed() ? "yes" : "no") << "\n";

    // 3. WRITE AND VERIFY --------------------------------------------------
    std::cout << "\n3. Writing the Patch back unchanged and verifying\n";
    if (!transfer.writeAndVerifyTemporaryPatch(original)) {
        std::cerr << "   refused: " << transfer.message() << "\n";
        return 7;
    }
    const auto settled = [&] {
        switch (transfer.state()) {
        case services::PatchTransfer::State::Verified:
        case services::PatchTransfer::State::Mismatch:
        case services::PatchTransfer::State::Failed:
        case services::PatchTransfer::State::Cancelled:
            return true;
        default:
            return false;
        }
    };
    if (!pumpUntil(settled, std::chrono::seconds(40))) {
        std::cerr << "   transfer did not settle within 40 s (state "
                  << transfer.stateName() << ")\n";
        return 7;
    }

    std::cout << "   state: " << transfer.stateLabel() << "\n";
    std::cout << "   " << transfer.message() << "\n";
    std::cout << "   DT1 messages sent: " << transfer.messagesSent() << "\n";
    std::cout << "   safety snapshot captured: " << (transfer.safetySnapshot() ? "yes" : "no") << "\n";

    int exitCode = 0;
    if (transfer.diff() && !transfer.diff()->identical()) {
        std::cout << "\n   " << transfer.diff()->summary() << "\n";
        std::cout << transfer.diff()->describe(40) << "\n";
    }
    if (transfer.state() != services::PatchTransfer::State::Verified) {
        std::cout << "\nROUND TRIP NOT VERIFIED\n";
        exitCode = 1;
    } else {
        std::cout << "\nROUND TRIP VERIFIED: the read-back equals what was sent, parameter for parameter.\n";
    }

    // 4. Optional restore --------------------------------------------------
    if (restoreAfter && transfer.safetySnapshot()) {
        std::cout << "\n4. Restoring the safety snapshot\n";
        if (!transfer.arm()) {
            std::cerr << "   arming refused\n";
        } else if (!transfer.restoreSafetySnapshot()) {
            std::cerr << "   restore refused: " << transfer.message() << "\n";
        } else if (!pumpUntil(settled, std::chrono::seconds(40))) {
            std::cerr << "   restore did not settle\n";
        } else {
            std::cout << "   state: " << transfer.stateLabel() << "\n   " << transfer.message() << "\n";
        }
    }

    session.disconnectEndpoints();
    return exitCode;
}
