# rj_schema
Fast JSON schema validation with RapidJSON
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
