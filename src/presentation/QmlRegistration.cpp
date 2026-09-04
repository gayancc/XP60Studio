#include "presentation/QmlRegistration.h"

#include "presentation/AppShellViewModel.h"
#include "presentation/ConnectionState.h"
#include "presentation/DevicesViewModel.h"
#include "presentation/PatchEditorViewModel.h"
#include "presentation/ToneViewModel.h"
#include "presentation/MidiEndpointListModel.h"
#include "presentation/ProtocolLogModel.h"
#include "presentation/RequestOperationModel.h"

#include <QQmlEngine>

namespace xp60studio::presentation {

void registerQmlTypes()
{
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    const char* reason = "Created by the application, not from QML";
    qmlRegisterUncreatableMetaObject(xp60studio::presentation::staticMetaObject, kPresentationModuleUri, 1, 0,
                                     "ConnectionState", QString::fromLatin1(reason));
    qmlRegisterUncreatableType<DevicesViewModel>(kPresentationModuleUri, 1, 0, "DevicesViewModel", QString::fromLatin1(reason));
    qmlRegisterUncreatableType<AppShellViewModel>(kPresentationModuleUri, 1, 0, "AppShellViewModel", QString::fromLatin1(reason));
    qmlRegisterUncreatableType<PatchEditorViewModel>(kPresentationModuleUri, 1, 0, "PatchEditorViewModel", QString::fromLatin1(reason));
    qmlRegisterUncreatableType<ToneViewModel>(kPresentationModuleUri, 1, 0, "ToneViewModel", QString::fromLatin1(reason));
    qmlRegisterUncreatableType<EditorParameterModel>(kPresentationModuleUri, 1, 0, "EditorParameterModel", QString::fromLatin1(reason));
    qmlRegisterUncreatableType<MidiEndpointListModel>(kPresentationModuleUri, 1, 0, "MidiEndpointListModel", QString::fromLatin1(reason));
    qmlRegisterUncreatableType<ProtocolLogModel>(kPresentationModuleUri, 1, 0, "ProtocolLogModel", QString::fromLatin1(reason));
    qmlRegisterUncreatableType<RequestOperationModel>(kPresentationModuleUri, 1, 0, "RequestOperationModel", QString::fromLatin1(reason));
}

} // namespace xp60studio::presentation
