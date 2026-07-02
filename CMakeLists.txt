cmake_minimum_required(VERSION 3.16)

project(OpenMix
    VERSION 0.1.0
    DESCRIPTION "Stage mixing software for live theatre that lets engineers program, automate, and run cues seamlessly across 30+ digital mixing consoles"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    if(MSVC)
        foreach(config DEBUG RELWITHDEBINFO)
            string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
            string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_${config} "${CMAKE_C_FLAGS_${config}}")
        endforeach()
    endif()
endif()

# find Qt6
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Svg
    Sql
)

# icons
include(FetchContent)
FetchContent_Declare(
    qlementine-icons
    GIT_REPOSITORY "https://github.com/oclero/qlementine-icons.git"
    GIT_TAG "e2ad8c109d245f11b3fcb2c39f8b26aeee87ff50"
)
FetchContent_MakeAvailable(qlementine-icons)

# find liblo for OSC; expose one platform-agnostic target (openmix_liblo) so app
# and tests link the same name on every OS
add_library(openmix_liblo INTERFACE)
if(WIN32)
    find_path(LIBLO_INCLUDE_DIRS lo/lo.h REQUIRED)
    find_library(LIBLO_LIBRARIES NAMES lo liblo REQUIRED)
    target_include_directories(openmix_liblo INTERFACE ${LIBLO_INCLUDE_DIRS})
    target_link_libraries(openmix_liblo INTERFACE ${LIBLO_LIBRARIES})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBLO REQUIRED IMPORTED_TARGET liblo)
    target_link_libraries(openmix_liblo INTERFACE PkgConfig::LIBLO)
endif()

# RtMidi
add_library(RtMidi STATIC libs/RtMidi/RtMidi.cpp libs/RtMidi/RtMidi.h)
target_include_directories(RtMidi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libs/RtMidi)
if(WIN32)
    target_compile_definitions(RtMidi PUBLIC __WINDOWS_MM__)
    target_compile_options(RtMidi PRIVATE /FIwindows.h)
    target_link_libraries(RtMidi PUBLIC winmm)
elseif(APPLE)
    target_compile_definitions(RtMidi PUBLIC __MACOSX_CORE__)
    find_library(COREMIDI_LIBRARY CoreMIDI REQUIRED)
    find_library(COREAUDIO_LIBRARY CoreAudio REQUIRED)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
    target_link_libraries(RtMidi PUBLIC
        ${COREMIDI_LIBRARY}
        ${COREAUDIO_LIBRARY}
        ${COREFOUNDATION_LIBRARY}
    )
else()
    target_compile_definitions(RtMidi PUBLIC __LINUX_ALSA__)
    find_package(ALSA REQUIRED)
    target_link_libraries(RtMidi PUBLIC ALSA::ALSA)
endif()

# source files
set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/OscRemoteServer.cpp
    src/app/QLabClient.cpp
    src/app/ReaperClient.cpp
    src/app/CuePlayerClient.cpp
    src/app/ScsClient.cpp
    src/app/UpdateChecker.cpp
    src/app/AutoUpdater.cpp
    src/core/Cue.cpp
    src/core/CueList.cpp
    src/core/Show.cpp
    src/core/SpareBackup.cpp
    src/core/FxLibrary.cpp
    src/core/ConsoleNameCache.cpp
    src/core/PlaybackEngine.cpp
    src/core/UndoCommands.cpp
    src/core/CueValidator.cpp
    src/core/PlaybackGuard.cpp
    src/core/PlaybackLogger.cpp
    src/core/DryRunEngine.cpp
    src/core/ShortcutManager.cpp
    src/core/DCAMapping.cpp
    src/core/ActorProfile.cpp
    src/core/Actor.cpp
    src/core/ActorProfileLibrary.cpp
    src/core/Ensemble.cpp
    src/core/FadeEngine.cpp
    src/core/Position.cpp
    src/core/CueZero.cpp
    src/core/ChannelUtilization.cpp
    src/core/TimecodeTrigger.cpp
    src/core/ChannelMonitor.cpp
    src/core/ScribbleController.cpp
    src/core/AppLogger.cpp
    src/core/ConnectionLogBridge.cpp
    src/ui/MainWindow.cpp
    src/ui/ChannelStripPanel.cpp
    src/ui/CueListView.cpp
    src/ui/CueEditor.cpp
    src/ui/CollapsibleSection.cpp
    src/ui/ConnectionPanel.cpp
    src/ui/CueTableModel.cpp
    src/ui/CueFilterProxyModel.cpp
    src/ui/CueFilterBar.cpp
    src/ui/CueItemDelegates.cpp
    src/ui/ConnectionStateWidget.cpp
    src/ui/MixerFeedbackPanel.cpp
    src/ui/DCAWidget.cpp
    src/ui/CueConfidenceIndicator.cpp
    src/ui/MacroPreviewWidget.cpp
    src/ui/DCAMappingPanel.cpp
    src/ui/ActorSetupPanel.cpp
    src/ui/EnsemblePanel.cpp
    src/ui/RemoteControlDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/PositionPanel.cpp
    src/ui/TimecodePanel.cpp
    src/ui/CueZeroDialog.cpp
    src/ui/ChannelUtilizationDialog.cpp
    src/ui/AllocateSpareDialog.cpp
    src/ui/ActiveCueInfoPanel.cpp
    src/ui/FxSetupDialog.cpp
    src/ui/WelcomeDialog.cpp
    src/ui/HelpDialog.cpp
    src/ui/MarkerNotesDialog.cpp
    src/ui/PopOutWindow.cpp
    src/ui/BubbleButton.cpp
    src/ui/BubbleBar.cpp
    src/ui/theme/Theme.cpp

    src/protocol/MixerProtocol.cpp
    src/protocol/MixerCapabilities.cpp
    src/protocol/ProtocolFactory.cpp
    src/protocol/LoopbackProtocol.cpp
    src/protocol/transport/OscTransport.cpp
    src/protocol/transport/TcpTransport.cpp

    src/protocol/behringer/X32Protocol.cpp
    src/protocol/behringer/WingProtocol.cpp

    src/protocol/allenheath/AllenHeathMidiProtocol.cpp
    src/protocol/allenheath/SQProtocol.cpp
    src/protocol/allenheath/GLDProtocol.cpp
    src/protocol/allenheath/AllenHeathTcpProtocol.cpp
    src/protocol/allenheath/AvantisProtocol.cpp
    src/protocol/allenheath/DLiveProtocol.cpp

    src/protocol/yamaha/YamahaProtocol.cpp
    src/protocol/yamaha/YamahaTFProtocol.cpp
    src/protocol/yamaha/YamahaQLProtocol.cpp
    src/protocol/yamaha/YamahaCLProtocol.cpp
    src/protocol/yamaha/YamahaDM7Protocol.cpp

    src/protocol/digico/DiGiCoProtocol.cpp

    src/protocol/discovery/DiscoveredConsole.cpp
    src/protocol/discovery/ConsoleDiscoveryService.cpp
    src/protocol/discovery/probes/BehringerX32ProbeStrategy.cpp
    src/protocol/discovery/probes/BehringerWingProbeStrategy.cpp
    src/protocol/discovery/probes/YamahaOscProbeStrategy.cpp

    src/ui/ConsoleDiscoveryWidget.cpp

    src/io/ProjectFile.cpp
    src/io/AutosaveManager.cpp
    src/io/CrashRecovery.cpp
    src/io/TmixImporter.cpp

    src/midi/MidiControlMapping.cpp
    src/midi/MidiInputManager.cpp
    src/ui/MidiConfigDialog.cpp
    src/ui/KeyboardShortcutsDialog.cpp
    src/ui/LogViewerDialog.cpp
    src/ui/LogListModel.cpp
    src/ui/LogItemDelegate.cpp
)

# header files
set(HEADERS
    src/app/Application.h
    src/app/OscRemoteServer.h
    src/app/QLabClient.h
    src/app/ReaperClient.h
    src/app/CuePlayerClient.h
    src/app/ScsClient.h
    src/app/UpdateChecker.h
    src/app/AutoUpdater.h
    src/core/Cue.h
    src/core/CueList.h
    src/core/Show.h
    src/core/SpareBackup.h
    src/core/FxLibrary.h
    src/core/ConsoleNameCache.h
    src/core/PlaybackEngine.h
    src/core/UndoCommands.h
    src/core/CueValidator.h
    src/core/PlaybackGuard.h
    src/core/PlaybackLogger.h
    src/core/DryRunEngine.h
    src/core/ShortcutManager.h
    src/core/DCAMapping.h
    src/core/ActorProfile.h
    src/core/Actor.h
    src/core/ActorProfileLibrary.h
    src/core/Ensemble.h
    src/core/FadeCurve.h
    src/core/FadeEngine.h
    src/core/Position.h
    src/core/CueZero.h
    src/core/ChannelUtilization.h
    src/core/TimecodeTrigger.h
    src/core/ChannelMonitor.h
    src/core/ScribbleController.h
    src/core/AppLogger.h
    src/core/ConnectionLogBridge.h
    src/ui/MainWindow.h
    src/ui/ChannelStripPanel.h
    src/ui/CueListView.h
    src/ui/CueEditor.h
    src/ui/CollapsibleSection.h
    src/ui/ConnectionPanel.h
    src/ui/CueTableModel.h
    src/ui/CueFilterProxyModel.h
    src/ui/CueFilterBar.h
    src/ui/CueItemDelegates.h
    src/ui/ConnectionStateWidget.h
    src/ui/MixerFeedbackPanel.h
    src/ui/DCAWidget.h
    src/ui/CueConfidenceIndicator.h
    src/ui/MacroPreviewWidget.h
    src/ui/DCAMappingPanel.h
    src/ui/ActorSetupPanel.h
    src/ui/ActorGroupIo.h
    src/ui/EnsemblePanel.h
    src/ui/RemoteControlDialog.h
    src/ui/SettingsDialog.h
    src/ui/PositionPanel.h
    src/ui/TimecodePanel.h
    src/ui/CueZeroDialog.h
    src/ui/ChannelUtilizationDialog.h
    src/ui/AllocateSpareDialog.h
    src/ui/ActiveCueInfoPanel.h
    src/ui/FxSetupDialog.h
    src/ui/WelcomeDialog.h
    src/ui/HelpDialog.h
    src/ui/MarkerNotesDialog.h
    src/ui/PopOutWindow.h
    src/ui/BubbleButton.h
    src/ui/BubbleBar.h
    src/ui/theme/Theme.h
    src/ui/theme/Icons.h

    src/protocol/MixerProtocol.h
    src/protocol/MixerCapabilities.h
    src/protocol/ProtocolFactory.h
    src/protocol/transport/OscTransport.h
    src/protocol/transport/TcpTransport.h

    src/protocol/behringer/X32Protocol.h
    src/protocol/behringer/WingProtocol.h

    src/protocol/allenheath/AllenHeathMidiProtocol.h
    src/protocol/allenheath/SQProtocol.h
    src/protocol/allenheath/GLDProtocol.h
    src/protocol/allenheath/AllenHeathTcpProtocol.h
    src/protocol/allenheath/AvantisProtocol.h
    src/protocol/allenheath/DLiveProtocol.h

    src/protocol/yamaha/YamahaProtocol.h
    src/protocol/yamaha/YamahaTFProtocol.h
    src/protocol/yamaha/YamahaQLProtocol.h
    src/protocol/yamaha/YamahaCLProtocol.h
    src/protocol/yamaha/YamahaDM7Protocol.h

    src/protocol/digico/DiGiCoProtocol.h

    src/protocol/discovery/DiscoveredConsole.h
    src/protocol/discovery/OscProbeStrategy.h
    src/protocol/discovery/ConsoleDiscoveryService.h
    src/protocol/discovery/probes/BehringerX32ProbeStrategy.h
    src/protocol/discovery/probes/BehringerWingProbeStrategy.h
    src/protocol/discovery/probes/YamahaOscProbeStrategy.h

    src/ui/ConsoleDiscoveryWidget.h

    src/io/ProjectFile.h
    src/io/AutosaveManager.h
    src/io/CrashRecovery.h
    src/io/TmixImporter.h

    src/midi/MidiControlMapping.h
    src/midi/MidiInputManager.h
    src/midi/MscParser.h
    src/midi/MtcParser.h
    src/ui/MidiConfigDialog.h
    src/ui/KeyboardShortcutsDialog.h
    src/ui/LogViewerDialog.h
    src/ui/LogListModel.h
    src/ui/LogItemDelegate.h
)

# Qt resources
set(RESOURCES
    resources/resources.qrc
)

# create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

# include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::Sql
    qlementine-icons
)

# liblo
target_link_libraries(${PROJECT_NAME} PRIVATE openmix_liblo)

target_link_libraries(${PROJECT_NAME} PRIVATE RtMidi)

# --- auto-update frameworks (Windows = WinSparkle, macOS = Sparkle) ----------
# Linux has no silent-update framework; AutoUpdater falls back to the notify-and
# -link checker there, so nothing extra is linked.
if(WIN32)
    FetchContent_Declare(winsparkle
        URL https://github.com/vslavik/winsparkle/releases/download/v0.8.1/WinSparkle-0.8.1.zip)
    FetchContent_MakeAvailable(winsparkle)
    # the 0.8.1 zip layout (nesting, x64/Release/ subdir) is fragile; glob for the
    # header, x64 lib, and x64 dll so the exact structure can't break the build
    file(GLOB_RECURSE _ws_hdrs "${winsparkle_SOURCE_DIR}/*winsparkle.h")
    file(GLOB_RECURSE _ws_libs "${winsparkle_SOURCE_DIR}/*WinSparkle.lib")
    file(GLOB_RECURSE _ws_dlls "${winsparkle_SOURCE_DIR}/*WinSparkle.dll")
    list(GET _ws_hdrs 0 _ws_hdr)
    get_filename_component(WINSPARKLE_INCLUDE ${_ws_hdr} DIRECTORY)
    foreach(f ${_ws_libs})
        if(f MATCHES "x64")
            set(WINSPARKLE_LIB ${f})
        endif()
    endforeach()
    foreach(f ${_ws_dlls})
        if(f MATCHES "x64")
            set(WINSPARKLE_DLL ${f})
        endif()
    endforeach()
    if(NOT WINSPARKLE_INCLUDE OR NOT WINSPARKLE_LIB OR NOT WINSPARKLE_DLL)
        message(FATAL_ERROR "WinSparkle header/x64 lib/dll not found under ${winsparkle_SOURCE_DIR}")
    endif()
    target_include_directories(${PROJECT_NAME} PRIVATE ${WINSPARKLE_INCLUDE})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${WINSPARKLE_LIB})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${WINSPARKLE_DLL} $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    install(FILES ${WINSPARKLE_DLL} DESTINATION ${CMAKE_INSTALL_BINDIR})
elseif(APPLE)
    FetchContent_Declare(sparkle
        URL https://github.com/sparkle-project/Sparkle/releases/download/2.6.4/Sparkle-2.6.4.tar.xz)
    FetchContent_MakeAvailable(sparkle)
    target_sources(${PROJECT_NAME} PRIVATE src/app/AutoUpdaterSparkle.mm)
    target_compile_options(${PROJECT_NAME} PRIVATE -F${sparkle_SOURCE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE "-F${sparkle_SOURCE_DIR}" "-framework Sparkle")
    configure_file(${CMAKE_SOURCE_DIR}/packaging/Info.plist.in
        ${CMAKE_BINARY_DIR}/Info.plist @ONLY)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/Info.plist
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks")
    install(DIRECTORY ${sparkle_SOURCE_DIR}/Sparkle.framework
        DESTINATION ${PROJECT_NAME}.app/Contents/Frameworks)
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE
    <QObject>
    <QString>
    <QStringList>
    <QVector>
    <QList>
    <QMap>
    <QHash>
    <QSet>
    <QTimer>
    <QVariant>
    <QJsonObject>
    <QJsonArray>
    <functional>
    <optional>
)

# platform-specific settings
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
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

# bundle the Qt runtime into the install tree so the Windows (NSIS) and macOS
# (DragNDrop) installers are self-contained. Linux relies on packaged Qt deps
# (CPACK_DEBIAN_PACKAGE_SHLIBDEPS below).
if(WIN32 OR APPLE)
    qt_generate_deploy_app_script(
        TARGET ${PROJECT_NAME}
        OUTPUT_SCRIPT openmix_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${openmix_deploy_script})
endif()

# desktop entry + icon (Linux/BSD)
if(UNIX AND NOT APPLE)
    install(FILES packaging/openmix.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/openmix.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

# packaging (cpack): deb/rpm/tarball on Linux, NSIS on Windows, drag-drop dmg on macOS
set(CPACK_PACKAGE_NAME "OpenMix")
set(CPACK_PACKAGE_VENDOR "OpenMix")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenMix")
set(CPACK_PACKAGE_CONTACT "OpenMix")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "OpenMix")
    set(CPACK_NSIS_DISPLAY_NAME "OpenMix")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
endif()

include(CPack)

# enable testing
enable_testing()
add_subdirectory(tests)
