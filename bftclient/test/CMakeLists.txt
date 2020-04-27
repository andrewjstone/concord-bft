find_package(GTest REQUIRED)
find_package(rapidcheck)

if(RAPIDCHECK_FOUND)

add_executable(bft_client_test bft_client_test.cpp $<TARGET_OBJECTS:logging_dev>)
add_test(bft_client_test bft_client_test)
target_include_directories(bft_client_test PRIVATE ${RAPIDCHECK_INCLUDE_DIRS} ../src)
target_link_libraries(bft_client_test PRIVATE
  GTest::Main
  GTest::GTest
  ${RAPIDCHECK_LIBRARIES}
  bftclient2
)

endif() # if (RAPIDCHECK_FOUND)