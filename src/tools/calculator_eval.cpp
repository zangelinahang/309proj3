#include "tools/calculator.h"

#include <string>
#include <vector>

namespace {

bool is_operator(CalcToken::Kind kind) {
    return kind == CalcToken::Kind::Plus ||
           kind == CalcToken::Kind::Minus ||
           kind == CalcToken::Kind::Times ||
           kind == CalcToken::Kind::Divide;
}

int precedence(CalcToken::Kind kind) {
    if (kind == CalcToken::Kind::Times || kind == CalcToken::Kind::Divide) {
        return 2;
    }
    if (kind == CalcToken::Kind::Plus || kind == CalcToken::Kind::Minus) {
        return 1;
    }
    return 0;
}

bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool apply_top(std::vector<double>* values,
               std::vector<CalcToken::Kind>* ops,
               std::string* error) {
    if (values->size() < 2 || ops->empty()) {
        return fail(error, "malformed expression");
    }

    const CalcToken::Kind op = ops->back();
    ops->pop_back();
    if (!is_operator(op)) {
        return fail(error, "malformed expression");
    }

    const double right = values->back();
    values->pop_back();
    const double left = values->back();
    values->pop_back();

    double result = 0.0;
    switch (op) {
        case CalcToken::Kind::Plus:
            result = left + right;
            break;
        case CalcToken::Kind::Minus:
            result = left - right;
            break;
        case CalcToken::Kind::Times:
            result = left * right;
            break;
        case CalcToken::Kind::Divide:
            if (right == 0.0) {
                return fail(error, "division by zero");
            }
            result = left / right;
            break;
        default:
            return fail(error, "malformed expression");
    }

    values->push_back(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    if (error != nullptr) {
        error->clear();
    }
    if (out == nullptr) {
        return fail(error, "missing output pointer");
    }
    if (tokens.empty()) {
        return fail(error, "empty expression");
    }

    std::vector<double> values;
    std::vector<CalcToken::Kind> ops;
    values.reserve(tokens.size());
    ops.reserve(tokens.size());

    for (const CalcToken& token : tokens) {
        switch (token.kind) {
            case CalcToken::Kind::Number:
                values.push_back(token.value);
                break;

            case CalcToken::Kind::LParen:
                ops.push_back(token.kind);
                break;

            case CalcToken::Kind::RParen:
                while (!ops.empty() && ops.back() != CalcToken::Kind::LParen) {
                    if (!apply_top(&values, &ops, error)) {
                        return false;
                    }
                }
                if (ops.empty() || ops.back() != CalcToken::Kind::LParen) {
                    return fail(error, "mismatched parentheses");
                }
                ops.pop_back();
                break;

            case CalcToken::Kind::Plus:
            case CalcToken::Kind::Minus:
            case CalcToken::Kind::Times:
            case CalcToken::Kind::Divide:
                while (!ops.empty() && is_operator(ops.back()) &&
                       precedence(ops.back()) >= precedence(token.kind)) {
                    if (!apply_top(&values, &ops, error)) {
                        return false;
                    }
                }
                ops.push_back(token.kind);
                break;
        }
    }

    while (!ops.empty()) {
        if (ops.back() == CalcToken::Kind::LParen ||
            ops.back() == CalcToken::Kind::RParen) {
            return fail(error, "mismatched parentheses");
        }
        if (!apply_top(&values, &ops, error)) {
            return false;
        }
    }

    if (values.size() != 1) {
        return fail(error, "malformed expression");
    }

    *out = values.back();
    return true;
}
