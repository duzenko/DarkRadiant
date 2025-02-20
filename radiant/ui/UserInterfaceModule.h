#pragma once

#include <sigc++/connection.h>
#include <wx/event.h>

#include "iuserinterface.h"
#include "iorthocontextmenu.h"
#include "icommandsystem.h"

#include "LongRunningOperationHandler.h"
#include "FileSelectionRequestHandler.h"
#include "FileOverwriteConfirmationHandler.h"
#include "AutoSaveRequestHandler.h"
#include "MapFileProgressHandler.h"
#include "ManipulatorToggle.h"
#include "SelectionModeToggle.h"
#include "statusbar/ShaderClipboardStatus.h"
#include "statusbar/EditingStopwatchStatus.h"
#include "statusbar/MapStatistics.h"
#include "messages/CommandExecutionFailed.h"
#include "messages/TextureChanged.h"
#include "messages/NotificationMessage.h"
#include "ui/mru/MRUMenu.h"
#include "DispatchEvent.h"
#include "map/AutoSaveTimer.h"

namespace ui
{

/**
 * Module responsible of registering and intialising the various
 * UI classes in DarkRadiant, e.g. the LayerSystem.
 * 
 * Currently many UI classes are spread and initialised all across
 * the main binary, so there's still work left to do.
 */
class UserInterfaceModule :
	public wxEvtHandler,
	public IUserInterfaceModule
{
private:
	std::unique_ptr<LongRunningOperationHandler> _longOperationHandler;
	std::unique_ptr<MapFileProgressHandler> _mapFileProgressHandler;
	std::unique_ptr<AutoSaveRequestHandler> _autoSaveRequestHandler;
	std::unique_ptr<FileSelectionRequestHandler> _fileSelectionRequestHandler;
	std::unique_ptr<FileOverwriteConfirmationHandler> _fileOverwriteConfirmationHandler;
	std::unique_ptr<statusbar::ShaderClipboardStatus> _shaderClipboardStatus;
	std::unique_ptr<statusbar::EditingStopwatchStatus> _editStopwatchStatus;
	std::unique_ptr<statusbar::MapStatistics> _mapStatisticsStatus;
	std::unique_ptr<ManipulatorToggle> _manipulatorToggle;
	std::unique_ptr<SelectionModeToggle> _selectionModeToggle;

	sigc::connection _entitySettingsConn;
	sigc::connection _coloursUpdatedConn;
    sigc::connection _mapEditModeChangedConn;

	std::size_t _execFailedListener;
	std::size_t _textureChangedListener;
	std::size_t _notificationListener;

	std::unique_ptr<MRUMenu> _mruMenu;

	std::unique_ptr<map::AutoSaveTimer> _autosaveTimer;

public:
	// RegisterableModule
	const std::string & getName() const override;
	const StringSet & getDependencies() const override;
	void initialiseModule(const IApplicationContext& ctx) override;
	void shutdownModule() override;

	// Runs the specified action in the UI thread 
	// this happens when the application has a chance to, usually during event processing
	// This method is safe to be called from any thread.
	void dispatch(const std::function<void()>& action) override;

private:
	void registerUICommands();
	void initialiseEntitySettings();
	void applyEntityVertexColours();
	void applyBrushVertexColours();
	void applyPatchVertexColours();
	void refreshShadersCmd(const cmd::ArgumentList& args);

	void handleCommandExecutionFailure(radiant::CommandExecutionFailedMessage& msg);
	static void HandleTextureChanged(radiant::TextureChangedMessage& msg);
	static void HandleNotificationMessage(radiant::NotificationMessage& msg);

	void onDispatchEvent(DispatchEvent& evt);
};

// Binary-internal accessor to the UI module
UserInterfaceModule& GetUserInterfaceModule();

}
