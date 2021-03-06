/*
 * Copyright 2013 Albert Vaca <albertvaka@gmail.com>
 * Copyright 2019 Richard Liebscher <richard.liebscher@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lanuploadjob.h"

#include <QDebug>
#include <QFile>

#include "lanlinkprovider.h"
#include "../../kdeconnectconfig.h"
#include "../../corelogging.h"

namespace SailfishConnect {

LanUploadJob::LanUploadJob(
        const NetworkPacket &np, const QString& deviceId,
        LanLinkProvider* provider, QObject* parent)
    : CopyJob(deviceId, np.payload(), QSharedPointer<QIODevice>(), np.payloadSize(), parent)
    , m_provider(provider)
    , m_server(new Server(this))
    , m_socket(nullptr)
    , m_port(0)
{
    connect(source().data(), &QIODevice::readyRead,
            this, &LanUploadJob::startUploading);
}

void LanUploadJob::start()
{
    m_port = MIN_PORT;
    while (!m_server->listen(QHostAddress::Any, m_port)) {
        m_port++;
        if (m_port > MAX_PORT) {
            // No ports available?
            qCWarning(coreLogger)
                    << "Error opening a port in range"
                    << MIN_PORT << "-" << MAX_PORT;
            m_port = 0;
            setError(2);
            setErrorText(tr("Couldn't find an available port"));
            return emitResult();
        }
    }
    connect(m_server, &QTcpServer::newConnection,
            this, &LanUploadJob::newConnection);
}

void LanUploadJob::newConnection()
{
    qCDebug(coreLogger) << "connection for payload upload";
    if (!source()->open(QIODevice::ReadOnly)) {
        qCWarning(coreLogger) << "error when opening the input to upload";
        setError(2);
        setErrorText(source()->errorString());
        return emitResult();
    }

    m_socket = QSharedPointer<QSslSocket>(m_server->nextPendingConnection());
    setDestination(m_socket);

    connect(m_socket.data(), &QSslSocket::encrypted,
            this, &LanUploadJob::startUploading);
//     connect(mSocket, &QAbstractSocket::stateChanged,
//             this, [](QAbstractSocket::SocketState state){
//                  qDebug() << "statechange" << state; });

    m_provider->configureSslSocket(m_socket.data(), deviceId(), true);
    m_socket->startServerEncryption();
}

void LanUploadJob::startUploading()
{
    qCDebug(coreLogger) << "start payload upload";
    CopyJob::doStart();
}

QVariantMap LanUploadJob::transferInfo()
{
    Q_ASSERT(isOkay());
    return {{"port", m_port}};
}

QString LanUploadJob::fileName()
{
    // TODO: set from outside
    QFile* file = qobject_cast<QFile*>(source().data());
    if (file) {
        return file->fileName();
    } else {
        return QString();
    }
}

} // namespace SailfishConnect
