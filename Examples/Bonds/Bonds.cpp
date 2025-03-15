/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*!
 Copyright (C) 2008 Florent Grenier

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

/*  This example shows how to set up a term structure and then price
    some simple bonds. The last part is dedicated to peripherical
    computations such as "Yield to Price" or "Price to Yield"
 */

// 引入需要的头文件
#include <ql/qldefines.hpp>
#if !defined(BOOST_ALL_NO_LIB) && defined(BOOST_MSVC)
#  include <ql/auto_link.hpp>
#endif
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/instruments/bonds/floatingratebond.hpp>
#include <ql/pricingengines/bond/discountingbondengine.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/bondhelpers.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <iostream>
#include <iomanip>

using namespace QuantLib;

int main(int, char* []) {

    try {

        std::cout << std::endl;

        // 设置欧洲TARGET日历
        Calendar calendar = TARGET();

        // 设置结算日期为2008年9月18日
        Date settlementDate(18, September, 2008);

        // 设置结算天数为3天
        Integer settlementDays = 3;

        // 计算今天的日期（结算日期前推3天）
        Date todaysDate = calendar.advance(settlementDate, -settlementDays, Days);
        // 设置全局评估日期为今天
        Settings::instance().evaluationDate() = todaysDate;

        // 输出今天和结算日期信息
        std::cout << "Today: " << todaysDate.weekday()
        << ", " << todaysDate << std::endl;

        std::cout << "Settlement date: " << settlementDate.weekday()
        << ", " << settlementDate << std::endl;


        /***************************************
         * BUILDING THE DISCOUNTING BOND CURVE *
         ***************************************/

        // RateHelpers are built from the quotes together with
        // other instrument-dependent info.  Quotes are passed in
        // relinkable handles which could be relinked to some other
        // data source later.

        // Note that bootstrapping might not be the optimal choice for
        // bond curves, since it requires to select a set of bonds
        // with maturities that are not too close.  For alternatives,
        // see the FittedBondCurve example.

        // 设置债券赎回价格为100.0
        Real redemption = 100.0;

        // 定义5只债券
        const Size numberOfBonds = 5;

        // 债券发行日期
        Date issueDates[] = {
            Date(15, March, 2005),
            Date(15, June, 2005),
            Date(30, June, 2006),
            Date(15, November, 2002),
            Date(15, May, 1987)
        };

        // 债券到期日期
        Date maturities[] = {
            Date(31, August, 2010),
            Date(31, August, 2011),
            Date(31, August, 2013),
            Date(15, August, 2018),
            Date(15, May, 2038)
        };

        // 票面利率
        Real couponRates[] = {
            0.02375,
            0.04625,
            0.03125,
            0.04000,
            0.04500
        };

        // 市场报价
        Real marketQuotes[] = {
            100.390625,
            106.21875,
            100.59375,
            101.6875,
            102.140625
        };

        // 创建SimpleQuote对象的向量来保存市场报价
        std::vector<ext::shared_ptr<SimpleQuote>> quote;
        for (Real marketQuote : marketQuotes) {
            quote.push_back(ext::make_shared<SimpleQuote>(marketQuote));
        }

        // 创建可重连接的报价句柄数组
        RelinkableHandle<Quote> quoteHandle[numberOfBonds];
        for (Size i=0; i<numberOfBonds; i++) {
            quoteHandle[i].linkTo(quote[i]);
        }
        
        // 创建债券利率助手的向量
        std::vector<ext::shared_ptr<RateHelper>> bondHelpers;

        for (Size i=0; i<numberOfBonds; i++) {

            // 为每只债券创建付息计划表
            Schedule schedule(issueDates[i], maturities[i], Period(Semiannual), calendar,
                              Unadjusted, Unadjusted, DateGeneration::Backward, false);

            // 创建固定利率债券助手
            auto bondHelper = ext::make_shared<FixedRateBondHelper>(
                quoteHandle[i],
                settlementDays,
                100.0,
                schedule,
                std::vector<Rate>(1,couponRates[i]),
                ActualActual(ActualActual::Bond),
                Unadjusted,
                redemption,
                issueDates[i]);

            // the above could also be done by creating a
            // FixedRateBond instance and writing:
            //
            // auto bondHelper = ext::make_shared<BondHelper>(quoteHandle[i], bond);
            //
            // This would also work for bonds that still don't have a
            // specialized helper, such as floating-rate bonds.

            // 将债券助手添加到向量中
            bondHelpers.push_back(bondHelper);
        }

        // The term structure uses its day counter internally to
        // convert between dates and times; it's not required to equal
        // the day counter of the bonds.  In fact, a regular day
        // counter is probably more appropriate.

        // 设置期限结构的日计数约定
        DayCounter termStructureDayCounter = Actual365Fixed();

        // The reference date of the term structure can be the
        // settlement date of the bonds (since, during pricing, it
        // won't be required to discount behind that date) but it can
        // also be today's date.  This allows one to calculate both
        // the price of the bond (based on the settlement date) and
        // the NPV, that is, the value as of today's date of holding
        // the bond and receiving its payments.

        // 创建债券折现期限结构（使用分段收益率曲线）
        auto bondDiscountingTermStructure = ext::make_shared<PiecewiseYieldCurve<Discount,LogLinear>>(
                         todaysDate, bondHelpers, termStructureDayCounter);


        /******************************************
         * BUILDING THE EURIBOR FORECASTING CURVE *
         ******************************************/

        // 6个月存款利率
        Rate d6mQuote=0.03385;
        // 利率互换报价，固定利率对6个月浮动利率
        Rate s2yQuote=0.0295;  // 2年
        Rate s3yQuote=0.0323;  // 3年
        Rate s5yQuote=0.0359;  // 5年
        Rate s10yQuote=0.0412; // 10年
        Rate s15yQuote=0.0433; // 15年

        // 创建SimpleQuote对象来保存各种利率
        auto d6mRate = ext::make_shared<SimpleQuote>(d6mQuote);
        auto s2yRate = ext::make_shared<SimpleQuote>(s2yQuote);
        auto s3yRate = ext::make_shared<SimpleQuote>(s3yQuote);
        auto s5yRate = ext::make_shared<SimpleQuote>(s5yQuote);
        auto s10yRate = ext::make_shared<SimpleQuote>(s10yQuote);
        auto s15yRate = ext::make_shared<SimpleQuote>(s15yQuote);

        // 设置存款相关参数
        DayCounter depositDayCounter = Actual360();
        Natural fixingDays = 2;

        // 创建6个月存款利率助手
        auto d6m = ext::make_shared<DepositRateHelper>(
                 Handle<Quote>(d6mRate),
                 6*Months, fixingDays,
                 calendar, ModifiedFollowing,
                 true, depositDayCounter);

        // 设置利率互换参数
        auto swFixedLegFrequency = Annual;  // 固定端支付频率：每年
        auto swFixedLegConvention = Unadjusted;  // 固定端日期调整约定：不调整
        auto swFixedLegDayCounter = Thirty360(Thirty360::European);  // 固定端日计数约定：欧式30/360
        auto swFloatingLegIndex = ext::make_shared<Euribor6M>();  // 浮动端指数：6个月欧元银行同业拆借利率

        // 创建各期限利率互换助手
        auto s2y = ext::make_shared<SwapRateHelper>(
                 Handle<Quote>(s2yRate), 2*Years,
                 calendar, swFixedLegFrequency,
                 swFixedLegConvention, swFixedLegDayCounter,
                 swFloatingLegIndex);
        auto s3y = ext::make_shared<SwapRateHelper>(
                 Handle<Quote>(s3yRate), 3*Years,
                 calendar, swFixedLegFrequency,
                 swFixedLegConvention, swFixedLegDayCounter,
                 swFloatingLegIndex);
        auto s5y = ext::make_shared<SwapRateHelper>(
                 Handle<Quote>(s5yRate), 5*Years,
                 calendar, swFixedLegFrequency,
                 swFixedLegConvention, swFixedLegDayCounter,
                 swFloatingLegIndex);
        auto s10y = ext::make_shared<SwapRateHelper>(
                 Handle<Quote>(s10yRate), 10*Years,
                 calendar, swFixedLegFrequency,
                 swFixedLegConvention, swFixedLegDayCounter,
                 swFloatingLegIndex);
        auto s15y = ext::make_shared<SwapRateHelper>(
                 Handle<Quote>(s15yRate), 15*Years,
                 calendar, swFixedLegFrequency,
                 swFixedLegConvention, swFixedLegDayCounter,
                 swFloatingLegIndex);

        // 将存款和互换利率助手放入一个向量中
        std::vector<ext::shared_ptr<RateHelper>> depoSwapInstruments;
        depoSwapInstruments.push_back(d6m);
        depoSwapInstruments.push_back(s2y);
        depoSwapInstruments.push_back(s3y);
        depoSwapInstruments.push_back(s5y);
        depoSwapInstruments.push_back(s10y);
        depoSwapInstruments.push_back(s15y);

        // The start of the curve can be today's date or spot, depending on your preferences.
        // Here we're picking spot (mostly because we picked today's date for the bond curve).
        
        // 设置即期日期（今天之后2个工作日）
        Date spotDate = calendar.advance(todaysDate, fixingDays, Days);
        // 创建Euribor预测期限结构
        auto depoSwapTermStructure = ext::make_shared<PiecewiseYieldCurve<Discount, LogLinear>>(
                         spotDate, depoSwapInstruments,
                         termStructureDayCounter);

        /***********
         * PRICING *
         ***********/
        
        // 创建可重连接的收益率期限结构句柄
        RelinkableHandle<YieldTermStructure> discountingTermStructure;
        RelinkableHandle<YieldTermStructure> forecastingTermStructure;

        // bonds to be priced

        // Common data
        // 设置债券面额为100
        Real faceAmount = 100;

        // Pricing engine
        // 创建债券定价引擎（使用折现方法）
        auto bondEngine = ext::make_shared<DiscountingBondEngine>(discountingTermStructure);

        // Zero coupon bond
        // 创建零息债券
        ZeroCouponBond zeroCouponBond(
                 settlementDays,
                 TARGET(),
                 faceAmount,
                 Date(15,August,2013),
                 Following,
                 Real(116.92),
                 Date(15,August,2003));

        // 设置零息债券的定价引擎
        zeroCouponBond.setPricingEngine(bondEngine);

        // Fixed 4.5% bond
        // 创建固定利率债券的支付计划
        Schedule fixedBondSchedule(
                 Date(15, May, 2007), Date(15,May,2017), Period(Annual),
                 TARGET(), Unadjusted, Unadjusted, DateGeneration::Backward, false);

        // 创建固定利率债券（年利率4.5%）
        FixedRateBond fixedRateBond(
                 settlementDays,
                 faceAmount,
                 fixedBondSchedule,
                 std::vector<Rate>(1, 0.045),
                 ActualActual(ActualActual::Bond),
                 ModifiedFollowing,
                 100.0, Date(15, May, 2007));

        // 设置固定利率债券的定价引擎
        fixedRateBond.setPricingEngine(bondEngine);

        // Floating rate bond (6M Euribor + 0.1%)
        // 创建欧元6个月银行同业拆借利率指数，并添加历史定盘利率
        const auto euribor6m = ext::make_shared<Euribor>(Period(6, Months), forecastingTermStructure);
        euribor6m->addFixing(Date(18, October, 2007), 0.026);
        euribor6m->addFixing(Date(17, April, 2008), 0.028);

        // 创建浮动利率债券的支付计划
        Schedule floatingBondSchedule(
                 Date(21, October, 2005), Date(21, October, 2010), Period(Semiannual),
                 TARGET(), Unadjusted, Unadjusted, DateGeneration::Backward, true);

        // 创建浮动利率债券（6个月Euribor + 0.1%）
        FloatingRateBond floatingRateBond(
                 settlementDays,
                 faceAmount,
                 floatingBondSchedule,
                 euribor6m,
                 Actual360(),
                 ModifiedFollowing,
                 Natural(2),
                 // Gearings - 杠杆率
                 std::vector<Real>(1, 1.0),
                 // Spreads - 利差
                 std::vector<Rate>(1, 0.001),
                 // Caps - 利率上限
                 std::vector<Rate>(),
                 // Floors - 利率下限
                 std::vector<Rate>(),
                 // Fixing in arrears - 是否后置定盘
                 false,
                 Real(100.0),
                 Date(21, October, 2005));

        // 设置浮动利率债券的定价引擎
        floatingRateBond.setPricingEngine(bondEngine);

        // Coupon pricers
        // 创建黑色模型利率票据定价器
        auto pricer = ext::make_shared<BlackIborCouponPricer>();

        // optionLet volatilities
        // 设置期权波动率为0
        Volatility volatility = 0.0;
        // 创建期权波动率结构句柄
        Handle<OptionletVolatilityStructure> vol;
        vol = Handle<OptionletVolatilityStructure>(
                 ext::make_shared<ConstantOptionletVolatility>(
                                 settlementDays,
                                 calendar,
                                 ModifiedFollowing,
                                 volatility,
                                 Actual365Fixed()));

        // 设置期权波动率到定价器，并将定价器应用到浮动利率债券的现金流
        pricer->setCapletVolatility(vol);
        setCouponPricer(floatingRateBond.cashflows(),pricer);

        // 连接预测与折现期限结构到相应的模型
        forecastingTermStructure.linkTo(depoSwapTermStructure);
        discountingTermStructure.linkTo(bondDiscountingTermStructure);

        std::cout << std::endl;

        // write column headings
        // 设置输出列宽
        Size widths[] = { 18, 10, 10, 10 };

        // 打印表头
        std::cout << std::setw(widths[0]) <<  "                 "
                  << std::setw(widths[1]) << "ZC"
                  << std::setw(widths[2]) << "Fixed"
                  << std::setw(widths[3]) << "Floating"
                  << std::endl;

        // 打印分隔线
        Size width = widths[0] + widths[1] + widths[2] + widths[3];
        std::string rule(width, '-');

        std::cout << rule << std::endl;

        // 设置输出格式为固定点数表示，保留两位小数
        std::cout << std::fixed;
        std::cout << std::setprecision(2);

        // 打印各债券的净现值
        std::cout << std::setw(widths[0]) << "Net present value"
                  << std::setw(widths[1]) << zeroCouponBond.NPV()
                  << std::setw(widths[2]) << fixedRateBond.NPV()
                  << std::setw(widths[3]) << floatingRateBond.NPV()
                  << std::endl;

        // 打印各债券的净价
        std::cout << std::setw(widths[0]) << "Clean price"
                  << std::setw(widths[1]) << zeroCouponBond.cleanPrice()
                  << std::setw(widths[2]) << fixedRateBond.cleanPrice()
                  << std::setw(widths[3]) << floatingRateBond.cleanPrice()
                  << std::endl;

        // 打印各债券的全价
        std::cout << std::setw(widths[0]) << "Dirty price"
                  << std::setw(widths[1]) << zeroCouponBond.dirtyPrice()
                  << std::setw(widths[2]) << fixedRateBond.dirtyPrice()
                  << std::setw(widths[3]) << floatingRateBond.dirtyPrice()
                  << std::endl;

        // 打印各债券的应计利息
        std::cout << std::setw(widths[0]) << "Accrued coupon"
                  << std::setw(widths[1]) << zeroCouponBond.accruedAmount()
                  << std::setw(widths[2]) << fixedRateBond.accruedAmount()
                  << std::setw(widths[3]) << floatingRateBond.accruedAmount()
                  << std::endl;

        // 打印各债券的前一期票息
        std::cout << std::setw(widths[0]) << "Previous coupon"
                  << std::setw(widths[1]) << "N/A" // 零息债券没有前一期票息
                  << std::setw(widths[2]) << io::rate(fixedRateBond.previousCouponRate())
                  << std::setw(widths[3]) << io::rate(floatingRateBond.previousCouponRate())
                  << std::endl;

        // 打印各债券的下一期票息
        std::cout << std::setw(widths[0]) << "Next coupon"
                  << std::setw(widths[1]) << "N/A" // 零息债券没有下一期票息
                  << std::setw(widths[2]) << io::rate(fixedRateBond.nextCouponRate())
                  << std::setw(widths[3]) << io::rate(floatingRateBond.nextCouponRate())
                  << std::endl;

        // 打印各债券的收益率
        std::cout << std::setw(widths[0]) << "Yield"
                  << std::setw(widths[1])
                  << io::rate(zeroCouponBond.yield(Actual360(),Compounded,Annual))
                  << std::setw(widths[2])
                  << io::rate(fixedRateBond.yield(Actual360(),Compounded,Annual))
                  << std::setw(widths[3])
                  << io::rate(floatingRateBond.yield(Actual360(),Compounded,Annual))
                  << std::endl;

        std::cout << std::endl;

        // Other computations
        // 其他计算示例
        std::cout << "Sample indirect computations (for the floating rate bond): " << std::endl;
        std::cout << rule << std::endl;

        // 根据收益率计算净价
        std::cout << "Yield to Clean Price: "
                  << floatingRateBond.cleanPrice(floatingRateBond.yield(Actual360(),Compounded,Annual),Actual360(),Compounded,Annual,settlementDate) << std::endl;

        // 根据净价计算收益率
        std::cout << "Clean Price to Yield: "
                  << io::rate(floatingRateBond.yield({floatingRateBond.cleanPrice(), Bond::Price::Clean},
                                                     Actual360(), Compounded, Annual, settlementDate))
                  << std::endl
                  << std::endl;

         return 0;

    } catch (std::exception& e) {
        // 异常处理：输出错误信息
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        // 捕获任何其他类型的异常
        std::cerr << "unknown error" << std::endl;
        return 1;
    }
}

