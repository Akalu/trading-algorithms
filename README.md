About
======

This is a PoC project that my journey to the FinTech realm started from. This document describes a highly profitable trading system that I used in real trading on the forex market.

General remarks
================

I developed a lot of different trading systems, but by some reason the most robust and stable ones were those based on the simplest idea. 
In general, in order to gain some speculative profit from price change two conditions are needed: 

1) market’s move, big enough to cover expenses and

2) enter and exit strategy, due to stochastic nature of all financial markets.

The latter condition is tied with imperfection in determine of the exact moment of time when to open position and when to close it.

The year 2008 was a perfect one to gain speculative profit from the volatility on financial markets, and it was an opportunity that I cannot afford myself to loose.
 
Overview
=========


I used trade platform proposed by Libertex Group (https://www.libertexgroup.com/) along with MetaTrader 4 trade terminal. That software used a custom format to save historical data (.cjd files, cjd stands for Component Jet Data). For PoC I used C/C++ and had had to write a simple parser of that format. In the next version of software, in MetaTrader 5, they added the ability to write custom logic in Python, and that enormously simplified the whole process of development of my own trading systems.

My first trading system was very simple and was based on several concepts:

1) follow the trend, i.e. perform only buy operations on growing market and sell operations on falling one. Trend is determined by moving average (MA) charts with exponential summation of previous stock's prices

2) enter and exit strategy was gradual and multi-staged one, and determined by sequential crossing of current price with continuation of three moving average charts of different periods (in particular 8, 20 and 30 hours)

3) use different non-correlated instruments, f.e. commodities currencies (CADUSD) and the base ones (EURUSD), etc

I used 6 instruments: audcad, audusd, nzdusd, eurchf, chfjpy, eurjpyh (hour-bar charts)

The result was pretty good, and trading system allowed to gain a stable profit with acceptable level of risk (the drawdown never was bigger than 25% of base capital).

Here is the report:


start capital 100.000000   end  capital: 679.230103

total deals: 1025

all-profit days: 0 all-lost days: 132 mixed profit days: 237

profitable deals: 418 (40.78%) loss deals: 607 (59.22%)

profit: 1507.100342  loss: 927.869629

avr profit per deal: 3.605503 avr loss per deal: 1.528615  avrProfit/avrLoss=2.368672

maxcap: 679.230103   maxfall: 27.860901

maxgain per deal: 9.000000   maxloss per deal: 5.400000 


Note, that the trading system is not perfect – it is not possible to make all deals the winning ones. This is due to the simple fact that it is not possible to predict future (obviously).

Note also, that the number of loss deals is bigger than the profitable ones, but due to good ratio of average profit vs loss, the trading system generated an excellent profit (up to 400% annual yield).

In real trading it is not always possible to achieve a such level of profitability. From my experience the real level of profit must be evaluated as a 50-70% from the simulated one (this is due to the effect of so-called price slippage). Usually I consider all trading systems which are able to reach a simulated (theoretical) level of profitability bigger that 100% yield as potential candidates for participating in trading on real accounts.

