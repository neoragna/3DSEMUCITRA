set(SRCS
            tests.cpp
            core/arm/arm_test_common.cpp
            core/arm/dyncom/arm_dyncom_tests.cpp
            )

set(HEADERS
            core/arm/arm_test_common.h
            )

create_directory_groups(${SRCS} ${HEADERS})

include_directories(../../externals/catch/single_include/)

add_executable(tests ${SRCS} ${HEADERS})
target_link_libraries(tests core video_core audio_core common)
target_link_libraries(tests ${PLATFORM_LIBRARIES})

add_test(NAME tests COMMAND $<TARGET_FILE:tests>)
