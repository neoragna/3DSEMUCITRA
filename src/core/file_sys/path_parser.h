// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "core/file_sys/archive_backend.h"

namespace FileSys {

class PathParser {
    std::vector<std::string> path_sequence;
    bool is_valid;

public:
    PathParser(const Path& path);

    bool IsValid() {
        return is_valid;
    }

    bool IsOutOfBounds();

    bool IsRootDirectory() {
        return path_sequence.empty();
    }

    enum HostStatus {
        InvalidMountPoint,
        PathNotFound,   // "a/b/c" when "a" doesn't exist
        FileInPath,     // "a/b/c" when "a" is a file
        FileFound,      // "a/b/c" when "c" is a file
        DirectoryFound, // "a/b/c" when "c" is a directory
        NotFound        // "a/b/c" when "a/b/" exists but "c" doesn't exist
    };

    HostStatus GetHostStatus(const std::string& mount_point);
    std::string BuildHostPath(const std::string& mount_point);
};

} // namespace FileSys
