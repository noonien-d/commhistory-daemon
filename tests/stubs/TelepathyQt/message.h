/* Message object used by text channel client-side proxy
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _TelepathyQt4_message_h_HEADER_GUARD_
#define _TelepathyQt4_message_h_HEADER_GUARD_

#include <QSharedDataPointer>

#include "Contact"
#include "Types"
#include "Constants"
#include "Types"

class QDateTime;

namespace Tp
{

class Contact;
class TextChannel;

class Message
{
public:
    Message();
    Message(const MessagePartList &parts);
    Message(uint, uint, const QString &);

    Message(ChannelTextMessageType, const QString &);
    Message(const Message &other);
    ~Message();

    Message &operator=(const Message &other);
    bool operator==(const Message &other) const;
    inline bool operator!=(const Message &other) const
    {
        return !(*this == other);
    }

    // Convenient access to headers

    QDateTime sent() const;

    ChannelTextMessageType messageType() const;

    QString messageToken() const;

    QString text() const;

    // Direct access to the whole message (header and body)

    MessagePart header() const;

    int size() const;
    MessagePart part(uint index) const;
    MessagePartList parts() const;

    MessagePart& ut_part(uint index);

private:
    friend class TextChannel;
    friend class ReceivedMessage;

    struct Private;
    friend struct Private;
    QSharedDataPointer<Private> mPriv;
};

class ReceivedMessage : public Message
{
public:
    class DeliveryDetails
    {
    public:
        DeliveryDetails();
        DeliveryDetails(const DeliveryDetails &other);
        ~DeliveryDetails();

        DeliveryDetails &operator=(const DeliveryDetails &other);

        bool isValid() const { return mPriv.constData() != nullptr; }

        DeliveryStatus status() const;

    private:
        friend class ReceivedMessage;

        DeliveryDetails(const MessagePartList &parts);

        struct Private;
        friend struct Private;
        QSharedDataPointer<Private> mPriv;
    };

    ReceivedMessage(const MessagePartList &parts);
    ReceivedMessage();

    ReceivedMessage(const ReceivedMessage &other);
    ReceivedMessage &operator=(const ReceivedMessage &other);
    ~ReceivedMessage();

    QDateTime received() const;
    ContactPtr sender() const;
    bool isScrollback() const;
    bool isSilent() const;

    DeliveryDetails deliveryDetails() const;

public: //ut
    void ut_setSender(const ContactPtr& sender);

private:
    friend class TextChannel;
};

} // Tp

#endif
