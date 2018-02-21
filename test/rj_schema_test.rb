require 'minitest/autorun'
require 'rj_schema'

class RjSchemaTest < Minitest::Test
require 'pry'
binding.pry


  TEST_SUITE_DIR = File.expand_path('../test-suite', __FILE__)
  VALIDATOR = RjSchema::Validator.new(
    Dir[TEST_SUITE_DIR + '/remotes/**/*.json'].
      each_with_object({}) { |file, hash| hash["http://localhost:1234/#{file.sub(%r{^.*/remotes/}, '')}"] = File.new(file) }
  )

  Dir["#{TEST_SUITE_DIR}/tests/draft4/**/*.json"].each do |file|
    test_list = JSON.parse(File.read(file))
    rel_file = file[TEST_SUITE_DIR.length+1..-1]

    next if rel_file.include? '/optional/'

    test_list.each do |test|
      schema = test["schema"]
      base_description = test["description"]

      test["tests"].each do |t|
        full_description = "#{base_description}/#{t['description']}"

        err_id = "#{rel_file}: #{full_description}"
        define_method("test_#{err_id}") do
          errors = VALIDATOR.validate(schema, t["data"].to_json)
          assert_equal t["valid"], errors.empty?, "Common test suite case failed: #{err_id}"
        end
      end
    end
  end
end
