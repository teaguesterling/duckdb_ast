#include "language_handler.hpp"
#include "grammars.hpp"
#include "duckdb/common/exception.hpp"
#include <tree_sitter/api.h>
#include <cstring>
#include <memory>

// Tree-sitter function declarations
extern "C" {
    const TSLanguage *tree_sitter_python();
    const TSLanguage *tree_sitter_javascript();
    const TSLanguage *tree_sitter_cpp();
    // Temporarily disabled due to ABI compatibility issues:
    // const TSLanguage *tree_sitter_rust();
}

namespace duckdb {

//==============================================================================
// Base LanguageHandler implementation
//==============================================================================

string LanguageHandler::FindIdentifierChild(TSNode node, const string &content) const {
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* child_type = ts_node_type(child);
        if (strcmp(child_type, "identifier") == 0) {
            return ExtractNodeText(child, content);
        }
    }
    return "";
}

string LanguageHandler::ExtractNodeText(TSNode node, const string &content) const {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (start < content.size() && end <= content.size()) {
        return content.substr(start, end - start);
    }
    return "";
}

void LanguageHandler::SetParserLanguageWithValidation(TSParser* parser, const TSLanguage* language, const string &language_name) const {
    if (!language) {
        throw InvalidInputException("Tree-sitter language for " + language_name + " is NULL");
    }
    
    // Check ABI compatibility
    uint32_t language_version = ts_language_version(language);
    if (language_version > TREE_SITTER_LANGUAGE_VERSION) {
        throw InvalidInputException(
            language_name + " grammar ABI version " + std::to_string(language_version) + 
            " is newer than tree-sitter library version " + std::to_string(TREE_SITTER_LANGUAGE_VERSION) +
            ". Please update the tree-sitter library.");
    }
    
    if (language_version < TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) {
        throw InvalidInputException(
            language_name + " grammar ABI version " + std::to_string(language_version) + 
            " is too old for tree-sitter library (minimum version: " + 
            std::to_string(TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) +
            "). Please regenerate the grammar with a newer tree-sitter CLI.");
    }
    
    if (!ts_parser_set_language(parser, language)) {
        throw InvalidInputException("Failed to set " + language_name + " language for parser");
    }
}

//==============================================================================
// PythonLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> PythonLanguageHandler::type_mappings = {
    // Declarations
    {"function_definition", NormalizedTypes::FUNCTION_DECLARATION},
    {"async_function_definition", NormalizedTypes::FUNCTION_DECLARATION},
    {"class_definition", NormalizedTypes::CLASS_DECLARATION},
    {"assignment", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Expressions
    {"call", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"string", NormalizedTypes::LITERAL},
    {"integer", NormalizedTypes::LITERAL},
    {"float", NormalizedTypes::LITERAL},
    {"true", NormalizedTypes::LITERAL},
    {"false", NormalizedTypes::LITERAL},
    {"none", NormalizedTypes::LITERAL},
    {"binary_operator", NormalizedTypes::BINARY_EXPRESSION},
    
    // Control flow
    {"if_statement", NormalizedTypes::IF_STATEMENT},
    {"for_statement", NormalizedTypes::LOOP_STATEMENT},
    {"while_statement", NormalizedTypes::LOOP_STATEMENT},
    {"return_statement", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"comment", NormalizedTypes::COMMENT},
    {"import_statement", NormalizedTypes::IMPORT_STATEMENT},
    {"import_from_statement", NormalizedTypes::IMPORT_STATEMENT},
};

string PythonLanguageHandler::GetLanguageName() const {
    return "python";
}

vector<string> PythonLanguageHandler::GetAliases() const {
    return {"python", "py"};
}

TSParser* PythonLanguageHandler::CreateParser() const {
    TSParser *parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    const TSLanguage *ts_language = tree_sitter_python();
    SetParserLanguageWithValidation(parser, ts_language, "Python");
    
    return parser;
}

string PythonLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string PythonLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    // Extract names for declaration types
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION || 
        normalized == NormalizedTypes::CLASS_DECLARATION ||
        normalized == NormalizedTypes::METHOD_DECLARATION) {
        return FindIdentifierChild(node, content);
    } else if (normalized == NormalizedTypes::VARIABLE_REFERENCE) {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

//==============================================================================
// JavaScriptLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> JavaScriptLanguageHandler::type_mappings = {
    // Declarations
    {"function_declaration", NormalizedTypes::FUNCTION_DECLARATION},
    {"arrow_function", NormalizedTypes::FUNCTION_DECLARATION},
    {"function_expression", NormalizedTypes::FUNCTION_DECLARATION},
    {"class_declaration", NormalizedTypes::CLASS_DECLARATION},
    {"lexical_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"variable_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"const", NormalizedTypes::VARIABLE_DECLARATION},
    {"let", NormalizedTypes::VARIABLE_DECLARATION},
    {"var", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Method declarations
    {"method_definition", NormalizedTypes::METHOD_DECLARATION},
    
    // Expressions
    {"call_expression", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"string", NormalizedTypes::LITERAL},
    {"number", NormalizedTypes::LITERAL},
    {"true", NormalizedTypes::LITERAL},
    {"false", NormalizedTypes::LITERAL},
    {"null", NormalizedTypes::LITERAL},
    {"template_string", NormalizedTypes::LITERAL},
    {"binary_expression", NormalizedTypes::BINARY_EXPRESSION},
    
    // Control flow
    {"if_statement", NormalizedTypes::IF_STATEMENT},
    {"for_statement", NormalizedTypes::LOOP_STATEMENT},
    {"while_statement", NormalizedTypes::LOOP_STATEMENT},
    {"do_statement", NormalizedTypes::LOOP_STATEMENT},
    {"for_in_statement", NormalizedTypes::LOOP_STATEMENT},
    {"return_statement", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"comment", NormalizedTypes::COMMENT},
    {"import_statement", NormalizedTypes::IMPORT_STATEMENT},
    {"export_statement", NormalizedTypes::EXPORT_STATEMENT},
};

string JavaScriptLanguageHandler::GetLanguageName() const {
    return "javascript";
}

vector<string> JavaScriptLanguageHandler::GetAliases() const {
    return {"javascript", "js"};
}

TSParser* JavaScriptLanguageHandler::CreateParser() const {
    TSParser *parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    const TSLanguage *ts_language = tree_sitter_javascript();
    SetParserLanguageWithValidation(parser, ts_language, "JavaScript");
    
    return parser;
}

string JavaScriptLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string JavaScriptLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    // Extract names for declaration types
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION || 
        normalized == NormalizedTypes::CLASS_DECLARATION ||
        normalized == NormalizedTypes::METHOD_DECLARATION) {
        // For method_definition, look for property_identifier child
        if (node_type == "method_definition") {
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char* child_type = ts_node_type(child);
                if (strcmp(child_type, "property_identifier") == 0) {
                    return ExtractNodeText(child, content);
                }
            }
        }
        return FindIdentifierChild(node, content);
    } else if (normalized == NormalizedTypes::VARIABLE_REFERENCE) {
        return ExtractNodeText(node, content);
    } else if (node_type == "property_identifier") {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

//==============================================================================
// CPPLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> CPPLanguageHandler::type_mappings = {
    // Declarations
    {"function_definition", NormalizedTypes::FUNCTION_DECLARATION},
    {"class_specifier", NormalizedTypes::CLASS_DECLARATION},
    {"struct_specifier", NormalizedTypes::CLASS_DECLARATION},
    {"declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"field_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"parameter_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Method declarations
    {"function_definition", NormalizedTypes::METHOD_DECLARATION}, // When inside a class
    {"field_declaration", NormalizedTypes::METHOD_DECLARATION}, // Method declarations in class
    
    // Expressions
    {"call_expression", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"field_expression", NormalizedTypes::VARIABLE_REFERENCE},
    {"string_literal", NormalizedTypes::LITERAL},
    {"number_literal", NormalizedTypes::LITERAL},
    {"true", NormalizedTypes::LITERAL},
    {"false", NormalizedTypes::LITERAL},
    {"null", NormalizedTypes::LITERAL},
    {"nullptr", NormalizedTypes::LITERAL},
    {"binary_expression", NormalizedTypes::BINARY_EXPRESSION},
    
    // Control flow
    {"if_statement", NormalizedTypes::IF_STATEMENT},
    {"for_statement", NormalizedTypes::LOOP_STATEMENT},
    {"while_statement", NormalizedTypes::LOOP_STATEMENT},
    {"do_statement", NormalizedTypes::LOOP_STATEMENT},
    {"for_range_loop", NormalizedTypes::LOOP_STATEMENT},
    {"return_statement", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"comment", NormalizedTypes::COMMENT},
    {"preproc_include", NormalizedTypes::IMPORT_STATEMENT},
    {"using_declaration", NormalizedTypes::IMPORT_STATEMENT},
};

string CPPLanguageHandler::GetLanguageName() const {
    return "cpp";
}

vector<string> CPPLanguageHandler::GetAliases() const {
    return {"cpp", "c++", "cxx"};
}

TSParser* CPPLanguageHandler::CreateParser() const {
    TSParser *parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    const TSLanguage *ts_language = tree_sitter_cpp();
    SetParserLanguageWithValidation(parser, ts_language, "C++");
    
    return parser;
}

string CPPLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string CPPLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type = string(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    // Extract names for declaration types
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION || 
        normalized == NormalizedTypes::CLASS_DECLARATION ||
        normalized == NormalizedTypes::METHOD_DECLARATION) {
        
        // For function_definition, ALWAYS look for function_declarator first
        if (node_type == "function_definition") {
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                if (strcmp(ts_node_type(child), "function_declarator") == 0) {
                    // Look for identifier or field_identifier within declarator
                    uint32_t declarator_child_count = ts_node_child_count(child);
                    for (uint32_t j = 0; j < declarator_child_count; j++) {
                        TSNode declarator_child = ts_node_child(child, j);
                        const char* declarator_child_type = ts_node_type(declarator_child);
                        if (strcmp(declarator_child_type, "identifier") == 0 || 
                            strcmp(declarator_child_type, "field_identifier") == 0) {
                            return ExtractNodeText(declarator_child, content);
                        }
                    }
                }
            }
        } else {
            // For other types (class_specifier, etc), use original logic
            uint32_t child_count = ts_node_child_count(node);
            for (uint32_t i = 0; i < child_count; i++) {
                TSNode child = ts_node_child(node, i);
                const char* child_type = ts_node_type(child);
                if (strcmp(child_type, "identifier") == 0 || 
                   strcmp(child_type, "type_identifier") == 0) {
                    return ExtractNodeText(child, content);
                }
            }
        }
    } else if (normalized == NormalizedTypes::VARIABLE_REFERENCE) {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

//==============================================================================
// RustLanguageHandler implementation
//==============================================================================

const std::unordered_map<string, string> RustLanguageHandler::type_mappings = {
    // Declarations
    {"function_item", NormalizedTypes::FUNCTION_DECLARATION},
    {"struct_item", NormalizedTypes::CLASS_DECLARATION},
    {"enum_item", NormalizedTypes::CLASS_DECLARATION},
    {"trait_item", NormalizedTypes::CLASS_DECLARATION},
    {"impl_item", NormalizedTypes::CLASS_DECLARATION},
    {"mod_item", NormalizedTypes::CLASS_DECLARATION},
    {"let_declaration", NormalizedTypes::VARIABLE_DECLARATION},
    {"const_item", NormalizedTypes::VARIABLE_DECLARATION},
    {"static_item", NormalizedTypes::VARIABLE_DECLARATION},
    
    // Expressions  
    {"call_expression", NormalizedTypes::FUNCTION_CALL},
    {"method_call_expression", NormalizedTypes::FUNCTION_CALL},
    {"macro_invocation", NormalizedTypes::FUNCTION_CALL},
    {"identifier", NormalizedTypes::VARIABLE_REFERENCE},
    {"field_identifier", NormalizedTypes::VARIABLE_REFERENCE},
    
    // Literals
    {"integer_literal", NormalizedTypes::LITERAL},
    {"float_literal", NormalizedTypes::LITERAL},
    {"string_literal", NormalizedTypes::LITERAL},
    {"char_literal", NormalizedTypes::LITERAL},
    {"boolean_literal", NormalizedTypes::LITERAL},
    {"raw_string_literal", NormalizedTypes::LITERAL},
    
    // Control flow
    {"binary_expression", NormalizedTypes::BINARY_EXPRESSION},
    {"if_expression", NormalizedTypes::IF_STATEMENT},
    {"match_expression", NormalizedTypes::IF_STATEMENT},
    {"while_expression", NormalizedTypes::LOOP_STATEMENT},
    {"loop_expression", NormalizedTypes::LOOP_STATEMENT},
    {"for_expression", NormalizedTypes::LOOP_STATEMENT},
    {"return_expression", NormalizedTypes::RETURN_STATEMENT},
    
    // Other
    {"line_comment", NormalizedTypes::COMMENT},
    {"block_comment", NormalizedTypes::COMMENT},
    {"use_declaration", NormalizedTypes::IMPORT_STATEMENT},
    {"extern_crate_declaration", NormalizedTypes::IMPORT_STATEMENT},
};

string RustLanguageHandler::GetLanguageName() const {
    return "rust";
}

vector<string> RustLanguageHandler::GetAliases() const {
    return {"rust", "rs"};
}

TSParser* RustLanguageHandler::CreateParser() const {
    TSParser* parser = ts_parser_new();
    if (!parser) {
        throw InvalidInputException("Failed to create tree-sitter parser");
    }
    
    //const TSLanguage* ts_language = tree_sitter_rust();
    //SetParserLanguageWithValidation(parser, ts_language, "Rust");
    
    return parser;
}

string RustLanguageHandler::GetNormalizedType(const string &node_type) const {
    auto it = type_mappings.find(node_type);
    if (it != type_mappings.end()) {
        return it->second;
    }
    return node_type;
}

string RustLanguageHandler::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    string node_type(node_type_str);
    string normalized = GetNormalizedType(node_type);
    
    if (normalized == NormalizedTypes::FUNCTION_DECLARATION) {
        // For function_item, look for identifier child
        return FindIdentifierChild(node, content);
    } else if (normalized == NormalizedTypes::CLASS_DECLARATION) {
        // For struct_item, enum_item, trait_item, etc., look for type_identifier
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);
            if (strcmp(child_type, "type_identifier") == 0 || strcmp(child_type, "identifier") == 0) {
                return ExtractNodeText(child, content);
            }
        }
    } else if (normalized == NormalizedTypes::VARIABLE_REFERENCE) {
        return ExtractNodeText(node, content);
    }
    
    return "";
}

//==============================================================================
// LanguageHandlerRegistry implementation
//==============================================================================

LanguageHandlerRegistry::LanguageHandlerRegistry() {
    InitializeDefaultHandlers();
}

LanguageHandlerRegistry& LanguageHandlerRegistry::GetInstance() {
    static LanguageHandlerRegistry instance;
    return instance;
}

void LanguageHandlerRegistry::RegisterHandler(std::unique_ptr<LanguageHandler> handler) {
    // Validate ABI compatibility before registering
    ValidateLanguageABI(handler.get());
    
    string language = handler->GetLanguageName();
    vector<string> aliases = handler->GetAliases();
    
    // Register all aliases
    for (const auto &alias : aliases) {
        alias_to_language[alias] = language;
    }
    
    handlers[language] = std::move(handler);
}

const LanguageHandler* LanguageHandlerRegistry::GetHandler(const string &language) const {
    // First try direct lookup
    auto it = handlers.find(language);
    if (it != handlers.end()) {
        return it->second.get();
    }
    
    // Try alias lookup
    auto alias_it = alias_to_language.find(language);
    if (alias_it != alias_to_language.end()) {
        auto handler_it = handlers.find(alias_it->second);
        if (handler_it != handlers.end()) {
            return handler_it->second.get();
        }
    }
    
    return nullptr;
}

vector<string> LanguageHandlerRegistry::GetSupportedLanguages() const {
    vector<string> languages;
    for (const auto &pair : handlers) {
        languages.push_back(pair.first);
    }
    return languages;
}

void LanguageHandlerRegistry::ValidateLanguageABI(const LanguageHandler* handler) const {
    // Create a test parser to validate the language
    TSParser* parser = handler->CreateParser();
    if (!parser) {
        throw InvalidInputException("Language handler for '" + handler->GetLanguageName() + 
                                   "' failed to create parser");
    }
    
    // CreateParser should have already validated ABI compatibility
    // If we got here, the language is compatible
    ts_parser_delete(parser);
}

void LanguageHandlerRegistry::InitializeDefaultHandlers() {
    RegisterHandler(std::unique_ptr<LanguageHandler>(new PythonLanguageHandler()));
    RegisterHandler(std::unique_ptr<LanguageHandler>(new JavaScriptLanguageHandler()));
    RegisterHandler(std::unique_ptr<LanguageHandler>(new CPPLanguageHandler()));
    // Temporarily disabled due to ABI compatibility issues:
    // RegisterHandler(std::unique_ptr<LanguageHandler>(new RustLanguageHandler()));
}

} // namespace duckdb
