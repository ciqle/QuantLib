/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2023 Marcin Rybacki

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

#include <ql/cashflows/equitycashflow.hpp>
#include <ql/cashflows/indexedcashflow.hpp>
#include <ql/indexes/equityindex.hpp>
#include <ql/termstructures/yield/quantotermstructure.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

namespace QuantLib {

    namespace {
        // 配置股息收益率曲线的辅助函数
        // 如果没有提供股息曲线，则创建一个零收益率的曲线
        Handle<YieldTermStructure>
            configureDividendHandle(const Handle<YieldTermStructure>& dividendHandle) {
            if (dividendHandle.empty()) {
                ext::shared_ptr<YieldTermStructure> flatTs(ext::make_shared<FlatForward>(
                    0, NullCalendar(), Handle<Quote>(ext::make_shared<SimpleQuote>(0.0)),
                    Actual365Fixed()));
                return Handle<YieldTermStructure>(flatTs);
            }
            return dividendHandle;
        }
    }

    // 为一组现金流设置定价器
    void setCouponPricer(const Leg& leg, const ext::shared_ptr<EquityCashFlowPricer>& p) {
        for (const auto& i : leg) {
            ext::shared_ptr<EquityCashFlow> c =
                ext::dynamic_pointer_cast<EquityCashFlow>(i);
            if (c != nullptr)
                c->setPricer(p);
        }
    }

    // 构造函数：初始化股权现金流
    EquityCashFlow::EquityCashFlow(Real notional,
                                   ext::shared_ptr<EquityIndex> index,
                                   const Date& baseDate,
                                   const Date& fixingDate,
                                   const Date& paymentDate,
                                   bool growthOnly)
    : IndexedCashFlow(notional, std::move(index), baseDate, fixingDate, paymentDate, growthOnly) {}

    // 设置现金流定价器
    void EquityCashFlow::setPricer(const ext::shared_ptr<EquityCashFlowPricer>& pricer) {
        // 如果已经有定价器，先取消注册
        if (pricer_ != nullptr)
            unregisterWith(pricer_);
        pricer_ = pricer;
        // 注册新的定价器并更新
        if (pricer_ != nullptr)
            registerWith(pricer_);
        update();
    }

    // 计算现金流金额
    Real EquityCashFlow::amount() const {
        // 如果没有设置定价器，则使用基类的计算方法
        if (!pricer_)
            return IndexedCashFlow::amount();
        pricer_->initialize(*this);
        return notional() * pricer_->price();
    }

    // Quanto股权现金流定价器构造函数
	EquityQuantoCashFlowPricer::EquityQuantoCashFlowPricer(
        Handle<YieldTermStructure> quantoCurrencyTermStructure,
        Handle<BlackVolTermStructure> equityVolatility,
        Handle<BlackVolTermStructure> fxVolatility,
        Handle<Quote> correlation)
    : quantoCurrencyTermStructure_(std::move(quantoCurrencyTermStructure)),
      equityVolatility_(std::move(equityVolatility)), fxVolatility_(std::move(fxVolatility)),
      correlation_(std::move(correlation)){
        // 注册对各种市场数据的观察
        registerWith(quantoCurrencyTermStructure_);
        registerWith(equityVolatility_);
        registerWith(fxVolatility_);
        registerWith(correlation_);
    }

    // 初始化Quanto定价器
    void EquityQuantoCashFlowPricer::initialize(const EquityCashFlow& cashFlow) {
        // 尝试将指数类型转换为股权指数
        index_ = ext::dynamic_pointer_cast<EquityIndex>(cashFlow.index());
        if (!index_) {
            QL_FAIL("Equity index required.");
        }
        baseDate_ = cashFlow.baseDate();
        fixingDate_ = cashFlow.fixingDate();
        QL_REQUIRE(fixingDate_ >= baseDate_, "Fixing date cannot fall before base date.");
        growthOnlyPayoff_ = cashFlow.growthOnly();
        
        // 确保所有必要的市场数据都已提供
        QL_REQUIRE(!quantoCurrencyTermStructure_.empty(),
                   "Quanto currency term structure handle cannot be empty.");
        QL_REQUIRE(!equityVolatility_.empty(),
                   "Equity volatility term structure handle cannot be empty.");
        QL_REQUIRE(!fxVolatility_.empty(),
                   "FX volatility term structure handle cannot be empty.");
        QL_REQUIRE(!correlation_.empty(), "Correlation handle cannot be empty.");

        // 确保所有市场数据使用相同的参考日期
        QL_REQUIRE(quantoCurrencyTermStructure_->referenceDate() ==
                           equityVolatility_->referenceDate() &&
                       equityVolatility_->referenceDate() == fxVolatility_->referenceDate(),
                   "Quanto currency term structure, equity and FX volatility need to have the same "
                   "reference date.");
    }

    // 计算Quanto调整后的价格
    Real EquityQuantoCashFlowPricer::price() const {
        Real strike = index_->fixing(fixingDate_);
        // 获取股息收益率曲线
        Handle<YieldTermStructure> dividendHandle =
            configureDividendHandle(index_->equityDividendCurve());

        // 创建Quanto调整后的收益率曲线
        Handle<YieldTermStructure> quantoTermStructure(ext::make_shared<QuantoTermStructure>(
            dividendHandle, quantoCurrencyTermStructure_,
            index_->equityInterestRateCurve(), equityVolatility_, strike, fxVolatility_, 1.0,
            correlation_->value()));
        
        // 创建Quanto调整后的股权指数
        ext::shared_ptr<EquityIndex> quantoIndex =
            index_->clone(quantoCurrencyTermStructure_, quantoTermStructure, index_->spot());

        // 计算基础日期和固定日期的指数值
        Real I0 = quantoIndex->fixing(baseDate_);
        Real I1 = quantoIndex->fixing(fixingDate_);

        // 根据是否只考虑增长返回适当的结果
        if (growthOnlyPayoff_)
            return I1 / I0 - 1.0;
        return I1 / I0;
    }
}