#ifndef _n2notify_H
#define _n2notify_H 1
#include <grace/daemon.h>
#include <grace/configdb.h>
#include <grace/httpd.h>

// The amount of seconds notfication states should be stable before
// anything is sent out.
#define NOTIFICATION_THRESHOLD 91

enum NotificationType {
	NOTIFY_PROBLEM,
	NOTIFY_RECOVERY
};

class Dispatcher;

//  -------------------------------------------------------------------------
/// Abstract class implementing a specific protocol in the uri-scheme
/// used for notification targets.
//  -------------------------------------------------------------------------
class NotificationProtocol
{
public:
						 /// Default constructor. Throws an exception.
						 NotificationProtocol (void);
						 
						 /// Constructor. 
						 /// \param d Parent-dispatcher.
						 NotificationProtocol (Dispatcher &d);
						~NotificationProtocol (void);
						
						 /// Virtual handler method.
						 /// \param url The notification-url
						 /// \param problems A dictionary of events
						 ///                 keyed by address. Each item
						 ///                 has a value of either
						 ///                 PROBLEM or RECOVERY.
						 /// \return True on succes.
	virtual bool		 sendNotification (const string &url,
										   const value &problems);

protected:
	Dispatcher			&dispatch; ///< Link back to parent dispatcher.
};

typedef dictionary<NotificationProtocol> ProtocolDict;

//  -------------------------------------------------------------------------
/// Collection class for NotificationProtocol instances.
//  -------------------------------------------------------------------------
class Dispatcher
{
public:
						 /// Constructor.
						 Dispatcher (void);
						 
						 /// Destructor.
						~Dispatcher (void);
						
						 /// Send out a notification through
						 /// the apropriate protocol-handler.
						 /// \param url The notification-url
						 /// \param problems A dictionary of events
						 ///                 keyed by address. Each item
						 ///                 has a value of either
						 ///                 PROBLEM or RECOVERY.
						 /// \return True on succes.
	bool				 sendNotification (const string &url,
						 				   const value &problems);
						 				   
	ProtocolDict		 protocols;
};

//  -------------------------------------------------------------------------
/// Implementation of the mailto: protocol.
//  -------------------------------------------------------------------------
class MailtoProtocol : public NotificationProtocol
{
public:
						 /// Constructor.
						 /// Adds the protocol to the dispatcher.
						 /// \param d Parent dispatcher
						 /// \param mf The message's source email address.
						 /// \param mn The message's source name.
						 MailtoProtocol (Dispatcher &d, const string &mf,
						 				 const string &mn);
						 
						 /// Destructor.
						~MailtoProtocol (void);
						
						 /// Implementation.
	bool				 sendNotification (const string &url,
						 				   const value &problems);

protected:
	string				_mailfrom;
	string				_mailname;
};

//  -------------------------------------------------------------------------
/// Thread handling the action
//  -------------------------------------------------------------------------
class NotificationThread : public thread
{
public:
						 NotificationThread (class n2notifydApp *);
						~NotificationThread (void);
						
	void				 run (void);
	void				 statusChange (const statstring &target,
									   const statstring &objectd,
									   NotificationType t);
	
	Dispatcher			 dispatch;
};

//  -------------------------------------------------------------------------
/// Implementation template for application config.
//  -------------------------------------------------------------------------
typedef configdb<class n2notifydApp> AppConfig;

//  -------------------------------------------------------------------------
/// Main daemon class.
//  -------------------------------------------------------------------------
class n2notifydApp : public daemon
{
public:
		 				 n2notifydApp (void);
		 				~n2notifydApp (void);
		 	
	int					 main (void);
	httpd				 srv;
	AppConfig			 conf;
	NotificationThread	 notificationThread;

protected:
	bool				 confLog (config::action act, keypath &path,
								  const value &nval, const value &oval);
};

$exception (defaultConstructorException, "Default constructor");

//  -------------------------------------------------------------------------
/// This class represents a notification state about a specific
/// resource for a specific recipient.
//  -------------------------------------------------------------------------
class NotificationItem
{
public:
						 /// Default constructor (must exist for the
						 /// dictionary class). Throws an exception.
						 NotificationItem (void) { throw defaultConstructorException(); }
						 
						 /// Constructor. Will link itself into the target's
						 /// _items dictionary.
						 /// \param p The parent target.
						 /// \param i The resource id.
						 NotificationItem (class NotificationTarget *p,
						 				   const statstring &i);
						 				   
						 /// Destructor.
						~NotificationItem (void);
	
						 /// Read id property.
	const statstring	&id (void);
	
						 /// Read the sent property.
						 /// \return True if notification has been sent
						 ///         for the object's current state.
	bool				 sent (void);
	
						 /// Write the sent property.
						 /// \param t New property value.
	void				 sent (bool t);
	
	bool				 isDue (int duetime = NOTIFICATION_THRESHOLD);
	bool				 isOverDue (void);
	
	NotificationType	 status (void);
	void				 status (NotificationType t);
	
protected:
	timestamp			 _lastchange;
	bool				 _sent;
	NotificationType	 _status;
	statstring			 _id;
};

typedef dictionary<NotificationItem> NotificationItemDict;

//  -------------------------------------------------------------------------
/// This class represents a user that wants to be notified about one
/// or more resources.
//  -------------------------------------------------------------------------
class NotificationTarget
{
friend class NotificationItem;
public:
							 NotificationTarget (void) { throw defaultConstructorException(); }
							 NotificationTarget (const statstring &who);
							~NotificationTarget (void);
						
	void					 recordStatusChange (const statstring &id,
												 NotificationType t);
	
	value					*harvestChanges (void);
	
	const statstring		&id (void) { return _id; };

protected:
	statstring				_id;
	NotificationItemDict	_items;
	timestamp				_lastchange;
	bool					_hasunsent;
};

//  -------------------------------------------------------------------------
/// HTTP handler for the /notify namespace.
//  -------------------------------------------------------------------------
class NotifyHandler : public httpdobject
{
public:
						 NotifyHandler (n2notifydApp *a);
						~NotifyHandler (void);
	
	void				 handleEvent (const statstring &oid,
									  NotificationType t);
	
	int					 run (string &uri, string &postbody, value &inhdr,
							  string &out, value &outhdr, value &env,
							  tcpsocket &s);

protected:
	n2notifydApp		&app;
};

//  -------------------------------------------------------------------------
/// Utility class for interacting with N2
//  -------------------------------------------------------------------------
class N2Util
{
public:
	static value		*getHostStats (const string &id);
	static value		*getSchemaXML (void);
	static string		*resolveLabel (const statstring &id);
};

#endif
