/*
 * This file contains D-Bus client proxy classes generated by qt-client-gen.py.
 *
 * This file can be distributed under the same terms as the specification from
 * which it was generated.
 */

#include "Types"
#include "Constants"

#include <QString>
#include <QObject>
#include <QVariant>

#include <QDBusPendingReply>

#include "AbstractInterface"
#include "DBusProxy"

namespace Tp
{

    class PendingOperation;

namespace Client
{

/**
 * \class PropertiesInterfaceInterface
 * \headerfile TelepathyQt/properties.h <TelepathyQt/Properties>
 * \ingroup clientprops
 *
 * Proxy class providing a 1:1 mapping of the D-Bus interface "org.freedesktop.Telepathy.Properties."
 */
class PropertiesInterfaceInterface : public Tp::AbstractInterface
{
    Q_OBJECT

public:
    /**
     * Returns the name of the interface "org.freedesktop.Telepathy.Properties", which this class
     * represents.
     *
     * \return The D-Bus interface name.
     */
    static inline const char *staticInterfaceName()
    {
        return "org.freedesktop.Telepathy.Properties";
    }

    /**
     * Creates a PropertiesInterfaceInterface associated with the given object on the session bus.
     *
     * \param busName Name of the service the object is on.
     * \param objectPath Path to the object on the service.
     * \param parent Passed to the parent class constructor.
     */
    PropertiesInterfaceInterface(
        const QString& busName,
        const QString& objectPath,
        QObject* parent = 0
    );

    /**
     * Creates a PropertiesInterfaceInterface associated with the same object as the given proxy.
     * Additionally, the created proxy will have the same parent as the given
     * proxy.
     *
     * \param mainInterface The proxy to use.
     */
    explicit PropertiesInterfaceInterface(const Tp::AbstractInterface&);

public Q_SLOTS:
    /**
     * Begins a call to the D-Bus method "GetProperties" on the remote object.
     *
     * Returns an array of (identifier, value) pairs containing the current
     * values of the given properties.
     *
     * \param properties
     *
     *     An array of property identifiers
     *
     * \return
     *
     *     <p>An array of structs containing:</p>
     *     <ul>
     *       <li>integer identifiers</li>
     *       <li>variant boxed values</li>
     *     </ul>
     */
    QDBusPendingReply<Tp::PropertyValueList> GetProperties(const Tp::UIntList& properties)
    {
        Q_UNUSED(properties)
        return asyncCall(QLatin1String("GetProperties"));
    }

    /**
     * Begins a call to the D-Bus method "ListProperties" on the remote object.
     *
     * Returns a dictionary of the properties available on this channel.
     *
     * \return
     *
     *     An array of structs containing: an integer identifier a string
     *     property name a string representing the D-Bus signature of this
     *     property a bitwise OR of the flags applicable to this property
     */
    QDBusPendingReply<Tp::PropertySpecList> ListProperties()
    {
        return asyncCall(QLatin1String("ListProperties"));
    }

Q_SIGNALS:
    /**
     * Represents the signal "PropertiesChanged" on the remote object.
     *
     * Emitted when the value of readable properties has changed.
     *
     * \param properties
     *
     *     <p>An array of structs containing:</p>
     *     <ul>
     *       <li>integer identifiers</li>
     *       <li>variant boxed values</li>
     *     </ul>
     *     <p>The array should contain only properties whose values have
     *       actually changed.</p>
     */
    void PropertiesChanged(const Tp::PropertyValueList& properties);

public: // stub methods
    void ut_setPropertyValues(const Tp::PropertyValueList& properties);
    void ut_setPropertySpecList(const Tp::PropertySpecList& specList);
};
}
}

Q_DECLARE_METATYPE(Tp::Client::PropertiesInterfaceInterface*)
