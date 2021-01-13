#pragma once
#include <vector>
#include <iostream>


using std::string;

//temp
string get_str_between_two_str(const string& s,
	const std::string& start_delim,
	const std::string& stop_delim);
//temp



void initiateCal();


//class calEvent
//{
//private:
//	string DTSTART, DTEND, DESCRIPTION;
//
//public:
//	// Function to convert string DTSTARTs into tm objects
//	void setEventParams(calEvent &event);
//	static void removePastEvents();
//	static void initEventTimers();
//	static void updateValidEventTimers();
//
//	// Whys isnt initiateCal part of calendar.h?
//
//
//
//	// public start and end times as timeobjects
//	tm start{0};
//	tm end{0};
//
//	// Difference (in minutes) between program runtime and event start/end time
//	int startTimer, endTimer;
//
//	// Keep track of if action has been done for start-end on a given shift
//	bool homeDone;
//	bool workDone;
//	void updateLastTriggeredEvent();
//
//	// Custom constructor
//	calEvent(string singleEvent_str);
//};
//
//class calEventGroup
//{
//public:
//	std::vector<calEvent> myCalEvents;
//	std::vector<calEvent> myValidEvents;
//	calEvent* lastTriggeredEvent;
//
//	// Static methods
//	string eventTimeCheck(int wakeTimer, int triggerTimer);
//
//	// Cleanup at end of program
//	static void cleanup();
//};