# Add sources to executable/library
target_sources(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/syscall.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/sysmem.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/st7789.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/sccb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/ov5640.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/dcmi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/cv.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/startup_stm32h743xx.S"
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/stm32h743xg_flash.ld" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stm32h743xg_flash.ld")
