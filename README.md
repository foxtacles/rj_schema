# rj_schema
Fast JSON schema validation with RapidJSON (https://github.com/Tencent/rapidjson)
# Usage
```
require 'rj_schema'
```
Create an instance of `RjSchema::Validator` and provide the path to the JSON schema and the JSON document to verify.
```
RjSchema::Validator.new.validate("schema/my_schema.json", '{"stuff": 1}')
```
To resolve remote schemas, specify the names and file paths (only local files are supported for now). Given `"$ref": "generic#/definitions/something"`:
```
RjSchema::Validator.new(
  'generic' => 'definitions/generic.json'
).validate("schema/my_schema.json", '{"stuff": 1}')
```
`validate` will return an array containing Hashes which describe the errors encountered during validation. If the array is empty, the JSON document is valid according to the schema.
