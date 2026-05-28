#ifndef RUNTIME_VALUE_H
#define RUNTIME_VALUE_H

#include <string>
#include <variant>

enum class DataType {
    INT,
    FLOAT,
    CHAR,
    BOOL,
    STRING // For strings/intermediate usage
};

struct RuntimeValue {
    DataType type;
    std::variant<int, float, char, bool, std::string> value;
    bool initialized;
    
    RuntimeValue() : type(DataType::INT), value(0), initialized(false) {}
    RuntimeValue(int v) : type(DataType::INT), value(v), initialized(true) {}
    RuntimeValue(float v) : type(DataType::FLOAT), value(v), initialized(true) {}
    RuntimeValue(char v) : type(DataType::CHAR), value(v), initialized(true) {}
    RuntimeValue(bool v) : type(DataType::BOOL), value(v), initialized(true) {}
    RuntimeValue(const std::string& v) : type(DataType::STRING), value(v), initialized(true) {}
};

#endif
