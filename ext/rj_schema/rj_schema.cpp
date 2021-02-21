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
#include <sstream>
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
#include <rapidjson/writer.h>
#include <rapidjson/error/error.h>
#include <rapidjson/error/en.h>

typedef rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator> ErrorType;
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
	else if (!RB_TYPE_P(arg, T_STRING)) {
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

static void CreateHumanErrorMessages(std::ostringstream& ss, const ErrorType& errors, size_t depth = 0, const char* context = 0) {
	auto get_string = [](const ErrorType& val) -> std::string {
		if (val.IsString())
			return val.GetString();
		else if (val.IsDouble())
			return std::to_string(val.GetDouble());
		else if (val.IsUint())
			return std::to_string(val.GetUint());
		else if (val.IsInt())
			return std::to_string(val.GetInt());
		else if (val.IsUint64())
			return std::to_string(val.GetUint64());
		else if (val.IsInt64())
			return std::to_string(val.GetInt64());
		else if (val.IsBool() && val.GetBool())
			return "true";
		else if (val.IsBool())
			return "false";
		else if (val.IsFloat())
			return std::to_string(val.GetFloat());
		return "";
	};

	auto handle_error = [&ss, &get_string](const char* errorName, const ErrorType& error, size_t depth, const char* context) {
		if (!error.ObjectEmpty()) {
			int code = error["errorCode"].GetInt();
			std::string message(GetValidateError_En(static_cast<rapidjson::ValidateErrorCode>(code)));

			for (const auto& member : error.GetObject()) {
				std::string insertName("%");
				insertName += member.name.GetString();
				std::size_t insertPos = message.find(insertName);

				if (insertPos != std::string::npos) {
					std::string insertString("");
					const ErrorType& insert = member.value;

					if (insert.IsArray()) {
						for (ErrorType::ConstValueIterator itemsItr = insert.Begin(); itemsItr != insert.End(); ++itemsItr) {
							if (itemsItr != insert.Begin())
								insertString += ",";

							insertString += get_string(*itemsItr);
						}
					}
					else {
						insertString += get_string(insert);
					}

					message.replace(insertPos, insertName.length(), insertString);
				}
			}

			std::string indent(depth * 2, ' ');
			ss << indent << "Error Name: " << errorName << std::endl;
			ss << indent << "Message: " << message << std::endl;
			ss << indent << "Instance: " << error["instanceRef"].GetString() << std::endl;
			ss << indent << "Schema: " << error["schemaRef"].GetString() << std::endl;
			if (depth > 0)
				ss << indent << "Context: " << context << std::endl;
			ss << std::endl;

			if (error.HasMember("errors")) {
				depth++;
				const ErrorType& childErrors = error["errors"];

				if (childErrors.IsArray()) {
					for (const auto& item : childErrors.GetArray()) {
						CreateHumanErrorMessages(ss, item, depth, errorName);
					}
				}
				else if (childErrors.IsObject()) {
					for (const auto& member : childErrors.GetObject()) {
						CreateHumanErrorMessages(ss, member.value, depth, errorName);
					}
				}
			}
		}
	};

	for (const auto& member : errors.GetObject()) {
		const char* errorName = member.name.GetString();
		const ErrorType& errorContent = member.value;

		if (errorContent.IsArray()) {
			for (const auto& item : errorContent.GetArray()) {
				handle_error(errorName, item, depth, context);
			}
		}
		else if (errorContent.IsObject()) {
			handle_error(errorName, errorContent, depth, context);
		}
	}
}

VALUE perform_validation(VALUE self, VALUE schema_arg, VALUE document_arg, VALUE continue_on_error, VALUE machine_errors, VALUE human_errors) {
	if (continue_on_error == Qnil)
		continue_on_error = Qfalse;
	if (machine_errors == Qnil)
		machine_errors = Qtrue;
	if (human_errors == Qnil)
		human_errors = Qfalse;

	try {
		SchemaManager* schema_manager;
		Data_Get_Struct(self, SchemaManager, schema_manager);

		auto document = parse_document(document_arg);
		auto validate = [&continue_on_error, &machine_errors, &human_errors, &document](const rapidjson::SchemaDocument& schema) -> VALUE {
			rapidjson::SchemaValidator validator(schema);

			if (continue_on_error == Qtrue)
				validator.SetValidateFlags(rapidjson::RAPIDJSON_VALIDATE_DEFAULT_FLAGS | rapidjson::kValidateContinueOnErrorFlag);

			document.Accept(validator);

			if (machine_errors == Qfalse && human_errors == Qfalse)
				return validator.IsValid() ? Qtrue : Qfalse;

			VALUE result = rb_hash_new();

			if (machine_errors == Qtrue) {
				rapidjson::StringBuffer buffer;
				rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
				validator.GetError().Accept(writer);
				rb_hash_aset(
				  result,
				  ID2SYM(rb_intern("machine_errors")),
				  rb_funcall(rb_const_get(rb_cObject, rb_intern("JSON")), rb_intern("parse"), 1, rb_str_new_cstr(buffer.GetString()))
				);
			}

			if (human_errors == Qtrue) {
				std::ostringstream ss;
				CreateHumanErrorMessages(ss, validator.GetError());
				rb_hash_aset(
				  result,
				  ID2SYM(rb_intern("human_errors")),
				  rb_str_new_cstr(ss.str().c_str())
				);
			}

			return result;
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

extern "C" VALUE validator_validate(int argc, VALUE* argv, VALUE self) {
	VALUE schema_arg, document_arg, opts;

	rb_scan_args(argc, argv, "2:", &schema_arg, &document_arg, &opts);

	if (NIL_P(opts))
		opts = rb_hash_new();

	VALUE result = perform_validation(
	  self,
	  schema_arg,
	  document_arg,
	  rb_hash_aref(opts, ID2SYM(rb_intern("continue_on_error"))),
	  rb_hash_aref(opts, ID2SYM(rb_intern("machine_errors"))),
	  rb_hash_aref(opts, ID2SYM(rb_intern("human_errors")))
	);

	return (result == Qtrue || result == Qfalse) ? rb_hash_new() : result;
}

extern "C" VALUE validator_valid(VALUE self, VALUE schema_arg, VALUE document_arg) {
	return perform_validation(self, schema_arg, document_arg, Qfalse, Qfalse, Qfalse);
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
	rb_define_method(cValidator, "validate", reinterpret_cast<VALUE(*)(...)>(validator_validate), -1);
	rb_define_method(cValidator, "valid?", reinterpret_cast<VALUE(*)(...)>(validator_valid), 2);
}
