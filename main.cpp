#include "n2notifyd.h"
#include <grace/process.h>
#include <grace/smtp.h>

$appobject (n2notifydApp);
$version (1.0.3);

// ==========================================================================
// CONSTRUCTOR n2notifydApp
// ==========================================================================
n2notifydApp::n2notifydApp (void)
	: daemon ("net.xl-is.svc.n2notifyd"),
	  conf (this),
	  notificationThread (this)
{
	opt = $("-h", $("long", "--help"));
}

// ==========================================================================
// DESTRUCTOR n2notifydApp
// ==========================================================================
n2notifydApp::~n2notifydApp (void)
{
}

// ==========================================================================
// METHOD n2notifydApp::main
// ==========================================================================
int n2notifydApp::main (void)
{
	string conferr; ///< Error return from configuration class.
	string confpath = "/etc/n2/n2notifyd.conf";
	if (! fs.exists (confpath)) confpath = "n2notifyd.conf";
	
	if (! conf.loadini (confpath, conferr))
	{
		ferr.writeln ("%% Error loading configuration: %s" %format (conferr));
		return 1;
	}
	
	addlogtarget (log::file, conf["system"]["eventlog"], log::all);

	string sockpath = "/var/state/n2/notify.socket";
	fs.rm (sockpath);
	srv.listento (sockpath);
	fs.chown (sockpath, "n2", "n2");
	settargetuser ("n2");
	daemonize ();
	
	log::write (log::info, "main", "Started");
	
	
	new NotifyHandler (this);
	srv.start ();
	notificationThread.spawn ();

	while (true)
	{
		value ev = waitevent ();
		if (ev.type() == "shutdown") break;
	}

	log::write (log::info, "main", "Shutting down");
	stoplog();
	exit (0);
}

// ==========================================================================
// METHOD n2notifydApp::confLog
// ==========================================================================
bool n2notifydApp::confLog (config::action act, keypath &kp,
							  const value &nval, const value &oval)
{
	string tstr;
	
	switch (act)
	{
		case config::isvalid:
			// Check if the path for the event log exists.
			tstr = strutil::makepath (nval.sval());
			if (! tstr.strlen()) return true;
			if (! fs.exists (tstr))
			{
				ferr.writeln ("%% Log path %s does not exist" %format (tstr));
				return false;
			}
			return true;
			
		case config::create:
			// Set the event log target and daemonize.
			fout.writeln ("%% Event log: %s\n" %format (nval));
			addlogtarget (log::file, nval.sval(), log::all, 1024*1024);
			return true;
	}
	
	return false;
}

// ==========================================================================
// CONSTRUCTOR NotificationItem
// ==========================================================================
NotificationItem::NotificationItem (class NotificationTarget *p,
									const statstring &i) : _id (i)
{
	_sent = true;
	_status = NOTIFY_RECOVERY;
	_lastchange = (time_t) 0;
	
	p->_items.set (i, this);
}

// ==========================================================================
// DESTRUCTOR NotificationItem
// ==========================================================================
NotificationItem::~NotificationItem (void)
{
}

// ==========================================================================
// METHOD NotificationItem::id
// ==========================================================================
const statstring &NotificationItem::id (void)
{
	return _id;
}

// ==========================================================================
// METHOD NotificationItem::sent
// ==========================================================================
bool NotificationItem::sent (void)
{
	return _sent;
}

// ==========================================================================
// METHOD NotificationItem::sent
// ==========================================================================
void NotificationItem::sent (bool setto)
{
	_sent = setto;
}

// ==========================================================================
// METHOD NotificationItem::isDue
// ==========================================================================
bool NotificationItem::isDue (int duetime)
{
	timestamp tnow;
	timestamp delta;
	
	tnow = core.time.now();
	delta = tnow - _lastchange;
	if (delta.unixtime() > duetime) return true;
	return false;
}

// ==========================================================================
// METHOD NotificationItem::isOverDue
// ==========================================================================
bool NotificationItem::isOverDue (void)
{
	return isDue (2 * NOTIFICATION_THRESHOLD);
}

// ==========================================================================
// METHOD NotificationItem::status
// ==========================================================================
NotificationType NotificationItem::status (void)
{
	return _status;
}

// ==========================================================================
// METHOD NotificationItem::status
// ==========================================================================
void NotificationItem::status (NotificationType nstatus)
{
	if (nstatus == _status) return;
	
	_sent = !_sent;
	_status = nstatus;
	_lastchange = core.time.now();
}

// ==========================================================================
// CONSTRUCTOR NotificationTarget
// ==========================================================================
NotificationTarget::NotificationTarget (const statstring &who)
{
	_id = who;
	_hasunsent = false;
	_lastchange = core.time.now();
}

// ==========================================================================
// DESTRUCTOR NotificationTarget
// ==========================================================================
NotificationTarget::~NotificationTarget (void)
{
}

// ==========================================================================
// METHOD NotificationTarget::recordStatusChange
// ==========================================================================
void NotificationTarget::recordStatusChange (const statstring &id,
											 NotificationType t)
{
	if (! _items.exists (id))
	{
		NotificationItem *i = new NotificationItem (this, id);
	}
	
	_items[id].status (t);
	_hasunsent = true;
	_lastchange = core.time.now ();
}

// ==========================================================================
// METHOD NotificationTarget::harvestChanges
// ==========================================================================
value *NotificationTarget::harvestChanges (void)
{
	if (! _hasunsent) return NULL;
	
	returnclass (value) res retain;
	timestamp tnow = core.time.now();
	timestamp tdelta = tnow - _lastchange;
	bool checkoverdue = false;
	if (tdelta.unixtime() < NOTIFICATION_THRESHOLD) checkoverdue = true;
	
	_hasunsent = false;
	foreach (item, _items)
	{
		if (item.sent()) continue;
		if ((checkoverdue && item.isOverDue()) ||
			(!checkoverdue && item.isDue()))
		{
			if (item.status() == NOTIFY_PROBLEM) res[item.id()] = "PROBLEM";
			else res[item.id()] = "RECOVERY";
			item.sent (true);
		}
		else _hasunsent = true;
	}
	
	if (res.count() && checkoverdue)
	{
		foreach (item, _items)
		{
			if (item.sent()) continue;
			if (item.isDue())
			{
				if (item.status() == NOTIFY_PROBLEM) res[item.id()] = "PROBLEM";
				else res[item.id()] = "RECOVERY";
				item.sent (true);
			}
		}
	}
	
	return &res;
}

// ==========================================================================
// CONSTRUCTOR NotificationThread
// ==========================================================================
NotificationThread::NotificationThread (n2notifydApp *app)
	: thread ("notification")
{
	new MailtoProtocol (dispatch, app->conf["system"]["mailfrom"],
						app->conf["system"]["mailname"]);
}

// ==========================================================================
// DESTRUCTOR NotificationThread
// ==========================================================================
NotificationThread::~NotificationThread (void)
{
}

// ==========================================================================
// METHOD NotificationThread::run
// ==========================================================================
void NotificationThread::run (void)
{
	dictionary<NotificationTarget> tgt;
	
	log::write (log::info, "nthread", "Thread spawned");
	
	while (true)
	{
		value ev = waitevent (15000);
		if (ev.type() == "statuschange")
		{
			log::write (log::info, "nthread",
						"Status change: %s" %format (ev.join("/")));
						
			statstring target = ev["target"];
			statstring objectid = ev["objectid"];
			string state = ev["state"];
			NotificationType ntype = NOTIFY_PROBLEM;
			if (state == "RECOVERY") ntype = NOTIFY_RECOVERY;
			
			if (! tgt.exists (target))
			{
				tgt.set (target, new NotificationTarget (target));
			}
			
			tgt[target].recordStatusChange (objectid, ntype);
		}
		foreach (t, tgt)
		{
			value n = t.harvestChanges ();
			if (n.count())
			{
				log::write (log::info, "nthread",
							"Notify <%s>: %J" %format (t.id(), n));
				dispatch.sendNotification (t.id(), n);
			}
		}
	}
}

// ==========================================================================
// METHOD NotificationThread::statusChange
// ==========================================================================
void NotificationThread::statusChange (const statstring &target,
									   const statstring &objectid,
									   NotificationType t)
{
	sendevent ("statuschange",
			   $("target",target) ->
			   $("objectid",objectid) ->
			   $("state",t == NOTIFY_RECOVERY ? "RECOVERY" : "PROBLEM")
			  );

}

// ==========================================================================
// CONSTRUCTOR NotifyHandler
// ==========================================================================
NotifyHandler::NotifyHandler (n2notifydApp *a)
	: httpdobject (a->srv, "/notify/*"),
	  app (*a)
{
}

// ==========================================================================
// DESTRUCTOR NotifyHandler
// ==========================================================================
NotifyHandler::~NotifyHandler (void)
{
}

// ==========================================================================
// METHOD NotifyHandler::handleEvent
// ==========================================================================
void NotifyHandler::handleEvent (const statstring &oid,
								 NotificationType t)
{
	value rcpt;
	string cmd = "/usr/bin/n2contact %S" %format (oid);

	systemprocess P(cmd);
	P.run ();
	while (! P.eof())
	{
		string ln = P.gets();
		if (ln) rcpt.newval() = ln;
	}
	P.close ();
	P.serialize ();
	
	foreach (r, rcpt)
	{
		app.notificationThread.statusChange (r, oid, t);
	}
}

// ==========================================================================
// METHOD NotifyHandler::run
// ==========================================================================
int NotifyHandler::run (string &uri, string &postbody, value &inhdr,
						string &out, value &outhdr, value &env,
						tcpsocket &s)
{
	value splt = strutil::split (uri, '/');
	string objectid = splt[3];
	caseselector (splt[2])
	{
		incaseof ("problem") :
			handleEvent (objectid, NOTIFY_PROBLEM);
			break;
		
		incaseof ("recovery") :
			handleEvent (objectid, NOTIFY_RECOVERY);
			break;
			
		defaultcase :
			out = "Error";
			return 500;
	}
	
	out = "OK";
	return 200;
}

// ==========================================================================
// CONSTRUCTOR NotificationProtocol
// ==========================================================================
NotificationProtocol::NotificationProtocol (void) : dispatch ((Dispatcher&) *(new Dispatcher))
{
	throw (defaultConstructorException());
}

NotificationProtocol::NotificationProtocol (Dispatcher &d) : dispatch(d)
{
}

// ==========================================================================
// DESTRUCTOR NotificationProtocol
// ==========================================================================
NotificationProtocol::~NotificationProtocol (void)
{
}

// ==========================================================================
// METHOD NotificationProtocol::sendNotification
// ==========================================================================
bool NotificationProtocol::sendNotification (const string &url,
											 const value &problems)
{
	log::write (log::error, "dispatch", "Protocol error: unimplemented");
	return false;
}

// ==========================================================================
// CONSTRUCTOR Dispatcher
// ==========================================================================
Dispatcher::Dispatcher (void)
{
}

// ==========================================================================
// DESTRUCTOR Dispatcher
// ==========================================================================
Dispatcher::~Dispatcher (void)
{
}

// ==========================================================================
// METHOD Dispatcher::sendNotification
// ==========================================================================
bool Dispatcher::sendNotification (const string &url, const value &problems)
{
	statstring proto = url.copyuntil (':');
	if (! protocols.exists (proto))
	{
		log::write (log::error, "dispatch", "Unimplemented protocol "
					"'%s'" %format (proto));
		return false;
	}
	
	return protocols[proto].sendNotification (url, problems);
}

// ==========================================================================
// CONSTRUCTOR MailtoProtocol
// ==========================================================================
MailtoProtocol::MailtoProtocol (Dispatcher &d, const string &mf,
								const string &mn)
	: NotificationProtocol (d), _mailfrom (mf), _mailname (mn)
{
	dispatch.protocols.set ("mailto", this);
}

// ==========================================================================
// DESTRUCTOR MailtoProtocol
// ==========================================================================
MailtoProtocol::~MailtoProtocol (void)
{
}

// ==========================================================================
// METHOD MailtoProtocol::sendNotification
// ==========================================================================
bool MailtoProtocol::sendNotification (const string &url,
									 const value &problems)
{
	string addr=url.copyafter(':');
	scriptparser scr;
	string tmpl;
	scr.build (tmpl);
	value senv;
	value hstat;
	
	if (fs.exists ("/etc/n2/mailmessage.tmpl"))
	{
		tmpl = fs.load ("/etc/n2/mailmessage.tmpl");
	}
	else if (fs.exists ("/etc/n2/mailmessage-default.tmpl"))
	{
		tmpl = fs.load ("/etc/n2/mailmessage-default.tmpl");
	}
	else if (fs.exists ("mailmessage.tmpl"))
	{
		tmpl = fs.load ("mailmessage.tmpl");
	}
	
	scr.build (tmpl);
	
	int numproblems = 0;
	int numrecoveries = 0;
	
	// go over the reported events
	foreach (p, problems)
	{
		if (p.sval() == "PROBLEM") numproblems++;
		else numrecoveries++;
		
		// Reference to the insertion point
		value &into = senv["problems"][p.id()];
		
		// Call n2hstat
		hstat = N2Util::getHostStats (p.id());
		
		// Copy all non-array values
		foreach (v, hstat)
		{
			if (v.count() == 0)
			{
				into[v.id()] = v;
			}
		}
		
		// Get a list of all active flags
		value flags;
		foreach (fl, hstat["flags"])
		{
			if ((fl == 1) && (fl.id() != "other"))
			{
				flags.newval() = fl.id();
			}
		}

		// Create $flags$
		if (flags.count())
		{
			into["flags"] = "(%s)" %format (flags.join (","));
		}
		
		// Calculate width for CPU usage meter
		int cpu = hstat["cpu"];
		if (cpu<0) cpu = 0;
		if (cpu>100) cpu = 100;
		into["cpuwidth"] = cpu;
		into["restwidth"] = 100 - cpu;
		
		// Resolve the host label
		into["label"] = N2Util::resolveLabel (p.id());
	}
	
	// Some extra parameters
	timestamp tnow = core.time.now ();
	senv["date"] = tnow.format ("%Y-%m-%d %H:%M");
	senv["numproblems"] = numproblems;
	senv["numrecoveries"] = numrecoveries;
	senv["mailto"] = addr;
	
	// Create the mime-separator
	string mimefield = strutil::uuid();
	mimefield = mimefield.filter ("0123456789abcdef");
	mimefield = "=_%s" %format (mimefield);
	senv["mimefield"] = mimefield;
	
	// Generate the mail message from the template.
	string message;
	scr.run (senv, message, "main");
	
	// Create the subject	
	string subject = "[N2]";
	if (numproblems) subject.strcat (" PROBLEM:%i" %format (numproblems));
	if (numrecoveries) subject.strcat (" RECOVERY:%i" %format (numrecoveries));
	subject.strcat (" <%s>" %format (senv["date"]));
	
	// Mail the message
	smtpsocket smtp;
	smtp.setsmtphost ("localhost");
	smtp.setsender ("support@xlshosting.com", "N2 Monitoring");
	smtp.setheader ("MIME-Version", "1.0");
	smtp.setheader ("Content-type", "multipart/related; boundary=\"%s\""
					%format (mimefield));
	
	log::write (log::info, "mailto", "Sending mail to <%s>" %format (addr));
	
	if (! smtp.sendmessage (addr, subject, message))
	{
		log::write (log::error, "mailto",
					"Error sending mail: %s" %format (smtp.error()));
	}
}

// ==========================================================================
// STATIC METHOD N2Util::getHostStats
// ==========================================================================
value *N2Util::getHostStats (const string &id)
{
	returnclass (value) res retain;
	static value schemaxml = N2Util::getSchemaXML ();
	xmlschema schema;
	string resxml;
	
	log::write (log::info, "n2util", "Getting hstat for <%s>" %format (id));
	schema.schema = schemaxml;
	
	string cmd = "/usr/bin/n2hstat -x %s" %format (id);
	try
	{
		systemprocess P(cmd);
		P.run ();
		while (! P.eof())
		{
			resxml.strcat (P.read (4096));
		}
		P.close ();
		P.serialize ();
	}
	catch (exception e)
	{
		log::write (log::error, "n2util", "Exception running hstat: %s"
											%format (e.description));
	}
	
	res.fromxml (resxml, schema);
	log::write (log::info, "n2util", "Stats gathered");
	return &res;
}

// ==========================================================================
// STATIC METHOD N2Util::resolveLabel
// ==========================================================================
string *N2Util::resolveLabel (const statstring &id)
{
	returnclass (string) res retain;
	
	log::write (log::info, "n2util", "Resolving <%s>" %format (id));
	
	file f("/var/state/n2/n2labels");
	value slabels;
	
	while (!f.eof())
	{
		string line = f.gets();
		if (! line) continue;
		value v = strutil::split (line, ':');
		if (v.count() != 2) continue;
		slabels[v[1].sval()] = v[0];
	}
	f.close ();
	
	if (slabels.exists (id))
	{
		res = slabels[id];
		return &res;
	}
	
	if (! fs.exists ("/usr/bin/n2resolve"))
	{
		res = id;
		return &res;
	}
	
	string cmd = "/usr/bin/n2resolve %s" %format (id);
	try
	{
		systemprocess P(cmd);
		P.run();
		res = P.gets();
		P.close();
		P.serialize();
	}
	catch (exception e)
	{
	}
	
	if (! res) res = id;
	return &res;
}
