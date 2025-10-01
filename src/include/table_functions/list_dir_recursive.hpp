#pragma once


#include "hostfs_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

#include <chrono>      // for std::chrono::duration_cast
#include <ctime>  // for std::time_t

#include <iomanip>    // for std::fixed and std::setprecision
#include <utility>

namespace fs = ghc::filesystem;

namespace duckdb {


    struct ListDirRecursiveFunctionData final : FunctionData {

        string directory;
        int depth; // -1 for infinite depth, 0 for no recursion
        bool skip_permission_denied;

        explicit ListDirRecursiveFunctionData(string directory, int depth, bool skip_permission_denied) : directory(
                std::move(directory)), depth(depth), skip_permission_denied(skip_permission_denied) {}

        unique_ptr<FunctionData> Copy() const override {
            return make_uniq<ListDirRecursiveFunctionData>(directory, depth, skip_permission_denied);
        }

        bool Equals(const FunctionData &other) const override {
            return directory == other.Cast<ListDirRecursiveFunctionData>().directory &&
                   depth == other.Cast<ListDirRecursiveFunctionData>().depth;

        }
    };

    struct ListDirRecursiveState final : GlobalTableFunctionState {
        ListDirRecursiveState() : gathered_paths(false), paths(), current_idx(0) {}

        std::atomic_bool gathered_paths;
        std::vector<std::string> paths;
        idx_t current_idx = 0;

        static unique_ptr<GlobalTableFunctionState> Init(ClientContext &context, TableFunctionInitInput &input) {
            return make_uniq<ListDirRecursiveState>();
        }
    };

    static unique_ptr<FunctionData> ListDirRecursiveBind(ClientContext &context, TableFunctionBindInput &input,
                                                         vector<LogicalType> &return_types, vector<string> &names) {
        names.emplace_back("path");
        return_types.emplace_back(LogicalType::VARCHAR);

        // if no arguments are provided, use the current working directory
        string directory = ".";
        int depth = -1;
        bool skip_permission_denied = true;
        if (input.inputs.size() == 1) {
            directory = input.inputs[0].GetValue<string>();
        } else if (input.inputs.size() == 2) {
            directory = input.inputs[0].GetValue<string>();
            depth = input.inputs[1].GetValue<int>();
        } else if (input.inputs.size() == 3) {
            directory = input.inputs[0].GetValue<string>();
            depth = input.inputs[1].GetValue<int>();
            skip_permission_denied = input.inputs[2].GetValue<bool>();
        }

        auto data = make_uniq<ListDirRecursiveFunctionData>(directory, depth, skip_permission_denied);
        return std::move(data);
    }

    static unique_ptr<FunctionData> ListDirBind(ClientContext &context, TableFunctionBindInput &input,
                                                vector<LogicalType> &return_types, vector<string> &names) {
        names.emplace_back("path");
        return_types.emplace_back(LogicalType::VARCHAR);

        // if no arguments are provided, use the current working directory
        string directory = ".";
        bool skip_permission_denied = true;
        if (input.inputs.size() == 1) {
            directory = input.inputs[0].GetValue<string>();
        } else if (input.inputs.size() == 2) {
            directory = input.inputs[0].GetValue<string>();
            skip_permission_denied = input.inputs[1].GetValue<bool>();
        }

        auto data = make_uniq<ListDirRecursiveFunctionData>(directory, 0, skip_permission_denied);
        return std::move(data);
    }

    void ListDirectoryRecursive(const std::string &directory, std::vector<std::string> &paths, int max_depth, bool skip_permission_denied) {
        try {
            // Check if the directory exists and is valid
            if (!fs::exists(directory) ){
                throw IOException("Directory does not exist: " + directory);
            } else if (!fs::is_directory(directory)) {
                throw IOException("Path is not a directory: " + directory);
            }

            auto options = fs::directory_options::none;
            if (skip_permission_denied) {
                options = fs::directory_options::skip_permission_denied;
            }

            fs::recursive_directory_iterator it(directory, options), end;
            while (it != end) {
                // Skip entries that exceed the max depth if max_depth is set
                if (max_depth != -1 && it.depth() > max_depth) {
                    it.disable_recursion_pending(); // Prevent descending further into this directory
                } else {
                    // Add the current path to the vector
                    paths.push_back(it->path().string());
                }

                ++it; // Move to the next entry
            }
        } catch (const std::exception &ex) {
            throw IOException(ex.what());
        }
    }

    static void ListDirRecursiveFun(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {

        // get the args
        auto &function_data = data_p.bind_data->Cast<ListDirRecursiveFunctionData>();
        auto directory = function_data.directory;
        auto depth = function_data.depth;
        auto skip_permission_denied = function_data.skip_permission_denied;

        auto &state = data_p.global_state->Cast<ListDirRecursiveState>();

        // gather the paths if not already done
        if (!state.gathered_paths.exchange(true)) {
            // first call, gather the paths
            ListDirectoryRecursive(directory, state.paths, depth, skip_permission_denied);
        }

        // we can max return STANDARD_VECTOR_SIZE paths at a time
        idx_t total_count = state.paths.size();
        idx_t remaining_count = total_count - state.current_idx;
        idx_t count = std::min<idx_t>(STANDARD_VECTOR_SIZE, remaining_count);

        // set up the chunk
        output.SetCardinality(count);

        // set the paths in the chunk
        for (idx_t index = 0; index < count; index++) {
            string path_str = state.paths[state.current_idx + index];
            output.data[0].SetValue(index, Value(path_str));
        }

        // update the current index
        state.current_idx += count;


    }

}