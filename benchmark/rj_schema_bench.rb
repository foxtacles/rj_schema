require 'rj_schema'
require 'json-schema'
require 'json_schema'
require 'benchmark'

TEST_SUITE_DIR = File.expand_path('../jsonschema', __FILE__)
RJ_VALIDATOR = RjSchema::Validator.new(
  Dir[TEST_SUITE_DIR + '/remotes/**/*.json'].
    each_with_object({}) { |file, hash| hash["http://localhost:1234/#{file.sub(%r{^.*/remotes/}, '')}"] = File.read(file) }.
    merge("http://json-schema.org/draft-04/schema" => File.read(File.join(TEST_SUITE_DIR, '../draft-04/schema')))
)
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
      define_method("test_#{err_id}") do |validator|
        return validator.call(schema, t["data"]) if validator.is_a?(Proc)
        validator.validate(schema, t["data"].to_json)
      end
    end.compact
  end
end

json_schema_validate = -> s, d {
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
  x.report("json-schema") { for i in 1..n; bench_methods.each { |m| send(m, JS_VALIDATOR) }; end }
  x.report("rj_schema") { for i in 1..n; bench_methods.each { |m| send(m, RJ_VALIDATOR) }; end }
end
