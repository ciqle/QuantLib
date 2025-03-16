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

#include <ql/time/calendars/target.hpp>

namespace QuantLib {

    // TARGET日历构造函数实现
    TARGET::TARGET() {
        // 所有日历实例共享相同的实现实例
        // 使用静态共享指针创建TARGET::Impl的实例
        static ext::shared_ptr<Calendar::Impl> impl(new TARGET::Impl);
        // 将基类的实现指针指向这个实例
        impl_ = impl;
    }

    // TARGET::Impl类的isBusinessDay方法实现，用于判断一个日期是否为工作日
    bool TARGET::Impl::isBusinessDay(const Date& date) const {
        // 获取日期的星期几
        Weekday w = date.weekday();
        // 获取日期的天、月、年以及一年中的第几天
        Day d = date.dayOfMonth(), dd = date.dayOfYear();
        Month m = date.month();
        Year y = date.year();
        // 计算该年复活节星期一的日期
        Day em = easterMonday(y);
        // 判断是否为非工作日：
        // 如果是周末，或者是以下任何一个节假日：
        if (isWeekend(w)
            // 新年（1月1日）
            || (d == 1  && m == January)
            // 耶稣受难日（复活节前的星期五，2000年及以后）
            || (dd == em-3 && y >= 2000)
            // 复活节星期一（2000年及以后）
            || (dd == em && y >= 2000)
            // 劳动节（5月1日，2000年及以后）
            || (d == 1  && m == May && y >= 2000)
            // 圣诞节（12月25日）
            || (d == 25 && m == December)
            // 节礼日（12月26日，2000年及以后）
            || (d == 26 && m == December && y >= 2000)
            // 12月31日（仅限1998年、1999年和2001年）
            || (d == 31 && m == December &&
                (y == 1998 || y == 1999 || y == 2001)))
            return false; // NOLINT(readability-simplify-boolean-expr)
        // 如果不是以上任何一种情况，则为工作日
        return true;
    }

}

