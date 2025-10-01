#define DUCKDB_EXTENSION_MAIN

#include "hostfs_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"

#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include <duckdb/function/macro_function.hpp>

#include "third_party/filesystem.hpp"

#include "table_functions/list_dir_recursive.hpp"
#include "table_functions/change_dir.hpp"

#include "scalar_functions/file_utils.hpp"
#include "scalar_functions/hostfs.hpp"

namespace fs = ghc::filesystem;

namespace duckdb {
    string PragmaChangeDir(ClientContext &context, const FunctionParameters &parameters) {
        return StringUtil::Format("SELECT * FROM cd(%s);",
                                  KeywordHelper::WriteQuoted(parameters.values[0].ToString(), '\''));
    }

    string PragmaPrintWorkingDirectory(ClientContext &context, const FunctionParameters &parameters) {
        return "SELECT pwd();";
    }

    string PragmaLSDefault(ClientContext &context, const FunctionParameters &parameters) {
        return "SELECT * FROM ls();";
    }

    string PragmaLSOneArg(ClientContext &context, const FunctionParameters &parameters) {
        return StringUtil::Format("SELECT * FROM ls(%s);",
                                  KeywordHelper::WriteQuoted(parameters.values[0].ToString(), '\''));
    }

    string PragmaLSRecursiveDefault(ClientContext &context, const FunctionParameters &parameters) {
        return "SELECT * FROM lsr();";
    }

    string PragmaLSRecursiveOneArg(ClientContext &context, const FunctionParameters &parameters) {
        return StringUtil::Format("SELECT * FROM lsr(%s);",
                                  KeywordHelper::WriteQuoted(parameters.values[0].ToString(), '\''));
    }

    string PragmaLSRecursiveTwoArgs(ClientContext &context, const FunctionParameters &parameters) {
        return StringUtil::Format("SELECT * FROM lsr(%s, %s);",
                                  KeywordHelper::WriteQuoted(parameters.values[0].ToString(), '\''),
                                  parameters.values[1].ToString());
    }

    static void LoadInternal(ExtensionLoader &loader) {

        auto &instance = loader.GetDatabaseInstance();
        // Register scalar functions
        auto hostfs_scalar_function = ScalarFunction("hostfs", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
                                                     HostfsScalarFun);
        loader.RegisterFunction(hostfs_scalar_function);

        auto hostfs_pwd_function = ScalarFunction("pwd", {}, LogicalType::VARCHAR, PrintWorkingDirectoryFun);
        loader.RegisterFunction(hostfs_pwd_function);




        auto hostfs_path_separator_function = ScalarFunction("path_separator", {}, LogicalType::VARCHAR,
                                                             GetPathSeparatorScalarFun);
        loader.RegisterFunction(hostfs_path_separator_function);

        // create split_path macro
        Connection conn(instance);
        // Execute the macro directly in the current context
        const auto macro =
                "CREATE MACRO path_split(path) AS list_filter(string_split(path, path_separator()), e -> len(e) > 0);";
        auto query_result = conn.Query(macro);


        auto hostfs_human_readable_size_function = ScalarFunction("hsize", {LogicalType::HUGEINT},
                                                                  LogicalType::VARCHAR, HumanReadableSizeScalarFun);
        loader.RegisterFunction(hostfs_human_readable_size_function);

        auto hostfs_is_file_function = ScalarFunction("is_file", {LogicalType::VARCHAR}, LogicalType::BOOLEAN,
                                                      IsFileScalarFun);
        loader.RegisterFunction(hostfs_is_file_function);

        auto hostfs_is_dir_function = ScalarFunction("is_dir", {LogicalType::VARCHAR}, LogicalType::BOOLEAN,
                                                     IsDirectoryScalarFun);
        loader.RegisterFunction(hostfs_is_dir_function);

        auto hostfs_get_filename_function = ScalarFunction("file_name", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
                                                           GetFilenameScalarFun);
        loader.RegisterFunction(hostfs_get_filename_function);


        auto hostfs_get_file_extension_function = ScalarFunction("file_extension", {LogicalType::VARCHAR},
                                                                 LogicalType::VARCHAR,
                                                                 GetFileExtensionScalarFun);
        loader.RegisterFunction(hostfs_get_file_extension_function);


        auto hostfs_get_file_size_function = ScalarFunction("file_size", {LogicalType::VARCHAR}, LogicalType::UBIGINT,
                                                            GetFileSizeScalarFun);
        loader.RegisterFunction(hostfs_get_file_size_function);


        auto hostfs_get_path_absolute_function = ScalarFunction("absolute_path", {LogicalType::VARCHAR},
                                                                LogicalType::VARCHAR,
                                                                GetPathAbsoluteScalarFun);
        loader.RegisterFunction(hostfs_get_path_absolute_function);


        auto hostfs_get_path_exists_function = ScalarFunction("path_exists", {LogicalType::VARCHAR},
                                                              LogicalType::BOOLEAN,
                                                              GetPathExistsScalarFun);
        loader.RegisterFunction(hostfs_get_path_exists_function);

        auto hostfs_get_path_type_function = ScalarFunction("path_type", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
                                                            GetPathTypeScalarFun);
        loader.RegisterFunction(hostfs_get_path_type_function);

        auto hostfs_last_modified_function = ScalarFunction("file_last_modified", {LogicalType::VARCHAR},
                                                            LogicalType::TIMESTAMP,
                                                            GetFileLastModifiedScalarFun);

        loader.RegisterFunction(hostfs_last_modified_function);

        // Register table functions
        TableFunctionSet list_dir_set("ls");

        TableFunction list_dir_default({}, ListDirRecursiveFun, ListDirBind, ListDirRecursiveState::Init);
        list_dir_set.AddFunction(list_dir_default);

        TableFunction list_dir_one_arg({LogicalType::VARCHAR}, ListDirRecursiveFun, ListDirBind,
                                       ListDirRecursiveState::Init);
        list_dir_set.AddFunction(list_dir_one_arg);

        TableFunction list_dir_two_arg({LogicalType::VARCHAR, LogicalType::BOOLEAN}, ListDirRecursiveFun, ListDirBind,
                                       ListDirRecursiveState::Init);
        list_dir_set.AddFunction(list_dir_two_arg);

        loader.RegisterFunction(list_dir_set);


        TableFunctionSet list_dir_recursive_set("lsr");

        TableFunction list_dir_recursive_default({}, ListDirRecursiveFun, ListDirRecursiveBind,
                                                 ListDirRecursiveState::Init);
        list_dir_recursive_set.AddFunction(list_dir_recursive_default);

        TableFunction list_dir_recursive_one_arg({LogicalType::VARCHAR}, ListDirRecursiveFun, ListDirRecursiveBind,
                                                 ListDirRecursiveState::Init);
        list_dir_recursive_set.AddFunction(list_dir_recursive_one_arg);

        TableFunction list_dir_recursive_two_args({LogicalType::VARCHAR, LogicalType::INTEGER}, ListDirRecursiveFun,
                                                  ListDirRecursiveBind, ListDirRecursiveState::Init);
        list_dir_recursive_set.AddFunction(list_dir_recursive_two_args);

        TableFunction list_dir_recursive_tree_args({LogicalType::VARCHAR, LogicalType::INTEGER, LogicalType::BOOLEAN},
                                                   ListDirRecursiveFun, ListDirRecursiveBind,
                                                   ListDirRecursiveState::Init);
        list_dir_recursive_set.AddFunction(list_dir_recursive_tree_args);

        loader.RegisterFunction(list_dir_recursive_set);


        TableFunction change_dir("cd", {LogicalType::VARCHAR}, ChangeDirFun, ChangeDirBind, ChangeDirState::Init);
        loader.RegisterFunction(change_dir);

        // Pragma functions

        PragmaFunction cd = PragmaFunction::PragmaCall("cd", PragmaChangeDir, {LogicalType::VARCHAR});
        loader.RegisterFunction(cd);

        PragmaFunction pwd = PragmaFunction::PragmaCall("pwd", PragmaPrintWorkingDirectory, {});
        loader.RegisterFunction(pwd);


        PragmaFunctionSet ls_set("ls");
        PragmaFunction ls_default = PragmaFunction::PragmaCall("ls", PragmaLSDefault, {});
        PragmaFunction ls_one_arg = PragmaFunction::PragmaCall("ls", PragmaLSOneArg, {LogicalType::VARCHAR});

        ls_set.AddFunction(ls_default);
        ls_set.AddFunction(ls_one_arg);

        loader.RegisterFunction(ls_set);


        PragmaFunctionSet lsr_set("lsr");

        PragmaFunction lsr_default = PragmaFunction::PragmaCall("lsr", PragmaLSRecursiveDefault, {});
        PragmaFunction lsr_one_arg = PragmaFunction::PragmaCall("lsr", PragmaLSRecursiveOneArg, {LogicalType::VARCHAR});
        PragmaFunction lsr_two_args = PragmaFunction::PragmaCall("lsr", PragmaLSRecursiveTwoArgs,
                                                                 {LogicalType::VARCHAR, LogicalType::INTEGER});

        lsr_set.AddFunction(lsr_default);
        lsr_set.AddFunction(lsr_one_arg);
        lsr_set.AddFunction(lsr_two_args);

        loader.RegisterFunction(lsr_set);
    }

    void HostfsExtension::Load(ExtensionLoader &loader) {
        LoadInternal(loader);
    }
    std::string HostfsExtension::Name() {
        return "quack";
    }

    std::string HostfsExtension::Version() const {
#ifdef EXT_VERSION_QUACK
        return EXT_VERSION_QUACK;
#else
        return "";
#endif
    }
} // namespace duckdb

extern "C" {

    DUCKDB_CPP_EXTENSION_ENTRY(hostfs, loader) {
        duckdb::LoadInternal(loader);
    }
}


#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
