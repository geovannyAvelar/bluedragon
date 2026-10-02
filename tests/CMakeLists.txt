# Runs against a simulated device (m711_open_custom); needs no hardware.
add_executable(test_bluedragon test_bluedragon.c)
target_link_libraries(test_bluedragon PRIVATE bluedragon::bluedragon)
add_test(NAME bluedragon COMMAND test_bluedragon)
