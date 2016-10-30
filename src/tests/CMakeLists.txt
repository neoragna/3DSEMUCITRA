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
target_link_libraries(tests ${SDL2_LIBRARY} ${OPENGL_gl_LIBRARY} inih glad)
if (MSVC)
    target_link_libraries(tests getopt)
endif()
target_link_libraries(tests ${PLATFORM_LIBRARIES} Threads::Threads)

add_test(NAME tests COMMAND $<TARGET_FILE:tests>)
