if (WIN_CREATE_INSTALLER)
    # download NuGet 3.4.4 commandline to install squirrel with
    if (NOT EXISTS "${CMAKE_BINARY_DIR}/nuget")
        message(STATUS "Downloading binaries for nuget...")
        file(DOWNLOAD https://dist.nuget.org/win-x86-commandline/v3.4.4/NuGet.exe
            "${CMAKE_BINARY_DIR}/nuget/nuget.exe" SHOW_PROGRESS)
    endif()
    # download squirrel as well
    if (NOT EXISTS "${CMAKE_BINARY_DIR}/squirrel")
        message(STATUS "Downloading binaries for nuget...")
        file(DOWNLOAD https://github.com/Squirrel/Squirrel.Windows/releases/download/1.4.4/Squirrel.Windows-1.4.4.zip
            "${CMAKE_BINARY_DIR}/squirrel/squirrel.zip" SHOW_PROGRESS)
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${CMAKE_BINARY_DIR}/squirrel/squirrel.zip"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/squirrel")
    endif()
    # fill in the necessary details into the nuspec file template
    set(VERSION_BUILD_NUMBER $ENV{APPVEYOR_BUILD_NUBMER})
    configure_file(${CMAKE_SOURCE_DIR}/citra.nuspec ${CMAKE_BINARY_DIR}/citra.nuspec)

    # set the source files to work with
    set (SRCS
        updater.cpp
        updater.rc
    )
    add_executable(updater ${SRCS})
    target_link_libraries(updater getopt)
    if (ENABLE_QT)
        add_dependencies(updater citra-qt)
    endif()
    if (ENABLE_SDL2)
        add_dependencies(updater citra)
    endif()
endif()
