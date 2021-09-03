#pragma once
#include <nlohmann/json.hpp>
#include <fstream>

using std::string;
using json = nlohmann::json;

// Stores program settings, including but not limited to settings.json stuff
class settings
{
public:
	// Initial breakdown into separate json objects
	static json teslaSettings, calendarSettings, generalSettings, carSettings, peopleSettings;
	static std::vector<settings*> people;
	static std::vector<settings> peopleActualInstances;



	// Important stuff, Tesla official token, Tesla URL
	static string teslaURL;
	static string teslaVURL;
	static string teslaVID;
	static json authReqPackage;
	static string teslaAuthString;


	// Parse settings.json and save its contents in the program's memory
	static void readSettings(string silent = "");




	// u_ser defined settings (from settings.json):

	// General Setting
	// Slack Channel for Slack notifications
	static string u_slackChannel;
	static bool slackEnabled;
	// General Setting
	// (Bool) If true, logs all logger message to file
	static string u_logToFile;
	// General Setting
	// (Seconds) How long to wait before the program loops entirely
	static string u_repeatDelay;
	static int intrepeatDelay;
	// General Setting
	// [lat, long] Home coordinates
	static std::vector<string> u_homeCoords;
	// General Setting
	// [lat, long] Work coordinates
	static std::vector<string> u_workCoords;
	// General Setting
	// URL to light to ping on and off
	static string u_lightURL;
	// General Setting
	// How many mins before a triggertime (event start or end) to turn on device
	static string u_minsBefore;
	// General Setting
	// How many mins after a triggertime (event start or end) to turn off device
	static string u_minsAfter;
	// General Setting
	// If false, ignore shift endings when scanning for events
	static bool u_shiftEndingsTriggerLight;



	// Car Setting
	// Limiter is to stop climate if you called sick / if still at home X amount of time before shift in MINS
	static string u_shutoffTimer;
	static int intshutoffTimer;
	// Car Setting
	// (default=5) (Minutes) The higher this is, the more time your car's HVAC will run. More details in docs
	static string u_default20CMinTime;
	static int intdefault20CMinTime;
	// Car Setting
	// (commute + max HVAC preheat time + buffer) How long before shift start to wake car to check car temps
	static int intwakeTimer;
	// Car Setting
	// (commute + HVAC preheat time) Determined based on temperature, settings.
	static int inttriggerTimer;
	// Car Setting
	// Real amount of time BEFORE shift (event) you leave from home
	static int intshiftStartTimer;
	// Car Setting
	// Real amount of time AFTER shift (event) end you leave from work
	static int intshiftEndTimer;


	// Calendar Setting
	// URL to Calendar file for event triggers
	string u_calendarURL;
	// Calendar Setting
	// Calendar (person) name
	string u_name;
	// Calendar Setting
	// (Minutes) If you leave home to target arriving at work earlier than event start, enter a NEGATIVE number.
	// Ex: Want to arrive at work 15 mins before event start? Enter -15. EXCLUDES commute time. 
	// The car will be ready at (commuteTime - shiftStartBias) mins before event start.
	string u_shiftStartBias;
	int intshiftStartBias;
	// Calendar Setting
	// (Minutes), if you leave work when you calendar event ends, this should be 0.
	// If you leave work early, enter a negative number, if late, positive number.
	string u_shiftEndBias;
	int intshiftEndBias;
	// Calendar Setting
	// (Minutes) Drive time between home and work
	string u_commuteTime;
	int intcommuteTime;
	// Calendar Setting
	// Enter words that will cause the program to IGNORE events containing them
	std::vector<string> u_wordsToIgnore;
	bool ignoredWordsExist(settings* person);
	string ignoredWordsPrint();


	// Tesla Setting
	// Tesla official API email
	static string u_teslaEmail;
	// Tesla Setting
	// Tesla official API password
	static string u_teslaPassword;
	// Tesla Setting
	// CLIENT_ID
	static string u_teslaClientID;
	// Tesla Setting
	// CLIENT_SECRET
	static string u_teslaClientSecret;


	// Trigger storage
	// Should lights currently be on for this person
	bool lightShouldBeOn;



	class calEvent
	{
	private:
		string DTSTART, DTEND, DESCRIPTION;

	public:
		friend class settings;
		// Function to convert string DTSTARTs into tm objects
		void setEventParams(calEvent& event);
		void removePastEvents(settings* person);
		void initEventTimers(settings* person);
		// Must be run of every iteration of any "stuck loop" or the start/end timers wont be updated
		static void updateValidEventTimers();
		
		// Whys isnt initiateCal part of calendar.h?

		// Log all details for triggered event
		void logDetail(int minsTrigger, string action);

		// public start and end times as timeobjects
		tm start{ 0 };
		tm end{ 0 };

		// Difference (in minutes) between program runtime and event start/end time
		int startTimer, endTimer;

		// Keep track of if action has been done for start-end on a given shift
		bool startOnDone;
		bool startOffDone;
		bool endOnDone;
		bool endOffDone;


		// Takes pointer to whom to update event for
		void updateLastTriggeredEvent(settings* person);

		// *******************************
		void updateThisEventStat(settings::calEvent &event, settings* person);

		// Custom constructor
		calEvent(string singleEvent_str);

		// this line is probably bad:
		calEvent() = default;
	};

	class calEventGroup
	{
	public:
		std::vector<calEvent> myCalEvents;
		std::vector<calEvent> myValidEvents;
		static calEvent* lastTriggeredEvent;

		// Run in main, when you're sure you're done with this trigger and it worked
		static void confirmDuplicateProtect(string type);

		// Static methods
		static string eventTimeCheck(int minsBefore, int minsAfter);
		
		// Check if any person is preventing light shut down
		static string verifyCanLightTurnOff(string action);

		// Cleanup at end of program
		static void cleanup();
	};

	calEvent event;
	calEventGroup allEvents;
};
