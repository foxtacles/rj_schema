require 'rj_schema'
require 'json-schema'
require 'json_schema'
require 'benchmark'

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

n = 75
Benchmark.bm do |x|
  x.report("json_schema") { for i in 1..n; bench_methods.each { |m| send(m, json_schema_validate) }; end }
  x.report("json-schema") { for i in 1..n; bench_methods.each { |m| send(m, -> s, d, m { JS_VALIDATOR.validate(s, d.to_json) }) }; end }
  x.report("rj_schema (validate)") { for i in 1..n; bench_methods.each { |m| send(m, -> s, d, m { RJ_VALIDATOR.validate(s, d.to_json) }) }; end }
  x.report("rj_schema (valid?)") { for i in 1..n; bench_methods.each { |m| send(m, -> s, d, m { RJ_VALIDATOR.valid?(s, d.to_json) }) }; end }
  x.report("rj_schema (validate) (cached)") { for i in 1..n; bench_methods.each { |m| send(m, -> s, d, m { RJ_VALIDATOR_CACHED.validate(m, d.to_json) }) }; end }
  x.report("rj_schema (valid?) (cached)") { for i in 1..n; bench_methods.each { |m| send(m, -> s, d, m { RJ_VALIDATOR_CACHED.valid?(m, d.to_json) }) }; end }
end
