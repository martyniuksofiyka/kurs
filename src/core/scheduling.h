#ifndef SCHEDULING_H
#define SCHEDULING_H

#include <QDateTime>
#include <QList>
#include <QTime>

// Перевірка розкладу: перетини інтервалів та доступність реквізиту.
namespace Scheduling {

struct Interval {
    QDateTime from;
    QDateTime to;
    int qty = 0;      // для реквізиту - скільки одиниць зайнято
};

// інтервали [a1, a2) та [b1, b2) перетинаються
bool overlaps(const QDateTime &a1, const QDateTime &a2,
              const QDateTime &b1, const QDateTime &b2);

// найбільша кількість одиниць, зайнятих одночасно протягом [from, to)
int peakUsage(const QList<Interval> &busy, const QDateTime &from, const QDateTime &to);

// скільки одиниць вільно на весь проміжок
int available(int total, const QList<Interval> &busy, const QDateTime &from, const QDateTime &to);

// найближчий до бажаного часу вільний проміжок заданої тривалості
// в межах робочого дня; крок пошуку - 30 хвилин.
// Повертає невалідний QDateTime, якщо вільного часу немає.
QDateTime findFreeSlot(const QList<Interval> &busy, const QDate &date, int durationMinutes,
                       const QTime &open, const QTime &close, const QTime &preferred);

// чи вкладається бронювання у робочі години студії
bool withinWorkingHours(const QDateTime &from, const QDateTime &to,
                        const QTime &open, const QTime &close);

}

#endif // SCHEDULING_H
