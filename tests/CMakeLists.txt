# Runs against a simulated device (m711_open_custom); needs no hardware.
add_executable(test_m711 test_m711.c)
target_link_libraries(test_m711 PRIVATE m711::m711)
add_test(NAME m711 COMMAND test_m711)
