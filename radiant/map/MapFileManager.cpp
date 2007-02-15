#include "MapFileManager.h"

#include "qerplugin.h"
#include "mainframe.h"
#include "gtkutil/filechooser.h"

namespace map
{

// Private constructor
MapFileManager::MapFileManager()
: _lastDir(GlobalRadiant().getMapsPath())
{ }

// Instance owner method
MapFileManager& MapFileManager::getInstance() {
	static MapFileManager _instance;
	return _instance;	
}

// Utility method to select a map file
std::string MapFileManager::selectFile(bool open, const std::string& title) {

	// Display a file chooser dialog to get a new path
	std::string filePath = file_dialog(GTK_WIDGET(MainFrame_getWindow()), 
									   open, 
									   title, 
									   _lastDir, 
									   "map");

	// If a filename was chosen, update the last path
	if (!filePath.empty())
		_lastDir = filePath.substr(0, filePath.rfind("/"));
		
	// Return the chosen file
	return filePath;
}

/* PUBLIC INTERFACE METHODS */

// Static method to get a load filename
std::string MapFileManager::getMapFilename(bool open, 
										   const std::string& title) 
{
	return getInstance().selectFile(open, title);
}

} // namespace map
