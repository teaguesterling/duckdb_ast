#define DUCKDB_EXTENSION_MAIN

#include "duckdb_ast_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/main/extension_helper.hpp"
#include "read_ast_objects_hybrid.hpp"
#include "ast_sql_macros.hpp"
#include "short_names_function.hpp"
#include "parse_ast_function.hpp"

namespace duckdb {

// Forward declaration of registration functions
void RegisterReadASTFunction(DatabaseInstance &instance);
void RegisterReadASTObjectsHybridFunction(DatabaseInstance &instance);
void RegisterASTSQLMacros(DatabaseInstance &instance);
void RegisterDuckDBASTShortNamesFunction(DatabaseInstance &instance);
// Temporarily disabled:
// void RegisterASTObjectsFunction(DatabaseInstance &instance);
// void RegisterASTHelperFunctions(DatabaseInstance &instance);

static void LoadInternal(DatabaseInstance &instance) {
	// Register the read_ast table function
	RegisterReadASTFunction(instance);
	
	// Register the hybrid read_ast_objects table function
	RegisterReadASTObjectsHybridFunction(instance);
	
	// Register the parse_ast scalar function
	ParseASTFunction::Register(instance);
	
	// Register SQL macros for natural AST querying
	// Note: These require json_each which is available in DuckDB 1.3+
	ExtensionHelper::AutoLoadExtension(instance, "json");
	RegisterASTSQLMacros(instance);
	
	// Register the short names function and pragma (doesn't depend on JSON)
	RegisterDuckDBASTShortNamesFunction(instance);
	
	// Check if user wants short names auto-loaded
	try {
		Connection con(instance);
		auto result = con.Query("SELECT current_setting('duckdb_ast_short_names')");
		if (result->RowCount() > 0) {
			auto value = result->GetValue(0, 0);
			if (!value.IsNull() && value.ToString() == "true") {
				// Auto-load short names by directly executing the chain methods
				// This avoids calling the pragma and any output it might produce
				auto chain_methods_result = con.Query("SELECT duckdb_ast_load_embedded_sql('02b_chain_methods.sql')");
				if (!chain_methods_result->HasError()) {
					// Execute the chain methods SQL
					auto sql_content = chain_methods_result->GetValue(0, 0).ToString();
					con.Query(sql_content);
				}
			}
		}
	} catch (...) {
		// Variable doesn't exist or error - that's fine, skip auto-load
	}
	
	// TODO: Re-enable once we fix the issues
	// Register AST helper functions  
	// RegisterASTHelperFunctions(instance);
}

void DuckdbAstExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}

string DuckdbAstExtension::Name() {
	return "duckdb_ast";
}

string DuckdbAstExtension::Version() const {
#ifdef EXT_VERSION_DUCKDB_AST
	return EXT_VERSION_DUCKDB_AST;
#else
	return "0.1.0";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void duckdb_ast_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::DuckdbAstExtension>();
}

DUCKDB_EXTENSION_API const char *duckdb_ast_version() {
	return duckdb::DuckDB::LibraryVersion();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif