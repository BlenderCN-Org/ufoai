#ifndef IREGISTRY_H_
#define IREGISTRY_H_

#include <string>
#include "xmlutil/Document.h"
#include "xmlutil/Node.h"
#include "generic/constant.h"

// Abstract base class for a registry key observer, gets called by the registry
// when a certain key changes.
class RegistryKeyObserver {
public:
	// the callback method
	virtual void keyChanged() = 0;
};

/** Abstract base class for a registry system
 */

class Registry {

public:
	INTEGER_CONSTANT(Version, 1);
	STRING_CONSTANT(Name, "registry");

	virtual ~Registry () {}

	// Sets a variable in the XMLRegistry or retrieves one
	virtual void set(const std::string& key, const std::string& value) = 0;
	virtual std::string get(const std::string& key) = 0;

	// Loads/saves a floating point value from/to the specified <key>, getFloat returns 0.0f if conversion failed
	virtual float getFloat(const std::string& key) = 0;
	virtual void setFloat(const std::string& key, const double& value) = 0;

	// Loads/saves an integer value from/to the specified <key>, getInt returns 0 if conversion failed
	virtual int getInt(const std::string& key) = 0;
	virtual void setInt(const std::string& key, const int& value) = 0;

	// Checks whether a key exists in the registry
	virtual bool keyExists(const std::string& key) = 0;

	// Adds a whole XML file to the registry
	virtual void importFromFile(const std::string& importFilePath,
			const std::string& parentKey) = 0;

	// Dumps the whole XML content to std::out for debugging purposes
	virtual void dump() const = 0;

	// Saves the specified node and all its children into the file <filename>
	virtual void exportToFile(const std::string& key,
			const std::string& filename = "-") = 0;

	// Retrieves the nodelist corresponding for the specified XPath (wraps to xml::Document)
	virtual xml::NodeList findXPath(const std::string& path) = 0;

	// Creates an empty key
	virtual xmlNodePtr createKey(const std::string& key) = 0;

	// Creates a new node named <key> as children of <path> with the name attribute set to <name>
	// The newly created node is returned after creation
	virtual xml::Node createKeyWithName(const std::string& path,
			const std::string& key, const std::string& name) = 0;

	// Deletes an entire subtree from the registry
	virtual void deleteXPath(const std::string& path) = 0;

	// Add an observer watching the <observedKey> to the internal list of observers.
	virtual void addKeyObserver(RegistryKeyObserver* observer,
			const std::string& observedKey) = 0;

	// Remove the specified observer from the list
	virtual void removeKeyObserver(RegistryKeyObserver* observer) = 0;
};

// Module definitions

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<Registry> GlobalRegistryModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<Registry> GlobalRegistryModuleRef;

// This is the accessor for the registry
inline Registry& GlobalRegistry() {
	return GlobalRegistryModule::getTable();
}

#endif /*IREGISTRY_H_*/
