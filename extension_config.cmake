# This file is included by DuckDB's build system. It specifies which extension to load

# Extension from this repo
duckdb_extension_load(hostfs
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
)

# Any extra extensions that should be built
# e.g.: duckdb_extension_load(json)

# Since DuckDB's v2.0-cyanoptera duckdb_extension_load() only decides what is built;
# the shell and the test runner link an extension only when a config asks for it.
# Linked as before the change.
duckdb_extension_statically_link(hostfs)
