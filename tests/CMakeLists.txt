find_package(Qt6 REQUIRED COMPONENTS Test)

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

add_executable(test_liveeditsession
    test_liveeditsession.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/LiveEditSession.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PreviewLayer.cpp
)
target_link_libraries(test_liveeditsession PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_liveeditsession PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME LiveEditSessionTest COMMAND test_liveeditsession)

add_executable(test_fadeconflictresolver
    test_fadeconflictresolver.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Cue.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueList.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CueValidator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FadeConflictResolver.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PlaybackGuard.cpp
    ${CMAKE_SOURCE_DIR}/src/core/LiveEditSession.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PreviewLayer.cpp
)
target_link_libraries(test_fadeconflictresolver PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_fadeconflictresolver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME FadeConflictResolverTest COMMAND test_fadeconflictresolver)

add_executable(test_previewlayer
    test_previewlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PreviewLayer.cpp
)
target_link_libraries(test_previewlayer PRIVATE
    Qt6::Core
    Qt6::Test
)
target_include_directories(test_previewlayer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME PreviewLayerTest COMMAND test_previewlayer)
