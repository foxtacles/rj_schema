Gem::Specification.new 'rj_schema', '0.0.1' do |s|
  s.summary = 'JSON schema validation with RapidJSON'
  s.authors = %w[mail@csemmler.com]
  s.extensions = %w[ext/rj_schema/extconf.rb]
  s.files = %w[
    Rakefile
    ext/rj_schema/extconf.rb
    ext/rj_schema/rj_schema.cpp
    lib/rj_schema.rb
  ] + Dir['ext/rj_schema/rapidjson/**/*']
end
