require 'mkmf'
$INCFLAGS << " -I$(srcdir)/rapidjson/include"
$CXXFLAGS << " -std=c++11 "
create_makefile 'rj_schema/rj_schema'
