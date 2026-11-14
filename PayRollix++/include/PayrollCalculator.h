#pragma once

struct PayStubs {
    double gross;
    double federalTax;
    double stateTax;
    double socialSecurity;
    double medicare;
    double healthInsurance;
    double retirement401k;
    double otherDeductions;
    double overtimePay;
    double totalDeductions;
    double net;
};

struct TaxBracket {
    double minIncome;
    double maxIncome;
    double rate;
};

class PayrollCalculator {
public:
    static PayStubs calculate(double grossSalary, double overtimeHours = 0, double hourlyRate = 0,
                               double overtimeRate = 1.5, double healthInsurance = 0,
                               double retirement401k = 0, double otherDeductions = 0);
    
    static PayStubs calculateOld(double salary) {
        double tax = salary * 0.20;
        PayStubs stub;
        stub.gross = salary;
        stub.federalTax = tax;
        stub.stateTax = 0;
        stub.socialSecurity = 0;
        stub.medicare = 0;
        stub.healthInsurance = 0;
        stub.retirement401k = 0;
        stub.otherDeductions = 0;
        stub.overtimePay = 0;
        stub.totalDeductions = tax;
        stub.net = salary - tax;
        return stub;
    }
    
private:
    static double calculateFederalTax(double annualIncome);
    static double calculateStateTax(double annualIncome);
    static double calculateSocialSecurity(double annualIncome);
    static double calculateMedicare(double annualIncome);
};
