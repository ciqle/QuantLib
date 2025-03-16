/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2021 Ralf Konrad Eckel

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/cashflows/zeroinflationcashflow.hpp>
#include <ql/indexes/inflationindex.hpp>
#include <ql/termstructures/inflationtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

namespace QuantLib {

    // 构造函数：初始化零通胀现金流
    ZeroInflationCashFlow::ZeroInflationCashFlow(Real notional,
                                                 const ext::shared_ptr<ZeroInflationIndex>& index,
                                                 CPI::InterpolationType observationInterpolation,
                                                 const Date& startDate,
                                                 const Date& endDate,
                                                 const Period& observationLag,
                                                 const Date& paymentDate,
                                                 bool growthOnly)
    : IndexedCashFlow(notional, index,
                      startDate - observationLag, endDate - observationLag,
                      paymentDate, growthOnly),
      zeroInflationIndex_(index), interpolation_(observationInterpolation),
      startDate_(startDate), endDate_(endDate), observationLag_(observationLag) {}

    // 获取基准固定值：根据通胀指数在基准日期的值
    Real ZeroInflationCashFlow::baseFixing() const {
        // 使用零通胀指数的特殊方法，考虑观察滞后和插值方式
        return CPI::laggedFixing(zeroInflationIndex_, startDate_, observationLag_, interpolation_);
    }

    // 获取指数固定值：根据通胀指数在结束日期的值
    Real ZeroInflationCashFlow::indexFixing() const {
        // 使用零通胀指数的特殊方法，考虑观察滞后和插值方式
        return CPI::laggedFixing(zeroInflationIndex_, endDate_, observationLag_, interpolation_);
    }

    // 访问者模式实现
    void ZeroInflationCashFlow::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<ZeroInflationCashFlow>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            IndexedCashFlow::accept(v);
    }

}
