require 'rake/extensiontask'
require 'rubygems/package_task'

Rake::ExtensionTask.new 'rj_schema' do |ext|
  ext.lib_dir = 'lib/rj_schema'
end

s = Gem::Specification.load("rj_schema.gemspec")

Gem::PackageTask.new s do end

task default: :compile
