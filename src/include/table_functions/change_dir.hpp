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

namespace fs = ghc::filesystem;

namespace duckdb {
    struct ChangeDirFunctionData final : FunctionData {
        string path;

        explicit ChangeDirFunctionData(string path) : path(path) {}

        unique_ptr<FunctionData> Copy() const override {
            return make_uniq<ChangeDirFunctionData>(path);
        }

        bool Equals(const FunctionData &other) const override {
            return path == other.Cast<ChangeDirFunctionData>().path;
        }
    };

    struct ChangeDirState final : GlobalTableFunctionState {
        ChangeDirState() : run(false) {};
        std::atomic_bool run;

        static unique_ptr<GlobalTableFunctionState> Init(ClientContext &context, TableFunctionInitInput &input) {
            return make_uniq<ChangeDirState>();
        }
    };


    static void ChangeDirFun(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {

        // get the args
        auto &function_data = data_p.bind_data->Cast<ChangeDirFunctionData>();
        auto path = function_data.path;

        auto &state = data_p.global_state->Cast<ChangeDirState>();
        if (state.run.exchange(true)) {
            return;
        }

        // change the current working directory
        fs::current_path(path);

        // get current absolute path
        auto cwd = fs::current_path();

        // set the output
        output.SetValue(0, 0, Value(cwd.string()));
        output.SetValue(1, 0, Value(true));

        output.SetCardinality(1);
    }

    static unique_ptr<FunctionData> ChangeDirBind(ClientContext &context, TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types, vector<string> &names) {
        names.emplace_back("current_directory");
        return_types.emplace_back(LogicalType::VARCHAR);

        names.emplace_back("success");
        return_types.emplace_back(LogicalType::BOOLEAN);

        // if no arguments are provided, use the current working directory
        string directory = ".";
        if (input.inputs.size() == 1) {
            directory = input.inputs[0].GetValue<string>();
        } else {
            // throw error if not one argument is provided
            throw NotImplementedException("ChangeDir requires exactly one argument");
        }

        auto data = make_uniq<ChangeDirFunctionData>(directory);
        return std::move(data);
    }
}