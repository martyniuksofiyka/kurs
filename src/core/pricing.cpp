#include "pricing.h"

namespace Pricing {

Money mulDiv(Money a, qint64 num, qint64 den)
{
    if (den == 0)
        return 0;
    const qint64 p = a * num;
    // округлення "половина - вгору" для додатних чисел
    return p >= 0 ? (p + den / 2) / den : (p - den / 2) / den;
}

int halfHours(const QDateTime &from, const QDateTime &to)
{
    if (!from.isValid() || !to.isValid() || to <= from)
        return 0;
    const qint64 minutes = (from.secsTo(to) + 59) / 60;
    return int((minutes + 29) / 30);
}

bool isWeekend(const QDate &date)
{
    return date.dayOfWeek() >= 6;
}

Money hallCost(Money pricePerHour, const QDateTime &from, const QDateTime &to)
{
    Money cost = mulDiv(pricePerHour, halfHours(from, to), 2);
    if (isWeekend(from.date()))
        cost = mulDiv(cost, WeekendPercent, 100);
    return cost;
}

Money propCostHourly(Money pricePerHour, int qty, int halfHourCount)
{
    if (qty <= 0 || halfHourCount <= 0)
        return 0;
    return mulDiv(pricePerHour * qty, halfHourCount, 2);
}

int rentalDays(const QDate &from, const QDate &to)
{
    if (!from.isValid() || !to.isValid() || to < from)
        return 0;
    return int(from.daysTo(to)) + 1;
}

Money propCostDaily(Money pricePerDay, int qty, int days)
{
    if (qty <= 0 || days <= 0)
        return 0;
    return pricePerDay * qty * days;
}

int loyaltyDiscount(int completedVisits)
{
    if (completedVisits >= 10)
        return 10;
    if (completedVisits >= 3)
        return 5;
    return 0;
}

int effectiveDiscount(int manual, int loyalty)
{
    return qBound(0, qMax(manual, loyalty), MaxDiscount);
}

Money applyDiscount(Money sum, int discountPct)
{
    return mulDiv(sum, 100 - qBound(0, discountPct, 100), 100);
}

Money lateFine(Money pricePerDay, int qty, const QDate &planned, const QDate &returned)
{
    if (!planned.isValid() || !returned.isValid() || returned <= planned || qty <= 0)
        return 0;
    return mulDiv(pricePerDay * qty * planned.daysTo(returned), LateFinePercent, 100);
}

Money fromHryvnia(double value)
{
    return qRound64(value * 100.0);
}

double toHryvnia(Money value)
{
    return value / 100.0;
}

}
