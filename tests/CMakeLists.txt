find_package(Qt6 REQUIRED COMPONENTS Test Network Widgets)

add_executable(test_cue
    test_cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
target_link_libraries(test_cue PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_cue PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME CueTest COMMAND test_cue)

add_executable(test_cuelist
    test_cuelist.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
)
target_link_libraries(test_cuelist PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_cuelist PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME CueListTest COMMAND test_cuelist)

add_executable(test_show
    test_show.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Show.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SpareBackup.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FxLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Ensemble.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueZero.cpp
)
target_link_libraries(test_show PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_show PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME ShowTest COMMAND test_show)

add_executable(test_actor_profiles
    test_actor_profiles.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Show.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SpareBackup.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FxLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Ensemble.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueZero.cpp
)
target_link_libraries(test_actor_profiles PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_actor_profiles PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME ActorProfilesTest COMMAND test_actor_profiles)

add_executable(test_playback_profiles
    test_playback_profiles.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueValidator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackGuard.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FadeEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/LoopbackProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_playback_profiles PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_playback_profiles PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME PlaybackProfilesTest COMMAND test_playback_profiles)

add_executable(test_playback_dca_assign
    test_playback_dca_assign.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueValidator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackGuard.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FadeEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/LoopbackProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_playback_dca_assign PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_playback_dca_assign PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME PlaybackDcaAssignTest COMMAND test_playback_dca_assign)

add_executable(test_fade_engine
    test_fade_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FadeEngine.cpp
)
target_link_libraries(test_fade_engine PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_fade_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME FadeEngineTest COMMAND test_fade_engine)

add_executable(test_msc test_msc.cpp)
target_link_libraries(test_msc PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_msc PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME MscTest COMMAND test_msc)

add_executable(test_cast_text_parse test_cast_text_parse.cpp)
target_link_libraries(test_cast_text_parse PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_cast_text_parse PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CastTextParseTest COMMAND test_cast_text_parse)

add_executable(test_dca_grid_nav test_dca_grid_nav.cpp)
target_link_libraries(test_dca_grid_nav PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_dca_grid_nav PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME DcaGridNavTest COMMAND test_dca_grid_nav)

add_executable(test_cue_table_model
    test_cue_table_model.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/CueTableModel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/theme/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Ensemble.cpp
)
target_link_libraries(test_cue_table_model PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
target_include_directories(test_cue_table_model PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CueTableModelTest COMMAND test_cue_table_model)

add_executable(test_mixer_capabilities
    test_mixer_capabilities.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_mixer_capabilities PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_mixer_capabilities PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME MixerCapabilitiesTest COMMAND test_mixer_capabilities)

add_executable(test_dca_override_strip
    test_dca_override_strip.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DCAOverrideStrip.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/theme/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
target_link_libraries(test_dca_override_strip PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
target_include_directories(test_dca_override_strip PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME DCAOverrideStripTest COMMAND test_dca_override_strip)

add_executable(test_midi_mute
    test_midi_mute.cpp
    ${CMAKE_SOURCE_DIR}/src/midi/MidiControlMapping.cpp
)
target_link_libraries(test_midi_mute PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_midi_mute PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME MidiMuteTest COMMAND test_midi_mute)

add_executable(test_osc_remote
    test_osc_remote.cpp
    ${CMAKE_SOURCE_DIR}/src/app/OscRemoteServer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueValidator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackGuard.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FadeEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_osc_remote PRIVATE Qt6::Core Qt6::Test openmix_liblo)
target_include_directories(test_osc_remote PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME OscRemoteTest COMMAND test_osc_remote)

add_executable(test_osc_transport
    test_osc_transport.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/transport/OscTransport.cpp
)
target_link_libraries(test_osc_transport PRIVATE Qt6::Core Qt6::Network Qt6::Test openmix_liblo)
target_include_directories(test_osc_transport PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME OscTransportTest COMMAND test_osc_transport)

add_executable(test_qlab_client
    test_qlab_client.cpp
    ${CMAKE_SOURCE_DIR}/src/app/QLabClient.cpp
)
target_link_libraries(test_qlab_client PRIVATE Qt6::Core Qt6::Test openmix_liblo)
target_include_directories(test_qlab_client PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME QLabClientTest COMMAND test_qlab_client)

add_executable(test_reaper_client
    test_reaper_client.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ReaperClient.cpp
)
target_link_libraries(test_reaper_client PRIVATE Qt6::Core Qt6::Test openmix_liblo)
target_include_directories(test_reaper_client PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ReaperClientTest COMMAND test_reaper_client)

add_executable(test_cueplayer_client
    test_cueplayer_client.cpp
    ${CMAKE_SOURCE_DIR}/src/app/CuePlayerClient.cpp
)
target_link_libraries(test_cueplayer_client PRIVATE Qt6::Core Qt6::Test openmix_liblo)
target_include_directories(test_cueplayer_client PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CuePlayerClientTest COMMAND test_cueplayer_client)

add_executable(test_undo_commands
    test_undo_commands.cpp
    ${CMAKE_SOURCE_DIR}/src/core/UndoCommands.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
target_link_libraries(test_undo_commands PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
)
target_include_directories(test_undo_commands PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME UndoCommandsTest COMMAND test_undo_commands)

add_executable(test_scs_client
    test_scs_client.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ScsClient.cpp
)
target_link_libraries(test_scs_client PRIVATE Qt6::Core Qt6::Test openmix_liblo)
target_include_directories(test_scs_client PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ScsClientTest COMMAND test_scs_client)

add_executable(test_delete_playing_cue
    test_delete_playing_cue.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/CueTableModel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/CueFilterProxyModel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/theme/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
)
target_link_libraries(test_delete_playing_cue PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
target_include_directories(test_delete_playing_cue PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME DeletePlayingCueTest COMMAND test_delete_playing_cue)

add_executable(test_digico_protocol
    test_digico_protocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/digico/DiGiCoProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/transport/TcpTransport.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
target_link_libraries(test_digico_protocol PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
target_include_directories(test_digico_protocol PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
if(WIN32)
    target_compile_definitions(test_digico_protocol PRIVATE NOMINMAX)
endif()
add_test(NAME DiGiCoProtocolTest COMMAND test_digico_protocol)

add_executable(test_allenheath_parsing
    test_allenheath_parsing.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/allenheath/AllenHeathTcpProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/transport/TcpTransport.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
target_link_libraries(test_allenheath_parsing PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
target_include_directories(test_allenheath_parsing PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
if(WIN32)
    target_compile_definitions(test_allenheath_parsing PRIVATE NOMINMAX)
endif()
add_test(NAME AllenHeathParsingTest COMMAND test_allenheath_parsing)

add_executable(test_allenheath_midi
    test_allenheath_midi.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/allenheath/AllenHeathMidiProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/transport/TcpTransport.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
target_link_libraries(test_allenheath_midi PRIVATE Qt6::Core Qt6::Network Qt6::Test)
target_include_directories(test_allenheath_midi PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME AllenHeathMidiTest COMMAND test_allenheath_midi)

add_executable(test_yamaha_scp
    test_yamaha_scp.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/yamaha/YamahaProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/yamaha/YamahaCLProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/yamaha/YamahaQLProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/yamaha/YamahaTFProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/yamaha/YamahaDM7Protocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/transport/TcpTransport.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
# Yamaha now speaks SCP over TCP (TcpTransport) and no longer needs liblo.
target_link_libraries(test_yamaha_scp PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
target_include_directories(test_yamaha_scp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
if(WIN32)
    target_compile_definitions(test_yamaha_scp PRIVATE NOMINMAX)
endif()
add_test(NAME YamahaScpTest COMMAND test_yamaha_scp)

# --- positions, cue zero, timecode, channel monitoring ---

add_executable(test_position
    test_position.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
)
target_link_libraries(test_position PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_position PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME PositionTest COMMAND test_position)

add_executable(test_mtc test_mtc.cpp)
target_link_libraries(test_mtc PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_mtc PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME MtcTest COMMAND test_mtc)

add_executable(test_timecode_trigger
    test_timecode_trigger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/TimecodeTrigger.cpp
)
target_link_libraries(test_timecode_trigger PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_timecode_trigger PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME TimecodeTriggerTest COMMAND test_timecode_trigger)

add_executable(test_channel_monitor
    test_channel_monitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ChannelMonitor.cpp
)
target_link_libraries(test_channel_monitor PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_channel_monitor PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ChannelMonitorTest COMMAND test_channel_monitor)

add_executable(test_cuezero
    test_cuezero.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueZero.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/LoopbackProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_cuezero PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_cuezero PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME CueZeroTest COMMAND test_cuezero)

# --- ensembles + scribble strips ---

add_executable(test_ensembles
    test_ensembles.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Ensemble.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Show.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SpareBackup.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FxLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueZero.cpp
)
target_link_libraries(test_ensembles PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_ensembles PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME EnsemblesTest COMMAND test_ensembles)

add_executable(test_scribble
    test_scribble.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ScribbleController.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_scribble PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_scribble PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ScribbleTest COMMAND test_scribble)

# --- fx mutes, snippets, gangs, color/skip, check mode ---

add_executable(test_show_control
    test_show_control.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Show.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SpareBackup.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FxLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueValidator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackGuard.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FadeEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueZero.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Ensemble.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_show_control PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_show_control PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME ShowControlTest COMMAND test_show_control)

add_executable(test_tmix_import
    test_tmix_import.cpp
    ${CMAKE_SOURCE_DIR}/src/io/TmixImporter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Show.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SpareBackup.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FxLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueZero.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Ensemble.cpp
)
target_link_libraries(test_tmix_import PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)
target_include_directories(test_tmix_import PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME TmixImportTest COMMAND test_tmix_import)

add_executable(test_channel_utilization
    test_channel_utilization.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ChannelUtilization.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Show.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SpareBackup.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FxLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueZero.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Ensemble.cpp
)
target_link_libraries(test_channel_utilization PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_channel_utilization PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME ChannelUtilizationTest COMMAND test_channel_utilization)

add_executable(test_spare_backup
    test_spare_backup.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SpareBackup.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FxLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
)
target_link_libraries(test_spare_backup PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_spare_backup PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME SpareBackupTest COMMAND test_spare_backup)

add_executable(test_fx_library
    test_fx_library.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FxLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
)
target_link_libraries(test_fx_library PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_fx_library PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME FxLibraryTest COMMAND test_fx_library)

add_executable(test_console_name_cache
    test_console_name_cache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConsoleNameCache.cpp
)
target_link_libraries(test_console_name_cache PRIVATE Qt6::Core Qt6::Test)
target_include_directories(test_console_name_cache PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ConsoleNameCacheTest COMMAND test_console_name_cache)

add_executable(test_midi_config_dialog
    test_midi_config_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/MidiConfigDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/WindowSizing.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/theme/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/midi/MidiInputManager.cpp
    ${CMAKE_SOURCE_DIR}/src/midi/MidiControlMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackGuard.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueValidator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Actor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ActorProfileLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FadeEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Position.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_midi_config_dialog PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Test
    RtMidi
)
target_include_directories(test_midi_config_dialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME MidiConfigDialogTest COMMAND test_midi_config_dialog)

add_executable(test_popout_window
    test_popout_window.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PopOutWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/WindowSizing.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/theme/Theme.cpp
)
target_link_libraries(test_popout_window PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
target_include_directories(test_popout_window PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME PopOutWindowTest COMMAND test_popout_window)

add_executable(test_bubble_button
    test_bubble_button.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/BubbleButton.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/BubbleBar.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/theme/Theme.cpp
)
target_link_libraries(test_bubble_button PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
target_include_directories(test_bubble_button PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME BubbleButtonTest COMMAND test_bubble_button)

add_executable(test_console_discovery
    test_console_discovery.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/discovery/ConsoleDiscoveryService.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/discovery/DiscoveredConsole.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/discovery/probes/BehringerX32ProbeStrategy.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/discovery/probes/BehringerWingProbeStrategy.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/discovery/probes/YamahaYsdpProbeStrategy.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/discovery/probes/AllenHeathProbeStrategy.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
)
target_link_libraries(test_console_discovery PRIVATE Qt6::Core Qt6::Network Qt6::Test)
target_include_directories(test_console_discovery PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ConsoleDiscoveryTest COMMAND test_console_discovery)
