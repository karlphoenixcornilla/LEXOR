#include "evaluator.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cctype>
#include <climits>
#include <cfloat>

Evaluator::Evaluator() {
    auto initialEnv = std::make_unique<Environment>();
    envStack.push_back(std::move(initialEnv));
    env = envStack.back().get();
}

void Evaluator::pushEnv() {
    auto newEnv = std::make_unique<Environment>(env);
    envStack.push_back(std::move(newEnv));
    env = envStack.back().get();
}

void Evaluator::popEnv() {
    if (envStack.size() > 1) {
        envStack.pop_back();
        env = envStack.back().get();
    }
}

RuntimeValue Evaluator::evaluateExpr(Expr* expr) {
    if (expr) {
        expr->accept(this);
    }
    return currentResult;
}

void Evaluator::evaluate(Program* program) {
    program->accept(this);
}

void Evaluator::visit(Program* node) {
    for (auto& stmt : node->statements) {
        stmt->accept(this);
    }
}

void Evaluator::visit(VarDeclStatement* node) {
    for (auto& decl : node->declarations) {
        RuntimeValue val;
        
        switch (node->dataType) {
            case TokenType::INT_TYPE: val = RuntimeValue(0); break;
            case TokenType::FLOAT_TYPE: val = RuntimeValue(0.0f); break;
            case TokenType::CHAR_TYPE: val = RuntimeValue('\0'); break;
            case TokenType::BOOL_TYPE: val = RuntimeValue(false); break;
            default: break;
        }

        if (decl.initializer) {
            RuntimeValue initVal = evaluateExpr(decl.initializer.get());
            if (val.type == DataType::INT && initVal.type == DataType::INT) val.value = initVal.value;
            else if (val.type == DataType::FLOAT && initVal.type == DataType::FLOAT) val.value = initVal.value;
            else if (val.type == DataType::CHAR && initVal.type == DataType::CHAR) val.value = initVal.value;
            else if (val.type == DataType::BOOL && initVal.type == DataType::BOOL) val.value = initVal.value;
            else if (val.type == DataType::BOOL && initVal.type == DataType::STRING) {
                if (std::get<std::string>(initVal.value) == "TRUE") val.value = true;
                else if (std::get<std::string>(initVal.value) == "FALSE") val.value = false;
                else throw std::runtime_error("Invalid BOOL value. Expected \"TRUE\" or \"FALSE\".");
            }
            else throw std::runtime_error("Type mismatch on initialization.");
            val.initialized = true;
        } else {
            val.initialized = false;
        }
        
        env->declareVariable(decl.name, val);
    }
}

void Evaluator::visit(AssignStatement* node) {
    RuntimeValue val = evaluateExpr(node->value.get());
    for (const auto& target : node->targets) {
        RuntimeValue targetVar = env->getVariable(target);
        

        if (targetVar.type == DataType::BOOL && val.type == DataType::STRING) {
             if (std::get<std::string>(val.value) == "TRUE") targetVar.value = true;
             else if (std::get<std::string>(val.value) == "FALSE") targetVar.value = false;
             else throw std::runtime_error("Invalid BOOL string. Expected \"TRUE\" or \"FALSE\".");
             env->assignVariable(target, targetVar);
             continue;
        }
        
        env->assignVariable(target, val);
    }
}

std::string stringify(const RuntimeValue& v) {
    switch (v.type) {
        case DataType::INT: return std::to_string(std::get<int>(v.value));
        case DataType::FLOAT: return std::to_string(std::get<float>(v.value));
        case DataType::CHAR: return std::string(1, std::get<char>(v.value));
        case DataType::BOOL: return std::get<bool>(v.value) ? "TRUE" : "FALSE";
        case DataType::STRING: return std::get<std::string>(v.value);
    }
    return "";
}

void Evaluator::visit(PrintStatement* node) {
    std::string out = "";
    for (size_t i = 0; i < node->expressions.size(); ++i) {
        RuntimeValue val = evaluateExpr(node->expressions[i].get());
        if (val.type == DataType::STRING && std::get<std::string>(val.value) == "$") {
             out += "\n";
        } else {
             out += stringify(val);
        }
    }
    std::cout << out;
}

void Evaluator::visit(ScanStatement* node) {
    for (size_t i = 0; i < node->targets.size(); ++i) {
        if (i > 0) {
            char comma;
            std::cin >> comma;
            if (comma != ',') {
                throw std::runtime_error("Expected ',' between input values for SCAN.");
            }
        }
        const auto& target = node->targets[i];
        RuntimeValue v = env->getVariable(target);
        
        std::string s = "";
        std::cin >> std::ws;
        while (std::cin.peek() != EOF && !isspace(std::cin.peek()) && std::cin.peek() != ',') {
            s += (char)std::cin.get();
        }

        if (v.type == DataType::INT) {
            if (s.find('.') != std::string::npos) {
                throw std::runtime_error("Type mismatch: Expected INT, got FLOAT input for '" + target + "'.");
            }
            try {
                size_t pos;
                int x = std::stoi(s, &pos);
                if (pos != s.length()) throw std::runtime_error("Invalid INT input for '" + target + "'.");
                v.value = x;
            } catch (...) {
                throw std::runtime_error("Invalid INT input for '" + target + "'.");
            }
        } else if (v.type == DataType::FLOAT) {
            // if (s.find('.') == std::string::npos) {
            //     throw std::runtime_error("Type mismatch: Expected FLOAT, got INT input for '" + target + "'.");
            // }
            try {
                size_t pos;
                float x = std::stof(s, &pos);
                if (pos != s.length()) throw std::runtime_error("Invalid FLOAT input for '" + target + "'.");
                v.value = x;
            } catch (...) {
                throw std::runtime_error("Invalid FLOAT input for '" + target + "'.");
            }
        } else if (v.type == DataType::CHAR) {
            if (s.length() != 1) throw std::runtime_error("Invalid CHAR input for '" + target + "'.");
            v.value = s[0];
        } else if (v.type == DataType::BOOL) {
            if (s == "TRUE") v.value = true;
            else if (s == "FALSE") v.value = false;
            else throw std::runtime_error("Invalid BOOL input for '" + target + "'. Expected TRUE or FALSE.");
        }
        env->assignVariable(target, v);
    }

    // Check for trailing comma or extra inputs on the same line
    int next = std::cin.peek();
    while (next != EOF && (next == ' ' || next == '\t')) {
        std::cin.get();
        next = std::cin.peek();
    }
    
    if (next != EOF && next != '\n' && next != '\r') {
        // If it's a comma or more data, it's too many inputs
        // Consume the rest of the line to stay in sync
        while (next != EOF && next != '\n' && next != '\r') {
            std::cin.get();
            next = std::cin.peek();
        }
        throw std::runtime_error("Too many inputs for SCAN");
    }
}

void Evaluator::visit(IfStatement* node) {
    RuntimeValue condVal = evaluateExpr(node->condition.get());
    if (condVal.type != DataType::BOOL) throw std::runtime_error("IF condition must be BOOL.");
    
    if (std::get<bool>(condVal.value)) {
        pushEnv();
        for (auto& stmt : node->thenBranch) stmt->accept(this);
        popEnv();
        return;
    }
    
    for (auto& elif : node->elseIfBranches) {
        RuntimeValue elifCond = evaluateExpr(elif.condition.get());
        if (elifCond.type != DataType::BOOL) throw std::runtime_error("ELSE IF condition must be BOOL.");
        if (std::get<bool>(elifCond.value)) {
            pushEnv();
            for (auto& stmt : elif.body) stmt->accept(this);
            popEnv();
            return;
        }
    }
    
    if (!node->elseBranch.empty()) {
        pushEnv();
        for (auto& stmt : node->elseBranch) stmt->accept(this);
        popEnv();
    }
}

void Evaluator::visit(ForStatement* node) {
    pushEnv();
    
    // Init
    RuntimeValue initVal = evaluateExpr(node->initValue.get());
    env->assignVariable(node->initTarget, initVal);
    
    while (true) {
        RuntimeValue condVal = evaluateExpr(node->condition.get());
        if (condVal.type != DataType::BOOL) throw std::runtime_error("FOR condition must be BOOL.");
        if (!std::get<bool>(condVal.value)) break;
        
        pushEnv();
        for (auto& stmt : node->body) stmt->accept(this);
        popEnv();
        
        RuntimeValue upVal = evaluateExpr(node->updateValue.get());
        env->assignVariable(node->updateTarget, upVal);
    }
    
    popEnv();
}

void Evaluator::visit(RepeatStatement* node) {
    while (true) {
        RuntimeValue condVal = evaluateExpr(node->condition.get());
        if (condVal.type != DataType::BOOL) throw std::runtime_error("REPEAT condition must be BOOL.");
        if (!std::get<bool>(condVal.value)) break;
        
        pushEnv();
        for (auto& stmt : node->body) stmt->accept(this);
        popEnv();
    }
}

void Evaluator::visit(BinaryExpr* node) {
    RuntimeValue left = evaluateExpr(node->left.get());
    RuntimeValue right = evaluateExpr(node->right.get());
    
    auto t = node->op.type;
    
    // Logical — convert STRING "TRUE"/"FALSE" to BOOL for AND/OR
    if (t == TokenType::AND || t == TokenType::OR) {
        // Auto-convert string "TRUE"/"FALSE" to BOOL for logical ops
        if (left.type == DataType::STRING) {
            std::string s = std::get<std::string>(left.value);
            if (s == "TRUE") left = RuntimeValue(true);
            else if (s == "FALSE") left = RuntimeValue(false);
        }
        if (right.type == DataType::STRING) {
            std::string s = std::get<std::string>(right.value);
            if (s == "TRUE") right = RuntimeValue(true);
            else if (s == "FALSE") right = RuntimeValue(false);
        }
        if (left.type != DataType::BOOL || right.type != DataType::BOOL) throw std::runtime_error("Logical operators AND/OR require BOOL.");
        bool l = std::get<bool>(left.value);
        bool r = std::get<bool>(right.value);
        if (t == TokenType::AND) currentResult = RuntimeValue(l && r);
        else currentResult = RuntimeValue(l || r);
        return;
    }
    
    // Equality — strict type matching
    if (t == TokenType::EQUAL_EQUAL || t == TokenType::NOT_EQUAL) {
        if (left.type != right.type) {
            throw std::runtime_error("Cannot compare values of different types.");
        }
        bool eq = false;
        if (left.type == DataType::INT && right.type == DataType::INT) eq = (std::get<int>(left.value) == std::get<int>(right.value));
        else if (left.type == DataType::FLOAT && right.type == DataType::FLOAT) eq = (std::get<float>(left.value) == std::get<float>(right.value));
        else if (left.type == DataType::CHAR && right.type == DataType::CHAR) eq = (std::get<char>(left.value) == std::get<char>(right.value));
        else if (left.type == DataType::BOOL && right.type == DataType::BOOL) eq = (std::get<bool>(left.value) == std::get<bool>(right.value));
        
        if (t == TokenType::NOT_EQUAL) eq = !eq;
        currentResult = RuntimeValue(eq);
        return;
    }
    
    // Comparison — strict type matching
    if (t == TokenType::GREATER || t == TokenType::LESS || t == TokenType::GREATER_EQUAL || t == TokenType::LESS_EQUAL) {
        if (left.type != right.type) {
            throw std::runtime_error("Cannot compare values of different types.");
        }
        if (left.type == DataType::BOOL) throw std::runtime_error("Cannot do Boolean Arithmetic");
        if (left.type == DataType::CHAR) throw std::runtime_error("Cannot do Character Arithmetic");
        
        bool res = false;
        if (left.type == DataType::INT) {
            int l = std::get<int>(left.value);
            int r = std::get<int>(right.value);
            if (t == TokenType::GREATER) res = l > r;
            if (t == TokenType::LESS) res = l < r;
            if (t == TokenType::GREATER_EQUAL) res = l >= r;
            if (t == TokenType::LESS_EQUAL) res = l <= r;
        } else if (left.type == DataType::FLOAT) {
            float l = std::get<float>(left.value);
            float r = std::get<float>(right.value);
            if (t == TokenType::GREATER) res = l > r;
            if (t == TokenType::LESS) res = l < r;
            if (t == TokenType::GREATER_EQUAL) res = l >= r;
            if (t == TokenType::LESS_EQUAL) res = l <= r;
        }
        currentResult = RuntimeValue(res);
        return;
    }
    
    // Arithmetic
    if (t == TokenType::PLUS || t == TokenType::MINUS || t == TokenType::STAR || t == TokenType::SLASH || t == TokenType::MODULO) {
        // Check for BOOL/CHAR operands first
        if (left.type == DataType::BOOL || right.type == DataType::BOOL) {
            throw std::runtime_error("Cannot do Boolean Arithmetic");
        }
        if (left.type == DataType::CHAR || right.type == DataType::CHAR) {
            throw std::runtime_error("Cannot do Character Arithmetic");
        }
        
        // Strict type checking: no mixed INT/FLOAT arithmetic
        if (left.type != right.type) {
            throw std::runtime_error("Cannot perform arithmetic between INT and FLOAT.");
        }
        
        if (left.type == DataType::FLOAT) {
            float l = std::get<float>(left.value);
            float r = std::get<float>(right.value);
            
            // Division by zero check
            if ((t == TokenType::SLASH || t == TokenType::MODULO) && r == 0.0f) {
                throw std::runtime_error("Cannot divide by zero.");
            }
            
            float res = 0;
            switch(t) {
                case TokenType::PLUS: res = l + r; break;
                case TokenType::MINUS: res = l - r; break;
                case TokenType::STAR: res = l * r; break;
                case TokenType::SLASH: res = l / r; break;
                case TokenType::MODULO: {
                    res = std::fmod(l, r);
                    if (res < 0) res += std::fabs(r);
                    break;
                }
                default: break;
            }
            
            // Float overflow check
            if (std::isinf(res) || res > FLT_MAX || res < -FLT_MAX) {
                throw std::runtime_error("Float overflow.");
            }
            
            currentResult = RuntimeValue(res);
            return;
        } else {
            int l = std::get<int>(left.value);
            int r = std::get<int>(right.value);
            
            // Division by zero check
            if ((t == TokenType::SLASH || t == TokenType::MODULO) && r == 0) {
                throw std::runtime_error("Cannot divide by zero.");
            }
            
            // Use long long for overflow detection
            long long ll = l;
            long long lr = r;
            long long lres = 0;
            switch(t) {
                case TokenType::PLUS: lres = ll + lr; break;
                case TokenType::MINUS: lres = ll - lr; break;
                case TokenType::STAR: lres = ll * lr; break;
                case TokenType::SLASH: lres = ll / lr; break;
                case TokenType::MODULO: {
                    lres = ll % lr;
                    if (lres < 0) lres += std::abs(r);
                    break;
                }
                default: break;
            }
            
            // Integer overflow check
            if (lres > INT_MAX || lres < INT_MIN) {
                throw std::runtime_error("Integer overflow.");
            }
            
            currentResult = RuntimeValue(static_cast<int>(lres));
            return;
        }
    }
}

void Evaluator::visit(UnaryExpr* node) {
    RuntimeValue right = evaluateExpr(node->right.get());
    if (node->op.type == TokenType::NOT) {
        // Auto-convert string "TRUE"/"FALSE" to BOOL for NOT
        if (right.type == DataType::STRING) {
            std::string s = std::get<std::string>(right.value);
            if (s == "TRUE") right = RuntimeValue(true);
            else if (s == "FALSE") right = RuntimeValue(false);
            else throw std::runtime_error("NOT requires BOOL.");
        }
        if (right.type != DataType::BOOL) throw std::runtime_error("NOT requires BOOL.");
        currentResult = RuntimeValue(!std::get<bool>(right.value));
    } else if (node->op.type == TokenType::MINUS) {
        if (right.type == DataType::INT) currentResult = RuntimeValue(-std::get<int>(right.value));
        else if (right.type == DataType::FLOAT) currentResult = RuntimeValue(-std::get<float>(right.value));
        else throw std::runtime_error("MINUS requires number.");
    } else if (node->op.type == TokenType::PLUS) {
        currentResult = right;
    }
}

void Evaluator::visit(LiteralExpr* node) {
    if (node->value.type == TokenType::INT_LITERAL) {
        currentResult = RuntimeValue(std::stoi(node->value.value));
    } else if (node->value.type == TokenType::FLOAT_LITERAL) {
        currentResult = RuntimeValue(std::stof(node->value.value));
    } else if (node->value.type == TokenType::BOOL_LITERAL) {
        currentResult = RuntimeValue(node->value.value == "TRUE");
    } else if (node->value.type == TokenType::CHAR_LITERAL) {
        currentResult = RuntimeValue(node->value.value[0]);
    } else if (node->value.type == TokenType::STRING_LITERAL) {
        currentResult = RuntimeValue(node->value.value);
    } else if (node->value.type == TokenType::DOLLAR) { // Treat dollar literal as "$", handled by Print
        currentResult = RuntimeValue(std::string("$")); 
    }
}

void Evaluator::visit(IdentifierExpr* node) {
    RuntimeValue val = env->getVariable(node->name);
    if (!val.initialized) {
        throw std::runtime_error("Cannot use uninitialized variable: " + node->name + ".");
    }
    currentResult = val;
}
