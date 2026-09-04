#include "midi/LibremidiTransport.h"

#include <chrono>
#include <iostream>
#include <thread>

// Read-only discovery aid. Never opens a MIDI port or sends a message.
int main()
{
    try {
        xp60studio::midi::LibremidiTransport transport;
        std::cout << transport.backendName() << std::endl;
        // Windows Runtime discovery is delivered asynchronously by DeviceWatcher.
        std::this_thread::sleep_for(std::chrono::seconds(2));
        const auto print = [](const char* direction, const auto& ports) {
            std::cout << direction << ": " << ports.size() << '\n';
            for (const auto& port : ports)
                std::cout << "  " << port.displayName << " [" << port.backendName << "]\n";
        };
        print("MIDI IN", transport.enumerateInputs());
        print("MIDI OUT", transport.enumerateOutputs());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "MIDI discovery failed: " << error.what() << '\n';
        return 1;
    }
}
