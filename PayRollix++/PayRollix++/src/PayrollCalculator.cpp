#include "PayrollCalculator.h"
#include <algorithm>

PayStubs PayrollCalculator::calculate(double grossSalary, double overtimeHours, double hourlyRate,
                                        double overtimeRate, double healthInsurance,
                                        double retirement401k, double otherDeductions) {
    PayStubs stub;
    
    double overtimePay = overtimeHours * hourlyRate * overtimeRate;
    double totalGross = grossSalary + overtimePay;
    
    double annualizedIncome = totalGross * 12;
    
    double federalTax = calculateFederalTax(annualizedIncome) / 12;
    double stateTax = calculateStateTax(annualizedIncome) / 12;
    double socialSecurity = calculateSocialSecurity(totalGross);
    double medicare = calculateMedicare(totalGross);
    
    double totalDeductions = federalTax + stateTax + socialSecurity + medicare +
                             healthInsurance + retirement401k + otherDeductions;
    
    stub.gross = totalGross;
    stub.federalTax = federalTax;
    stub.stateTax = stateTax;
    stub.socialSecurity = socialSecurity;
    stub.medicare = medicare;
    stub.healthInsurance = healthInsurance;
    stub.retirement401k = retirement401k;
    stub.otherDeductions = otherDeductions;
    stub.overtimePay = overtimePay;
    stub.totalDeductions = totalDeductions;
    stub.net = totalGross - totalDeductions;
    
    return stub;
}

double PayrollCalculator::calculateFederalTax(double annualIncome) {
    static const TaxBracket brackets[] = {
        {0, 11000, 0.10},
        {11000, 44725, 0.12},
        {44725, 95375, 0.22},
        {95375, 171050, 0.24},
        {171050, 326450, 0.32},
        {326450, 414700, 0.35},
        {414700, 1e12, 0.37}
    };
    
    double tax = 0;
    double remainingIncome = annualIncome;
    
    for (const auto& bracket : brackets) {
        if (remainingIncome <= 0) break;
        
        if (annualIncome > bracket.minIncome) {
            double incomeInBracket = std::min(annualIncome, bracket.maxIncome) - bracket.minIncome;
            if (incomeInBracket > 0) {
                tax += incomeInBracket * bracket.rate;
            }
        }
        remainingIncome -= (bracket.maxIncome - bracket.minIncome);
    }
    
    return tax;
}

double PayrollCalculator::calculateStateTax(double annualIncome) {
    return annualIncome * 0.05;
}

double PayrollCalculator::calculateSocialSecurity(double monthlyIncome) {
    double annualCap = 160200;
    double monthlyCap = annualCap / 12;
    double taxableAmount = std::min(monthlyIncome, monthlyCap);
    return taxableAmount * 0.062;
}

double PayrollCalculator::calculateMedicare(double monthlyIncome) {
    double tax = monthlyIncome * 0.0145;
    double annualized = monthlyIncome * 12;
    if (annualized > 200000) {
        double excessMonthly = (annualized - 200000) / 12;
        if (monthlyIncome > excessMonthly) {
            tax += std::min(monthlyIncome, excessMonthly) * 0.009;
        }
    }
    return tax;
}
