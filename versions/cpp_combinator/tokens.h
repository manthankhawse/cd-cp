#pragma once
#include <string>

enum class TokenCode {
    KW_ALLOW, KW_DENY, KW_ROLE, KW_ACTION, KW_ON, KW_RESOURCE, KW_WHERE,
    KW_AND, KW_OR, KW_NOT, KW_TRUE, KW_FALSE,
    ATTR_IP, ATTR_TIME, ATTR_MFA, ATTR_REGION, ATTR_TAG,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LEQ, OP_GEQ,
    SEMICOLON, STRING, NUMBER, END_OF_FILE, UNKNOWN
};

struct Token {
    TokenCode code;
    std::string value;
    int line;
    int column;

    std::string toString() const {
        return "Token(" + std::to_string(static_cast<int>(code)) + ", '" + value + "', " +
               std::to_string(line) + ":" + std::to_string(column) + ")";
    }
};
