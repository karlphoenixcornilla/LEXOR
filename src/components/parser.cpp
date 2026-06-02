#include "parser.h"
#include <iostream>

Parser::Parser(bool isRepl) : replMode(isRepl), currentLine(0), currentToken(0) {}

bool Parser::isAtEnd() {
    return currentLine >= lines.size();
}

bool Parser::isLineAtEnd() {
    if (isAtEnd()) return true;
    return currentToken >= lines[currentLine].size();
}

Token Parser::peek() {
    if (isLineAtEnd()) {
        if (currentLine + 1 < lines.size()) return lines[currentLine + 1][0]; // fallback
        return {TokenType::END_OF_FILE, "", 0};
    }
    return lines[currentLine][currentToken];
}

Token Parser::advance() {
    Token t = peek();
    if (!isLineAtEnd()) currentToken++;
    return t;
}

Token Parser::previous() {
    return lines[currentLine][currentToken - 1];
}

bool Parser::check(TokenType type) {
    if (isLineAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, std::string message) {
    if (check(type)) return advance();
    throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: " + message);
}

void Parser::advanceLine() {
    currentLine++;
    currentToken = 0;
}

// Helper Function for Keyword Line Matching (e.g. SCRIPT AREA, START SCRIPT, END SCRIPT)
bool isKeyLineMatch(const std::vector<Token>& line, std::initializer_list<TokenType> expected) {
    if (line.size() < expected.size()) return false;
    
    size_t i = 0;
    for (TokenType type : expected) {
        if (line[i++].type != type) return false;
    }
    
    return true;
}

std::unique_ptr<Program> Parser::parse(const std::vector<std::vector<Token>>& tokens) {
    // Catch any lexer errors before parsing
    for (const auto& lineTokens : tokens) {
        for (const auto& t : lineTokens) {
            if (t.type == TokenType::ERROR) {
                throw std::runtime_error("Line " + std::to_string(t.line) + " Parser Error: " + t.value);
            }
        }
    }

    lines = tokens;
    currentLine = 0;
    currentToken = 0;
    
    auto program = std::make_unique<Program>();

    if (!replMode) {
        // SCRIPT AREA
        if (isAtEnd() ||
            lines[currentLine].size() != 2 ||
            !isKeyLineMatch(lines[currentLine], {TokenType::SCRIPT, TokenType::AREA}))
        {
            throw std::runtime_error(
                "Line 1 Parser Error: 'SCRIPT AREA' must appear alone on this line"
            );
        }
        advanceLine();

        // START SCRIPT
        if (isAtEnd() ||
            lines[currentLine].size() != 2 ||
            !isKeyLineMatch(lines[currentLine], {TokenType::START, TokenType::SCRIPT}))
        {
            throw std::runtime_error(
                "Line 2 Parser Error: 'START SCRIPT' must appear alone on this line"
            );
        }
        advanceLine();
    }

    bool inExecutableCode = false;

    // Code body and END SCRIPT
    while (!isAtEnd()) {

        // END SCRIPT 
        if (check(TokenType::END_OF_FILE)) break;
        if (lines[currentLine].size() >= 2 &&
            isKeyLineMatch(lines[currentLine], {TokenType::END, TokenType::SCRIPT}))
        {   

            if(lines[currentLine].size() != 2) {
                throw std::runtime_error(
                    "Line " + std::to_string(peek().line) + 
                    " Parser Error: No code allowed after 'END SCRIPT'"
                );
            }
            
            advanceLine();

            if(!(lines[currentLine].size() == 1 && 
                lines[currentLine][0].type == TokenType::END_OF_FILE)) {
                throw std::runtime_error(
                    "Line " + std::to_string(peek().line) + 
                    " Parser Error: No code allowed after 'END SCRIPT'"
                );
            }

            break;
        }

        // Ensure Variable Declarations appear before executable code
        if (check(TokenType::DECLARE)) {
            if (inExecutableCode) {
                throw std::runtime_error(
                    "Line " + std::to_string(peek().line) + 
                    " Parser Error: Variable declarations must appear before executable code."
                );
            }
        } else {
            inExecutableCode = true;
        }

        // Ensure Keyword does not appear in the code body
        if(isKeyLineMatch(lines[currentLine], {TokenType::SCRIPT, TokenType::AREA}) ||
           isKeyLineMatch(lines[currentLine], {TokenType::START, TokenType::SCRIPT}))
        {
            throw std::runtime_error(
                "Line " + std::to_string(peek().line) + 
                " Parser Error: 'SCRIPT AREA' and 'START SCRIPT' are not allowed in the code body."
            );
        }

        program->statements.push_back(declaration());
        advanceLine();
    }
    
    return program;
}

std::unique_ptr<Statement> Parser::parseSingleLine(const std::vector<Token>& tokenLine) {
    for (const auto& t : tokenLine) {
        if (t.type == TokenType::ERROR) {
            throw std::runtime_error("Line " + std::to_string(t.line) + " Parser Error: " + t.value);
        }
    }

    lines = {tokenLine};
    currentLine = 0;
    currentToken = 0;
    return declaration();
}

std::unique_ptr<Statement> Parser::declaration() {
    if (match(TokenType::DECLARE)) {
        return varDecl();
    }
    return statement();
}

std::unique_ptr<Statement> Parser::varDecl() {
    TokenType type = advance().type;
    if (type != TokenType::INT_TYPE && type != TokenType::FLOAT_TYPE && type != TokenType::CHAR_TYPE && type != TokenType::BOOL_TYPE) {
        throw std::runtime_error("Expected INT, FLOAT, CHAR, or BOOL after DECLARE.");
    }

    auto declStmt = std::make_unique<VarDeclStatement>(type);

    do {
        Token name = consume(TokenType::IDENTIFIER, "Expected variable name.");
        std::unique_ptr<Expr> init = nullptr;
        if (match(TokenType::ASSIGN)) {
            init = expression();
        }
        VarDeclStatement::DeclInfo info{name.value, std::move(init)};
        declStmt->declarations.push_back(std::move(info));
    } while(match(TokenType::COMMA));

    return declStmt;
}

std::unique_ptr<Statement> Parser::statement() {
    if (match(TokenType::PRINT)) {
        consume(TokenType::COLON, "Expected ':' after PRINT.");
        return printStmt();
    }
    if (match(TokenType::SCAN)) {
        consume(TokenType::COLON, "Expected ':' after SCAN.");
        return scanStmt();
    }
    if (match(TokenType::IF)) return ifStmt();
    if (match(TokenType::FOR)) return forStmt();
    if (match(TokenType::REPEAT)) {
        if (match(TokenType::WHEN)) {
             return repeatStmt();
        }
        throw std::runtime_error("Expected WHEN after REPEAT.");
    }
    
    // Checks Missing END IF/END FOR/END REPEAT before trying to parse an assignment or expression statement
    if (check(TokenType::ELSE)) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Unexpected ELSE keyword.");
    }
    if (check(TokenType::START)) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Unexpected START keyword.");
    }
    if (check(TokenType::END)) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Unexpected END keyword.");
    }
    
    return assignStmt();
}

std::unique_ptr<Statement> Parser::assignStmt() {
    // We expect chained assignment target=target=expr
    std::vector<std::string> targets;
    targets.push_back(consume(TokenType::IDENTIFIER, "Expected variable name.").value);
    
    while(match(TokenType::ASSIGN)) {
        if (check(TokenType::IDENTIFIER) && currentToken + 1 < lines[currentLine].size() && lines[currentLine][currentToken + 1].type == TokenType::ASSIGN) {
            targets.push_back(advance().value);
        } else {
            auto value = expression();
            return std::make_unique<AssignStatement>(targets, std::move(value));
        }
    }
    throw std::runtime_error("Invalid assignment.");
}

std::unique_ptr<Statement> Parser::printStmt() {
    auto stmt = std::make_unique<PrintStatement>();
    do {
        stmt->expressions.push_back(expression());
    } while (match(TokenType::AMPERSAND));
    if (!isLineAtEnd()) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: PRINT arguments must be separated by '&'.");
    }
    return stmt;
}

std::unique_ptr<Statement> Parser::scanStmt() {
    auto stmt = std::make_unique<ScanStatement>();
    stmt->targets.push_back(consume(TokenType::IDENTIFIER, "Expected variable name.").value);
    while (!isLineAtEnd()) {
        consume(TokenType::COMMA, "Expected ',' between variables in SCAN statement.");
        stmt->targets.push_back(consume(TokenType::IDENTIFIER, "Expected variable name.").value);
    }
    return stmt;
}

std::unique_ptr<Statement> Parser::ifStmt() {
    consume(TokenType::LPAREN, "Expected '(' after IF.");
    auto condition = expression();
    consume(TokenType::RPAREN, "Expected ')' after IF condition.");
    if (!isLineAtEnd()) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Unexpected tokens after IF condition. No code allowed on the same line.");
    }

    advanceLine(); // move to START IF line
    int errLineIf = isAtEnd() ? lines[currentLine - 1][0].line : peek().line;
    if (isAtEnd() || !check(TokenType::START) || currentToken + 1 >= lines[currentLine].size() || lines[currentLine][currentToken + 1].type != TokenType::IF) {
        throw std::runtime_error("Line " + std::to_string(errLineIf) + " Parser Error: Missing: START IF");
    }
    if (lines[currentLine].size() != 2) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: 'START IF' must appear alone on this line.");
    }
    
    auto thenBranch = block(TokenType::IF, "IF"); // Block will leave currentLine on END IF

    auto stmt = std::make_unique<IfStatement>(std::move(condition), std::move(thenBranch));

    // Handle ELSE IF / ELSE
    while (currentLine + 1 < lines.size()) {
        int nextL = currentLine + 1;
        // Skip empty lines (could happen with ERROR tokens but those will be caught if processed)
        while (nextL < lines.size() && lines[nextL].empty()) {
            nextL++;
        }

        int tIndex = 0;

        if (nextL < lines.size() && tIndex < lines[nextL].size() && lines[nextL][tIndex].type == TokenType::ELSE) {
            currentLine = nextL; currentToken = tIndex; // Move from END IF to ELSE line
            
            advance(); // Consume ELSE
            if (match(TokenType::IF)) {
                consume(TokenType::LPAREN, "Expected '(' after ELSE IF.");
                auto elifCondition = expression();
                consume(TokenType::RPAREN, "Expected ')' after condition.");
                if (!isLineAtEnd()) {
                    throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Unexpected tokens after ELSE IF condition. No code allowed on the same line.");
                }
                advanceLine();
                int errLineElif = isAtEnd() ? lines[currentLine - 1][0].line : peek().line;
                if (isAtEnd() || !check(TokenType::START) || currentToken + 1 >= lines[currentLine].size() || lines[currentLine][currentToken + 1].type != TokenType::IF) {
                    throw std::runtime_error("Line " + std::to_string(errLineElif) + " Parser Error: Missing: START IF");
                }
                if (lines[currentLine].size() != 2) {
                    throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: 'START IF' must appear alone on this line.");
                }
                auto elifBody = block(TokenType::IF, "IF");
                IfStatement::ElseIfBranch elifBranch{std::move(elifCondition), std::move(elifBody)};
                stmt->elseIfBranches.push_back(std::move(elifBranch));
            } else {
                // ELSE
                if (!isLineAtEnd()) {
                    throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Unexpected tokens after ELSE. No code allowed on the same line.");
                }
                advanceLine();
                int errLineElse = isAtEnd() ? lines[currentLine - 1][0].line : peek().line;
                if (isAtEnd() || !check(TokenType::START) || currentToken + 1 >= lines[currentLine].size() || lines[currentLine][currentToken + 1].type != TokenType::IF) {
                    throw std::runtime_error("Line " + std::to_string(errLineElse) + " Parser Error: Missing: START IF");
                }
                if (lines[currentLine].size() != 2) {
                    throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: 'START IF' must appear alone on this line.");
                }
                stmt->elseBranch = block(TokenType::IF, "IF");
                break; // Only 1 ELSE allowed
            }
        } else {
            break;
        }
    }

    return stmt;
}

std::unique_ptr<Statement> Parser::forStmt() {
    consume(TokenType::LPAREN, "Expected '(' after FOR.");
    std::string initTarget = consume(TokenType::IDENTIFIER, "Expected init target.").value;
    consume(TokenType::ASSIGN, "Expected '='.");
    auto initVal = expression();
    consume(TokenType::COMMA, "Expected ','.");
    auto cond = expression();
    consume(TokenType::COMMA, "Expected ','.");
    std::string updateTarget = consume(TokenType::IDENTIFIER, "Expected update target.").value;
    consume(TokenType::ASSIGN, "Expected '='.");
    auto updateVal = expression();
    consume(TokenType::RPAREN, "Expected ')'.");
    if (!isLineAtEnd()) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Unexpected tokens after FOR loop parameters. No code allowed on the same line.");
    }
    
    advanceLine();
    int errLineFor = isAtEnd() ? lines[currentLine - 1][0].line : peek().line;
    if (isAtEnd() || !check(TokenType::START) || currentToken + 1 >= lines[currentLine].size() || lines[currentLine][currentToken + 1].type != TokenType::FOR) {
        throw std::runtime_error("Line " + std::to_string(errLineFor) + " Parser Error: Missing: START FOR");
    }
    if (lines[currentLine].size() != 2) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: 'START FOR' must appear alone on this line.");
    }
    
    auto stmt = std::make_unique<ForStatement>();
    stmt->initTarget = initTarget;
    stmt->initValue = std::move(initVal);
    stmt->condition = std::move(cond);
    stmt->updateTarget = updateTarget;
    stmt->updateValue = std::move(updateVal);
    stmt->body = block(TokenType::FOR, "FOR");

    return stmt;
}

std::unique_ptr<Statement> Parser::repeatStmt() {
    consume(TokenType::LPAREN, "Expected '(' for WHEN condition.");
    auto cond = expression();
    consume(TokenType::RPAREN, "Expected ')'.");
    if (!isLineAtEnd()) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Unexpected tokens after REPEAT condition. No code allowed on the same line.");
    }
    
    advanceLine();
    int errLineRep = isAtEnd() ? lines[currentLine - 1][0].line : peek().line;
    if (isAtEnd() || !check(TokenType::START) || currentToken + 1 >= lines[currentLine].size() || lines[currentLine][currentToken + 1].type != TokenType::REPEAT) {
        throw std::runtime_error("Line " + std::to_string(errLineRep) + " Parser Error: Missing: START REPEAT");
    }
    if (lines[currentLine].size() != 2) {
        throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: 'START REPEAT' must appear alone on this line.");
    }
    
    auto body = block(TokenType::REPEAT, "REPEAT");
    return std::make_unique<RepeatStatement>(std::move(cond), std::move(body));
}

std::vector<std::unique_ptr<Statement>> Parser::block(TokenType expectedEndType, const std::string& blockName) {
    std::vector<std::unique_ptr<Statement>> statements;
    advanceLine(); // move past START block keyword line
    
    while (!isAtEnd()) {
        if (check(TokenType::END)) {
            if (currentToken + 1 < lines[currentLine].size() && lines[currentLine][currentToken + 1].type == expectedEndType) {
                if (lines[currentLine].size() != 2) {
                    throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: 'END " + blockName + "' must appear alone on this line.");
                }
                break;
            } else {
                throw std::runtime_error("Line " + std::to_string(peek().line) + " Parser Error: Missing: END " + blockName);
            }
        }

        if (check(TokenType::DECLARE)) {
            throw std::runtime_error(
                "Line " + std::to_string(peek().line) + 
                " Parser Error: Variable declarations are not allowed inside blocks. They must appear before executable code."
            );
        }

        statements.push_back(declaration());
        advanceLine();
    }
    
    if (isAtEnd()) {
        throw std::runtime_error("Parser Error: Missing: END " + blockName);
    }
    
    // consume END X line (where X is IF, FOR, REPEAT)
    // DO NOT invoke advanceLine() here, leave it to the outer declaration loop!
    return statements;
}

std::unique_ptr<Expr> Parser::expression() {
    return logicalOr();
}

std::unique_ptr<Expr> Parser::logicalOr() {
    auto expr = logicalAnd();
    while (match(TokenType::OR)) {
        Token op = previous();
        auto right = logicalAnd();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::logicalAnd() {
    auto expr = equality();
    while (match(TokenType::AND)) {
        Token op = previous();
        auto right = equality();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::equality() {
    auto expr = comparison();
    while (match(TokenType::EQUAL_EQUAL) || match(TokenType::NOT_EQUAL)) {
        Token op = previous();
        auto right = comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
    auto expr = term();
    while (match(TokenType::GREATER) || match(TokenType::GREATER_EQUAL) || match(TokenType::LESS) || match(TokenType::LESS_EQUAL)) {
        Token op = previous();
        auto right = term();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::term() {
    auto expr = factor();
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        Token op = previous();
        auto right = factor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::factor() {
    auto expr = unary();
    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::MODULO)) {
        Token op = previous();
        auto right = unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::unary() {
    if (match(TokenType::NOT) || match(TokenType::MINUS) || match(TokenType::PLUS)) {
        Token op = previous();
        auto right = unary();
        return std::make_unique<UnaryExpr>(op, std::move(right));
    }
    return primary();
}

std::unique_ptr<Expr> Parser::primary() {
    if (match(TokenType::INT_LITERAL) || match(TokenType::FLOAT_LITERAL) || match(TokenType::CHAR_LITERAL) || match(TokenType::BOOL_LITERAL) || match(TokenType::STRING_LITERAL) || match(TokenType::DOLLAR)) {
        return std::make_unique<LiteralExpr>(previous());
    }

    if (match(TokenType::IDENTIFIER)) {
        return std::make_unique<IdentifierExpr>(previous().value);
    }

    if (match(TokenType::LPAREN)) {
        auto expr = expression();
        consume(TokenType::RPAREN, "Expected ')' after expression.");
        return expr;
    }

    throw std::runtime_error("Expected expression.");
}
