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

//  -------------------------------------------------------------------------
/// Thread handling the action
//  -------------------------------------------------------------------------
class NotificationThread : public thread
{
public:
					 NotificationThread (void);
					~NotificationThread (void);
					
	void			 run (void);
	void			 statusChange (const statstring &target,
								   const statstring &objectd,
								   NotificationType t);
	
};

//  -------------------------------------------------------------------------
/// Implementation template for application config.
//  -------------------------------------------------------------------------
typedef configdb<class n2notifydApp> appconfig;

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
	appconfig			 conf;
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
	n2notifydApp			&app;
};

#endif
