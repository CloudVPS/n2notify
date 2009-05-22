#include <grace/application.h>
#include <grace/http.h>

class n2eventApp : public application
{
public:
	n2eventApp (void) : application ("net.xl-is.tools.n2event")
	{
	}
	
	~n2eventApp (void)
	{
	}
	
	int main (void)
	{
		if (argv["*"].count() < 2)
		{
			ferr.writeln ("Usage: n2event <objectid> <problem|recovery>");
			return 1;
		}
		
		string id = argv["*"][0];
		string ntype = argv["*"][1];
		
		string url = "unix://[/var/state/n2/notify.socket]/notify"
					 "/%s/%s" %format (ntype, id);
		
		httpsocket ht;
		fout.writeln (ht.get (url));
		return 0;
	}
};

$appobject (n2eventApp);
$version (1.0.3);
