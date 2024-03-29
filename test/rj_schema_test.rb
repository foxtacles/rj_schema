require 'minitest/autorun'
require 'rj_schema'

class RjSchemaTest < Minitest::Test
  TEST_SUITE_DIR = File.expand_path('../jsonschema', __FILE__)
  remotes = Dir[TEST_SUITE_DIR + '/remotes/**/*.json'].
    each_with_object({}) { |file, hash| hash["http://localhost:1234/#{file.sub(%r{^.*/remotes/}, '')}"] = File.new(file) }.
    merge("http://json-schema.org/draft-04/schema" => File.new(File.join(TEST_SUITE_DIR, '../draft-04/schema')))
  VALIDATOR = RjSchema::Validator.new(remotes)

  locals = {}
  Dir["#{TEST_SUITE_DIR}/tests/draft4/**/*.json"].each do |file|
    test_list = JSON.parse(File.read(file))
    rel_file = file[TEST_SUITE_DIR.length+1..-1]

    test_list.each do |test|
      schema = test["schema"]
      base_description = test["description"]

      test["tests"].each do |t|
        full_description = "#{base_description}/#{t['description']}"

        err_id = "#{rel_file}: #{full_description}"
        locals["test_#{err_id}"] = schema unless rel_file.include?('/optional/')
        define_method("test_#{err_id}") do
          skip('the "format" keyword is not implemented in RapidJSON') if rel_file.include? '/optional/'

          errors = VALIDATOR.validate(schema, t["data"].to_json, continue_on_error: true, machine_errors: true, human_errors: true)
          assert_equal t["valid"], errors[:machine_errors].empty?, "Common test suite case failed: #{err_id}"
          assert_equal t["valid"], errors[:human_errors].empty?, "Common test suite case failed: #{err_id}"
          assert_equal t["valid"], VALIDATOR.valid?(schema, t["data"].to_json), "Common test suite case failed: #{err_id}"
          file = Tempfile.new('foo')
          file.write t["data"].to_json
          file.close
          assert_equal t["valid"], VALIDATOR.sax_valid?(schema, file.path), "Common test suite case failed: #{err_id}"
          file.unlink

          errors = VALIDATOR_CACHED.validate(__method__, t["data"].to_json, continue_on_error: true, machine_errors: true, human_errors: true)
          assert_equal t["valid"], errors[:machine_errors].empty?, "Common test suite case failed: #{err_id}"
          assert_equal t["valid"], errors[:human_errors].empty?, "Common test suite case failed: #{err_id}"
          assert_equal t["valid"], VALIDATOR_CACHED.valid?(__method__, t["data"].to_json), "Common test suite case failed: #{err_id}"
          file = Tempfile.new('foo')
          file.write t["data"].to_json
          file.close
          assert_equal t["valid"], VALIDATOR_CACHED.sax_valid?(__method__, file.path), "Common test suite case failed: #{err_id}"
          file.unlink
        end
      end
    end
  end

  VALIDATOR_CACHED = RjSchema::Validator.new(Hash[remotes.merge(locals).to_a.shuffle])

  define_method("test_nil") do
    assert_equal VALIDATOR.validate(nil, nil), {machine_errors: {}}
  end
end
