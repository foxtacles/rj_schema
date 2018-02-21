#include <unordered_map>
#include <ruby.h>

#define RAPIDJSON_SCHEMA_USE_INTERNALREGEX 0
#define RAPIDJSON_SCHEMA_USE_STDREGEX 1

#include <rapidjson/schema.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>

typedef std::unordered_map<std::string, rapidjson::SchemaDocument> RemoteSchemaCollection;

class RemoteSchemaDocumentProvider : public rapidjson::IRemoteSchemaDocumentProvider {
	RemoteSchemaCollection* schema_collection;

	public:
		RemoteSchemaDocumentProvider(RemoteSchemaCollection* schema_collection) : schema_collection(schema_collection) { }
		virtual const rapidjson::SchemaDocument* GetRemoteDocument(const char* uri, rapidjson::SizeType) {
			std::string remote_uri(uri);
			auto it = schema_collection->find(remote_uri.substr(0, remote_uri.find('#')));

			if (it == schema_collection->end())
				throw std::invalid_argument(std::string("$ref remote schema not provided: ") + remote_uri);
			return &it->second;
		}
};

struct RemoteSchemaManager {
	RemoteSchemaCollection collection;
	RemoteSchemaDocumentProvider provider;

	RemoteSchemaManager() : provider(&collection) { }
};

rapidjson::Document parse_document(VALUE arg) {
	const char* json_document;

	if (RB_TYPE_P(arg, T_FILE)) {
		VALUE content = rb_funcall(arg, rb_intern("read"), 0);
		json_document = StringValueCStr(content);
	}
	else if (RB_TYPE_P(arg, T_HASH)) {
		VALUE content = rb_funcall(arg, rb_intern("to_json"), 0);
		json_document = StringValueCStr(content);
	}
	else {
		json_document = StringValueCStr(arg);
	}

	rapidjson::Document document;

	if (document.Parse(json_document).HasParseError())
		throw std::invalid_argument("document is not valid JSON");

	return document;
}

extern "C" VALUE validator_validate(VALUE self, VALUE schema_arg, VALUE document_arg) {
	try {
		RemoteSchemaManager* schema_manager;
		Data_Get_Struct(self, RemoteSchemaManager, schema_manager);

		auto document = parse_document(document_arg);
		auto schema = rapidjson::SchemaDocument(
		  parse_document(schema_arg),
		  0,
		  0,
		  &schema_manager->provider
		);

		rapidjson::SchemaValidator validator(schema);

		if (!document.Accept(validator)) {
			rapidjson::StringBuffer buffer;
			rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
			validator.GetError().Accept(writer);

			VALUE errors = rb_funcall(rb_const_get(rb_cObject, rb_intern("JSON")), rb_intern("parse"), 1, rb_str_new_cstr(buffer.GetString()));
			return RB_TYPE_P(errors, T_HASH) ? rb_ary_new4(1, &errors) : errors;
		}
	}
	catch (const std::invalid_argument& e) {
		rb_raise(rb_eArgError, e.what());
	}

	return rb_ary_new();
}

extern "C" void validator_free(RemoteSchemaManager* schema_manager) {
	delete schema_manager;
}

extern "C" VALUE validator_alloc(VALUE self) {
	auto* schema_manager = new RemoteSchemaManager;
	return Data_Wrap_Struct(self, NULL, validator_free, schema_manager);
}

extern "C" int validator_initialize_load_schema(VALUE key, VALUE value, VALUE input) {
	auto* schema_manager = reinterpret_cast<RemoteSchemaManager*>(input);

	try {
		schema_manager->collection.emplace(
		  StringValueCStr(key),
		  rapidjson::SchemaDocument(
		    parse_document(value),
		    StringValueCStr(key),
		    RSTRING_LEN(key),
		    &schema_manager->provider
		  )
		);
	}
	catch (const std::invalid_argument& e) {
		rb_raise(rb_eArgError, e.what());
	}

	return ST_CONTINUE;
}

extern "C" VALUE validator_initialize(int argc, VALUE* argv, VALUE self) {
	if (argc == 0)
		return self;

	RemoteSchemaManager* schema_manager;
	Data_Get_Struct(self, RemoteSchemaManager, schema_manager);

	rb_hash_foreach(argv[0], reinterpret_cast<int(*)(...)>(validator_initialize_load_schema), reinterpret_cast<VALUE>(schema_manager));

	return self;
}

extern "C" void Init_rj_schema(void) {
	VALUE cValidator = rb_define_class_under(
	  rb_const_get(rb_cObject, rb_intern("RjSchema")),
	  "Validator",
	  rb_cObject
	);

	rb_define_alloc_func(cValidator, validator_alloc);
	rb_define_method(cValidator, "initialize", reinterpret_cast<VALUE(*)(...)>(validator_initialize), -1);
	rb_define_method(cValidator, "validate", reinterpret_cast<VALUE(*)(...)>(validator_validate), 2);
}
