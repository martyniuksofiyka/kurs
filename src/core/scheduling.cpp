#include "scheduling.h"

#include <QMap>

namespace Scheduling {

bool overlaps(const QDateTime &a1, const QDateTime &a2,
              const QDateTime &b1, const QDateTime &b2)
{
    return a1 < b2 && b1 < a2;
}

int peakUsage(const QList<Interval> &busy, const QDateTime &from, const QDateTime &to)
{
    // метод "сканувальної прямої": +qty на початку інтервалу, -qty в кінці
    QMap<QDateTime, int> events;
    for (const Interval &i : busy) {
        if (!overlaps(i.from, i.to, from, to))
            continue;
        QDateTime s = qMax(i.from, from);
        QDateTime e = qMin(i.to, to);
        events[s] += i.qty;
        events[e] -= i.qty;
    }

    int current = 0;
    int peak = 0;
    for (auto it = events.cbegin(); it != events.cend(); ++it) {
        current += it.value();
        peak = qMax(peak, current);
    }
    return peak;
}

int available(int total, const QList<Interval> &busy, const QDateTime &from, const QDateTime &to)
{
    return qMax(0, total - peakUsage(busy, from, to));
}

QDateTime findFreeSlot(const QList<Interval> &busy, const QDate &date, int durationMinutes,
                       const QTime &open, const QTime &close, const QTime &preferred)
{
    if (durationMinutes <= 0 || !date.isValid())
        return QDateTime();

    QDateTime best;
    qint64 bestDistance = -1;
    const QDateTime dayEnd(date, close);
    const QDateTime want(date, preferred);

    // перебираємо всі можливі початки з кроком 30 хв і залишаємо найближчий
    for (QDateTime start(date, open); start.addSecs(durationMinutes * 60) <= dayEnd; start = start.addSecs(1800)) {
        const QDateTime end = start.addSecs(durationMinutes * 60);
        bool free = true;
        for (const Interval &i : busy) {
            if (overlaps(start, end, i.from, i.to)) {
                free = false;
                break;
            }
        }
        if (!free)
            continue;
        const qint64 distance = qAbs(start.secsTo(want));
        if (bestDistance < 0 || distance < bestDistance) {
            best = start;
            bestDistance = distance;
        }
    }
    return best;
}

bool withinWorkingHours(const QDateTime &from, const QDateTime &to,
                        const QTime &open, const QTime &close)
{
    // бронювання має починатися і закінчуватися в межах одного робочого дня
    if (from.date() != to.date() || to <= from)
        return false;
    return from.time() >= open && to.time() <= close;
}

}
