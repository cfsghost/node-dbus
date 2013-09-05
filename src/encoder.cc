#include <v8.h>
#include <node.h>
#include <string>
#include <dbus/dbus.h>

#include "encoder.h"

namespace Encoder {

	using namespace node;
	using namespace v8;
	using namespace std;

	bool EncodeObject(Local<Value> value, DBusMessageIter *iter, char *signature)
	{
		DBusSignatureIter siter;
		int type;

		// Get type of current value
		dbus_signature_iter_init(&siter, signature);
		type = dbus_signature_iter_get_current_type(&siter);

		switch(type) {
		case DBUS_TYPE_BOOLEAN:
		{
			dbus_bool_t data = value->BooleanValue();

			if (!dbus_message_iter_append_basic(iter, type, &data)) {
				printf("Failed to encode boolean value\n");
				return false;
			}

			break;
		}

		case DBUS_TYPE_INT16:
		case DBUS_TYPE_INT32:
		case DBUS_TYPE_INT64:
		case DBUS_TYPE_UINT16:
		case DBUS_TYPE_UINT32:
		case DBUS_TYPE_UINT64:
		case DBUS_TYPE_BYTE:
		{
			dbus_uint64_t data = value->IntegerValue();

			if (!dbus_message_iter_append_basic(iter, type, &data)) {
				printf("Failed to encode numeric value\n");
				return false;
			}

			break;
		}

		case DBUS_TYPE_STRING:
		case DBUS_TYPE_OBJECT_PATH:
		case DBUS_TYPE_SIGNATURE:
		{
			String::Utf8Value data_val(value->ToString());
			char *data = *data_val;

			if (!dbus_message_iter_append_basic(iter, type, &data)) {
				printf("Failed to encode string value\n");
				return false;
			}

			break;
		}

		case DBUS_TYPE_DOUBLE:
		{
			double data = value->NumberValue();

			if (!dbus_message_iter_append_basic(iter, type, &data)) {
				printf("Failed to encode double value\n");
				return false;
			}

			break;
		}

		case DBUS_TYPE_ARRAY:
		{
			if (!value->IsObject()) {
				printf("Failed to encode dictionary\n");
				return false;
			}

			DBusMessageIter subIter;
			DBusSignatureIter arraySiter;
			char *array_sig = NULL;

			// Getting signature of array object
			dbus_signature_iter_recurse(&siter, &arraySiter);
			array_sig = dbus_signature_iter_get_signature(&arraySiter);

			// Open array container to process elements in there
			if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, array_sig, &subIter)) {
				printf("Can't open container for Array type\n");
				dbus_free(array_sig); 
				return false; 
			}

			// It's a dictionary
			if (dbus_signature_iter_get_element_type(&siter) == DBUS_TYPE_DICT_ENTRY) {

				Local<Object> value_object = value->ToObject();
				DBusMessageIter subIter;
				DBusSignatureIter dictSubSiter;

				dbus_free(array_sig);

				// Getting sub-signature object
				dbus_signature_iter_recurse(&arraySiter, &dictSubSiter);
				dbus_signature_iter_next(&dictSubSiter);

				// process each elements
				Local<Array> prop_names = value_object->GetPropertyNames();
				unsigned int len = prop_names->Length();

				for (unsigned int i = 0; i < len; ++i) {
					DBusMessageIter dict_iter;

					// Open dict entry container
					if (!dbus_message_iter_open_container(&subIter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_iter)) {
						printf("Can't open container for DICT-ENTRY\n");
						return false;
					}

					Local<Value> prop_key = prop_names->Get(i);
					char *prop_key_str = *String::Utf8Value(prop_key->ToString());

					// Append the key
					dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &prop_key_str);

					// Append the value
					char *cstr = dbus_signature_iter_get_signature(&dictSubSiter);
					if (!EncodeObject(value_object->Get(prop_key), &dict_iter, cstr)) {
						printf("Failed to encode element of dictionary\n");
						return false;
					}

					dbus_free(cstr);
					dbus_message_iter_close_container(&subIter, &dict_iter); 
				}

				break;

			}

			if (!value->IsArray()) {
				printf("Failed to encode array object\n");
				return false;
			}

			// process each elements
			Local<Array> arrayData = Local<Array>::Cast(value);
			for (unsigned int i = 0; i < arrayData->Length(); ++i) {
				Local<Value> arrayItem = arrayData->Get(i);
				if (!EncodeObject(arrayItem, &subIter, array_sig))
					break;
			}

			dbus_message_iter_close_container(iter, &subIter);
			dbus_free(array_sig);

			break;
		}
		

		}

		return true;
	}

}
