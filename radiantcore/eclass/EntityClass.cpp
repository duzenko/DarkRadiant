#include "EntityClass.h"

#include "itextstream.h"
#include "ieclasscolours.h"
#include "os/path.h"
#include "string/convert.h"

#include "string/predicate.h"
#include <fmt/format.h>
#include <functional>

namespace eclass
{

const std::string EntityClass::DefaultWireShader("<0.3 0.3 1>");
const std::string EntityClass::DefaultFillShader("(0.3 0.3 1)");
const Vector3 EntityClass::DefaultEntityColour(0.3, 0.3, 1);

EntityClass::EntityClass(const std::string& name, const vfs::FileInfo& fileInfo) :
    EntityClass(name, fileInfo, false)
{}

EntityClass::EntityClass(const std::string& name, const vfs::FileInfo& fileInfo, bool fixedSize)
: _name(name),
  _fileInfo(fileInfo),
  _isLight(false),
  _colour(-1, -1, -1),
  _colourTransparent(false),
  _fixedSize(fixedSize),
  _model(""),
  _skin(""),
  _inheritanceResolved(false),
  _modName("base"),
  _emptyAttribute("", "", ""),
  _parseStamp(0),
  _blockChangeSignal(false)
{}

std::string EntityClass::getName() const
{
    return _name;
}

const IEntityClass* EntityClass::getParent() const
{
    return _parent;
}

sigc::signal<void>& EntityClass::changedSignal()
{
    return _changedSignal;
}

bool EntityClass::isFixedSize() const
{
    if (_fixedSize) {
        return true;
    }
    else {
        // Check for the existence of editor_mins/maxs attributes, and that
        // they do not contain only a question mark
        return (getAttribute("editor_mins").getValue().size() > 1
                && getAttribute("editor_maxs").getValue().size() > 1);
    }
}

AABB EntityClass::getBounds() const
{
    if (isFixedSize())
    {
        return AABB::createFromMinMax(
            string::convert<Vector3>(getAttribute("editor_mins").getValue()),
            string::convert<Vector3>(getAttribute("editor_maxs").getValue())
        );
    }
    else
    {
        return AABB(); // null AABB
    }
}

bool EntityClass::isLight() const
{
    return _isLight;
}

void EntityClass::setIsLight(bool val)
{
    _isLight = val;
    if (_isLight)
        _fixedSize = true;
}

void EntityClass::setColour(const Vector3& colour)
{
    _colour = colour;

    // Set the entity colour to default, if none was specified
    if (_colour == Vector3(-1, -1, -1))
    {
        _colour = DefaultEntityColour;
    }

    // Define fill and wire versions of the entity colour
    _fillShader = _colourTransparent ?
        fmt::format("[{0:f} {1:f} {2:f}]", _colour[0], _colour[1], _colour[2]) :
        fmt::format("({0:f} {1:f} {2:f})", _colour[0], _colour[1], _colour[2]);

    _wireShader = fmt::format("<{0:f} {1:f} {2:f}>", _colour[0], _colour[1], _colour[2]);

    emitChangedSignal();
}

void EntityClass::resetColour()
{
    // An override colour which matches this exact class is final, and overrides
    // everything else
    if (GlobalEclassColourManager().applyColours(*this))
        return;

    // Look for an editor_color on this class only
    const EntityClassAttribute& attr = getAttribute("editor_color", false);
    if (!attr.getValue().empty())
        return setColour(string::convert<Vector3>(attr.getValue()));

    // If there is a parent, inherit its getColour() directly, which takes into
    // account any EClassColourManager overrides at the parent level.
    if (_parent)
        return setColour(_parent->getColour());

    // No parent and no attribute, all we can use is the default colour
    setColour(DefaultEntityColour);
}

const Vector3& EntityClass::getColour() const {
    return _colour;
}

const std::string& EntityClass::getWireShader() const
{
    // Use a fallback shader colour in case we don't have anything
    return !_wireShader.empty() ? _wireShader : DefaultWireShader;
}

const std::string& EntityClass::getFillShader() const
{
    // Use a fallback shader colour in case we don't have anything
    return !_fillShader.empty() ? _fillShader : DefaultFillShader;
}

/* ATTRIBUTES */

/**
 * Insert an EntityClassAttribute, without overwriting previous values.
 */
void EntityClass::addAttribute(const EntityClassAttribute& attribute)
{
    // Try to insert the class attribute
    std::pair<EntityAttributeMap::iterator, bool> result = _attributes.insert(
        EntityAttributeMap::value_type(attribute.getName(), attribute)
    );

    if (!result.second)
    {
        EntityClassAttribute& existing = result.first->second;

        // greebo: Attribute already existed, check if we have some
        // descriptive properties to be added to the existing one.
        if (!attribute.getDescription().empty() && existing.getDescription().empty())
        {
            // Use the shared string reference to save memory
            existing.setDescription(attribute.getDescription());
        }

        // Check if we have a more descriptive type than "text"
        if (attribute.getType() != "text" && existing.getType() == "text")
        {
            // Use the shared string reference to save memory
            existing.setType(attribute.getType());
        }
    }
}

EntityClass::Ptr EntityClass::create(const std::string& name, bool brushes)
{
    vfs::FileInfo emptyFileInfo("def/", "_autogenerated_by_darkradiant_.def", vfs::Visibility::HIDDEN);
    return std::make_shared<EntityClass>(name, emptyFileInfo, !brushes);
}

void EntityClass::forEachAttributeInternal(InternalAttrVisitor visitor,
                                           bool editorKeys) const
{
    // Visit parent attributes, making sure we set the inherited flag
    if (_parent)
        _parent->forEachAttributeInternal(visitor, editorKeys);

    // Visit our own attributes
    for (const auto& pair: _attributes)
    {
        // Visit if it is a non-editor key or we are visiting all keys
        if (editorKeys || !string::istarts_with(pair.first, "editor_"))
        {
            visitor(pair.second);
        }
    }
}

void EntityClass::forEachAttribute(AttributeVisitor visitor,
                                   bool editorKeys) const
{
    // First compile a map of all attributes we need to pass to the visitor,
    // ensuring that there is only one attribute per name (i.e. we don't want to
    // visit the same-named attribute on both a child and one of its ancestors)
    using AttrsByName = std::map<std::string, const EntityClassAttribute*>;
    AttrsByName attrsByName;

    // Internal visit function visits parent first, then child, ensuring that
    // more derived attributes will replace parents in the map
    forEachAttributeInternal(
        [&attrsByName](const EntityClassAttribute& a) {
            attrsByName[a.getName()] = &a;
        },
        editorKeys
    );

    // Pass attributes to the visitor function, setting the inherited flag on
    // any which are not present on this EntityClass
    for (const auto& pair: attrsByName)
    {
        visitor(*pair.second, (_attributes.count(pair.first) == 0));
    }
}

// Resolve inheritance for this class
void EntityClass::resolveInheritance(EntityClasses& classmap)
{
    // If we have already resolved inheritance, do nothing
    if (_inheritanceResolved)
        return;

    // Lookup the parent name and return if it is not set. Also return if the
    // parent name is the same as our own classname, to avoid infinite
    // recursion.
    std::string parName = getAttribute("inherit").getValue();
    if (parName.empty() || parName == _name)
        return;

    // Find the parent entity class
    EntityClasses::iterator pIter = classmap.find(parName);
    if (pIter != classmap.end())
    {
        // Recursively resolve inheritance of parent
        pIter->second->resolveInheritance(classmap);

        // Set our parent pointer
        _parent = pIter->second.get();
    }
    else
    {
        rWarning() << "[eclassmgr] Entity class "
                              << _name << " specifies unknown parent class "
                              << parName << std::endl;
    }

    // Set the resolved flag
    _inheritanceResolved = true;

    if (!getAttribute("model").getValue().empty())
    {
        // We have a model path (probably an inherited one)
        setModelPath(getAttribute("model").getValue());
    }

    if (getAttribute("editor_light").getValue() == "1" || getAttribute("spawnclass").getValue() == "idLight")
    {
        // We have a light
        setIsLight(true);
    }

    if (getAttribute("editor_transparent").getValue() == "1")
    {
        _colourTransparent = true;
    }

    // Set up inheritance of entity colours: colours inherit from parent unless
    // there is an explicit editor_color defined at this level
    resetColour();
    if (_parent)
    {
        _parent->changedSignal().connect(
            sigc::mem_fun(this, &EntityClass::resetColour)
        );
    }
}

bool EntityClass::isOfType(const std::string& className)
{
	for (const IEntityClass* currentClass = this;
         currentClass != NULL;
         currentClass = currentClass->getParent())
    {
        if (currentClass->getName() == className)
		{
			return true;
		}
    }

	return false;
}

std::string EntityClass::getDefFileName()
{
    return _fileInfo.fullPath();
}

// Find a single attribute
EntityClassAttribute& EntityClass::getAttribute(const std::string& name,
                                                bool includeInherited)
{
    return const_cast<EntityClassAttribute&>(
        std::as_const(*this).getAttribute(name, includeInherited)
    );
}

// Find a single attribute
const EntityClassAttribute&
EntityClass::getAttribute(const std::string& name,
                               bool includeInherited) const
{
    // First look up the attribute on this class; if found, we can simply return it
    auto f = _attributes.find(name);
    if (f != _attributes.end())
        return f->second;

    // If there is no parent or we have been instructed to ignore inheritance,
    // this is the end of the line: return an empty attribute
    if (!_parent || !includeInherited)
        return _emptyAttribute;

    // Otherwise delegate to the parent (which will recurse until an attribute
    // is found or a null parent ends the process)
    return _parent->getAttribute(name);
}

void EntityClass::clear()
{
    // Don't clear the name
    _isLight = false;

    _colour = Vector3(-1,-1,-1);
    _colourTransparent = false;

    _fixedSize = false;

    _attributes.clear();
    _model.clear();
    _skin.clear();
    _inheritanceResolved = false;

    _modName = "base";
}

void EntityClass::parseEditorSpawnarg(const std::string& key,
                                           const std::string& value)
{
    // "editor_yyy" represents an attribute that may be set on this
    // entity. Construct a value-less EntityClassAttribute to add to
    // the class, so that it will show in the entity inspector.

    // Locate the space in "editor_bool myVariable", starting after "editor_"
    std::size_t spacePos = key.find(' ', 7);

    // Only proceed if we have a space (some keys like "editor_displayFolder"
    // don't have spaces)
    if (spacePos != std::string::npos)
    {
        // The part beyond the space is the name of the attribute
        std::string attName = key.substr(spacePos + 1);

        // Get the type by trimming the string left and right
        std::string type = key.substr(7, key.length() - attName.length() - 8);

        if (!attName.empty() && type != "setKeyValue") // Ignore editor_setKeyValue
        {
            // Transform the type into a better format
            if (type == "var" || type == "string")
            {
                type = "text";
            }

            // Construct an attribute with empty value, but with valid
            // description
            addAttribute(EntityClassAttribute(type, attName, "", value));
        }
    }
}

void EntityClass::parseFromTokens(parser::DefTokeniser& tokeniser)
{
    // Clear this structure first, we might be "refreshing" ourselves from tokens
    clear();

    // Required open brace (the name has already been parsed by the EClassManager)
    tokeniser.assertNextToken("{");

    // Loop over all of the keys in this entitydef
    std::string key;
    while ((key = tokeniser.nextToken()) != "}")
    {
        const std::string value = tokeniser.nextToken();

        // Handle some keys specially
        if (key == "model")
        {
            setModelPath(os::standardPath(value));
        }
        else if (key == "editor_color")
        {
            setColour(string::convert<Vector3>(value));
        }
        else if (key == "editor_light")
        {
            setIsLight(value == "1");
        }
        else if (key == "spawnclass")
        {
            setIsLight(value == "idLight");
        }
        else if (string::istarts_with(key, "editor_"))
        {
            parseEditorSpawnarg(key, value);
        }

        // We're only interested in non-inherited key/values when parsing
        auto& attribute = getAttribute(key, false);

        // Add the EntityClassAttribute for this key/val
        if (attribute.getType().empty())
        {
            // Following key-specific processing, add the keyvalue to the eclass
            EntityClassAttribute attribute("text", key, value, "");

            // Type is empty, attribute does not exist, add it.
            addAttribute(attribute);
        }
        else if (attribute.getValue().empty())
        {
            // Attribute type is set, but value is empty, set the value.
            attribute.setValue(value);
        }
        else
        {
            // Both type and value are not empty, emit a warning
            rWarning() << "[eclassmgr] attribute " << key
                << " already set on entityclass " << _name << std::endl;
        }
    } // while true

    // Notify the observers
    emitChangedSignal();
}

} // namespace eclass
