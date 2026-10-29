add_executable(bridge-tests bridge_tests.cpp
    "${lidar_handle_core_SOURCE_DIR}/tests/serial/FakeSerialPort.cpp"
    "${lidar_handle_core_SOURCE_DIR}/tests/pcap/FakePcap.cpp")
target_include_directories(bridge-tests PRIVATE
    "${lidar_handle_core_SOURCE_DIR}/tests/serial"
    "${lidar_handle_core_SOURCE_DIR}/tests/pcap")
target_link_libraries(bridge-tests PRIVATE bridge_protocols)
target_compile_options(bridge-tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME forwarding COMMAND bridge-tests)
set_tests_properties(forwarding PROPERTIES TIMEOUT 30)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME cli COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cli_tests.py" "$<TARGET_FILE:bridge>")
set_tests_properties(cli PROPERTIES TIMEOUT 20)
add_executable(bridge-process-fixture ../src/main.cpp process_fixture.cpp
    "${lidar_handle_core_SOURCE_DIR}/tests/serial/FakeSerialPort.cpp"
    "${lidar_handle_core_SOURCE_DIR}/tests/pcap/FakePcap.cpp")
target_include_directories(bridge-process-fixture PRIVATE
    "${lidar_handle_core_SOURCE_DIR}/tests/serial"
    "${lidar_handle_core_SOURCE_DIR}/tests/pcap")
target_link_libraries(bridge-process-fixture PRIVATE bridge_protocols)
target_compile_options(bridge-process-fixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME process-lifecycle COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/process_tests.py" "$<TARGET_FILE:bridge-process-fixture>")
set_tests_properties(process-lifecycle PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
