// formatnum.h
// 
// Copyright (C) 2003, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef CELUTIL_FORMATNUM_H_
#define CELUTIL_FORMATNUM_H_

#include <iostream>


class FormattedNumber
{
public:
    FormattedNumber(double, uint32_t _precision, uint32_t _flags);
    double getValue() const;
    double getRoundedValue() const;

    enum 
    {
        GroupThousands    = 0x1,
        SignificantDigits = 0x2,
    };
    
    friend std::ostream& operator<<(std::ostream& out, const FormattedNumber& num);
    
    static FormattedNumber SigDigitNum(double v, int digits) {
        return FormattedNumber(v, digits, FormattedNumber::GroupThousands | FormattedNumber::SignificantDigits);
    }

private:
    double value;
    uint32_t precision;
    uint32_t flags;
};


#endif // CELUTIL_FORMATNUM_H_

