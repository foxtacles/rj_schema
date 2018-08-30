# rj_schema [![build](https://travis-ci.org/foxtacles/rj_schema.svg?branch=master)](https://travis-ci.org/foxtacles/rj_schema) [![Gem Version](https://badge.fury.io/rb/rj_schema.svg)](https://badge.fury.io/rb/rj_schema)
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
# Caching
Another feature of `rj_schema` is the ability to preload schemas. This can boost performance by a lot, especially in applications that routinely perform validations against static schemas, i.e. validations of client inputs inside the endpoints of a web app. Add the schemas to preload into the initializer and pass a `Symbol` to the validation function:
```
RjSchema::Validator.new(
  'generic' => File.new("definitions/generic.json"),
  'schema/my_schema.json' => File.new("schema/my_schema.json")
).validate(:"schema/my_schema.json", '{"stuff": 1}')
```
# Limitations

Some limitations apply due to RapidJSON:

- only JSON schema draft-04 is supported
- the `format` keyword is not supported
- there are a few edge cases with regards to `$ref` that will cause issues (for more details, see the tests)
# Benchmark
The main motivation for this gem was that we needed a faster JSON schema validation for our Ruby apps. We have been using Ruby JSON Schema Validator for a while (https://github.com/ruby-json-schema/json-schema) but some of our endpoints became unacceptably slow.

A benchmark to compare various gem performances can be run with: `rake benchmark`. These are the results collected on my machine (with `g++ (Debian 7.3.0-3) 7.3.0`, `ruby 2.5.0p0 (2017-12-25 revision 61468) [x86_64-linux]`).

report | i/s | x
--- | --- | ---
rj_schema (valid?) (cached) | 370.9 | 1
rj_schema (validate) (cached) | 187.5 | 1.98x slower
[json_schemer](https://github.com/davishmcclurg/json_schemer) (valid?) (cached) | 135.6 | 2.73x slower
rj_schema (valid?) | 132.7 | 2.79x slower
rj_schema (validate) | 96.9 | 3.83x slower
[json-schema](https://github.com/ruby-json-schema/json-schema) | 10.6 | 34.92x slower
[json_schema](https://github.com/brandur/json_schema) | 3.7 | 101.26x slower

The error reporting of `rj_schema` is implemented inefficiently at the time of writing, so in this benchmark environment (based on JSON Schema test suite which includes many failing validations) `validate` performs significantly worse than `valid?`. This may not be an issue in production environments though, where failing validations are usually the exception (the overhead is only incurred in case of an error).
