#include "session.h"

#include <QSettings>

Session &Session::instance()
{
    static Session s;
    return s;
}

QTime Session::openTime() const
{
    QSettings st;
    return QTime::fromString(st.value("studio/open", "08:00").toString(), "HH:mm");
}

QTime Session::closeTime() const
{
    QSettings st;
    return QTime::fromString(st.value("studio/close", "22:00").toString(), "HH:mm");
}

void Session::setWorkingHours(const QTime &open, const QTime &close)
{
    QSettings st;
    st.setValue("studio/open", open.toString("HH:mm"));
    st.setValue("studio/close", close.toString("HH:mm"));
}

QString Session::studioName() const
{
    QSettings st;
    return st.value("studio/name", QStringLiteral("Фотостудія «Світлотінь»")).toString();
}

void Session::setStudioName(const QString &name)
{
    QSettings st;
    st.setValue("studio/name", name);
}
