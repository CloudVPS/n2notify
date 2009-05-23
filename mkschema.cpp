#include <grace/application.h>
#include <grace/xmlschema.h>

class MkSchemaApp : public application
{
public:
	MkSchemaApp (void) : application ("net.xl-is.tools.n2.mkschema")
	{
	}
	
	~MkSchemaApp (void)
	{
	}
	
	int main (void)
	{
		xmlschema schema (XMLRootSchemaType);
		value v;
		
		v.loadxml ("n2host.schema.xml", schema);
		string code = "#include <grace/value.h>\n"
					  "#include \"n2notifyd.h\"\n"
					  "value *N2Util::getSchemaXML (void) {\n";
					  "  return ";
		code.strcat (v.tograce());
		code.strcat ("\n}\n");
		fs.save ("schema.cpp", code);
		return 0;
	}
};

$appobject (MkSchemaApp);
$version (1.0);
