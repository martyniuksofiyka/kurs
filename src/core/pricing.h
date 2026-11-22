#ifndef PRICING_H
#define PRICING_H

#include <QDateTime>

// Розрахунок вартості послуг студії.
// Усі суми зберігаються в копійках (ціле число), щоб уникнути похибок
// округлення, які дає тип double при роботі з грошима.
// Функції не звертаються до БД, тому їх зручно перевіряти тестами.
namespace Pricing {

using Money = qint64;

// надбавка за суботу та неділю: +20 %
constexpr int WeekendPercent = 120;
// максимальна знижка, яку може дати менеджер
constexpr int MaxDiscount = 30;
// штраф за кожен день прострочення: 150 % добової ціни
constexpr int LateFinePercent = 150;

// a * num / den з округленням до найближчого цілого
Money mulDiv(Money a, qint64 num, qint64 den);

// кількість півгодинних інтервалів (неповний інтервал рахується як повний)
int halfHours(const QDateTime &from, const QDateTime &to);

bool isWeekend(const QDate &date);

// вартість оренди залу з урахуванням надбавки за вихідні
Money hallCost(Money pricePerHour, const QDateTime &from, const QDateTime &to);

// вартість реквізиту, який бронюється разом із залом
Money propCostHourly(Money pricePerHour, int qty, int halfHourCount);

// кількість днів оренди, обидві дати включно
int rentalDays(const QDate &from, const QDate &to);

Money propCostDaily(Money pricePerDay, int qty, int days);

// накопичувальна знижка за кількістю завершених візитів
int loyaltyDiscount(int completedVisits);

// діє більша зі знижок, але не більше MaxDiscount
int effectiveDiscount(int manual, int loyalty);

Money applyDiscount(Money sum, int discountPct);

// штраф за несвоєчасне повернення реквізиту
Money lateFine(Money pricePerDay, int qty, const QDate &planned, const QDate &returned);

// перетворення для полів введення (гривні з копійками)
Money fromHryvnia(double value);
double toHryvnia(Money value);

}

#endif // PRICING_H
