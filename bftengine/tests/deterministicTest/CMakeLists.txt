add_executable(deterministic_test
    app_state.cpp
    communication.cpp
    replica_test.cpp)

target_link_libraries(deterministic_test gtest_main rapidcheck_gtest corebft)
