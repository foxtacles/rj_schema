Gem::Specification.new 'rj_schema', '1.0.1' do |s|
  s.licenses = %w[MIT]
  s.summary = 'JSON schema validation with RapidJSON'
  s.description = 'gem using RapidJSON as a backend to provide fast validation for JSON schemas'
  s.authors = ['Christian Semmler']
  s.email = 'mail@csemmler.com'
  s.homepage = 'https://github.com/foxtacles/rj_schema'
  s.extensions = %w[ext/rj_schema/extconf.rb]
  s.files = %w[
    Rakefile
    ext/rj_schema/extconf.rb
    ext/rj_schema/rj_schema.cpp
    lib/rj_schema.rb
  ] + Dir['ext/rj_schema/rapidjson/**/*']
  s.add_development_dependency 'rake-compiler', '~> 1.0'
  s.add_development_dependency 'minitest', '~> 5.0'
  s.add_development_dependency 'json-schema', '~> 2.8'
  s.add_development_dependency 'json_schema', '~> 0.17'
  s.add_development_dependency 'json_schemer', '~> 0.2'
  s.add_development_dependency 'benchmark-ips', '~> 2.7'
end
