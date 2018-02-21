require 'mkmf'
$INCFLAGS << " -I$(srcdir)/rapidjson/include"
$CXXFLAGS << " -std=c++11 "
CONFIG['CXX'] = ENV['CXX'] unless ENV['CXX'].nil?
CONFIG['CC'] = ENV['CC'] unless ENV['CC'].nil?
create_makefile 'rj_schema/rj_schema'
