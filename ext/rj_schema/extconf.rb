require 'mkmf'
$INCFLAGS << " -I$(srcdir)/rapidjson/include"
$CXXFLAGS << " -std=c++14 "
CONFIG['CXX'] = ENV['CXX'] unless ENV['CXX'].nil?
CONFIG['CC'] = ENV['CC'] unless ENV['CC'].nil?
puts "compiler ENV: CC #{ENV['CC']}, CXX #{ENV['CXX']}"
create_makefile 'rj_schema/rj_schema'
