require 'minitest/autorun'
require 'rj_schema'

class RjSchemaTest < Minitest::Test
  TEST_SUITE_DIR = File.expand_path('../jsonschema', __FILE__)
  VALIDATOR = RjSchema::Validator.new(
    Dir[TEST_SUITE_DIR + '/remotes/**/*.json'].
      each_with_object({}) { |file, hash| hash["http://localhost:1234/#{file.sub(%r{^.*/remotes/}, '')}"] = File.new(file) }.
      merge("http://json-schema.org/draft-04/schema" => File.new(File.join(TEST_SUITE_DIR, '../draft-04/schema')))
  )

  Dir["#{TEST_SUITE_DIR}/tests/draft4/**/*.json"].each do |file|
    test_list = JSON.parse(File.read(file))
    rel_file = file[TEST_SUITE_DIR.length+1..-1]

    test_list.each do |test|
      schema = test["schema"]
      base_description = test["description"]

      test["tests"].each do |t|
        full_description = "#{base_description}/#{t['description']}"

        err_id = "#{rel_file}: #{full_description}"
        define_method("test_#{err_id}") do
          skip('most notably, the "format" keyword is not implemented in RapidJSON') if rel_file.include? '/optional/'
          skip('The failed test is "changed scope ref invalid" of "change resolution scope" in refRemote.json. It is due to that id schema keyword and URI combining function are not implemented.') if err_id.include? "change resolution scope"

          errors = VALIDATOR.validate(schema, t["data"].to_json)
          assert_equal t["valid"], errors.empty?, "Common test suite case failed: #{err_id}"
          assert_equal t["valid"], VALIDATOR.valid?(schema, t["data"].to_json), "Common test suite case failed: #{err_id}"
        end
      end
    end
  end
end
