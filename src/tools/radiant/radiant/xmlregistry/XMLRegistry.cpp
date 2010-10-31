/*	This is the implementation of the XMLRegistry structure providing easy methods to store
 * 	all kinds of information like ui state, toolbar structures and anything that fits into an XML file.
 *
 * 	This is the actual implementation of the abstract base class defined in iregistry.h.
 *
 *  Note: You need to include iregistry.h in order to use the Registry like in the examples below.
 *
 *  Example: store a global variable:
 *  	GlobalRegistry().set("user/ui/showAllLightRadii", "1");
 *
 *  Example: retrieve a global variable
 *  (this returns "" if the key is not found and an error is written to globalOutputStream):
 *  	std::string value = GlobalRegistry().get("user/ui/showalllightradii");
 *
 *  Example: import an XML file into the registry (note: imported keys overwrite previous ones!)
 * 		GlobalRegistry().importFromFile(absolute_path_to_file[, where_to_import]);
 *
 *  Example: export a path/key to a file:
 *  	GlobalRegistry().exportToFile(node_to_export, absolute_path_to_file);
 */

#include "iregistry.h"		// The Abstract Base Class
#include <vector>
#include <map>

#include "stringio.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"

#include "iplugin.h"

// Needed for the split algorithm
typedef std::vector<std::string> stringParts;

typedef std::multimap<const std::string, RegistryKeyObserver*> KeyObserverMap;

class XMLRegistry: public Registry
{
	public:
		// Radiant Module stuff
		typedef Registry Type;
		STRING_CONSTANT(Name, "*");

		// Return the static instance
		Registry* getTable() {
			return this;
		}

	private:

		// The default import node and toplevel node
		std::string _topLevelNode;
		std::string _defaultImportNode;

		// The private pointers to the libxml2 and xmlutil objects
		xml::Document _registry;
		xmlDocPtr _origXmlDocPtr;
		xmlNodePtr _importNode;

		// The map with all the keyobservers that are currently connected
		KeyObserverMap _keyObservers;

	public:

		/* Constructor:
		 * Creates an empty XML structure in the memory and adds the nodes _topLevelNode
		 */
		XMLRegistry() :
			_registry(NULL), _origXmlDocPtr(NULL), _importNode(NULL) {
			_topLevelNode = "uforadiant";
			_defaultImportNode = std::string("/") + _topLevelNode;

			// Create the base XML structure with the <darkradiant> top-level tag
			_origXmlDocPtr = xmlNewDoc(xmlCharStrdup("1.0"));
			_origXmlDocPtr->children = xmlNewDocNode(_origXmlDocPtr, NULL, xmlCharStrdup(_topLevelNode.c_str()),
					xmlCharStrdup(""));

			// Store the newly created document into the member variable _registry
			_registry = xml::Document(_origXmlDocPtr);
			_importNode = _origXmlDocPtr->children;
		}

		xml::NodeList findXPath(const std::string& path) {
			return _registry.findXPath(prepareKey(path));
		}

		/*	Checks whether a key exists in the XMLRegistry by querying the XPath
		 */
		bool keyExists(const std::string& key) {
			std::string fullKey = prepareKey(key);

			xml::NodeList result = _registry.findXPath(fullKey);
			return (result.size() > 0);
		}

		/*	Checks whether the key is an absolute or a relative path
		 * 	Absolute paths are returned unchanged,
		 *  a prefix with the toplevel node (e.g. "/darkradiant") is appended to the relative ones
		 */
		std::string prepareKey(const std::string& key) {
			std::string returnValue = key;

			if (returnValue.length() == 0) {
				// no string passed, return to sender
				return returnValue;
			} else if (returnValue[0] == '/') {
				// this is a path relative to root, don't alter it
				return returnValue;
			} else {
				// add the prefix <darkradiant> and return
				returnValue = std::string("/") + _topLevelNode + std::string("/") + key;
				return returnValue;
			}
		}

		/* Deletes this key and all its children,
		 * this includes multiple instances nodes matching this key
		 */
		void deleteXPath(const std::string& path) {
			// Add the toplevel node to the path if required
			std::string fullPath = prepareKey(path);
			xml::NodeList nodeList = _registry.findXPath(fullPath);

			if (nodeList.size() > 0) {
				for (unsigned int i = 0; i < nodeList.size(); i++) {
					// unlink the node from the list first, otherwise: crashes ahead!
					xmlUnlinkNode(nodeList[i].getNodePtr());

					// All child nodes are freed recursively
					xmlFreeNode(nodeList[i].getNodePtr());
				}
			}
		}

		/*	Adds a key <key> as child to <path> to the XMLRegistry (with the name attribute set to <name>)
		 */
		xml::Node createKeyWithName(const std::string& path, const std::string& key, const std::string& name) {
			// Add the toplevel node to the path if required
			std::string fullPath = prepareKey(path);

			xmlNodePtr insertPoint;

			// Check if the insert point <path> exists, create it otherwise
			if (!keyExists(fullPath)) {
				insertPoint = createKey(fullPath);
			} else {
				xml::NodeList nodeList = _registry.findXPath(fullPath);
				insertPoint = nodeList[0].getNodePtr();
			}

			// Add the <key> to the insert point <path>
			xmlNodePtr createdNode = xmlNewChild(insertPoint, NULL, xmlCharStrdup(key.c_str()), xmlCharStrdup(""));

			if (createdNode != NULL) {
				addWhiteSpace(createdNode);

				// Create an xml::Node out of the xmlNodePtr createdNode and set the name attribute
				xml::Node node(createdNode);
				node.setAttributeValue("name", name);

				// Return the newly created node
				return node;
			} else {
				globalOutputStream() << "XMLRegistry: Critical: Could not create insert point.\n";
				return NULL;
			}
		}

		void addWhiteSpace(xmlNodePtr& node) {
			xmlNodePtr whitespace = xmlNewText(xmlCharStrdup("\n"));
			xmlAddSibling(node, whitespace);
		}

		/*	Adds a key to the XMLRegistry (without value, just the node)
		 *  The key has to be an absolute path like "/darkradiant/globals/ui/something
		 *  All required parent nodes are created automatically, if they don't exist
		 */
		xmlNodePtr createKey(const std::string& key) {
			// Add the toplevel node to the path if required
			std::string fullKey = prepareKey(key);

			stringParts parts;
			string::splitBy(fullKey, parts, "/");

			//globalOutputStream() << "XMLRegistry: Inserting key: " << key.c_str() << "\n";

			xmlNodePtr createdNode = NULL;

			// Are there any slashes in the path at all? If not, exit, we've no use for this
			if (parts.size() > 0) {

				// The temporary path variable for walking through the hierarchy
				std::string path("");

				// If the whole path does not exist, insert at the root node
				xmlNodePtr insertPoint = _importNode;

				for (unsigned int i = 0; i < parts.size(); i++) {
					if (parts[i] == "")
						continue;

					// Construct the new path to be searched for
					path += "/" + parts[i];

					// Check if the path exists
					xml::NodeList nodeList = _registry.findXPath(path);
					if (nodeList.size() > 0) {
						// node exists, set the insertPoint to this node and continue
						insertPoint = nodeList[0].getNodePtr();
					} else {
						// Node not found, insert it and store the newly created node as new insertPoint
						createdNode
								= xmlNewChild(insertPoint, NULL, xmlCharStrdup(parts[i].c_str()), xmlCharStrdup(""));
						//addWhiteSpace(createdNode);
						insertPoint = createdNode;
					}
				}

				// return the pointer to the deepest, newly created node
				return createdNode;
			} else {
				globalOutputStream() << "XMLRegistry: Cannot insert key/path without slashes.\n";
				return NULL;
			}
		}

		/* Gets a key from the registry, /darkradiant is automatically added by prepareKey()
		 * if relative paths are used
		 */
		std::string get(const std::string& key) {
			// Add the toplevel node to the path if required
			std::string fullKey = prepareKey(key);

			//globalOutputStream() << "XMLRegistry: Querying key: " << fullKey.c_str() << "\n";

			// Try to load the node, return an empty string if nothing is found
			xml::NodeList nodeList = _registry.findXPath(fullKey);

			// Does it even exist?
			// There is the theoretical case that this returns two nodes that match the key criteria
			// This function always uses the first one, but this may be changed if this turns out to be problematic
			if (nodeList.size() > 0) {
				// Load the node and get the value
				xml::Node node = nodeList[0];
				return node.getAttributeValue("value");
			} else {
				//globalOutputStream() << "XMLRegistry: GET: Key " << fullKey.c_str() << " not found, returning empty string!\n";
				return std::string("");
			}
		}

		/* Gets a key containing a float from the registry, basically loads the string and
		 * converts it into a float */
		float getFloat(const std::string& key) {
			// Load the key
			const std::string valueStr = get(key);
			return string::toFloat(valueStr);
		}

		/* Sets a registry key value to the given float. */
		void setFloat(const std::string& key, const double& value) {
			// Try to convert the float into a string
			std::string valueStr = string::toString(value);

			// Pass the call to set() to do the rest
			set(key, valueStr);
		}

		/* Gets a key containing an integer from the registry, basically loads the string and
		 * converts it into an int */
		int getInt(const std::string& key) {
			// Load the key
			const std::string valueStr = get(key);
			return string::toInt(valueStr);
		}

		// Sets a registry key value to the given integer. The value is converted via boost libraries first.
		void setInt (const std::string& key, const int& value) {
			// Try to convert the int into a string
			std::string valueStr = string::toString(value);

			// Pass the call to set() to do the rest
			set(key, valueStr);
		}


		// Sets the value of a key from the registry,
		// "/darkradiant" is automatically added if relative paths are used
		void set(const std::string& key, const std::string& value) {
			// Add the toplevel node to the path if required
			std::string fullKey = prepareKey(key);

			// If the key doesn't exist, we have to create an empty one
			if (!keyExists(fullKey)) {
				createKey(fullKey);
			}

			// Try to find the node
			xml::NodeList nodeList = _registry.findXPath(fullKey);

			if (nodeList.size() > 0) {
				// Load the node and set the value
				xml::Node node = nodeList[0];
				node.setAttributeValue("value", value);

				// Notify the observers, but use the unprepared key as argument!
				notifyKeyObservers(key);
			} else {
				// If the key is still not found, something nasty has happened
				globalOutputStream() << "XMLRegistry: Critical: Key " << fullKey.c_str()
						<< " not found (it really should be there)!\n";
			}
		}

		/* Appends a whole (external) XML file to the XMLRegistry. The toplevel nodes of this file
		 * are appended to _topLevelNode (e.g. <darkradiant>) if parentKey is set to the empty string "",
		 * otherwise they are imported as a child of the specified parentKey
		 */
		void importFromFile(const std::string& importFilePath, const std::string& parentKey) {
			std::string importKey = parentKey;

			// If an empty parentKey was passed, set it to the default import node
			if (importKey == "") {
				importKey = _defaultImportNode;
			}

			// Check if the importKey exists - if not: create it
			std::string fullImportKey = prepareKey(importKey);

			//globalOutputStream() << "XMLRegistry: Looking for key " << fullImportKey.c_str() << "\n";

			if (!keyExists(fullImportKey)) {
				createKey(fullImportKey);
			}

			// Set the "mountpoint" to its default value, the toplevel node
			xmlNodePtr importNode = _importNode;

			xml::NodeList importNodeList = _registry.findXPath(fullImportKey);
			if (importNodeList.size() > 0) {
				importNode = importNodeList[0].getNodePtr();
			} else {
				globalOutputStream() << "XMLRegistry: Critical: ImportNode could not be found.\n";
			}

			globalOutputStream() << "XMLRegistry: Importing XML file: " << importFilePath.c_str() << "\n";

			// Load the file
			xmlDocPtr pImportDoc = xmlParseFile(importFilePath.c_str());

			if (pImportDoc) {
				// Convert it into xml::Document and load the top-level node(s) (there should only be one)
				xml::Document importDoc(pImportDoc);
				xml::NodeList topLevelNodes = importDoc.findXPath("/*");

				if (importNode->children != NULL) {

					if (importNode->name != NULL) {
						for (unsigned int i = 0; i < topLevelNodes.size(); i++) {
							xmlNodePtr newNode = topLevelNodes[0].getNodePtr();

							// Add each of the imported nodes at the top to the registry
							xmlAddPrevSibling(importNode->children, newNode);
						}
					}
				} else {
					globalOutputStream() << "XMLRegistry: Critical: Could not import XML file. importNode is NULL!\n";
				}
			} else {
				globalOutputStream() << "XMLRegistry: Critical: Could not parse " << importFilePath.c_str() << "\n";
				globalOutputStream() << "XMLRegistry: Critical: File does not exist or is not valid XML!\n";
			}
		}

		// Dumps the current registry to std::out, for debugging purposes
		void dump() const {
			xmlSaveFile("-", _origXmlDocPtr);
		}

		/*	Saves a specified path to the file <filename>. Use "-" if you want to write to std::out
		 */
		void exportToFile(const std::string& key, const std::string& filename) {
			// Add the toplevel node to the key if required
			std::string fullKey = prepareKey(key);

			// Try to find the specified node
			xml::NodeList result = _registry.findXPath(fullKey);

			if (result.size() > 0) {
				// Create a new XML document
				xmlDocPtr targetDoc = xmlNewDoc(xmlCharStrdup("1.0"));

				// Copy the node from the XMLRegistry and set it as root node of the newly created document
				xmlNodePtr exportNode = xmlCopyNode(result[0].getNodePtr(), 1);
				xmlDocSetRootElement(targetDoc, exportNode);

				// Save the whole document to the specified filename
				xmlSaveFormatFile(filename.c_str(), targetDoc, 1);

				globalOutputStream() << "XMLRegistry: Saved " << key << " to " << filename << "\n";
			} else {
				globalOutputStream() << "XMLRegistry: Failed to save path " << fullKey << "\n";
			}
		}

		// Add an observer watching the <observedKey> to the internal list of observers.
		void addKeyObserver(RegistryKeyObserver* observer, const std::string& observedKey) {
			_keyObservers.insert(std::make_pair(observedKey, observer));
		}

		// Removes an observer watching the <observedKey> from the internal list of observers.
		void removeKeyObserver(RegistryKeyObserver* observer) {
			// Traverse through the keyObserverMap and try to find the specified observer
			for (KeyObserverMap::iterator it = _keyObservers.begin(); it != _keyObservers.end(); it++) {
				if (it->second == observer) {
					// Delete the found key (by passing the iterator pointing to it)
					_keyObservers.erase(it);
				}
			}
		}

		// Destructor
		~XMLRegistry() {
		}

	private:

		// Cycles through the key observers and notifies the ones that observe the given <changedKey>
		void notifyKeyObservers(const std::string& changedKey) {
			for (KeyObserverMap::iterator it = _keyObservers.find(changedKey); it != _keyObservers.upper_bound(
					changedKey) && it != _keyObservers.end(); it++) {
				RegistryKeyObserver* keyObserver = it->second;

				if (keyObserver != NULL) {
					keyObserver->keyChanged();
				}
			}
		}
};

/* XMLRegistry dependencies class.
 */

class XMLRegistryDependencies
{
};

/* Required code to register the module with the ModuleServer.
 */

#include "modulesystem/singletonmodule.h"

typedef SingletonModule<XMLRegistry, XMLRegistryDependencies> XMLRegistryModule;

typedef Static<XMLRegistryModule> StaticXMLRegistrySystemModule;
StaticRegisterModule staticRegisterXMLRegistrySystem(StaticXMLRegistrySystemModule::instance());
