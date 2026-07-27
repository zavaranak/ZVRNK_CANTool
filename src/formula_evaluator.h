#pragma once

#include <QString>

class FormulaEvaluator
{
public:
    static double evaluate(double x, const QString& expression);
};
