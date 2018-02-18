require 'mkmf'
$INCFLAGS << " -I$(srcdir)/rapidjson/include"
create_makefile 'rj_schema/rj_schema'
