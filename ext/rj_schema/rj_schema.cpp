#include <memory>
#include <vector>
#include <libgen.h>
#include <ruby.h>

#define RAPIDJSON_SCHEMA_USE_INTERNALREGEX 0
#define RAPIDJSON_SCHEMA_USE_STDREGEX 1

#include <rapidjson/schema.h>
#include <rapidjson/filereadstream.h>

rapidjson::Document parse_schema(const char* schema_path);

class RemoteSchemaDocumentProvider : public rapidjson::IRemoteSchemaDocumentProvider {
	std::string schema_path;

	public:
		RemoteSchemaDocumentProvider(const char* schema_path) : schema_path(schema_path) { }
		virtual const rapidjson::SchemaDocument* GetRemoteDocument(const char* uri, rapidjson::SizeType) {
			std::vector<char> buffer(schema_path.begin(), schema_path.end());
			buffer.push_back('\0');

			char* directory = dirname(&buffer[0]);
			std::string remote_schema_path = directory;
			std::string remote_uri = uri;
			remote_schema_path += '/';
			remote_schema_path += remote_uri.substr(0, remote_uri.find('#'));

			return new rapidjson::SchemaDocument(parse_schema(remote_schema_path.c_str()));
		}
};

rapidjson::Document parse_schema(const char* schema_path) {
	auto* fp = fopen(schema_path, "r");

	if (!fp)
		throw std::invalid_argument(std::string("could not open schema file ") + schema_path);

	rapidjson::Document document;
	char buffer[4096];

	rapidjson::FileReadStream fs(fp, buffer, sizeof(buffer));
	document.ParseStream(fs);

	if (document.HasParseError()) {
		fclose(fp);
		throw std::invalid_argument("schema is not valid JSON");
	}

	fclose(fp);

	return document;
}

rapidjson::Document parse_document(const char* json_document) {
	rapidjson::Document document;

	if (document.Parse(json_document).HasParseError())
		throw std::invalid_argument("document is not valid JSON");

	return document;
}

extern "C" VALUE validate(VALUE self, VALUE schema_path, VALUE json_document) {
	try {
		RemoteSchemaDocumentProvider provider(StringValueCStr(schema_path));

		auto document = parse_document(StringValueCStr(json_document));
		auto schema = rapidjson::SchemaDocument(parse_schema(StringValueCStr(schema_path)), 0, 0, &provider);

		rapidjson::SchemaValidator validator(schema);

		if (!document.Accept(validator)) {
			std::string error;
			rapidjson::StringBuffer sb;

			validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
			error += "Invalid schema: ";
                        error += std::string(sb.GetString());
                        error += "\n";
			error += "Invalid keyword: ";
			error += validator.GetInvalidSchemaKeyword();
			error += "\n";

			sb.Clear();
			validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);

			error += "Invalid document: ";
			error += sb.GetString();
			throw std::runtime_error(error);
		}
	}
	catch (const std::invalid_argument& e) {
		rb_raise(rb_eArgError, e.what());
	}
	catch (const std::runtime_error& e) {
                rb_raise(rb_eRuntimeError, e.what());
	}

	return Qnil;
}

extern "C" void Init_rj_schema(void) {
	rb_define_method(
	  rb_define_class_under(
	    rb_const_get(rb_cObject, rb_intern("RjSchema")),
	    "Validator",
	    rb_cObject
	  ),
	  "validate",
	  reinterpret_cast<VALUE(*)(...)>(validate),
	  2
	);
}
