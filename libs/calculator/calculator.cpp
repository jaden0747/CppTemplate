#include "calculator/calculator.hpp"

double Calculator::add(double a, double b)
{
    return a + b;
}

double Calculator::subtract(double a, double b)
{
    return a - b;
}

double Calculator::multiply(double a, double b)
{
    return a * b;
}

double Calculator::divide(double a, double b)
{
    if (b != 0.0)
    {
        return a / b;
    }
    return 0.0;
}

