#include "calendar.h"
#include "utils.h"
#include "settings.h"
#include <curl/curl.h>

using std::string;

static Log lg("Calendar", Log::LogLevel::Debug);
settings::calEvent* settings::calEventGroup::lastTriggeredEvent;


// Get long string of raw calendar data from URL
string GetCalRawData(settings* person) {

	try
	{
		string rawCalContent = curl_GET(person->u_calendarURL);
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
		lg.d("Triggered because the shift starts in (timer (", startTimer, ") - minsBefore (", minsTrigger, ")): " + std::to_string(startTimer - minsTrigger));

	}
	else if (action == "startOff")
	{
		lg.d("Triggered because the shift starts in (timer (", startTimer, ") + minsAfter (", minsTrigger, ")): " + std::to_string(startTimer + minsTrigger));

	}
	else if (action == "endOn")
	{
		lg.d("Triggered because the shift ends in (timer (", endTimer, ") - minsBefore (", minsTrigger, ")): " + std::to_string(endTimer - minsTrigger));
	}
	else if (action == "endOff")
	{
		lg.d("Triggered because the shift ends in (timer (", endTimer, ") + minsAfter (", minsTrigger, ")): " + std::to_string(endTimer + minsTrigger));
	}
	lg.d("All times in minutes");

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
	for (settings* person : settings::people)
	{
		string calRawData = GetCalRawData(person);

		// Cut out the "header" data from the raw cal string
		int	calHeadEndPos = calRawData.find("END:VTIMEZONE");
		calRawData = calRawData.substr(calHeadEndPos);

		// Create custom calEvents and stick them in a vector (code by Carl!!!!)
		std::vector<string> calEventsVector = calStringSep(calRawData);



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
		lg.d(person->u_name, "'s calendar ignored words filtered, now " + std::to_string(person->allEvents.myCalEvents.size()) + " events down from " + std::to_string(calEventsVector.size()) + ". (includes past events)");

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
string settings::calEventGroup::eventTimeCheck(int minsBefore, int minsAfter)
{
	for (settings* person : settings::people)
	{
		// Verify if any event timer is coming up soon (within the defined timer parameter) return action
		for (calEvent& event : person->allEvents.myValidEvents)
		{
			/* For the operations, the "on" ops will be before the start or end timer (the timer will be >0)
			For the "off" ops, since they occur after the event timer has past, we need to allow negative values
			We must stop searching for the trigger after a certain time otherwise we'll stay stuck in the loop
			with negative end/start timers forever. Using the minsAfter - a few extra mins should work */
			int operationShiftStart = event.startTimer - person->intcommuteTime + person->intshiftStartBias;
			int operationShiftEnd = event.endTimer + person->intcommuteTime + person->intshiftEndBias;
			bool operationStartOn = operationShiftStart <= minsBefore && operationShiftStart > 0;
			bool operationStartOff = operationShiftStart <= -minsAfter && operationShiftStart > -minsAfter - 4;
			bool operationEndOn = operationShiftEnd <= minsBefore && operationShiftEnd > 0;
			bool operationEndOff = operationShiftEnd <= -minsAfter && operationShiftEnd > -minsAfter - 4;

			if (operationStartOn)
			{
				if (!event.startOnDone) // make sure this event hasn't be triggered before
				{
					lg.p("Operation event.startTimer - person->intcommuteTime + person->intshiftStartBias = ",
						event.startTimer - person->intcommuteTime + person->intshiftStartBias);
					event.logDetail(minsBefore, "startOn");
					event.updateLastTriggeredEvent(person);
					lg.i("Event triggered for ", person->u_name);
					return "startOn";
				}
				else
				{
					lg.i("This event has already turned on the lights, ignoring.");
					return "duplicate";
				}
			}
			else if (operationStartOff)
			{
				if (!event.startOffDone) // make sure this event hasn't be triggered before
				{
					lg.p("Operation event.startTimer - person->intcommuteTime + person->intshiftStartBias = ",
						event.startTimer - person->intcommuteTime + person->intshiftStartBias);
					event.logDetail(minsAfter, "startOff");
					event.updateLastTriggeredEvent(person);
					lg.i("Event triggered for ", person->u_name);
					return "startOff";
				}
				else
				{
					lg.i("This event has already turned off the lights, ignoring.");
					return "duplicate";
				}
			}
			else if (operationEndOn)
			{
				if (!event.endOnDone) // make sure this event hasn't be triggered before
				{
					lg.p("Operation event.startTimer - person->intcommuteTime + person->intshiftStartBias = ",
						event.endTimer + person->intshiftEndBias);
					event.logDetail(minsBefore, "endOn");
					event.updateLastTriggeredEvent(person);
					lg.i("Event triggered for ", person->u_name);
					return "endOn";
				}
				else
				{
					lg.i("This event has already turned on the lights, ignoring.");
					return "duplicate";
				}
			}
			else if (operationEndOff)
			{
				if (!event.endOffDone) // make sure this event hasn't be triggered before
				{
					lg.p("Operation event.startTimer - person->intcommuteTime + person->intshiftStartBias = ",
						event.endTimer + person->intshiftEndBias);
					event.logDetail(minsAfter, "endOff");
					event.updateLastTriggeredEvent(person);
					lg.i("Event triggered for ", person->u_name);
					return "endOff";
				}
				else
				{
					lg.i("This event has already turned off the lights, ignoring.");
					return "duplicate";
				}
			}
			else
			{
				lg.p("No match for startOn: ", event.startTimer - minsBefore,
					"\nNo match for startOff: ", event.startTimer + minsAfter,
					"\nNo match for endOn: ", event.endTimer - minsBefore,
					"\nNo match for endOff: ", event.endTimer + minsAfter);
			}
		}
		lg.d(person->u_name, "'s schedule has been checked, nothing found");
	}
	// If we're here, no event matched any parameter
	lg.d("No events triggered for anyone with minsBefore:", minsBefore, " and minsAfter:", minsAfter, " at ", return_current_time_and_date());
	return "";
}

void settings::calEvent::updateLastTriggeredEvent(settings* person)
{
	lg.d("lastTriggeredEvent has been updated");
	settings::calEventGroup::lastTriggeredEvent = this;
}

void settings::calEventGroup::confirmDuplicateProtect(string type)
{
	if (type == "startOn")
	{
		settings::calEventGroup::lastTriggeredEvent->startOnDone = true;
	}
	if (type == "startOff")
	{
		settings::calEventGroup::lastTriggeredEvent->startOffDone = true;
	}
	if (type == "endOn")
	{
		settings::calEventGroup::lastTriggeredEvent->endOnDone = true;
	}
	if (type == "endOff")
	{
		settings::calEventGroup::lastTriggeredEvent->endOffDone = true;
	}
}

void settings::calEventGroup::cleanup()
{
	settings::people.clear();
	settings::peopleActualInstances.clear();
}