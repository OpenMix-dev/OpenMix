cmake_minimum_required(VERSION 3.16)

project(OpenMix
    VERSION 0.1.0
    DESCRIPTION "Open Source Theatre Sound Mixing Control"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

# find liblo for OSC
if(WIN32)
    find_path(LIBLO_INCLUDE_DIRS lo/lo.h REQUIRED)
    find_library(LIBLO_LIBRARIES NAMES lo liblo REQUIRED)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBLO REQUIRED IMPORTED_TARGET liblo)
endif()

# source files
set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/core/Cue.cpp
    src/core/CueList.cpp
    src/core/Show.cpp
    src/core/PlaybackEngine.cpp
    src/core/UndoCommands.cpp
    src/core/CueValidator.cpp
    src/core/PlaybackGuard.cpp
    src/core/PlaybackLogger.cpp
    src/core/FadeConflictResolver.cpp
    src/core/DryRunEngine.cpp
    src/core/ShortcutManager.cpp
    src/core/OperationMode.cpp
    src/ui/MainWindow.cpp
    src/ui/CueListView.cpp
    src/ui/CueEditor.cpp
    src/ui/ConnectionPanel.cpp
    src/ui/CueTableModel.cpp
    src/ui/CueFilterProxyModel.cpp
    src/ui/CueFilterBar.cpp
    src/ui/ConnectionStateWidget.cpp
    src/ui/TimelineView.cpp
    src/ui/TimelineItem.cpp
    src/ui/MixerFeedbackPanel.cpp
    src/ui/DCAWidget.cpp
    src/ui/CueConfidenceIndicator.cpp
    src/ui/MacroPreviewWidget.cpp
    src/ui/ShortcutConfigDialog.cpp
    src/ui/RecoveryDialog.cpp
    src/ui/LimitationsDialog.cpp

    src/protocol/MixerProtocol.cpp
    src/protocol/MixerCapabilities.cpp
    src/protocol/ProtocolFactory.cpp
    src/protocol/transport/OscTransport.cpp
    src/protocol/transport/TcpTransport.cpp
    # Behringer / Midas protocols
    src/protocol/behringer/X32Protocol.cpp
    src/protocol/behringer/WingProtocol.cpp

    src/protocol/allenheath/AllenHeathMidiProtocol.cpp
    src/protocol/allenheath/SQProtocol.cpp
    src/protocol/allenheath/GLDProtocol.cpp
    src/protocol/allenheath/AllenHeathTcpProtocol.cpp
    src/protocol/allenheath/AvantisProtocol.cpp
    src/protocol/allenheath/DLiveProtocol.cpp

    src/protocol/yamaha/YamahaTcpProtocol.cpp
    src/protocol/yamaha/YamahaTFProtocol.cpp
    src/protocol/yamaha/YamahaQLProtocol.cpp
    src/protocol/yamaha/YamahaCLProtocol.cpp
    src/protocol/yamaha/YamahaDM7Protocol.cpp

    src/io/ProjectFile.cpp
    src/io/AutosaveManager.cpp
    src/io/CrashRecovery.cpp
    src/io/ProjectMigration.cpp
)

# header files
set(HEADERS
    src/app/Application.h
    src/core/Cue.h
    src/core/CueList.h
    src/core/Show.h
    src/core/PlaybackEngine.h
    src/core/UndoCommands.h
    src/core/CueValidator.h
    src/core/PlaybackGuard.h
    src/core/PlaybackLogger.h
    src/core/FadeConflictResolver.h
    src/core/DryRunEngine.h
    src/core/ShortcutManager.h
    src/core/OperationMode.h
    src/ui/MainWindow.h
    src/ui/CueListView.h
    src/ui/CueEditor.h
    src/ui/ConnectionPanel.h
    src/ui/CueTableModel.h
    src/ui/CueFilterProxyModel.h
    src/ui/CueFilterBar.h
    src/ui/ConnectionStateWidget.h
    src/ui/TimelineView.h
    src/ui/TimelineItem.h
    src/ui/MixerFeedbackPanel.h
    src/ui/DCAWidget.h
    src/ui/CueConfidenceIndicator.h
    src/ui/MacroPreviewWidget.h
    src/ui/ShortcutConfigDialog.h
    src/ui/RecoveryDialog.h
    src/ui/LimitationsDialog.h

    src/protocol/MixerProtocol.h
    src/protocol/MixerCapabilities.h
    src/protocol/ProtocolFactory.h
    src/protocol/transport/OscTransport.h
    src/protocol/transport/TcpTransport.h
    # Behringer / Midas protocols
    src/protocol/behringer/X32Protocol.h
    src/protocol/behringer/WingProtocol.h

    src/protocol/allenheath/AllenHeathMidiProtocol.h
    src/protocol/allenheath/SQProtocol.h
    src/protocol/allenheath/GLDProtocol.h
    src/protocol/allenheath/AllenHeathTcpProtocol.h
    src/protocol/allenheath/AvantisProtocol.h
    src/protocol/allenheath/DLiveProtocol.h

    src/protocol/yamaha/YamahaTcpProtocol.h
    src/protocol/yamaha/YamahaTFProtocol.h
    src/protocol/yamaha/YamahaQLProtocol.h
    src/protocol/yamaha/YamahaCLProtocol.h
    src/protocol/yamaha/YamahaDM7Protocol.h

    src/io/ProjectFile.h
    src/io/AutosaveManager.h
    src/io/CrashRecovery.h
    src/io/ProjectMigration.h
)

# create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# add liblo for OSC support
if(WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBLO_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBLO_LIBRARIES})
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::LIBLO)
endif()

# platform-specific settings
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "OpenMix"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

# install rules
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# enable testing
enable_testing()
add_subdirectory(tests)
