find_package(Qt6 REQUIRED COMPONENTS Test Network Widgets)

add_executable(test_cue
    test_cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
target_link_libraries(test_show PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_show PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME ShowTest COMMAND test_show)

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

add_executable(test_yamaha_osc
    test_yamaha_osc.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/yamaha/YamahaProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/MixerCapabilities.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DCAMapping.cpp
)
target_link_libraries(test_yamaha_osc PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
if(WIN32)
    target_include_directories(test_yamaha_osc PRIVATE ${LIBLO_INCLUDE_DIRS})
    target_link_libraries(test_yamaha_osc PRIVATE ${LIBLO_LIBRARIES})
else()
    target_link_libraries(test_yamaha_osc PRIVATE PkgConfig::LIBLO)
endif()
target_include_directories(test_yamaha_osc PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
if(WIN32)
    target_compile_definitions(test_yamaha_osc PRIVATE NOMINMAX)
endif()
add_test(NAME YamahaOscTest COMMAND test_yamaha_osc)
