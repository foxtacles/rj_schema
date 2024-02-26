# rj_schema ![Ruby](https://github.com/foxtacles/rj_schema/workflows/Ruby/badge.svg) [![Gem Version](https://badge.fury.io/rb/rj_schema.svg)](https://badge.fury.io/rb/rj_schema)
Fast JSON schema validation with RapidJSON (https://github.com/Tencent/rapidjson)
# Usage
```
require 'rj_schema'
```
Create an instance of `RjSchema::Validator` and provide a JSON schema and a JSON document. If you pass a `File`, it will be read and parsed as JSON. Otherwise, `to_json` will be called on the arguments internally:
```
RjSchema::Validator.new.validate(File.new("schema/my_schema.json"), '{"stuff": 1}')
```
It is possible to resolve remote schemas by specifying them in the initializer. For example, if your schema contains `"$ref": "/path/to/generic#/definitions/something"`:
```
RjSchema::Validator.new(
  '/path/to/generic' => File.new("definitions/generic.json")
).validate(File.new("schema/my_schema.json"), '{"stuff": 1}')
```
`validate` will return a hash containing various descriptions of the errors (for details, see Options below). An `ArgumentError` exception will be raised if any of the inputs are malformed or missing.

You can also call `valid?`, which returns a boolean value indicating success/failure instead.

## SAX parser validation 

If you prefer SAX validation with a simple boolean value, which also dramatically reduces memory requirements, then you can also call `sax_valid?` and pass the filepath instead of the file reference for the document you want to validate, eg:

```
RjSchema::Validator.new(
  '/path/to/generic' => File.new("definitions/generic.json")
).sax_valid?(File.new("schema/my_schema.json"), '<<FILEPATH to Doc>>')
```

# Options

`validate` currently offers three options to customize the validation process. They can be specified as keyword arguments:

```
RjSchema::Validator.new.validate(
  File.new("schema/my_schema.json"),
  '{"stuff": 1}', 
  continue_on_error: true, 
  machine_errors: false, 
  human_errors: true
)
```

### `continue_on_error` (default: `false`). 

When set to `true`, validation will not stop upon the first error. Instead, an attempt will be made to determine all errors in the document based on the provided schema.

### `machine_errors` (default: `true`). 

When set to `true`, the return value of `validate` will contain a symbol key called `machine_errors`, which is a structured hash describing the encountered errors. The hash will be empty if no errors were found. The documentation for the error codes can be [found here](https://github.com/Tencent/rapidjson/blob/05e7b3397758bd31032aa66620e15fd8ab2869f5/include/rapidjson/error/error.h#L162). Example:

`{:machine_errors=>{"maximum"=>{"actual"=>31, "expected"=>20, "errorCode"=>2, "instanceRef"=>"#/aaaa", "schemaRef"=>"#/patternProperties/aaa%2A"}}}`

### `human_errors` (default: `false`). 

When set to `true`, the return value of `validate` will contain a symbol key called `human_errors`, which is a printable and human readable string describing the encountered errors. The string will be empty if no errors were found. Example:

`{:human_errors=>"Error Name: maximum\nMessage: Number '31' is greater than the 'maximum' value '20'.\nInstance: #/aaaa\nSchema: #/patternProperties/aaa%2A\n\n"}`

# Caching
Another feature of `rj_schema` is the ability to preload schemas. This can boost performance by a lot, especially in applications that routinely perform validations against static schemas, i.e. validations of client inputs inside the endpoints of a web app. Add the schemas to preload into the initializer and pass a `Symbol` to the validation function:
```
RjSchema::Validator.new(
  '/path/to/generic' => File.new("definitions/generic.json"),
  '/schema/my_schema.json' => File.new("schema/my_schema.json")
).validate(:"/schema/my_schema.json", '{"stuff": 1}')
```
# Limitations

Some limitations apply due to RapidJSON:

- only JSON schema draft-04 is supported
- the `format` keyword is not supported

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
