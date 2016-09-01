#include <getopt.h>
#include <iostream>
#include <windows.h>

const char* UPDATE_EXE = "Update.exe ";

void runCommand(const char* command) {
    STARTUPINFO startupInfo={sizeof(startupInfo)};
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION processInfo;

    char cmdArgs[50];
    strcpy(cmdArgs, UPDATE_EXE);
    strcat(cmdArgs, command);
    std::cout << "cmdArg " << cmdArgs << std::endl;
    if (!CreateProcess(TEXT("..\\Update.exe"), cmdArgs,
        NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startupInfo, &processInfo)
    )
    {
        std::cout << "CreateProcess failed (" << GetLastError() << ")" << std::endl;
        return;
    }
    WaitForSingleObject(&processInfo.hProcess, INFINITE);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
}

int main(int argc, char** argv) {
    int option_index = 0;
    int argc_w;
    auto argv_w = CommandLineToArgvW(GetCommandLineW(), &argc_w);

    if (argv_w == nullptr) {
        std::cout << "Failed to get command line arguments" << std::endl;
        return -1;
    }

    static struct option long_options[] = {
        { "squirrel-firstrun", no_argument, 0, 'f' },
        { "squirrel-install", required_argument, 0, 'i' },
        { "squirrel-obsolete", required_argument, 0, 'o' },
        { "squirrel-updated", required_argument, 0, 'u' },
        { "squirrel-uninstall", required_argument, 0, 'r' },
        { 0, 0, 0, 0 }
    };

    while (optind < argc) {
        char arg = getopt_long(argc, argv, "fi:o:u:r:", long_options, &option_index);
        if (arg != -1) {
            switch (arg) {
            case 'f':
                // do nothing on first run
                return 0;
            case 'i':
                runCommand("--createShortcut=citra-qt.exe");
                return 0;
            case 'o':
                // do nothing when obsoleted
                return 0;
            case 'u':
                // do nothing on update
                return 0;
            case 'r':
                runCommand("--removeShortcut=citra-qt.exe");
                return 0;
            }
        } else {
            optind++;
        }
    }
    return -2;
}
