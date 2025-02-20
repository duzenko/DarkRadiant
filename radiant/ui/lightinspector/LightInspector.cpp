#include "LightInspector.h"

#include "i18n.h"
#include "ientity.h"
#include "ieclass.h"
#include "igame.h"
#include "ishaders.h"
#include "iradiant.h"
#include "imainframe.h"
#include "iselection.h"
#include "iundo.h"

#include <wx/tglbtn.h>
#include <wx/clrpicker.h>
#include <wx/checkbox.h>
#include <wx/artprov.h>
#include <wx/stattext.h>
#include <wx/radiobut.h>

#include "ui/common/ShaderChooser.h" // for static displayLightInfo() function
#include "util/ScopedBoolLock.h"
#include "gamelib.h"

namespace ui
{

/* CONSTANTS */

namespace
{
    const char* LIGHTINSPECTOR_TITLE = N_("Light properties");

    const std::string RKEY_WINDOW_STATE = "user/ui/lightInspector/window";

    const char* LIGHT_PREFIX_XPATH = "/light/texture//prefix";

    /** greebo: Loads the prefixes from the registry and creates a
     *          comma-separated list string
     */
    inline std::string getPrefixList() {
        std::string prefixes;

        // Get the list of light texture prefixes from the registry
        xml::NodeList prefList = GlobalGameManager().currentGame()->getLocalXPath(LIGHT_PREFIX_XPATH);

        // Copy the Node contents into the prefix vector
        for (xml::NodeList::iterator i = prefList.begin();
             i != prefList.end();
             ++i)
        {
            prefixes += (prefixes.empty()) ? "" : ",";
            prefixes += i->getContent();
        }

        return prefixes;
    }
}

// Private constructor sets up dialog
LightInspector::LightInspector() :
    wxutil::TransientWindow(_(LIGHTINSPECTOR_TITLE), GlobalMainFrame().getWxTopLevelWindow(), true),
    _isProjected(false),
    _texSelector(nullptr),
    _updateActive(false),
    _supportsAiSee(game::current::getValue<bool>("/light/supportsAiSeeSpawnarg", false))
{
    // Load XRC panel and access widgets
    wxPanel* contents = loadNamedPanel(this, "LightInspectorMainPanel");
    _brightnessSlider = findNamedObject<wxSlider>(this, "BrightnessSlider");

    setupLightShapeOptions();
    setupOptionsPanel();
    setupTextureWidgets();

    makeLabelBold(this, "LightInspectorVolumeLabel");
    makeLabelBold(this, "LightInspectorColourLabel");
    makeLabelBold(this, "LightInspectorOptionsLabel");

    SetMinSize(contents->GetEffectiveMinSize());
    InitialiseWindowPosition(600, 360, RKEY_WINDOW_STATE);
}

LightInspectorPtr& LightInspector::InstancePtr()
{
    static LightInspectorPtr _instancePtr;
    return _instancePtr;
}

void LightInspector::onMainFrameShuttingDown()
{
    if (IsShownOnScreen())
    {
        Hide();
    }

    // Destroy the window
    SendDestroyEvent();
    InstancePtr().reset();
}

void LightInspector::shaderSelectionChanged(
    const std::string& shader,
    wxutil::TreeModel& listStore)
{
    // Get the shader, and its image map if possible
    MaterialPtr ishader = _texSelector->getSelectedShader();
    // Pass the call to the static member of ShaderSelector
    ShaderSelector::displayLightShaderInfo(ishader, listStore);

    // greebo: Do not write to the entities if this call resulted from an update()
    if (_updateActive) return;

    std::string commandStr("setLightTexture: ");
    commandStr += _texSelector->getSelection();
    UndoableCommand command(commandStr);

    // Write the texture key
    setKeyValueAllLights("texture", _texSelector->getSelection());
}

// Set up the point/projected light options
void LightInspector::setupLightShapeOptions()
{
    // wxRadioButton does not emit a signal when it becomes UN-checked, so we
    // must connect to both buttons.
    wxRadioButton* omni = findNamedObject<wxRadioButton>(this, "omniRbtn");
    omni->SetValue(true);
    omni->Connect(wxEVT_RADIOBUTTON,
                  wxCommandEventHandler(LightInspector::_onProjToggle),
                  NULL, this);

    wxRadioButton* proj = findNamedObject<wxRadioButton>(this, "projectedRbtn");
    proj->Connect(wxEVT_RADIOBUTTON,
                  wxCommandEventHandler(LightInspector::_onProjToggle), NULL,
                  this);

    // Start/end checkbox
    wxControl* startEnd = findNamedObject<wxCheckBox>(this, "LightInspectorStartEnd");
    startEnd->Connect(
        wxEVT_CHECKBOX, wxCommandEventHandler(LightInspector::_onOptionsToggle),
        NULL, this
    );
    startEnd->Enable(false);
}

// Connect the options checkboxes
void LightInspector::setupOptionsPanel()
{
    // Colour and brightness
    findNamedObject<wxColourPickerCtrl>(this, "LightInspectorColour")->Bind(
        wxEVT_COLOURPICKER_CHANGED, &LightInspector::_onColourChange, this
    );
    _brightnessSlider->Bind( // drag in progress
        wxEVT_SCROLL_THUMBTRACK,
        [=](wxScrollEvent&) {
            if (!_adjustingBrightness && !GlobalUndoSystem().operationStarted())
            {
                GlobalUndoSystem().start();
                _adjustingBrightness = true;
            }
            adjustBrightness();
        }
    );
    _brightnessSlider->Bind( // drag finished
        wxEVT_SCROLL_CHANGED,
        [=](wxScrollEvent&) {
            if (_adjustingBrightness) {
                GlobalUndoSystem().finish("Adjust light brightness");
                _adjustingBrightness = false;
            }
            updateColourPicker();
        }
    );

    findNamedObject<wxCheckBox>(this, "LightInspectorParallel")->Bind(wxEVT_CHECKBOX, &LightInspector::_onOptionsToggle, this);
    findNamedObject<wxCheckBox>(this, "LightInspectorNoShadows")->Bind(wxEVT_CHECKBOX, &LightInspector::_onOptionsToggle, this);
    findNamedObject<wxCheckBox>(this, "LightInspectorSkipSpecular")->Bind(wxEVT_CHECKBOX, &LightInspector::_onOptionsToggle, this);
    findNamedObject<wxCheckBox>(this, "LightInspectorSkipDiffuse")->Bind(wxEVT_CHECKBOX, &LightInspector::_onOptionsToggle, this);

    if (_supportsAiSee)
    {
        findNamedObject<wxCheckBox>(this, "LightInspectorAiSee")->Show();
        findNamedObject<wxCheckBox>(this, "LightInspectorAiSee")->Bind(wxEVT_CHECKBOX, &LightInspector::_onOptionsToggle, this);
    }
    else
    {
        findNamedObject<wxCheckBox>(this, "LightInspectorAiSee")->Hide();
    }
}

// Create the texture widgets
void LightInspector::setupTextureWidgets()
{
    wxPanel* parent = findNamedObject<wxPanel>(this, "LightInspectorChooserPanel");

    _texSelector = new ShaderSelector(parent, this, getPrefixList(), true);
    parent->GetSizer()->Add(_texSelector, 1, wxEXPAND);
}

// Update dialog from map
void LightInspector::update()
{
    // Clear the list of light entities
    _lightEntities.clear();

    // Find all selected objects which are lights, and add them to our list of
    // light entities

    class LightEntityFinder
    : public SelectionSystem::Visitor
    {
        // List of light entities to add to
        EntityList& _entityList;

    public:

        // Constructor initialises entity list
        LightEntityFinder(EntityList& list)
        : _entityList(list)
        { }

        // Required visit method
        void visit(const scene::INodePtr& node) const
        {
            Entity* ent = Node_getEntity(node);
            if (ent && ent->getEntityClass()->isLight())
            {
                // Add light to list
                _entityList.push_back(ent);
            }
        }
    };
    LightEntityFinder lightFinder(_lightEntities);

    // Find the selected lights
    GlobalSelectionSystem().foreachSelected(lightFinder);

    wxPanel* panel = findNamedObject<wxPanel>(this, "LightInspectorMainPanel");

    if (!_lightEntities.empty())
    {
        // Update the dialog with the correct values from the first entity
        getValuesFromEntity();

        // Enable the dialog
        panel->Enable(true);
    }
    else
    {
        // Nothing found, disable the dialog
        panel->Enable(false);
    }
}

// Pre-hide callback
void LightInspector::_preHide()
{
    TransientWindow::_preHide();

    // Remove as observer, an invisible inspector doesn't need to receive events
    _selectionChanged.disconnect();

    _undoHandler.disconnect();
    _redoHandler.disconnect();
}

// Pre-show callback
void LightInspector::_preShow()
{
    TransientWindow::_preShow();

    _selectionChanged.disconnect();
    _undoHandler.disconnect();
    _redoHandler.disconnect();

    // Register self as observer to receive events
    _undoHandler = GlobalUndoSystem().signal_postUndo().connect(
        sigc::mem_fun(this, &LightInspector::update));
    _redoHandler = GlobalUndoSystem().signal_postRedo().connect(
        sigc::mem_fun(this, &LightInspector::update));

    // Register self to the SelSystem to get notified upon selection changes.
    _selectionChanged = GlobalSelectionSystem().signal_selectionChanged().connect(
        [this](const ISelectable&) { update(); });

    // Update the widgets before showing
    update();
}

// Static method to toggle the dialog
void LightInspector::toggleInspector(const cmd::ArgumentList& args)
{
    // Toggle the instance
    Instance().ToggleVisibility();
}

LightInspector& LightInspector::Instance()
{
    LightInspectorPtr& instancePtr = InstancePtr();

    if (instancePtr == NULL)
    {
        // Not yet instantiated, do it now
        instancePtr.reset(new LightInspector);

        // Pre-destruction cleanup
        GlobalMainFrame().signal_MainFrameShuttingDown().connect(
            sigc::mem_fun(*instancePtr, &LightInspector::onMainFrameShuttingDown)
        );
    }

    return *instancePtr;
}

// CALLBACKS

void LightInspector::updateLightShapeWidgets()
{
    // Set radio button values. wxWidgets only allows setting a radio button,
    // not clearing it.
    wxRadioButton* omni = findNamedObject<wxRadioButton>(this, "omniRbtn");
    wxRadioButton* proj = findNamedObject<wxRadioButton>(this, "projectedRbtn");
    if (_isProjected)
        proj->SetValue(true);
    else
        omni->SetValue(true);

    // Enable start/end checkbox if the light is projected
    findNamedObject<wxCheckBox>(this, "LightInspectorStartEnd")->Enable(_isProjected);
}

void LightInspector::_onProjToggle(wxCommandEvent& ev)
{
    if (_updateActive) return; // avoid callback loops

    // Set the projected flag
    _isProjected = findNamedObject<wxRadioButton>(this, "projectedRbtn")->GetValue();

    // Set button state based on the value of the flag
    findNamedObject<wxCheckBox>(this, "LightInspectorStartEnd")->Enable(_isProjected);

    writeToAllEntities();
}

namespace
{
    // Return highest RGB component from a given colour
    float highestComponent(const Vector3& colour)
    {
        float highest = colour.x();
        if (colour.y() > highest) highest = colour.y();
        if (colour.z() > highest) highest = colour.z();
        return highest;
    }

    // Get colour of entity as float vector [0.0 - 1.0]
    Vector3 entityColour(const Entity& entity)
    {
        // If the light has no colour key, use a default of white rather than
        // the Vector3 default of black (0, 0, 0).
        std::string colString = entity.getKeyValue("_color");
        if (colString.empty())
        {
            colString = "1.0 1.0 1.0";
        }
        return string::convert<Vector3>(colString);
    }

    inline void setEntityValueIfDifferent(Entity* entity,
                                          const std::string& key,
                                          const std::string& value)
    {
        // Only set the values if the entity carries different ones
        // to avoid triggering lots of undo system state savings
        if (entity->getKeyValue(key) != value)
        {
            entity->setKeyValue(key, value);
        }
    }

    // Set colour on entity
    void setEntityColour(Entity* entity, const Vector3& col)
    {
        setEntityValueIfDifferent(
            entity, "_color",
            fmt::format("{:.3f} {:.3f} {:.3f}", col.x(), col.y(), col.z())
        );
    }

    // Convert Vector3 colour to wxColour
    wxColour toWx(const Vector3& rgb)
    {
        Vector3 eightBit = rgb * 255;
        return wxColour(eightBit.x(), eightBit.y(), eightBit.z());
    }

    // Convert linear light value to/from displayed slider value
    const float SLIDER_POWER = 1.25;
    float fromSlider(float value)
    {
        return std::pow(value / 100.f, SLIDER_POWER);
    }
    float toSlider(float value)
    {
        return std::pow(value, 1.0/SLIDER_POWER) * 100.f;
    }
}

float LightInspector::highestComponentAllLights() const
{
    float highest = 0;
    for (const Entity* e: _lightEntities)
    {
        Vector3 col = entityColour(*e);
        float entityHighest = highestComponent(col);
        if (entityHighest > highest)
            highest = entityHighest;
    }
    return highest;
}

void LightInspector::updateColourPicker()
{
    // Examine colour of all entities. If they are the same, use this colour in
    // the picker, otherwise show an "inconsistent" placeholder value.
    auto col = wxNullColour;
    for (const Entity* e: _lightEntities)
    {
        wxColour entityCol = toWx(entityColour(*e));
        if (col == wxNullColour) {
            // Store first colour seen
            col = entityCol;
        }
        else if (col != entityCol) {
            // Inconsistent
            col = wxTransparentColour;
            break;
        }
    }

    // Set the picker to show the chosen colour
    auto picker = findNamedObject<wxColourPickerCtrl>(this, "LightInspectorColour");
    picker->SetColour(col);
}

void LightInspector::updateColourWidgets()
{
    // Set colour chooser button
    updateColourPicker();

    // Set brightness slider based on the brightest channel. This means that
    // 100% blue and 100% white will both show as maximum brightness, which
    // isn't correct in terms of optics, but prevents the slider from
    // overbrightening a colour and changing the hue.
    _brightnessSlider->SetValue(toSlider(highestComponentAllLights()));
}

// Get keyvals from entity and insert into text entries
void LightInspector::getValuesFromEntity()
{
    // Disable unwanted callbacks
    util::ScopedBoolLock updateLock(_updateActive);

    // Read values from the first entity in the list of lights (we have to pick
    // one, so might as well use the first).
    assert(!_lightEntities.empty());
    Entity* entity = _lightEntities[0];

    // Populate the value map with defaults
    _valueMap["light_radius"] = "320 320 320";
    _valueMap["light_center"] = "0 0 0";
    _valueMap["light_target"] = "0 0 -256";
    _valueMap["light_right"] = "128 0 0";
    _valueMap["light_up"] = "0 128 0";
    _valueMap["light_start"] = "0 0 -64";
    _valueMap["light_end"] = "0 0 -256";

	// Now load values from entity, overwriting the defaults if the value is
	// set
	for (auto& pair : _valueMap)
	{
		// Overwrite the map value if the key exists on the entity
		std::string val = entity->getKeyValue(pair.first);

		if (!val.empty())
		{
			pair.second = val;
		}
	}

    updateColourWidgets();

    // Set the texture selection from the "texture" key
    _texSelector->setSelection(entity->getKeyValue("texture"));

    // Determine whether this is a projected light, and set the toggles
    // appropriately
    _isProjected = (!entity->getKeyValue("light_target").empty() &&
                    !entity->getKeyValue("light_right").empty() &&
                    !entity->getKeyValue("light_up").empty());
    updateLightShapeWidgets();

    wxCheckBox* useStartEnd = findNamedObject<wxCheckBox>(this, "LightInspectorStartEnd");

    // If this entity has light_start and light_end keys, set the checkbox
    if (!entity->getKeyValue("light_start").empty()
        && !entity->getKeyValue("light_end").empty())
    {
        useStartEnd->SetValue(true);
    }
    else
    {
        useStartEnd->SetValue(false);
    }

    // Set the options checkboxes
    findNamedObject<wxCheckBox>(this, "LightInspectorParallel")->SetValue(entity->getKeyValue("parallel") == "1");
    findNamedObject<wxCheckBox>(this, "LightInspectorSkipSpecular")->SetValue(entity->getKeyValue("nospecular") == "1");
    findNamedObject<wxCheckBox>(this, "LightInspectorSkipDiffuse")->SetValue(entity->getKeyValue("nodiffuse") == "1");
    findNamedObject<wxCheckBox>(this, "LightInspectorNoShadows")->SetValue(entity->getKeyValue("noshadows") == "1");

    if (_supportsAiSee)
    {
        findNamedObject<wxCheckBox>(this, "LightInspectorAiSee")->SetValue(entity->getKeyValue("ai_see") == "1");
    }
}

void LightInspector::adjustBrightness() const
{
    // The slider represents the absolute brightness of the highest component
    // (which means that 100% sets that component to 1.0, and it is hopefully
    // not possible to overbrighten and lose colour information).
    float origHighest = highestComponentAllLights();

    // Adjust all lights in proportion to slider motion
    for (auto light: _lightEntities)
    {
        // Get existing colour for THIS light
        Vector3 colour = entityColour(*light);

        // Calculate the adjustment ratio to be applied to all lights
        float newHighest = std::max(
            fromSlider(_brightnessSlider->GetValue()), 0.01f
        );
        Vector3 newColour;
        if (origHighest > 0.0f)
        {
            float ratio = newHighest / origHighest;
            newColour = colour * ratio;
        }
        else
        {
            // No point in trying to brighten black, just set a grey value based
            // on the brightness value
            newColour = Vector3(newHighest, newHighest, newHighest);
        }

        setEntityColour(light, newColour);
    }

    // Update camera immediately to provide user feedback
    GlobalCameraManager().getActiveView().queueDraw();
}

// Write to all entities
void LightInspector::writeToAllEntities()
{
	UndoableCommand command("setLightProperties");

    for (auto entity : _lightEntities)
    {
        setValuesOnEntity(entity);
    }
}

// Set a given key value on all light entities
void LightInspector::setKeyValueAllLights(const std::string& key,
                                          const std::string& value)
{
	for (auto entity : _lightEntities)
    {
        entity->setKeyValue(key, value);
    }
}

// Set the keyvalues on the entity from the dialog widgets
void LightInspector::setValuesOnEntity(Entity* entity)
{
    // Set the "_color" keyvalue
    wxColour col = findNamedObject<wxColourPickerCtrl>(this, "LightInspectorColour")->GetColour();
    Vector3 colFloat(col.Red() / 255.0f, col.Green() / 255.0f,
                     col.Blue() / 255.0f);
    setEntityColour(entity, colFloat);

    // Write out all vectors to the entity
	for (const auto& pair : _valueMap)
	{
		// Only set the values if the entity carries different ones
		// to avoid triggering lots of undo system state savings
		setEntityValueIfDifferent(entity, pair.first, pair.second);
	}

	// Remove vector keys that should not exist, depending on the lightvolume
	// options
	if (_isProjected)
	{
		// Clear start/end vectors if checkbox is disabled
		if (!findNamedObject<wxCheckBox>(this, "LightInspectorStartEnd")->GetValue())
		{
			setEntityValueIfDifferent(entity, "light_start", "");
			setEntityValueIfDifferent(entity, "light_end", "");
		}

		// Blank out pointlight values
		setEntityValueIfDifferent(entity, "light_radius", "");
		setEntityValueIfDifferent(entity, "light_center", "");
	}
	else
	{
		// Blank out projected light values
		setEntityValueIfDifferent(entity, "light_target", "");
		setEntityValueIfDifferent(entity, "light_right", "");
		setEntityValueIfDifferent(entity, "light_up", "");
		setEntityValueIfDifferent(entity, "light_start", "");
		setEntityValueIfDifferent(entity, "light_end", "");
	}

	// Write the texture key
	setEntityValueIfDifferent(entity, "texture", _texSelector->getSelection());

	// Write the options
	setEntityValueIfDifferent(entity, "parallel", findNamedObject<wxCheckBox>(this, "LightInspectorParallel")->GetValue() ? "1" : "0");
	setEntityValueIfDifferent(entity, "nospecular", findNamedObject<wxCheckBox>(this, "LightInspectorSkipSpecular")->GetValue() ? "1" : "0");
	setEntityValueIfDifferent(entity, "nodiffuse", findNamedObject<wxCheckBox>(this, "LightInspectorSkipDiffuse")->GetValue() ? "1" : "0");
	setEntityValueIfDifferent(entity, "noshadows", findNamedObject<wxCheckBox>(this, "LightInspectorNoShadows")->GetValue() ? "1" : "0");

    if (_supportsAiSee)
    {
        setEntityValueIfDifferent(entity, "ai_see", findNamedObject<wxCheckBox>(this, "LightInspectorAiSee")->GetValue() ? "1" : "0");
    }
}

void LightInspector::_onOptionsToggle(wxCommandEvent& ev)
{
    if (_updateActive) return; // avoid callback loops

    writeToAllEntities();
}

void LightInspector::_onColourChange(wxColourPickerEvent& ev)
{
    if (_updateActive) return; // avoid callback loops

    writeToAllEntities();
}

} // namespace ui
