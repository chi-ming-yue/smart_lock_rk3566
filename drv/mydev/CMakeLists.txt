add_custom_target(mydev
    COMMAND ${CMAKE_COMMAND} -E make_directory ${KMOD_OUTPUT_DIR}
    COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR} all
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/build/mydev.ko ${KMOD_OUTPUT_DIR}/mydev.ko
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/build/key_test ${KMOD_OUTPUT_DIR}/key_test
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/build/key_debug_test ${KMOD_OUTPUT_DIR}/key_debug_test
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/build/key_once ${KMOD_OUTPUT_DIR}/key_once
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/build/sensor_test ${KMOD_OUTPUT_DIR}/sensor_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building mydev kernel module and board-side tests"
)
