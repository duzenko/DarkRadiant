#include "icommandsystem.h"
#include "iscenegraph.h"

class MessageTcp;
class CameraObserver;

class GameConnection : public wxEvtHandler {
public:
	~GameConnection();

	//connect to TDM instance if not connected yet
	//return false if failed to connect
	bool Connect();

	//disconnect from TDM instance if connected
	//note: blocks until pending requests are processed
	void Disconnect();

	//send given request synchronously, i.e. wait until its completition
	//returns response content
	std::string Execute(const std::string &request);

	//flush all async commands (e.g. camera update) and wait until everything finishes
	void Finish();

	//called from camera modification callback: schedules async "setviewpos" action
	void UpdateCamera();

	//called from entity/scene observers: remember that entity with given name has been changed
	//  type == -1: entity has been removed
	//  type ==  0: entity has been modified
	//  type ==  1: entity has been added
	void EntityUpdated(const std::string &name, int type);

	//camera sync:
	static void CameraSyncEnable(const cmd::ArgumentList& args);
	static void CameraSyncDisable(const cmd::ArgumentList& args);
	static void CameraBackSync(const cmd::ArgumentList& args);
	//reload map from .map file:
	static void ReloadMapEnable(const cmd::ArgumentList& args);
	static void ReloadMapDisable(const cmd::ArgumentList& args);
	static void ReloadMap(const cmd::ArgumentList& args);
	//reload map fast without saving:
	static void UpdateMapOff(const cmd::ArgumentList& args);
	static void UpdateMapOn(const cmd::ArgumentList& args);
	static void UpdateMapAlways(const cmd::ArgumentList& args);
	static void UpdateMap(const cmd::ArgumentList& args);
	//game:
	static void PauseGame(const cmd::ArgumentList& args);

private:
	//connection to TDM game (i.e. the socket with custom message framing)
	std::unique_ptr<MessageTcp> _connection;
	//when connected, this timer calls Think periodically
	std::unique_ptr<wxTimer> _thinkTimer;
	void onTimerEvent(wxTimerEvent& ev) { Think(); }
	//sequence number of the last sent request (incremented sequentally)
	int _seqno = 0;

	//nonzero: request with this seqno sent to game, response not recieved yet
	int _seqnoInProgress = 0;
	//response from current in-progress request will be saved here
	std::vector<char> _response;

	//true <=> cameraOutData holds new camera position, which should be sent to TDM
	bool _cameraOutPending = false;
	//data for camera position (setviewpos format: X Y Z -pitch yaw roll)
	Vector3 _cameraOutData[2];
	//the observer put onto global camera when camera sync is enabled
	std::unique_ptr<CameraObserver> _cameraObserver;

	//the observer put onto global scene when "update map" is enabled
	std::unique_ptr<scene::Graph::Observer> _sceneObserver;
	//set to true when "update map" is set to "always"
	bool _updateMapAlways = false;
	//observers put on every entity on scene
	std::map<IEntityNode*, Entity::Observer*> _entityObservers;		//note: values owned
	//set of entities with changes since last update: -1 - deleted, 1 - added, 0 - modified
	std::map<std::string, int> _entityChangesPending;

	//every request should get unique seqno, otherwise we won't be able to distinguish their responses
	int NewSeqno() { return ++_seqno; }

	//given a command to be executed in game console (no EOLs), returns its full response text (except for seqno)
	static std::string ComposeConExecRequest(std::string consoleLine);

	//prepend seqno to specified request and send it to game
	void SendRequest(const std::string &request);

	//if there are any pending async commands (camera update), send one now
	//returns true iff anything was sent to game
	bool SendAnyAsync();

	//check how socket is doing, accept responses and send pending async requests 
	void Think();

	//wait until the currently executed request is finished
	void WaitAction();

	//set noclip/god/notarget to specific state (blocking)
	//toggleCommand is the command which toggles state
	//offKeyword is the part of phrase printed to game console when the state becomes disabled
	void ExecuteSetTogglableFlag(const std::string &toggleCommand, bool enable, const std::string &offKeyword);

	//learn state of the specified cvar (blocking)
	std::string ExecuteGetCvarValue(const std::string &cvarName, std::string *defaultValue = nullptr);

	//make sure camera observer is present iff enable == true, and attach/detach it to global camera
	void SetCameraObserver(bool enable);

	//make sure scene observer is present iff enable == true, and attach/detach it to global scene
	void SetSceneObserver(bool enable);
	//add/remove entity observers on the set of entity nodes
	void SetEntityObservers(const std::vector<IEntityNodePtr> &entityNodes, bool enable);
	//implementation of "update map" level toggling
	void SetUpdateMapLevel(bool on, bool always);
	//send map update to TDM right now
	void DoUpdateMap();

	//friend zone:
	friend class GameConnectionSceneObserver;
};
extern GameConnection g_gameConnection;
