#include "AFKDetection.h"
#include "../../Common/MyString.h"
#include "Server/Server.h"
#include "../Globals.h"

namespace halo
{
	DWORD CAFKDetection::max_duration = 0;
	bool CAFKDetection::bDisable = false;
	const DWORD CAFKDetection::kMoveCountThreshold = 100;

	// timer used by
	class CAFKDetectionEvent : public TimerEvent
	{
	private:
		DWORD dwDelay;
		CAFKDetection& parent;

	public:
		CAFKDetectionEvent(DWORD dwDelay, CAFKDetection& parent)
			: dwDelay(dwDelay), parent(parent), TimerEvent(dwDelay)
		{
		}

		virtual ~CAFKDetectionEvent() {}

		virtual bool OnExpiration(Timers& timers)
		{
			parent.CheckInactivity();
			return true;
		}
	};

	CAFKDetection::CAFKDetection(s_player& player, Timers& timers)
		: player(player), timers(timers), move_count(0), afk_duration(0)
	{
		timer_ptr timer(new CAFKDetectionEvent(60000,*this));
		timer_id = timers.AddTimer(std::move(timer));
	}

	CAFKDetection::~CAFKDetection()
	{
		timers.RemoveTimer(timer_id);
	}

	void CAFKDetection::CheckPlayerActivity()
	{
		// make sure the object is available
		objects::s_halo_object* object = player.get_object();
		if (object)	{
			vect3d new_camera = object->biped.cameraView;

			// check if the camera has moved
			if (new_camera == camera)
				move_count++;

			camera = new_camera;

			// Check if the player is shooting/throwing nades etc, if so they are not afk
			if (object->biped.actionFlags.melee || 
				object->biped.actionFlags.primaryWeaponFire ||
				object->biped.actionFlags.secondaryWeaponFire)
				MarkPlayerActive();
		}
	}

	void CAFKDetection::MarkPlayerActive()
	{
		afk_duration = 0;
		move_count = kMoveCountThreshold + 1;
	}

	void CAFKDetection::CheckInactivity()
	{
		if (max_duration == 0 || bDisable || player.IsAdmin()) return;
		if (move_count <= kMoveCountThreshold) {
			afk_duration++;
			if (afk_duration >= max_duration) {
				player.Kick();
				std::wstring msg = m_swprintf(L"%s has been kicked due to inactivity.",
					player.mem->playerName);
				server::MessageAllPlayers(L"%s", msg.c_str());
				g_PrintStream << msg << endl;
			} else {
				player.Message(L"You don't appear to be playing. You will be kicked in %i minute(s), if you remain inactive.", 
					max_duration - afk_duration);
			}
		}
	}

	void CAFKDetection::Disable() {	bDisable = true; }
	void CAFKDetection::Enable()  {	bDisable = false; }

}