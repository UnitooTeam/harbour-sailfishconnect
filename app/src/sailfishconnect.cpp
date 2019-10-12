/*
 * Copyright 2018 Richard Liebscher <richard.liebscher@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <memory>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QtQml>
#include <QQuickView>
#include <QGuiApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <sailfishapp.h>

#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>

#include "appdaemon.h"
#include <device.h>
#include <kdeconnectplugin.h>
//#include "plugins/mprisremote/mprisremoteplugin.h"
//#include "plugins/touchpad/touchpadplugin.h"
#include "ui/devicelistmodel.h"
#include "ui/sortfiltermodel.h"
#include "ui/devicepluginsmodel.h"
// #include "ui/mprisplayersmodel.h"
// #include "ui/jobsmodel.h"
#include "ui.h"
#include "js/qmlregister.h"
#include "dbus/ofono.h"

#include <QtPlugin>

Q_IMPORT_PLUGIN(opensslPlugin)

namespace SailfishConnect {

static Q_LOGGING_CATEGORY(logger, "sailfishconnect.ui")


QString PACKAGE_VERSION = QStringLiteral("0.3");

QString DBUS_SERVICE_NAME =
        QStringLiteral("org.harbour.SailfishConnect");

QString PACKAGE_NAME =
        QStringLiteral("harbour-sailfishconnect");

QString PRETTY_PACKAGE_NAME =
        QStringLiteral("Sailfish Connect");

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = qFormatLogMessage(type, context, msg).toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
    case QtInfoMsg:
    case QtWarningMsg:
    case QtCriticalMsg:
        fprintf(stderr, "%s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        fprintf(stderr, "%s\n", localMsg.constData());
        abort();
    }
}

void registerQmlTypes() {
    // TODO: register in plugin factories when possible
    qmlRegisterType<DeviceListModel>(
                "SailfishConnect.UI", 0, 3, "DeviceListModel");
    qmlRegisterType<SortFilterModel>(
                "SailfishConnect.UI", 0, 3, "SortFilterModel");
    qmlRegisterType<DevicePluginsModel>(
                "SailfishConnect.UI", 0, 3, "DevicePluginsModel");
    // qmlRegisterType<JobsModel>(
    //             "SailfishConnect.UI", 0, 3, "JobsModel");
    // qmlRegisterType<MprisPlayersModel>(
    //             "SailfishConnect.UI", 0, 3, "MprisPlayersModel");

    qmlRegisterUncreatableType<Device>(
                "SailfishConnect.Core", 0, 3, "Device",
                QStringLiteral("Instances of are only creatable from C++."));
    qmlRegisterUncreatableType<KdeConnectPlugin>(
                "SailfishConnect.Core", 0, 3, "Plugin",
                QStringLiteral("Instances of are only creatable from C++."));

    // qmlRegisterUncreatableType<MprisPlayer>(
    //             "SailfishConnect.Mpris", 0, 3, "MprisPlayer",
    //             QStringLiteral("not intented to be created from users"));
    // qmlRegisterUncreatableType<TouchpadPlugin>(
    //             "SailfishConnect.RemoteControl", 0, 3, "RemoteControlPlugin",
    //             QStringLiteral("not intented to be created from users"));    

    QmlJs::registerTypes();
}

std::unique_ptr<QGuiApplication> createApplication(int &argc, char **argv)
{
    std::unique_ptr<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setApplicationVersion(PACKAGE_VERSION);
    return app;
}

} // SailfishConnect

int main(int argc, char *argv[])
{  
    using namespace SailfishConnect;

    qDebug() << __PRETTY_FUNCTION__ << __LINE__;
    qInstallMessageHandler(myMessageOutput);

    auto app = createApplication(argc, argv);
qDebug() << __PRETTY_FUNCTION__ << __LINE__;
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.registerService(DBUS_SERVICE_NAME)) {
        qCInfo(logger) << "Other daemon exists.";
        UI::raise();
        return 0;
    }
qDebug() << __PRETTY_FUNCTION__ << __LINE__;
    Ofono::registerTypes();
    registerQmlTypes();
qDebug() << __PRETTY_FUNCTION__ << __LINE__;
    AppDaemon daemon;
    qDebug() << __PRETTY_FUNCTION__ << __LINE__;
    //KeyboardLayoutProvider keyboardLayoutProvider;
    UI ui(&daemon);
    ui.showMainWindow();
qDebug() << __PRETTY_FUNCTION__ << __LINE__;
    return app->exec();
}
