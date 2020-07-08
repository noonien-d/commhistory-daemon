/* Generated from CommHistory telepathy-qt extensions
 */

#include <QFlags>
#include <QList>

/**
 * \addtogroup typesconstants Types and constants
 *
 * Enumerated, flag, structure, list and mapping types and utility constants.
 */

/**
 * \defgroup flagtypeconsts Flag type constants
 * \ingroup typesconstants
 *
 * Types generated from the specification representing bit flag constants and
 * combinations of them (bitfields).
 */

/**
 * \defgroup enumtypeconsts Enumerated type constants
 * \ingroup typesconstants
 *
 * Types generated from the specification representing enumerated types ie.
 * types the values of which are mutually exclusive integral constants.
 */

/**
 * \defgroup ifacestrconsts Interface string constants
 * \ingroup typesconstants
 *
 * D-Bus interface names of the interfaces in the specification.
 */

/**
 * \defgroup errorstrconsts Error string constants
 * \ingroup typesconstants
 *
 * Names of the D-Bus errors in the specification.
 */

/**
 * \ingroup flagtypeconsts
 *
 * Flag type generated from the specification.
 */
enum ContactInfoFieldFlag
{
    /**
     * <p>If present, exactly the parameters indicated must be set on this
     *   field; in the case of an empty list of parameters, this implies that
     *   parameters may not be used.</p>
     *
     * <p>If absent, and the list of allowed parameters is non-empty,
     *   any (possibly empty) subset of that list may be
     *   used.</p>
     *
     * <p>If absent, and the list of allowed parameters is empty,
     *   any parameters may be used.</p>
     */
     ContactInfoFieldFlagParametersMandatory = 1
};

/**
 * \typedef QFlags<ContactInfoFieldFlag> ContactInfoFieldFlags
 * \ingroup flagtypeconsts
 *
 * Type representing combinations of #ContactInfoFieldFlag values.
 *
 * Flags describing the behaviour of a vCard field.
 */
typedef QFlags<ContactInfoFieldFlag> ContactInfoFieldFlags;
Q_DECLARE_OPERATORS_FOR_FLAGS(ContactInfoFieldFlags)

/**
 * \enum ContactInfoFlag
 * \ingroup enumtypeconsts
 *
 * Enumerated type generated from the specification.
 *
 * Flags defining the behaviour of contact information on this protocol. Some
 * protocols provide no information on contacts without an explicit request;
 * others always push information to the connection manager as and when it
 * changes.
 */
enum ContactInfoFlag
{
    /**
     * Indicates that SetContactInfo is supported on this connection.
     */
     ContactInfoFlagCanSet = 1,

    /**
     * Indicates that the protocol pushes all contacts&apos; information to
     * the connection manager without prompting. If set, RequestContactInfo
     * will not cause a network roundtrip and ContactInfoChanged will be
     * emitted whenever contacts&apos; information changes.
     */
     ContactInfoFlagPush = 2
};

/**
 * \ingroup enumtypeconsts
 *
 * 1 higher than the highest valid value of ContactInfoFlag.
 */
const int NUM_CONTACT_INFO_FLAGS = (2+1);

/**
 * \enum AccessPolicy
 * \ingroup enumtypeconsts
 *
 * Enumerated type generated from the specification.
 *
 * Values to define acceptance policy for a certain kind of 	 incoming
 * communication based on the status of the initiator 	 contact.
 */
enum AccessPolicy
{
    /**
     * Any remote contact can contact the local user.
     */
     AccessPolicyAnyone = 0,

    /**
     * Only contacts in the buddy list or the authorized contacts 	 (in
     * Telepathy, members of the &quot;stored&quot; contact list) can
     * contact the local user.
     */
     AccessPolicyAuthorizedOnly = 1
};

/**
 * \ingroup enumtypeconsts
 *
 * 1 higher than the highest valid value of AccessPolicy.
 */
const int NUM_ACCESS_POLICYS = (1+1);

/**
 * \enum ChannelContactSearchState
 * \ingroup enumtypeconsts
 *
 * Enumerated type generated from the specification.
 */
enum ChannelContactSearchState
{
    /**
     * The search has not started
     */
     ChannelContactSearchStateNotStarted = 0,

    /**
     * The search is in progress
     */
     ChannelContactSearchStateInProgress = 1,

    /**
     * The search has paused, but more results can be retrieved by calling
     * More.
     */
     ChannelContactSearchStateMoreAvailable = 2,

    /**
     * The search has been completed
     */
     ChannelContactSearchStateCompleted = 3,

    /**
     * The search has failed
     */
     ChannelContactSearchStateFailed = 4
};

/**
 * \ingroup enumtypeconsts
 *
 * 1 higher than the highest valid value of ChannelContactSearchState.
 */
const int NUM_CHANNEL_CONTACT_SEARCH_STATES = (4+1);

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.Nokia.Telepathy.Connection.Interface.Forwarding".
 */
#define COMM_HISTORY_TP_INTERFACE_CONNECTION_INTERFACE_FORWARDING "com.Nokia.Telepathy.Connection.Interface.Forwarding"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Connection.Interface.StoredMessages".
 */
#define COMM_HISTORY_TP_INTERFACE_CONNECTION_INTERFACE_STORED_MESSAGES "com.nokia.Telepathy.Connection.Interface.StoredMessages"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Connection.Interface.GSM".
 */
#define COMM_HISTORY_TP_INTERFACE_CONNECTION_INTERFACE_GSM "com.nokia.Telepathy.Connection.Interface.GSM"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Connection.Interface.Emergency".
 */
#define COMM_HISTORY_TP_INTERFACE_CONNECTION_INTERFACE_EMERGENCY "com.nokia.Telepathy.Connection.Interface.Emergency"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "org.freedesktop.Telepathy.Connection.Interface.Privacy.DRAFT".
 */
#define COMM_HISTORY_TP_INTERFACE_CONNECTION_INTERFACE_PRIVACY "org.freedesktop.Telepathy.Connection.Interface.Privacy.DRAFT"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "org.freedesktop.Telepathy.Connection.Interface.ContactInfo.DRAFT".
 */
#define COMM_HISTORY_TP_INTERFACE_CONNECTION_INTERFACE_CONTACT_INFO "org.freedesktop.Telepathy.Connection.Interface.ContactInfo.DRAFT"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Connection.Interface.Skype.CommunicationPolicy".
 */
#define COMM_HISTORY_TP_INTERFACE_CONNECTION_INTERFACE_SKYPE_COMMUNICATIONPOLICY "com.nokia.Telepathy.Connection.Interface.Skype.CommunicationPolicy"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Connection.Interface.Skype.Balance".
 */
#define COMM_HISTORY_TP_INTERFACE_CONNECTION_INTERFACE_SKYPE_BALANCE "com.nokia.Telepathy.Connection.Interface.Skype.Balance"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "org.freedesktop.Telepathy.Channel.Type.ContactSearch.DRAFT".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_TYPE_CONTACT_SEARCH "org.freedesktop.Telepathy.Channel.Type.ContactSearch.DRAFT"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.Nokia.Telepathy.Channel.Interface.DialStrings".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_DIAL_STRINGS "com.Nokia.Telepathy.Channel.Interface.DialStrings"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Channel.Interface.Mute".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_MUTE "com.nokia.Telepathy.Channel.Interface.Mute"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Channel.Interface.Emergency".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_EMERGENCY "com.nokia.Telepathy.Channel.Interface.Emergency"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Channel.Interface.Persistent".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_PERSISTENT "com.nokia.Telepathy.Channel.Interface.Persistent"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "org.freedesktop.Telepathy.Channel.Interface.Privacy.DRAFT".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_PRIVACY "org.freedesktop.Telepathy.Channel.Interface.Privacy.DRAFT"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Channel.Interface.SMS".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_SMS "com.nokia.Telepathy.Channel.Interface.SMS"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Channel.Interface.Voicemail.Skype".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_VOICEMAIL_SKYPE "com.nokia.Telepathy.Channel.Interface.Voicemail.Skype"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Channel.Interface.Skype.GroupChat".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_SKYPE_GROUPCHAT "com.nokia.Telepathy.Channel.Interface.Skype.GroupChat"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "com.nokia.Telepathy.Channel.Interface.Skype.CallRate".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_SKYPE_CALLRATE "com.nokia.Telepathy.Channel.Interface.Skype.CallRate"

/**
 * \ingroup ifacestrconsts
 *
 * The interface name "org.freedesktop.Telepathy.Channel.Interface.Conference.DRAFT".
 */
#define COMM_HISTORY_TP_INTERFACE_CHANNEL_INTERFACE_TP_CONFERENCE "org.freedesktop.Telepathy.Channel.Interface.Conference.DRAFT"

