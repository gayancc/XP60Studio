#include "simulation/DemoBootstrap.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QtGlobal>

#include <cstring>

namespace xp60studio::simulation {

namespace {

bool envForcesDemo(bool* forcedOff)
{
    *forcedOff = false;
    if (!qEnvironmentVariableIsSet("XP60STUDIO_DEMO")) {
        return false;
    }
    const QByteArray value = qgetenv("XP60STUDIO_DEMO");
    if (value.isEmpty() || value == "0" || value.compare("false", Qt::CaseInsensitive) == 0) {
        *forcedOff = true;
        return false;
    }
    return true;
}

bool argsRequestDemo(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--demo") == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

DemoTransportBundle createDemoTransport()
{
    DemoTransportBundle bundle;
    bundle.transport = std::make_unique<midi::LoopbackMidiTransport>();
    bundle.transport->addInput(kDemoInputId, kDemoInputName);
    bundle.transport->addOutput(kDemoOutputId, kDemoOutputName);
    bundle.raw = bundle.transport.get();
    return bundle;
}

SimulatedXp60 createSeededSimulatedXp60(int userPatchNumber)
{
    return SimulatedXp60(temporaryAreaFromFixture(userPatchNumber));
}

bool demoModePreference()
{
    return QSettings().value(QLatin1String(kDemoModeSettingsKey), false).toBool();
}

void setDemoModePreference(bool enabled)
{
    QSettings settings;
    settings.setValue(QLatin1String(kDemoModeSettingsKey), enabled);
    settings.sync();
}

bool isDemoModeRequested(int argc, char* argv[])
{
    bool forcedOff = false;
    if (envForcesDemo(&forcedOff)) {
        return true;
    }
    if (forcedOff) {
        return false;
    }
    if (argsRequestDemo(argc, argv)) {
        return true;
    }
    return demoModePreference();
}

bool relaunchForDemoMode(bool enabled)
{
    setDemoModePreference(enabled);

    QStringList args;
    const QStringList current = QCoreApplication::arguments();
    for (int i = 1; i < current.size(); ++i) {
        if (current.at(i) == QLatin1String("--demo")) {
            continue;
        }
        args.append(current.at(i));
    }

    // Preference drives the next launch; keep --demo off the consumer path.
    const QString program = QCoreApplication::applicationFilePath();
    if (!QProcess::startDetached(program, args)) {
        qWarning("Could not relaunch %s for Demo Mode change", qUtf8Printable(program));
        return false;
    }
    QCoreApplication::quit();
    return true;
}

} // namespace xp60studio::simulation
