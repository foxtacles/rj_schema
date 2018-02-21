require 'rake/extensiontask'
require 'rake/testtask'
require 'rubygems/package_task'

Rake::ExtensionTask.new 'rj_schema' do |t|
  t.lib_dir = 'lib/rj_schema'
end

Rake::TestTask.new do |t|
  t.libs << 'lib'
  t.test_files = FileList.new('test/*_test.rb')
end

s = Gem::Specification.load("rj_schema.gemspec")

Gem::PackageTask.new s do end

task default: %w[compile test]

task :benchmark do
  puts `ruby -Ilib -Ibenchmark -rrj_schema -rrj_schema_bench -e ""`
end
