/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl

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

/*! \file target.hpp
    \brief TARGET calendar
*/

#ifndef quantlib_target_calendar_h
#define quantlib_target_calendar_h

#include <ql/time/calendar.hpp>

namespace QuantLib {

    //! %TARGET calendar
    /*! Holidays (see http://www.ecb.int):
        <ul>
        <li>Saturdays</li>
        <li>Sundays</li>
        <li>New Year's Day, January 1st</li>
        <li>Good Friday (since 2000)</li>
        <li>Easter Monday (since 2000)</li>
        <li>Labour Day, May 1st (since 2000)</li>
        <li>Christmas, December 25th</li>
        <li>Day of Goodwill, December 26th (since 2000)</li>
        <li>December 31st (1998, 1999, and 2001)</li>
        </ul>

        \ingroup calendars

        \test the correctness of the returned results is tested
              against a list of known holidays.
    */
    // TARGET日历类
    // TARGET代表"Trans-European Automated Real-time Gross settlement Express Transfer system"
    // 即"泛欧自动实时总额结算快速转账系统"，是欧洲中央银行使用的支付系统
    class TARGET : public Calendar {
      private:
        // 内部实现类，继承自Calendar::WesternImpl
        class Impl final : public Calendar::WesternImpl {
          public:
            // 返回日历名称
            std::string name() const override { return "TARGET"; }
            // 判断给定日期是否为工作日
            bool isBusinessDay(const Date&) const override;
        };
      public:
        // TARGET日历构造函数
        TARGET();
    };

}


#endif
