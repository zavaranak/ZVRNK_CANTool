#include "formula_evaluator.h"

#include <stdexcept>
#include <utility>

class Parser
{
public:
    Parser(double x, QString expression)
        : x_(x), expression_(std::move(expression))
    {
    }

    double parse()
    {
        const double value = parseExpression();
        skipSpaces();
        if (pos_ != expression_.size()) {
            throw std::runtime_error("Invalid formula");
        }
        return value;
    }

private:
    double parseExpression()
    {
        double value = parseTerm();
        while (true) {
            skipSpaces();
            if (match('+')) {
                value += parseTerm();
            } else if (match('-')) {
                value -= parseTerm();
            } else {
                return value;
            }
        }
    }

    double parseTerm()
    {
        double value = parseFactor();
        while (true) {
            skipSpaces();
            if (match('*')) {
                value *= parseFactor();
            } else if (match('/')) {
                const double divisor = parseFactor();
                if (divisor == 0.0) {
                    throw std::runtime_error("Divide by zero");
                }
                value /= divisor;
            } else {
                return value;
            }
        }
    }

    double parseFactor()
    {
        skipSpaces();
        if (match('+')) {
            return parseFactor();
        }
        if (match('-')) {
            return -parseFactor();
        }
        if (match('(')) {
            const double value = parseExpression();
            if (!match(')')) {
                throw std::runtime_error("Missing ')'");
            }
            return value;
        }
        if (peekIsX()) {
            ++pos_;
            return x_;
        }
        return parseNumber();
    }

    double parseNumber()
    {
        skipSpaces();
        const qsizetype start = pos_;
        while (pos_ < expression_.size() &&
               (expression_[pos_].isDigit() || expression_[pos_] == '.')) {
            ++pos_;
        }
        if (start == pos_) {
            throw std::runtime_error("Expected number");
        }
        bool ok = false;
        const double value = expression_.mid(start, pos_ - start).toDouble(&ok);
        if (!ok) {
            throw std::runtime_error("Invalid number");
        }
        return value;
    }

    void skipSpaces()
    {
        while (pos_ < expression_.size() && expression_[pos_].isSpace()) {
            ++pos_;
        }
    }

    bool match(QChar c)
    {
        skipSpaces();
        if (pos_ < expression_.size() && expression_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool peekIsX() const
    {
        return pos_ < expression_.size() &&
            (expression_[pos_] == 'x' || expression_[pos_] == 'X');
    }

    double x_;
    QString expression_;
    qsizetype pos_ = 0;
};

double FormulaEvaluator::evaluate(double x, const QString& expression)
{
    QString trimmed = expression.trimmed();
    if (trimmed.isEmpty()) {
        trimmed = "x";
    }

    for (const QChar c : trimmed) {
        if (!(c.isDigit() || c.isSpace() || c == 'x' || c == 'X' ||
              c == '+' || c == '-' || c == '*' || c == '/' ||
              c == '.' || c == '(' || c == ')')) {
            throw std::runtime_error("Invalid formula");
        }
    }

    return Parser(x, trimmed).parse();
}
