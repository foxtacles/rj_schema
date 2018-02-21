require 'rj_schema'
require 'json-schema'
require 'benchmark'

TEST_SUITE_DIR = File.expand_path('../jsonschema', __FILE__)
RJ_VALIDATOR = RjSchema::Validator.new(
  Dir[TEST_SUITE_DIR + '/remotes/**/*.json'].
    each_with_object({}) { |file, hash| hash["http://localhost:1234/#{file.sub(%r{^.*/remotes/}, '')}"] = File.new(file).read }.
    merge("http://json-schema.org/draft-04/schema" => File.new(File.join(TEST_SUITE_DIR, '../draft-04/schema')).read)
)
JS_VALIDATOR = JSON::Validator

Dir[TEST_SUITE_DIR + '/remotes/**/*.json'].each do |file|
  uri = Addressable::URI.parse("http://localhost:1234/#{file.sub(%r{^.*/remotes/}, '')}")
  JSON::Validator.add_schema(JSON::Schema.new(JSON.parse(File.new(file).read), uri))
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
        validator.validate(schema, t["data"].to_json)
      end
    end.compact
  end
end

n = 75
Benchmark.bm do |x|
  x.report("json-schema") { for i in 1..n; bench_methods.each { |m| send(m, JS_VALIDATOR) }; end }
  x.report("rj_schema") { for i in 1..n; bench_methods.each { |m| send(m, RJ_VALIDATOR) }; end }
end

