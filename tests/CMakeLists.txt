# Tests CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS Test)

# Test for Cue
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

# Test for CueList
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
