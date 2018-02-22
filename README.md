# rj_schema ![build](https://travis-ci.org/foxtacles/rj_schema.svg?branch=master)
Fast JSON schema validation with RapidJSON (https://github.com/Tencent/rapidjson)
# Usage
```
require 'rj_schema'
```
Create an instance of `RjSchema::Validator` and provide a JSON schema and a JSON document. You can pass a `File`, `Hash`, or `String`:
```
RjSchema::Validator.new.validate(File.new("schema/my_schema.json"), '{"stuff": 1}')
```
It is possible to resolve remote schemas by specifying them in the initializer. For example, if your schema contains `"$ref": "generic#/definitions/something"`:
```
RjSchema::Validator.new(
  'generic' => File.new("definitions/generic.json")
).validate(File.new("schema/my_schema.json"), '{"stuff": 1}')
```
`validate` will return an array containing hashes which describe the errors encountered during validation. If the array is empty, the JSON document is valid according to the schema.
An `ArgumentError` exception will be raised if any of the inputs are malformed or missing.

You can also call `valid?` if you are not interested in the error messages.
# Limitations

Some limitations apply due to RapidJSON:

- only JSON schema draft-04 is supported
- the `format` keyword is not supported
- there are a few edge cases with regards to `$ref` that will cause issues (for more details, see the tests)
# Benchmark
The main motivation for this gem was that we needed a faster JSON schema validation for our Ruby apps. We have been using Ruby JSON Schema Validator for a while (https://github.com/ruby-json-schema/json-schema) but some of our endpoints became unacceptably slow.

A benchmark to compare `rj_schema` and `json-schema` performances can be run with: `rake benchmark`. On average, `rj_schema` is about 4-5 times faster in our tests.
```
user     system      total        real
json_schema 20.580000   0.010000  20.590000 ( 20.582265)
json-schema  8.350000   0.100000   8.450000 (  8.447435)
rj_schema (validate)  1.920000   0.020000   1.940000 (  1.942872)
rj_schema (valid?)  1.600000   0.000000   1.600000 (  1.602149)
```
