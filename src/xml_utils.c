#include "PWML/xml_utils.h"
#include "PWML/file_utils.h"
#include "libxml/xmlstring.h"
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xmlerror.h>

bool _xml_utils_combine_files(const char* path_a, const char* path_b, const char* destination_path) {
	xmlDoc* doc_a = xmlReadFile(path_a, NULL, 0);
	if (!doc_a) {
		const xmlError* error = xmlGetLastError();
		g_printerr("Failed to read xml file %s\nError: %s\n", path_a, error->message);
		return false;
	}

	xmlDoc* doc_b = xmlReadFile(path_b, NULL, 0);
	if (!doc_b) {
		const xmlError* error = xmlGetLastError();
		g_printerr("Failed to read xml file %s\nError: %s\n", path_b, error->message);
		xmlFreeDoc(doc_a);
		return false;
	}

	xmlNode* root_a = xmlDocGetRootElement(doc_a);
	xmlNode* root_b = xmlDocGetRootElement(doc_b);

	xmlDoc* new_doc = xmlNewDoc(BAD_CAST "1.0");
	xmlNode* new_root = xmlNewNode(NULL, root_a->name);
	xmlDocSetRootElement(new_doc, new_root);

	for (xmlAttr* attr = root_a->properties; attr; attr = attr->next) {
		xmlChar* value = xmlGetProp(root_a, attr->name);
		if (value) {
			xmlSetProp(new_root, attr->name, value);
			xmlFree(value);
		}
	}

	if (root_a->ns) {
		xmlNewNs(new_root, root_a->ns->href, root_a->ns->prefix);
	}

	for (xmlNode* cur = root_a->children; cur; cur = cur->next) {
		xmlNode* copy = xmlDocCopyNode(cur, new_doc, 1);
		xmlAddChild(new_root, copy);
	}

	for (xmlNode* cur = root_b->children; cur; cur = cur->next) {
		xmlNode* copy = xmlDocCopyNode(cur, new_doc, 1);
		xmlAddChild(new_root, copy);
	}

	xmlSaveFormatFileEnc(destination_path, new_doc, "UTF-8", 1);

	xmlFreeDoc(doc_a);
	xmlFreeDoc(doc_b);
	xmlFreeDoc(new_doc);
	xmlCleanupParser();

	return true;
}

bool _xml_utils_combine_all_files(GPtrArray* files, const char* destination_path) {
	for (uint i = 0; i < files->len; i++) {
		const char* path = g_ptr_array_index(files, i);
		if (g_file_test(destination_path, G_FILE_TEST_EXISTS)) {
			if (!_xml_utils_combine_files(destination_path, path, destination_path))
				return false;
		} else {
			_file_utils_copy_file_with_path(path, destination_path);
		}
	}

	return true;
}
