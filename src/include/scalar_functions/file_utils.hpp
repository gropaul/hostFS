namespace duckdb {
    std::string HumanReadableSize(uint64_t size) {
        constexpr uint64_t KB = 1024;
        constexpr uint64_t MB = KB * 1024;
        constexpr uint64_t GB = MB * 1024;

        std::ostringstream oss;
        if (size >= GB) {
            oss << std::fixed << std::setprecision(2) << (double) size / GB << " GB";
        } else if (size >= MB) {
            oss << std::fixed << std::setprecision(2) << (double) size / MB << " MB";
        } else if (size >= KB) {
            oss << std::fixed << std::setprecision(2) << (double) size / KB << " KB";
        } else {
            oss << size << " B";
        }
        return oss.str();
    }


    static void HumanReadableSizeScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &size_vector = input.data[0];
        UnaryExecutor::Execute<hugeint_t, string_t>(
            size_vector, result, input.size(),
            [&](hugeint_t size) {
                return StringVector::AddString(result, HumanReadableSize((uint64_t) size).c_str());
            });
    }

    static void PrintWorkingDirectoryFun(DataChunk &input, ExpressionState &state, Vector &result) {
        // get the current working directory
        auto cwd = fs::current_path();
        auto val = Value(cwd.string());
        result.Reference(val);
    }

    static void GetPathSeparatorScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        // get the preferred separator
        auto path_seperator = fs::path::preferred_separator;
        auto path_seperator_str = std::string(1, path_seperator);
        auto val = Value(path_seperator_str);
        result.Reference(val);
    }

    static void IsFileScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];
        UnaryExecutor::ExecuteWithNulls<string_t, bool>(
            path_vector, result, input.size(),
            [&](string_t path, ValidityMask &mask, idx_t idx) {
                if (!fs::exists(path.GetString())) {
                    mask.SetInvalid(idx);
                    return false;
                }
                return fs::is_regular_file(path.GetString());
            });
    }

    static void IsDirectoryScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];
        UnaryExecutor::ExecuteWithNulls<string_t, bool>(
            path_vector, result, input.size(),
            [&](string_t path, ValidityMask &mask, idx_t idx) {
                if (!fs::exists(path.GetString())) {
                    mask.SetInvalid(idx);
                    return false;
                }
                return fs::is_directory(path.GetString());
            });
    }

    static void GetFilenameScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];
        UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
            path_vector, result, input.size(),
            [&](string_t path, ValidityMask &mask, idx_t idx) {
                if (!fs::exists(path.GetString())) {
                    mask.SetInvalid(idx);
                    const string_t empty_string("");
                    return empty_string;
                }
                return StringVector::AddString(result, fs::path(path.GetString()).filename().string());
            });
    }

    static void GetFileExtensionScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];
        UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
            path_vector, result, input.size(),
            [&](string_t path, ValidityMask &mask, idx_t idx) {
                if (!fs::exists(path.GetString())) {
                    mask.SetInvalid(idx);
                    const string_t empty_string("");
                    return empty_string;
                }
                std::string path_str = path.GetString();
                if (fs::is_directory(path_str)) {
                    return StringVector::AddString(result, "");
                } else {
                    return StringVector::AddString(result, fs::path(path_str).extension().string());
                }
            });
    }

    static void GetFileSizeScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];
        UnaryExecutor::ExecuteWithNulls<string_t, uint64_t>(
            path_vector, result, input.size(),
            [&](string_t path, ValidityMask &mask, idx_t idx) {
                // check if the path exists
                if (!fs::exists(path.GetString())) {
                    mask.SetInvalid(idx);
                    return static_cast<uint64_t>(0);
                }

                // return 0 if dir
                if (fs::is_directory(path.GetString())) {
                    return static_cast<uint64_t>(0);
                }

                // return 0 if symlink
                if (fs::is_symlink(path.GetString())) {
                    return static_cast<uint64_t>(0);
                }

                uintmax_t file_size_tmp = fs::file_size(path.GetString());
                auto file_size = static_cast<uint64_t>(file_size_tmp);
                return file_size;
            });
    }

    static void GetPathAbsoluteScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];
        UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
            path_vector, result, input.size(),
            [&](string_t path, ValidityMask &mask, idx_t idx) {
                if (!fs::exists(path.GetString())) {
                    mask.SetInvalid(idx);
                    const string_t empty_string("");
                    return empty_string;
                }

                // get the absolute path without any '/./' or '/../' components
                auto abs_path = fs::absolute(path.GetString());

                // only canonical if the path exists
                if (fs::exists(abs_path)) {
                    auto canonical_path = fs::canonical(abs_path);
                    return StringVector::AddString(result, canonical_path.string());
                } else {
                    return StringVector::AddString(result, abs_path.string());
                }
            });
    }

    static void GetPathExistsScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];
        UnaryExecutor::Execute<string_t, bool>(
            path_vector, result, input.size(),
            [&](string_t path) {
                return fs::exists(path.GetString());
            });
    }

    static void GetPathTypeScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];
        UnaryExecutor::ExecuteWithNulls<string_t, string_t>(
            path_vector, result, input.size(),
            [&](string_t path, ValidityMask &mask, idx_t idx) {
                // check if the path exists
                if (!fs::exists(path.GetString())) {
                    mask.SetInvalid(idx);
                    const string_t empty_string("");
                    return empty_string;
                }

                std::string path_str = path.GetString(); // Convert string_t to std::string
                if (fs::is_directory(path_str)) {
                    return StringVector::AddString(result, "directory");
                } else if (fs::is_regular_file(path_str)) {
                    return StringVector::AddString(result, "file");
                } else if (fs::is_symlink(path_str)) {
                    return StringVector::AddString(result, "symlink");
                } else {
                    return StringVector::AddString(result, "other");
                }
            });
    }

    // date related functions

    static void GetFileLastModifiedScalarFun(DataChunk &input, ExpressionState &state, Vector &result) {
        auto &path_vector = input.data[0];

        UnaryExecutor::ExecuteWithNulls<string_t, timestamp_t>(
            path_vector, result, input.size(),
            [&](string_t path, ValidityMask &mask, idx_t idx) {
                auto resolved_path = path.GetString();

                // if the path does not exist, return NULL
                if (!fs::exists(resolved_path)) {
                    mask.SetInvalid(idx);
                    return Timestamp::FromEpochSeconds(0);
                }

                // Get the last modified time of the (possibly resolved) path
                auto last_modified_time_point = fs::last_write_time(resolved_path);
                auto last_modified_int = std::chrono::duration_cast<std::chrono::seconds>(
                    last_modified_time_point.time_since_epoch()).count();
                timestamp_t timestamp = Timestamp::FromEpochSeconds(last_modified_int);
                return timestamp;
            });
    }
}
