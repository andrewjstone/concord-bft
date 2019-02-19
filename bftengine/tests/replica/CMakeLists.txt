add_executable(replica_tests
    cluster.cpp
    replica_test.cpp
    communication.cpp)

target_link_libraries(replica_tests gtest_main)
target_link_libraries(replica_tests corebft)
