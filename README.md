# rj_schema
Fast JSON schema validation with RapidJSON (https://github.com/Tencent/rapidjson)
# Usage
```
require 'rj_schema'

begin
  RjSchema::Validator.new.validate("schema/my_schema.json", '{"stuff": 1}')
  # valid according to schema
rescue RuntimeError => e
  puts e.message # validation error
end
```
