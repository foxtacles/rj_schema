#include <regex>
#if !(__cplusplus >= 201103L &&                           \
    (!defined(__GLIBCXX__) || (__cplusplus >= 201402L) || \
        (defined(_GLIBCXX_REGEX_DFS_QUANTIFIERS_LIMIT) || \
         defined(_GLIBCXX_REGEX_STATE_LIMIT)           || \
             (defined(_GLIBCXX_RELEASE)                && \
             _GLIBCXX_RELEASE > 4))))
#error "Your compiler does not support std::regex. Please upgrade to a newer version. (i.e. >=g++-5)"
#endif

#include <unordered_map>
#include <ruby.h>
#include <ruby/version.h>

#if (RUBY_API_VERSION_MAJOR > 2 || (RUBY_API_VERSION_MAJOR == 2 && RUBY_API_VERSION_MINOR >= 7)) && defined(__GLIBC__)
namespace std {
	static inline void* ruby_nonempty_memcpy(void *dest, const void *src, size_t n) {
		return ::ruby_nonempty_memcpy(dest, src, n);
	}
};
#endif

#define RAPIDJSON_SCHEMA_USE_INTERNALREGEX 0
#define RAPIDJSON_SCHEMA_USE_STDREGEX 1

#include <rapidjson/schema.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>

typedef std::unordered_map<std::string, VALUE> SchemaInput;
typedef std::unordered_map<ID, rapidjson::SchemaDocument> SchemaCollection;
struct SchemaManager;

class SchemaDocumentProvider : public rapidjson::IRemoteSchemaDocumentProvider {
	SchemaManager* schema_manager;

	public:
		SchemaDocumentProvider(SchemaManager* schema_manager) : schema_manager(schema_manager) { }
		virtual const rapidjson::SchemaDocument* GetRemoteDocument(const char* uri, rapidjson::SizeType length);
};

struct SchemaManager {
	SchemaInput input;
	SchemaCollection collection;
	SchemaDocumentProvider provider;
	SchemaManager() : provider(this) { }

	const rapidjson::SchemaDocument* provide(const char* uri, rapidjson::SizeType length);
};

rapidjson::Document parse_document(VALUE arg) {
	if (RB_TYPE_P(arg, T_FILE)) {
		VALUE str = rb_funcall(arg, rb_intern("read"), 0);
		rb_funcall(arg, rb_intern("rewind"), 0);
		arg = str;
	}
	else if (RB_TYPE_P(arg, T_HASH)) {
		arg = rb_funcall(arg, rb_intern("to_json"), 0);
	}

	rapidjson::Document document;

	if (document.Parse(StringValuePtr(arg), RSTRING_LEN(arg)).HasParseError())
		throw std::invalid_argument("document is not valid JSON");

	return document;
}

const rapidjson::SchemaDocument* SchemaDocumentProvider::GetRemoteDocument(const char* uri, rapidjson::SizeType length) {
	return schema_manager->provide(uri, length);
}

const rapidjson::SchemaDocument* SchemaManager::provide(const char* uri, rapidjson::SizeType length) {
	auto collection_it = collection.find(rb_intern2(uri, length));

	if (collection_it != collection.end())
		return &collection_it->second;

	auto input_it = input.find(std::string(uri, length));

	if (input_it != input.end()) {
		return &collection.emplace(
		  rb_intern2(uri, length),
		  rapidjson::SchemaDocument(
		    parse_document(input_it->second),
		    uri,
		    length,
		    &provider
		  )
		).first->second;
	}

	throw std::invalid_argument(std::string("$ref remote schema not provided: ") + uri);
}

VALUE perform_validation(VALUE self, VALUE schema_arg, VALUE document_arg, bool with_errors) {
	try {
		SchemaManager* schema_manager;
		Data_Get_Struct(self, SchemaManager, schema_manager);
		auto document = parse_document(document_arg);
		auto validate = [with_errors, &document](const rapidjson::SchemaDocument& schema) -> VALUE {
			rapidjson::SchemaValidator validator(schema);
			if (document.Accept(validator)) {
				return with_errors ? rb_ary_new() : Qtrue;
			}
			else {
				if (!with_errors)
					return Qfalse;
				rapidjson::StringBuffer buffer;
				rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
				validator.GetError().Accept(writer);
				VALUE errors = rb_funcall(rb_const_get(rb_cObject, rb_intern("JSON")), rb_intern("parse"), 1, rb_str_new_cstr(buffer.GetString()));
				return RB_TYPE_P(errors, T_HASH) ? rb_ary_new4(1, &errors) : errors;
			}
		};

		if (SYMBOL_P(schema_arg)) {
			auto it = schema_manager->collection.find(SYM2ID(schema_arg));
			if (it == schema_manager->collection.end())
				rb_raise(rb_eArgError, "schema not found: %s", rb_id2name(SYM2ID(schema_arg)));
			return validate(it->second);
		}
		else {
			auto schema = rapidjson::SchemaDocument(
			  parse_document(schema_arg),
			  0,
			  0,
			  &schema_manager->provider
			);
			return validate(schema);
		}
	}
	catch (const std::invalid_argument& e) {
		rb_raise(rb_eArgError, "%s", e.what());
	}
}

extern "C" VALUE validator_validate(VALUE self, VALUE schema_arg, VALUE document_arg) {
	return perform_validation(self, schema_arg, document_arg, true);
}

extern "C" VALUE validator_valid(VALUE self, VALUE schema_arg, VALUE document_arg) {
	return perform_validation(self, schema_arg, document_arg, false);
}

extern "C" void validator_free(SchemaManager* schema_manager) {
	delete schema_manager;
}

extern "C" VALUE validator_alloc(VALUE self) {
	auto* schema_manager = new SchemaManager;
	return Data_Wrap_Struct(self, NULL, validator_free, schema_manager);
}

extern "C" int validator_initialize_load_schema(VALUE key, VALUE value, VALUE input) {
	reinterpret_cast<SchemaManager*>(input)->input.emplace(StringValueCStr(key), value);
	return ST_CONTINUE;
}

extern "C" VALUE validator_initialize(int argc, VALUE* argv, VALUE self) {
	if (argc == 0)
		return self;

	SchemaManager* schema_manager;
	Data_Get_Struct(self, SchemaManager, schema_manager);

#if RUBY_API_VERSION_MAJOR > 2 || (RUBY_API_VERSION_MAJOR == 2 && RUBY_API_VERSION_MINOR >= 7)
	rb_hash_foreach(argv[0], reinterpret_cast<st_foreach_callback_func*>(validator_initialize_load_schema), reinterpret_cast<VALUE>(schema_manager));
#else
	rb_hash_foreach(argv[0], reinterpret_cast<int(*)(...)>(validator_initialize_load_schema), reinterpret_cast<VALUE>(schema_manager));
#endif

	try {
		for (const auto& schema : schema_manager->input) {
			schema_manager->provide(schema.first.c_str(), schema.first.length());
		}
	}
	catch (const std::invalid_argument& e) {
		rb_raise(rb_eArgError, "%s", e.what());
	}

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
	rb_define_method(cValidator, "valid?", reinterpret_cast<VALUE(*)(...)>(validator_valid), 2);
}
