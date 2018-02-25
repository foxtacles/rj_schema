require 'rj_schema'
require 'json-schema'
require 'json_schema'
require 'json_schemer'
require 'benchmark/ips'

TEST_SUITE_DIR = File.expand_path('../jsonschema', __FILE__)

remotes = Dir[TEST_SUITE_DIR + '/remotes/**/*.json'].
  each_with_object({}) { |file, hash| hash["http://localhost:1234/#{file.sub(%r{^.*/remotes/}, '')}"] = File.read(file) }.
  merge("http://json-schema.org/draft-04/schema" => File.read(File.join(TEST_SUITE_DIR, '../draft-04/schema')))

RJ_VALIDATOR = RjSchema::Validator.new(remotes)
JS_VALIDATOR = JSON::Validator

json_schema_dstore = JsonSchema::DocumentStore.new
json_schema = JsonSchema.parse!(JSON.parse(File.read(File.join(TEST_SUITE_DIR, '../draft-04/schema'))))
json_schema.uri = "http://json-schema.org/draft-04/schema"
json_schema_dstore.add_schema(json_schema)

Dir[TEST_SUITE_DIR + '/remotes/**/*.json'].each do |file|
  uri = Addressable::URI.parse("http://localhost:1234/#{file.sub(%r{^.*/remotes/}, '')}")
  json = JSON.parse(File.read(file))
  JSON::Validator.add_schema(JSON::Schema.new(json, uri))

  json_schema = JsonSchema.parse!(json)
  json_schema.uri = uri.to_s
  json_schema_dstore.add_schema(json_schema)
end

locals = {}
bench_methods = Dir["#{TEST_SUITE_DIR}/tests/draft4/**/*.json"].flat_map do |file|
  test_list = JSON.parse(File.read(file))
  rel_file = file[TEST_SUITE_DIR.length+1..-1]

  test_list.flat_map do |test|
    schema = test["schema"]
    base_description = test["description"]

    test["tests"].map do |t|
      full_description = "#{base_description}/#{t['description']}"

      err_id = "#{rel_file}: #{full_description}"

      next if rel_file.include?('/optional/') || err_id.include?("change resolution scope")

      locals["test_#{err_id}"] = schema
      define_method("test_#{err_id}") do |validator|
        validator.call(schema, t["data"], __method__)
      end
    end.compact
  end
end

RJ_VALIDATOR_CACHED = RjSchema::Validator.new(remotes.merge(locals))

json_schema_validate = -> s, d, m {
  schema = JsonSchema.parse!(s)
  begin
    schema.expand_references!(store: json_schema_dstore)
    schema.validate!(d)
  rescue
    # this gem has a number of bugs (does not pass test suite)
  end
}

json_schemer_ref_cache = remotes.map do |r|
  [r.first, JSON.parse(r.last)]
end.to_h
json_schemer_cache = bench_methods.map do |m|
  [m, send(m, -> s, d, m { JSONSchemer.schema(s.merge("$schema" => 'http://json-schema.org/draft-04/schema#'), format: false, ref_resolver: -> uri{ uri = uri.to_s; json_schemer_ref_cache[uri.slice(0, uri.index('#') || uri.length)] }) })]
end.to_h

Benchmark.ips do |x|
  x.config(time: 8, warmup: 3)
  x.report("json_schema") { bench_methods.each { |m| send(m, json_schema_validate) } }
  x.report("json-schema") { bench_methods.each { |m| send(m, -> s, d, m { JS_VALIDATOR.validate(s, d.to_json) }) }  }
  x.report("rj_schema (validate)") { bench_methods.each { |m| send(m, -> s, d, m { RJ_VALIDATOR.validate(s, d.to_json) }) } }
  x.report("rj_schema (valid?)") { bench_methods.each { |m| send(m, -> s, d, m { RJ_VALIDATOR.valid?(s, d.to_json) }) } }
  x.report("rj_schema (validate) (cached)") { bench_methods.each { |m| send(m, -> s, d, m { RJ_VALIDATOR_CACHED.validate(m, d.to_json) }) } }
  x.report("json_schemer (valid?) (cached)") { bench_methods.each { |m| send(m, -> s, d, m { json_schemer_cache[m].valid?(d) }) } }
  x.report("rj_schema (valid?) (cached)") { bench_methods.each { |m| send(m, -> s, d, m { RJ_VALIDATOR_CACHED.valid?(m, d.to_json) }) } }
  x.compare!
end
