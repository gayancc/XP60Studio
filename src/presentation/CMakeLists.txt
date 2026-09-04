add_library(xp60studio_presentation STATIC
  ConnectionState.h
  MidiEndpointListModel.h
  MidiEndpointListModel.cpp
  ProtocolLogModel.h
  ProtocolLogModel.cpp
  RequestOperationModel.h
  RequestOperationModel.cpp
  DevicesViewModel.h
  DevicesViewModel.cpp
  AppShellViewModel.h
  AppShellViewModel.cpp
  QmlRegistration.h
  QmlRegistration.cpp
)
add_library(xp60studio::presentation ALIAS xp60studio_presentation)
target_include_directories(xp60studio_presentation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(xp60studio_presentation
  PUBLIC
    Qt6::Core
    Qt6::Qml
    xp60studio::services
)
set_target_properties(xp60studio_presentation PROPERTIES AUTOMOC ON)
xp60studio_apply_warnings(xp60studio_presentation)
