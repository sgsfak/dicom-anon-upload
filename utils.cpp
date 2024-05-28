#include "utils.h"
#include <QSettings>

QString mdicom_path()
{
#ifdef Q_OS_WINDOWS
    QSettings s("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\mDicom.exe",
                QSettings::NativeFormat);
    return s.value("Default").toString();
#else
    return "";
#endif
}
