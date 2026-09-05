#include "services/UserBankRead.h"

#include "xpmodel/Xp60BankLocation.h"
#include "xpmodel/Xp60PatchLayout.h"

namespace xp60studio::services {

using xpmodel::Xp60BankLocation;
using xpmodel::Xp60PatchLayout;

namespace {

QString destinationLabel(int userNumber)
{
    const auto location = Xp60BankLocation::fromUserNumber(userNumber);
    const auto linear = QStringLiteral("USER:%1").arg(userNumber, 3, 10, QLatin1Char('0'));
    if (!location) {
        return linear;
    }
    return QStringLiteral("%1 · %2").arg(QString::fromStdString(location->panelLabel()), linear);
}

} // namespace

UserBankRead::UserBankRead(DeviceSession& session, QObject* parent)
    : QObject(parent)
    , m_session(session)
{
    connect(&m_session, &DeviceSession::patchFetchChanged, this, &UserBankRead::onPatchFetchChanged);
    connect(&m_session, &DeviceSession::connectionStateChanged, this, [this] {
        if (isBusy() && m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
            finish(State::Failed,
                   tr("Disconnected after reading %n Patch(es). What was read is kept.", "",
                      static_cast<int>(m_slots.size())));
        }
    });
}

bool UserBankRead::read(std::vector<int> userNumbers)
{
    if (isBusy() || userNumbers.empty()) {
        return false;
    }
    if (m_session.connectionState() != DeviceSession::ConnectionState::Connected) {
        m_state = State::Failed;
        m_message = tr("Not connected to an XP-60.");
        emit changed();
        return false;
    }
    for (const int userNumber : userNumbers) {
        if (!Xp60BankLocation::isValidUserNumber(userNumber)) {
            m_state = State::Failed;
            m_message = tr("USER:%1 is outside the 128-slot User bank.").arg(userNumber);
            emit changed();
            return false;
        }
    }

    m_wanted = std::move(userNumbers);
    m_slots.clear();
    m_slots.reserve(m_wanted.size());
    m_index = 0;
    m_cancelRequested = false;
    m_state = State::Reading;
    m_message.clear();
    emit changed();
    requestNext();
    return true;
}

bool UserBankRead::readWholeBank()
{
    std::vector<int> all;
    all.reserve(static_cast<std::size_t>(Xp60BankLocation::kUserPatchCount));
    for (int userNumber = 1; userNumber <= Xp60BankLocation::kUserPatchCount; ++userNumber) {
        all.push_back(userNumber);
    }
    return read(std::move(all));
}

void UserBankRead::requestNext()
{
    if (m_cancelRequested) {
        finish(State::Cancelled,
               tr("Stopped after reading %n Patch(es). What was read is kept.", "",
                  static_cast<int>(m_slots.size())));
        return;
    }
    if (m_index >= m_wanted.size()) {
        finish(State::Completed,
               tr("Read %n Patch(es) from the XP-60's USER memory.", "", static_cast<int>(m_slots.size())));
        return;
    }

    const int userNumber = m_wanted[m_index];
    const auto address = Xp60PatchLayout::userPatchAddress(userNumber);
    m_message = tr("Reading %1 (%2 of %3)")
                    .arg(destinationLabel(userNumber))
                    .arg(m_index + 1)
                    .arg(m_wanted.size());
    m_awaitingFetch = true;
    emit changed();
    // Read-only: RQ1 cannot modify device memory, so a whole-bank read is safe
    // to run against an instrument whatever else is going on.
    if (!address || !m_session.fetchPatch(*address, DeviceSession::PatchFetchPurpose::Transfer)) {
        finish(State::Failed, tr("Could not start reading %1.").arg(destinationLabel(userNumber)));
    }
}

void UserBankRead::onPatchFetchChanged()
{
    if (!m_awaitingFetch || !isBusy()) {
        return;
    }
    const auto& fetch = m_session.patchFetch();
    if (fetch.state == DeviceSession::PatchFetchState::Completed && fetch.patch) {
        m_awaitingFetch = false;
        m_slots.push_back(Slot{m_wanted[m_index], *fetch.patch, fetch.originalSysEx});
        ++m_index;
        emit progressed(m_slots.size(), m_wanted.size());
        emit changed();
        requestNext();
        return;
    }
    if (fetch.state == DeviceSession::PatchFetchState::Failed) {
        m_awaitingFetch = false;
        // A Patch that will not read stops the run rather than leaving a hole
        // nobody would notice in a backup.
        finish(State::Failed,
               tr("%1 could not be read, so the run stopped there: %2. The %n Patch(es) read before it are kept.", "",
                  static_cast<int>(m_slots.size()))
                   .arg(destinationLabel(m_wanted[m_index]), QString::fromStdString(fetch.message)));
    }
}

void UserBankRead::cancel()
{
    if (!isBusy()) {
        return;
    }
    // Honoured between Patches: stopping mid-Patch would leave a slot half read
    // and there is nothing useful to do with that.
    m_cancelRequested = true;
    m_message = tr("Stopping after the Patch being read.");
    emit changed();
}

int UserBankRead::currentUserNumber() const noexcept
{
    if (!isBusy() || m_index >= m_wanted.size()) {
        return 0;
    }
    return m_wanted[m_index];
}

void UserBankRead::finish(State state, QString message)
{
    m_awaitingFetch = false;
    m_state = state;
    m_message = std::move(message);
    emit changed();
    emit finished(state == State::Completed);
}

} // namespace xp60studio::services
