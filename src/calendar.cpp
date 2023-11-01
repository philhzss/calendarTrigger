#include "utils.h"
#include "settings.h"
#include <curl/curl.h>

using std::string;

static Log lg("Calendar", Log::LogLevel::Debug);


// Get long string of raw calendar data from URL
string GetCalRawData(settings* person) {
	lg.p("GetCalRawData - start");
	try
	{
		string rawCalContent = curl_GET(person->u_calendarURL);
		lg.p("GetCalRawData - curl_GET success");
		if (rawCalContent.find("VCALENDAR") != std::string::npos)
		{
			lg.p("VCALENDAR found, ", person->u_name, "'s link is a valid calendar");
		}
		else {
			lg.d("VCALENDAR NOT found, ", person->u_name, "'s calendar invalid, throwing error");
			throw lg.prepareOnly(person->u_name, " CALENDAR INVALID");
		}
		return rawCalContent;
	}
	catch (string e)
	{
		throw "initiateCal/GetCalRawData error: " + e + "\nCheck calendar URL??? Internet connection?";
	};
	lg.p("GetCalRawData - end (should never be here)");
}

// Take the entire ICS file string and convert it into vector of strings with events
std::vector<string> calStringSep(string entire_ical_str)
{
	std::vector<string> vectorOfEvents;

	// We are stopping when we reach this string:
	string delimiter = "END:VEVENT";

	// Initial, start at position 0
	size_t last = 0;
	size_t next = 0;
	string token;

	while ((next = entire_ical_str.find(delimiter, last)) != string::npos)
	{
		string a = entire_ical_str.substr(last, next - last);
		/* cout << "Next event here" << endl;
		cout << a << endl; */
		vectorOfEvents.push_back(a);
		// + 11 chars to cutoff the END:VEVENT and make the strings start with BEGIN
		last = next + 11;
	}
	return vectorOfEvents;
}

// From StackOverflow website
// 1-String, 2-Start delim, 3-End delim
string get_str_between_two_str(const string& s,
	const std::string& start_delim,
	const std::string& stop_delim)
{
	unsigned first_delim_pos = s.find(start_delim);
	unsigned end_pos_of_first_delim = first_delim_pos + start_delim.length();
	// (Phil mod); make it start at end_post_of_first_delim instead of at pos 0, otherwise doesnt do shit right
	unsigned last_delim_pos = s.find(stop_delim, end_pos_of_first_delim);

	// Substr that start at the end of first delim, and ends at
	return s.substr(end_pos_of_first_delim,
		last_delim_pos - end_pos_of_first_delim);
}

// Constructor for calEvent object type
settings::calEvent::calEvent(string singleEvent_str)
{
	// Delimiters for finding values
	string startString_delim = "DTSTART:";
	string startString_delimEND = "Z";
	string endString_delim = "DTEND:";
	string endString_delimEND = "Z";
	string description_delim = "(Eastern Standard Time)";
	string description_delimEND = "refres";

	// Get and store the raw strings for these private vars
	DTEND = get_str_between_two_str(singleEvent_str, endString_delim, endString_delimEND);
	DTSTART = get_str_between_two_str(singleEvent_str, startString_delim, startString_delimEND);
	DESCRIPTION = get_str_between_two_str(singleEvent_str, description_delim, description_delimEND);

	// Debug stuff;
	/*cout << "A calEvent Object DTSTART:" << endl;
	cout << DTSTART << endl;
	cout << "A calEvent Object DTEND:" << endl;
	cout << DTEND << endl;
	cout << "A calEvent Object DESCRIPTION" << endl;
	cout << DESCRIPTION << endl;*/
}

void settings::calEvent::logDetail(int minsTrigger, string action)
{
	if (action == "startOn")
	{
		lg.d("Triggered because the shift starts in (timer (", startTimer, ") - minsBefore (", minsTrigger, ")): " + std::to_string(startTimer - minsTrigger) + " mins");

	}
	else if (action == "startOff")
	{
		lg.d("Triggered because the shift starts in (timer (", startTimer, ") + minsAfter (", minsTrigger, ")): " + std::to_string(startTimer + minsTrigger) + " mins");

	}
	else if (action == "endOn")
	{
		lg.d("Triggered because the shift ends in (timer (", endTimer, ") - minsBefore (", minsTrigger, ")): " + std::to_string(endTimer - minsTrigger) + " mins");
	}
	else if (action == "endOff")
	{
		lg.d("Triggered because the shift ends in (timer (", endTimer, ") + minsAfter (", minsTrigger, ")): " + std::to_string(endTimer + minsTrigger) + " mins");
	}

	lg.p
	(
		"::Trigger debug::"
		"\nYear=" + (std::to_string(end.tm_year)) +
		"\nMonth=" + (std::to_string(end.tm_mon)) +
		"\nDay=" + (std::to_string(end.tm_mday)) +
		"\nTime=" + (std::to_string(end.tm_hour)) + ":" + (std::to_string(end.tm_min)) +
		"\nStartTimer=" + (std::to_string(startTimer)) +
		"\nEndTimer=" + (std::to_string(endTimer)) + "\n"
	);
	lg.i("Shift starting at " + string_time_and_date(start) + " lights should be turned ", action);
	lg.i("Current time: " + return_current_time_and_date() + " LOCAL");
}

// Function to be called from main.cpp, creates myCalEvents vector containing all calEvent objects from scratch
void initiateCal()
{
	lg.p("initiateCal - start");
	for (settings* person : settings::people)
	{
		string calRawData = GetCalRawData(person);

		// Cut out the "header" data from the raw cal string
		int	calHeadEndPos;
		size_t endPos = calRawData.find("END:VTIMEZONE");
		calHeadEndPos;
		if (endPos != string::npos)
		{
			calHeadEndPos = endPos;
		}
		else {
			// If the calendar is in UTC there is no END:VTIMEZONE so we need this:
			calHeadEndPos = calRawData.find("X-WR-TIMEZONE:UTC");
		};

		calRawData = calRawData.substr(calHeadEndPos);

		// Create custom calEvents and stick them in a vector (code by Carl!!!!)
		std::vector<string> calEventsVector = calStringSep(calRawData);


		lg.p("initiateCal - start filtering words");
		for (string s : calEventsVector)
		{
			bool noIgnoredWordsInEvent = true;
			if (person->ignoredWordsExist(person)) {
				// Check for every word in the words to ignore vector, for every event.
				for (string ignoredWord : person->u_wordsToIgnore)
				{
					lg.p("Checking word ", ignoredWord);
					if (s.find(ignoredWord) != string::npos)
					{
						// Tell the user what is the date of the ignored event for debugging
						string ignoredWordDTSTART = "DTSTART";
						int datePos = s.find(ignoredWordDTSTART);
						// sFromDTSTART is DTSTART: the date and some extra (40 chars total)
						string sFromDTSTART = s.substr(datePos, 40);
						// Start at where the DTSTART starts, add its length, +1 for the : or ; which changes
						// -10 is because I must subtract the length of the word DTSTART + 1, AND the first char of LOC (L). I think
						string dateOfignoredWord = s.substr(datePos + ignoredWordDTSTART.length() + 1, sFromDTSTART.find("LOC") - 10);
						// Remove the first 11 chars (they are VALUE=DATE:) if its an all day event
						dateOfignoredWord = (dateOfignoredWord.find("VALUE=") != string::npos) ? dateOfignoredWord.erase(0, 11) : dateOfignoredWord;
						lg.p("Ignored word \"", ignoredWord, "\" found in event on ", dateOfignoredWord, " skipping.");
						noIgnoredWordsInEvent = false;
						break;
					}
					else {
						noIgnoredWordsInEvent = true;
					}
				}
			}
			else {
				noIgnoredWordsInEvent = true;
			}
			if (noIgnoredWordsInEvent)
			{
				person->allEvents.myCalEvents.push_back(settings::calEvent(s));
			}

		}




		// The custom myCalEvents vector is initialized
		//lg.d(person->u_name, "'s calendar ignored words filtered, now " + std::to_string(person->allEvents.myCalEvents.size()) + " events down from " + std::to_string(calEventsVector.size()) + ". (includes past events)");

		// Parse myCalEvents items to get their event start-end times set as datetime objects
		for (settings::calEvent& event : person->allEvents.myCalEvents)
		{
			event.setEventParams(event);
		}

		// Calculate start & end timers for each event and store them in the event instance
		person->event.initEventTimers(person);

		// Get rid of all events in the past
		person->event.removePastEvents(person);
	}
}

// Convert raw DTSTART/DTEND strings into datetime objects
void settings::calEvent::setEventParams(calEvent& event)
{
	// Convert str to tm struct taking (raw string & reference to timeStruct to store time in)
	auto convertToTm = [](string rawstr, tm& timeStruct)
	{
		int year, month, day, hour, min, sec;
		const char* rawCstr = rawstr.c_str();
		// Example string: DTSTART:20200827T103000Z
		sscanf(rawCstr, "%4d%2d%2d%*1c%2d%2d%2d", &year, &month, &day, &hour, &min, &sec);

		// Set struct values, (Year-1900 and month counts from 0)
		timeStruct.tm_year = year - 1900;
		timeStruct.tm_mon = month - 1;
		timeStruct.tm_mday = day;
		timeStruct.tm_hour = hour;
		timeStruct.tm_min = min;
		timeStruct.tm_sec = sec;

		/*cout << "Year is " << timeStruct.tm_year << endl;
		cout << "Month is " << timeStruct.tm_mon << endl;
		cout << "Day is " << timeStruct.tm_mday << endl;
		cout << "Hour is " << timeStruct.tm_hour << endl;
		cout << "Minute is " << timeStruct.tm_min << endl;*/
		return timeStruct;
	};

	// lg.p("Converting DTSTART, raw string is: " + DTSTART);
	event.start = convertToTm(event.DTSTART, event.start);
	event.end = convertToTm(event.DTEND, event.end);
	event.startOnDone = false;
	event.startOffDone = false;
	event.endOnDone = false;
	event.endOffDone = false;
}

void settings::calEvent::initEventTimers(settings* person)
{
	for (calEvent& event : person->allEvents.myCalEvents)
	{
		/*lg.p
		(
			"::BEFORE modifications by eventTimeCheck::"
			"\nYear=" + (std::to_string(event.end.tm_year)) +
			"\nMonth=" + (std::to_string(event.end.tm_mon)) +
			"\nDay=" + (std::to_string(event.end.tm_mday)) +
			"\nTime=" + (std::to_string(event.end.tm_hour)) + ":" + (std::to_string(event.end.tm_min)) + "\n"
		);*/
		// Make a temporary tm struct to not let the mktime function overwrite my event struct
		tm tempEventStart = event.start;
		tm tempEventEnd = event.end;
		// Using the temp tm structs, convert tm to time_t epoch seconds
		time_t startTime_secs = mktime(&tempEventStart) - timezone;
		time_t endTime_secs = mktime(&tempEventEnd) - timezone;
		lg.p("Now time in secs: " + std::to_string(nowTime_secs));
		lg.p("Start time in secs: " + std::to_string(startTime_secs));
		lg.p("End time in secs: " + std::to_string(endTime_secs));
		// Calculate diff between now and event start/stop times, store value in minutes in object timers
		event.startTimer = (difftime(startTime_secs, nowTime_secs)) / 60;
		event.endTimer = (difftime(endTime_secs, nowTime_secs)) / 60;
		lg.p
		(
			"::AFTER modifications by initEventTimers, based on shift END TIME::"
			"\nYear=" + (std::to_string(event.end.tm_year)) +
			"\nMonth=" + (std::to_string(event.end.tm_mon)) +
			"\nDay=" + (std::to_string(event.end.tm_mday)) +
			"\nTime=" + (std::to_string(event.end.tm_hour)) + ":" + (std::to_string(event.end.tm_min)) +
			"\nStartTimer=" + (std::to_string(event.startTimer)) +
			"\nEndTimer=" + (std::to_string(event.endTimer)) + "\n"
		);
	}
}

// Runs on every single event
void settings::calEvent::updateValidEventTimers()
{
	nowTime_secs;
	for (settings* person : settings::people)
	{
		for (calEvent& event : person->allEvents.myValidEvents) // applies to valid (non-past) events only
		{
			// Make a temporary tm struct to not let the mktime function overwrite my event struct
			tm tempEventStart = event.start;
			tm tempEventEnd = event.end;

			// Using the temp tm structs, convert tm to time_t epoch seconds
			time_t startTime_secs = mktime(&tempEventStart) - timezone;
			time_t endTime_secs = mktime(&tempEventEnd) - timezone;

			// Calculate diff between now and event start/stop times, store value in minutes in object timers
			event.startTimer = (difftime(startTime_secs, nowTime_secs)) / 60;
			event.endTimer = (difftime(endTime_secs, nowTime_secs)) / 60;

		}
	}
}

void settings::calEvent::removePastEvents(settings* person)
{
	int origSize = person->allEvents.myCalEvents.size();
	for (calEvent& event : person->allEvents.myCalEvents)
	{
		/* + 120 to not delete event as soon as it's passed, to be able to turn light off
		* This gives us 120 mins to drive back home, should be enough in 99% of cases */
		if ((event.startTimer + 120 > 0) || (event.endTimer + 120 > 0))
		{
			person->allEvents.myValidEvents.push_back(event);
		}

	}
	int newSize = person->allEvents.myValidEvents.size();
	lg.i(person->u_name, " has " + std::to_string(newSize) + " upcoming events, filtered from " + std::to_string(origSize) + " events.");

	// Only print this if level is higher than programming as it's a lot of lines
	if (lg.ReadLevel() >= Log::Programming) {
		lg.d("All events INCLUDING past:");
		for (calEvent event : person->allEvents.myCalEvents)
		{
			lg.d("Start");
			lg.d(event.DTSTART);
			lg.d("End");
			lg.d(event.DTEND);
			lg.b();
		}
		lg.b("\n");
		lg.d("All events EXCLUDINTG past (valid only):");
		for (calEvent event : person->allEvents.myValidEvents)
		{
			lg.d("Start");
			lg.d(event.DTSTART);
			lg.d("End");
			lg.d(event.DTEND);
			lg.b();
		}
	}

}

// Check if any start-end event is valid, and return appropriate string based on timers
string settings::calEventGroup::eventTimeCheck(int minsBefore, int minsAfter, int hoursFuture, bool scanFuturesOnly)
{
	int futureMins = 60 * hoursFuture;
	string result;
	std::vector<string> results;
	for (settings* person : settings::people)
	{
		person->allEvents.myFutureEvents.clear();
		// Verify if any event timer is coming up soon (within the defined timer parameter) return action
		for (calEvent& event : person->allEvents.myValidEvents)
		{
			/* For the operations, the "on" ops will be before the start or end timer (the timer will be >0)
			For the "off" ops, since they occur after the event timer has past, we need to allow negative values
			We must stop searching for the trigger after a certain time otherwise we'll stay stuck in the loop
			with negative end/start timers forever. Using the minsAfter - a few extra mins should work*/
			int operationLengthBuffer = 1.5 * (stoi(settings::u_minsAfter) + stoi(settings::u_minsBefore));
			int operationShiftStart = person->getOperationShiftStart(event);
			int operationShiftEnd = person->getOperationShiftEnd(event);
			bool operationStartOn = operationShiftStart <= minsBefore && operationShiftStart > 0;
			bool operationStartHold = operationShiftStart <= minsBefore && operationShiftStart > -minsAfter && operationShiftStart > -operationLengthBuffer;
			bool operationStartOff = operationShiftStart <= -minsAfter && operationShiftStart > -minsAfter - 4;
			bool operationEndOn = operationShiftEnd <= minsBefore && operationShiftEnd > 0;
			bool operationEndHold = operationShiftEnd <= minsBefore && operationShiftEnd > -minsAfter && operationShiftEnd > -operationLengthBuffer;
			bool operationEndOff = operationShiftEnd <= -minsAfter && operationShiftEnd > -minsAfter - 4;

			// Everything doesnt work because operation Shift Start ends and then kicks us out of the main loop :(
			// ???

			// Test for events within futureMins to update FutureEvents vector
			bool futureEventStart = operationShiftStart <= futureMins && operationShiftStart > 0;
			bool futureEventEnd = operationShiftEnd <= futureMins && operationShiftEnd > 0;

			// For a given event, start is always before end (duh) so we can test for start ELSE end. To avoid double events
			if (futureEventStart)
			{
				person->allEvents.myFutureEvents.push_back(event);
			}
			else if (futureEventEnd) {
				person->allEvents.myFutureEvents.push_back(event);
			}

			if (!scanFuturesOnly) {
				result = "fresh"; // Must be reset each loop or stays the same for all following events until overwritten again!!
				if (operationStartOn)
				{
					if (!event.startOnDone) // make sure this event hasn't be triggered before
					{
						lg.p("Operation event.startTimer - person->intcommuteTime + person->intshiftStartBias = ",
							event.startTimer - person->intcommuteTime + person->intshiftStartBias);
						event.logDetail(minsBefore, "startOn");
						lg.i("Event triggered for ", person->u_name);
						result = "startOn";
						event.startOnDone = true;
					}
					else
					{
						lg.p("This event has already turned on the lights, ignoring.");
						result = "duplicate";
					}
				}
				else if (operationStartOff)
				{
					if (!event.startOffDone) // make sure this event hasn't be triggered before
					{
						lg.p("Operation event.startTimer - person->intcommuteTime + person->intshiftStartBias = ",
							event.startTimer - person->intcommuteTime + person->intshiftStartBias);
						event.logDetail(minsAfter, "startOff");
						lg.i("Event triggered for ", person->u_name);
						result = "startOff";
						event.startOffDone = true;
					}
					else
					{
						lg.p("This event has already turned off the lights, ignoring.");
						result = "duplicate";
					}
				}
				else if (operationEndOn)
				{
					if (!event.endOnDone) // make sure this event hasn't be triggered before
					{
						lg.p("Operation event.startTimer - person->intcommuteTime + person->intshiftStartBias = ",
							event.endTimer + person->intshiftEndBias);
						event.logDetail(minsBefore, "endOn");
						lg.i("Event triggered for ", person->u_name);
						if (settings::u_shiftEndingsTriggerLight) {
							result = "endOn";
							event.endOnDone = true;
						}
						else {
							lg.i("Event end would have triggered endOff, skipping due to settings");
						}
					}
					else
					{
						lg.p("This event has already turned on the lights, ignoring.");
						result = "duplicate";
					}
				}
				else if (operationEndOff)
				{
					if (!event.endOffDone) // make sure this event hasn't be triggered before
					{
						lg.p("Operation event.startTimer - person->intcommuteTime + person->intshiftStartBias = ",
							event.endTimer + person->intshiftEndBias);
						event.logDetail(minsAfter, "endOff");
						lg.i("Event triggered for ", person->u_name);
						if (settings::u_shiftEndingsTriggerLight) {
							result = "endOff";
							event.endOffDone = true;
						}
						else {
							lg.i("Event end would have triggered endOff, skipping due to settings");
						}
					}
					else
					{
						lg.p("This event has already turned off the lights, ignoring.");
						result = "duplicate";
					}
				}
				else if (operationStartHold || operationEndHold)
				{
					lg.d("Waiting between On & Off timers, holding (operationHold // duplicate)");
					result = "duplicate";
				}
				else
				{
					lg.p("No match for startOn: ", event.startTimer - minsBefore,
						"\nNo match for startOff: ", event.startTimer + minsAfter,
						"\nNo match for endOn: ", event.endTimer - minsBefore,
						"\nNo match for endOff: ", event.endTimer + minsAfter);
				}
				if (result != "" && result != "fresh") // If result is blank (it will be for all non triggered events), we don't want it
				{
					lg.i(person->u_name, "'s schedule has a match, calculated result: ", result);
					results.push_back(result); // Add all results into vector
					event.updateThisEventStat(event, person);
				}
			} // If scan futures only
		} // For every event
		person->allEvents.updateNextFutureEvent(hoursFuture, person);
		if (!scanFuturesOnly) {
			lg.i("No further events triggered for calendar: ", person->u_name);
			lg.b();
		}
	}
	if (scanFuturesOnly) {
		// We're done for future events
		lg.d("scanFuturesOnly eventTimeCheck done, returning");
		return "";
	}
	// Parse all the results to choose what to do
	if (std::any_of(results.cbegin(), results.cend(), [](string anyResult) { return anyResult.find("startOn") != std::string::npos; }))
	{
		lg.d("At least one event is calling for startOn");
		return "startOn";
	}
	else if (std::any_of(results.cbegin(), results.cend(), [](string anyResult) { return anyResult.find("endOn") != std::string::npos; }))
	{
		lg.d("At least one event is calling for endOn");
		return "endOn";
	}
	else if (std::any_of(results.cbegin(), results.cend(), [](string anyResult) { return anyResult.find("startOff") != std::string::npos; }))
	{
		lg.d("At least one event is calling for startOff, verifying");
		return verifyCanLightTurnOffAction("startOff");
	}
	else if (std::any_of(results.cbegin(), results.cend(), [](string anyResult) { return anyResult.find("endOff") != std::string::npos; }))
	{
		lg.d("At least one event is calling for endOff, verifying");
		return verifyCanLightTurnOffAction("endOff");
	}
	// At this point, the vector is either empty or contains only duplicates
	else if (std::any_of(results.cbegin(), results.cend(), [](string anyResult) { return anyResult.find("duplicate") != std::string::npos; }))
	{
		lg.d("All events calling for duplicate, sending action back to main");
		return "duplicate";
	}

	// If we're here, no event matched any parameter
	lg.d("\nNo events triggered for anyone with minsBefore:", minsBefore, " and minsAfter:", minsAfter, " at ", return_current_time_and_date());
	return "";
}


void settings::calEvent::updateThisEventStat(settings::calEvent& event, settings* person) {
	// Update if lights should be on for multi-calendar support
	if (event.startOnDone && !event.startOffDone) {
		person->lightShouldBeOn = true;
	}
	else if (event.endOnDone && !event.endOffDone) {
		person->lightShouldBeOn = true;
	}
	else {
		person->lightShouldBeOn = false;
	}
	// Runs on every single event (in the future), careful with logging this line:
	lg.d(person->u_name, "'s lightShouldBeOn set to [", person->lightShouldBeOn, "] by startOnDone[", event.startOnDone, "], startOffDone[", event.startOffDone, "], endOnDone[", event.endOnDone, "], endOffDone[", event.endOffDone, "], ");
}


void settings::calEventGroup::cleanup()
{
	settings::people.clear();
	settings::peopleActualInstances.clear();
}

void settings::calEventGroup::updateNextFutureEvent(int hoursFuture, settings* person) {
	int earliestEventStart = hoursFuture * 60; // init value
	int earliestEventEnd = earliestEventStart;
	tm determinedNextEvent; // the start or the end, depennding on what triggers
	bool eventIsAnEnd = false;
	lg.d("[Futures] We have ", this->myFutureEvents.size(), " events for this person within the next ", u_hoursFutureLookAhead, " hours.");

	if (this->myFutureEvents.size() == 0) {
		// No future events within range
		this->nextFutureEvent = "None within range";
		this->nextFutureTriggerON = "None";
		this->nextFutureTriggerOFF = "None";
		this->nextFutureEventType = "None";
		return;
	}

	for (calEvent& event : this->myFutureEvents) {
		if ((event.startTimer <= 0) && (event.endTimer > 0)) {
			lg.d("[Futures] !!!Negative startTimer and positive endTimer, we are during an event.");
			determinedNextEvent = event.end; // The next is therefor the end
			int minsTilTrigger = person->getOperationShiftEnd(event) - event.endTimer;
			//lg.d("minsTilTrigger is ", minsTilTrigger);
			tm nextEventTriggerTm = determinedNextEvent;
			AddTime(minsTilTrigger, &nextEventTriggerTm);
			tm nextEventTriggerTmON = nextEventTriggerTm;
			tm nextEventTriggerTmOFF = nextEventTriggerTm;
			AddTime(-std::stoi(settings::u_minsBefore), &nextEventTriggerTmON);
			this->nextFutureTriggerON = string_time_and_date(nextEventTriggerTmON, false);
			AddTime(std::stoi(settings::u_minsAfter), &nextEventTriggerTmOFF);
			this->nextFutureTriggerOFF = string_time_and_date(nextEventTriggerTmOFF, false);
			eventIsAnEnd = true;
			this->nextFutureEventType = "ShiftEnd";
		}
	}

	if (eventIsAnEnd) {
		// We are done, go to end
	}
	else {
		// If here, next event should be a startTimer event
		for (calEvent& event : this->myFutureEvents) {
			if ((event.startTimer < earliestEventStart) && (event.startTimer > 0)) {
				earliestEventStart = event.startTimer;
				determinedNextEvent = event.start;
				int minsTilTrigger = person->getOperationShiftStart(event) - event.startTimer;
				tm nextEventTriggerTm = determinedNextEvent;
				AddTime(minsTilTrigger, &nextEventTriggerTm);
				tm nextEventTriggerTmON = nextEventTriggerTm;
				tm nextEventTriggerTmOFF = nextEventTriggerTm;
				AddTime(-std::stoi(settings::u_minsBefore), &nextEventTriggerTmON);
				this->nextFutureTriggerON = string_time_and_date(nextEventTriggerTmON, false);
				AddTime(std::stoi(settings::u_minsAfter), &nextEventTriggerTmOFF);
				this->nextFutureTriggerOFF = string_time_and_date(nextEventTriggerTmOFF, false);
				this->nextFutureEventType = "ShiftStart";
			}
		}
	}
	lg.i("[Futures] Next event (future) determined to be next: ", string_time_and_date(determinedNextEvent));
	this->nextFutureEvent = string_time_and_date(determinedNextEvent, false);
}

bool settings::calEventGroup::updateCanLightTurnOffBool() {
	bool canLightTurnOff = true;
	for (settings* person : settings::people) {
		if (person->lightShouldBeOn) {
			canLightTurnOff = false;
			break;
		}
	}
	return canLightTurnOff;
}

string settings::calEventGroup::verifyCanLightTurnOffAction(string action) {
	bool blockLightFromTurningOff = !updateCanLightTurnOffBool();
	if (blockLightFromTurningOff) {
		lg.d("Verification result: Poweroff BLOCKED");
		return "blocked";
	}
	else {
		lg.d("Verification result: Poweroff allowed");
		return action;
	}
}


