#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
    const TSLanguage *tree_sitter_c();
}

namespace duckdb {

//==============================================================================
// C Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, value_strat, flags) \
    {raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat, ExtractionStrategy::value_strat, flags)},

const unordered_map<string, NodeConfig> CAdapter::node_configs = {
    #include "../language_configs/c_types.def"
};

#undef DEF_TYPE

string CAdapter::GetLanguageName() const {
    return "c";
}

vector<string> CAdapter::GetAliases() const {
    return {"c"};
}

void CAdapter::InitializeParser() const {
    parser_wrapper_ = make_uniq<TSParserWrapper>();
    parser_wrapper_->SetLanguage(tree_sitter_c(), "C");
}

unique_ptr<TSParserWrapper> CAdapter::CreateFreshParser() const {
    auto fresh_parser = make_uniq<TSParserWrapper>();
    fresh_parser->SetLanguage(tree_sitter_c(), "C");
    return fresh_parser;
}

string CAdapter::GetNormalizedType(const string &node_type) const {
    const NodeConfig* config = GetNodeConfig(node_type);
    if (config) {
        return SemanticTypes::GetSemanticTypeName(config->semantic_type);
    }
    return node_type;  // Fallback to raw type
}

string CAdapter::ExtractNodeName(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->name_strategy);
    }
    
    // C-specific fallbacks
    string node_type = string(node_type_str);
    if (node_type.find("declarator") != string::npos || 
        node_type.find("specifier") != string::npos ||
        node_type.find("definition") != string::npos) {
        return FindChildByType(node, content, "identifier");
    }
    
    return "";
}

string CAdapter::ExtractNodeValue(TSNode node, const string &content) const {
    const char* node_type_str = ts_node_type(node);
    const NodeConfig* config = GetNodeConfig(node_type_str);
    
    if (config) {
        return ExtractByStrategy(node, content, config->value_strategy);
    }
    
    return "";
}

bool CAdapter::IsPublicNode(TSNode node, const string &content) const {
    // In C, check for static keyword (which makes things file-local)
    TSNode parent = ts_node_parent(node);
    while (!ts_node_is_null(parent)) {
        uint32_t child_count = ts_node_child_count(parent);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(parent, i);
            const char* child_type = ts_node_type(child);
            if (string(child_type) == "storage_class_specifier") {
                string specifier_text = ExtractNodeText(child, content);
                if (specifier_text == "static") {
                    return false;  // Static = file-local
                }
            }
        }
        parent = ts_node_parent(parent);
    }
    
    // Default to public (external linkage)
    return true;
}

const unordered_map<string, NodeConfig>& CAdapter::GetNodeConfigs() const {
    return node_configs;
}

ParsingFunction CAdapter::GetParsingFunction() const {
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {
        auto typed_adapter = static_cast<const CAdapter*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    };
}




} // namespace duckdb
