# osx updater is built differently
# and linux should just use package managers

set (SRCS
    updater.cpp
    updater.rc
)
add_executable(updater ${SRCS})
target_link_libraries(updater getopt common)
