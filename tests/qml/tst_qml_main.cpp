#include "support/FakeXp60.h"

#include "midi/LoopbackMidiTransport.h"
#include "presentation/AppShellViewModel.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/PatchEditorViewModel.h"
#include "presentation/QmlRegistration.h"
#include "services/DeviceSession.h"
#include "services/PatchTransfer.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QtQuickTest>

#include <chrono>
#include <memory>

namespace {

// One application stack: transport, session and the write engine over it.
struct Stack
{
    xp60studio::midi::LoopbackMidiTransport* transport = nullptr;
    std::unique_ptr<xp60studio::services::DeviceSession> session;
    std::unique_ptr<xp60studio::services::PatchTransfer> transfer;

    Stack()
    {
        auto loopback = std::make_unique<xp60studio::midi::LoopbackMidiTransport>();
        loopback->addInput("in-1", "XP-60 IN");
        loopback->addOutput("out-1", "XP-60 OUT");
        transport = loopback.get();
        session = std::make_unique<xp60studio::services::DeviceSession>(std::move(loopback));
        session->setAutomaticTimeoutPolling(false);
        auto pacing = session->pacing();
        pacing.interMessageDelay = std::chrono::milliseconds(0);
        session->setPacing(pacing);
        transfer = std::make_unique<xp60studio::services::PatchTransfer>(*session);
    }
};

} // namespace

// Provides real presentation objects to the QML tests.
//
// The Devices and Shell tests get a pristine stack (`testDevices`, `testShell`)
// so they can drive connect/fetch/arm from the beginning. The editor gets its
// own stack (`testEditor`) already holding a Patch read from the golden fixture
// through the ordinary fetch path, so the screen is exercised against real
// data without disturbing the other tests. `testHarness.reloadPatch()` puts
// the editor back to that just-fetched state between tests.
class Setup : public QObject
{
    Q_OBJECT

public:
    // Re-reads the Patch, which clears the editor's history and A/B state.
    Q_INVOKABLE void reloadPatch()
    {
        m_editorStack->session->fetchTemporaryPatch();
        pump(*m_editorStack);
    }

public slots:
    void applicationAvailable()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        xp60studio::presentation::registerQmlTypes();
    }

    void qmlEngineAvailable(QQmlEngine* engine)
    {
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));

        m_deviceStack = std::make_unique<Stack>();
        m_devices = std::make_unique<xp60studio::presentation::DevicesViewModel>(*m_deviceStack->session,
                                                                                m_deviceStack->transfer.get());
        m_shell = std::make_unique<xp60studio::presentation::AppShellViewModel>(m_devices.get());

        m_editorStack = std::make_unique<Stack>();
        m_editor = std::make_unique<xp60studio::presentation::PatchEditorViewModel>(*m_editorStack->session,
                                                                                    m_editorStack->transfer.get());
        m_fake = std::make_unique<xp60studio::testsupport::FakeXp60>(xp60studio::testsupport::temporaryAreaWith(4));
        m_editorStack->session->connectEndpoints("in-1", "out-1");
        reloadPatch();

        engine->rootContext()->setContextProperty(QStringLiteral("testDevices"), m_devices.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testShell"), m_shell.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testEditor"), m_editor.get());
        engine->rootContext()->setContextProperty(QStringLiteral("testHarness"), this);
    }

private:
    // Runs the fake conversation until nothing is left in flight.
    void pump(Stack& stack)
    {
        for (int i = 0; i < 40; ++i) {
            QCoreApplication::processEvents();
            const auto replies = m_fake->exchange(*stack.transport);
            for (const auto& reply : replies) {
                const auto bytes = reply.encode();
                stack.transport->injectIncoming(xp60studio::midi::MidiByteSpan(bytes.data(), bytes.size()));
            }
            QCoreApplication::processEvents();
            if (replies.empty() && stack.transport->sentMessages().empty() && stack.session->pendingSendCount() == 0) {
                return;
            }
        }
    }

    std::unique_ptr<Stack> m_deviceStack;
    std::unique_ptr<Stack> m_editorStack;
    std::unique_ptr<xp60studio::presentation::DevicesViewModel> m_devices;
    std::unique_ptr<xp60studio::presentation::AppShellViewModel> m_shell;
    std::unique_ptr<xp60studio::presentation::PatchEditorViewModel> m_editor;
    std::unique_ptr<xp60studio::testsupport::FakeXp60> m_fake;
};

QUICK_TEST_MAIN_WITH_SETUP(xp60studio_qml, Setup)

#include "tst_qml_main.moc"
