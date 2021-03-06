/*
 * Copyright 2013 Albert Vaca <albertvaka@gmail.com>
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

#include "networkpacket.h"

#include <QMetaObject>
#include <QMetaProperty>
#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QDebug>
#include <QLoggingCategory>

#include "corelogging.h"
#include "kdeconnectconfig.h"
#include "pluginloader.h"
#include "downloadjob.h"

using namespace SailfishConnect;

QDebug operator<<(QDebug s, const NetworkPacket& pkg)
{
    s.nospace() << "NetworkPacket(" << pkg.type() << ':' << pkg.body();
    if (pkg.hasPayload()) {
        s.nospace() << ":withpayload";
    }
    s.nospace() << ')';
    return s.space();
}

const int NetworkPacket::s_protocolVersion = 7;

NetworkPacket::NetworkPacket(const QString& type, const QVariantMap& body)
    : m_id(QString::number(QDateTime::currentMSecsSinceEpoch()))
    , m_type(type)
    , m_body(body)
    , m_payload()
    , m_payloadSize(0)
{
}

void NetworkPacket::createIdentityPacket(KdeConnectConfig* config, NetworkPacket* np)
{
    np->m_id = QString::number(QDateTime::currentMSecsSinceEpoch());
    np->m_type = PACKET_TYPE_IDENTITY;
    np->m_payload = QSharedPointer<QIODevice>();
    np->m_payloadSize = 0;
    np->set(QStringLiteral("deviceId"), config->deviceId());
    np->set(QStringLiteral("deviceName"), config->name());
    np->set(QStringLiteral("deviceType"), config->deviceType());
    np->set(QStringLiteral("protocolVersion"), NetworkPacket::s_protocolVersion);
    np->set(QStringLiteral("incomingCapabilities"), PluginManager::instance()->incomingCapabilities());
    np->set(QStringLiteral("outgoingCapabilities"), PluginManager::instance()->outgoingCapabilities());

    //qCDebug(coreLogger) << "createIdentityPacket" << np->serialize();
}

template<class T>
QVariantMap qobject2qvariant(const T* object)
{
    QVariantMap map;
    auto metaObject = T::staticMetaObject;
    for(int i = metaObject.propertyOffset(); i < metaObject.propertyCount(); ++i) {
        QMetaProperty prop = metaObject.property(i);
        map.insert(QString::fromLatin1(prop.name()), prop.readOnGadget(object));
    }

    return map;
}

QByteArray NetworkPacket::serialize() const
{
    //Object -> QVariant
    //QVariantMap variant;
    //variant["id"] = mId;
    //variant["type"] = mType;
    //variant["body"] = mBody;
    QVariantMap variant = qobject2qvariant(this);

    if (hasPayload()) {
        qCDebug(coreLogger) << "Serializing payloadTransferInfo";
        variant[QStringLiteral("payloadSize")] = payloadSize();
        variant[QStringLiteral("payloadTransferInfo")] = m_payloadTransferInfo;
    }

    //QVariant -> json
    auto jsonDocument = QJsonDocument::fromVariant(variant);
    QByteArray json = jsonDocument.toJson(QJsonDocument::Compact);
    if (json.isEmpty()) {
        qCDebug(coreLogger) << "Serialization error";
    } else {
        //qCDebug(coreLogger) << "Serialized packet:" << json;
        json.append('\n');
    }

    return json;
}

template <class T>
void qvariant2qobject(const QVariantMap& variant, T* object)
{
    for ( QVariantMap::const_iterator iter = variant.begin(); iter != variant.end(); ++iter )
    {
        const int propertyIndex = T::staticMetaObject.indexOfProperty(iter.key().toLatin1());
        if (propertyIndex < 0) {
            qCWarning(coreLogger) << "missing property" << object << iter.key();
            continue;
        }

        QMetaProperty property = T::staticMetaObject.property(propertyIndex);
        bool ret = property.writeOnGadget(object, *iter);
        if (!ret) {
            qCWarning(coreLogger) << "couldn't set" << object << "->" << property.name() << '=' << *iter;
        }
    }
}

bool NetworkPacket::unserialize(const QByteArray& a, NetworkPacket* np)
{
    //Json -> QVariant
    QJsonParseError parseError;
    auto parser = QJsonDocument::fromJson(a, &parseError);
    if (parser.isNull()) {
        qCDebug(coreLogger) << "Unserialization error:" << parseError.errorString();
        return false;
    }

    auto variant = parser.toVariant().toMap();
    qvariant2qobject(variant, np);

    // Will return 0 if was not present, which is ok
    np->m_payloadSize = variant[QStringLiteral("payloadSize")].toLongLong();
    if (np->m_payloadSize == -1) {
        np->m_payloadSize = np->get<qint64>(QStringLiteral("size"), -1);
    }

    if (np->m_payloadSize < -1) {
        qCWarning(coreLogger) << "Invalid payload size" << np->m_payloadSize;
    }

    //Will return an empty qvariantmap if was not present, which is ok
    np->m_payloadTransferInfo = variant[QStringLiteral("payloadTransferInfo")].toMap();

    return true;
}

KJob* NetworkPacket::createDownloadPayloadJob(
        const QString& deviceId, const QString& destination) const
{
    return new DownloadJob(deviceId, payload(), destination, payloadSize());
}

