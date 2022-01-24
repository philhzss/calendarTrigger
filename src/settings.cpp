#include "utils.h"
#include "settings.h"
#include <vector>
#include <nlohmann/json.hpp>
#include <thread>

static Log lg("Settings", Log::LogLevel::Debug);



json settings::peopleSettings;
json settings::generalSettings;
std::vector<settings*> settings::people;
std::vector<settings> settings::peopleActualInstances;
string settings::u_lightURL;
string settings::u_minsBefore;
string settings::u_minsAfter;
string settings::u_hoursFutureLookAhead;
int settings::u_apiPort;
bool settings::u_shiftEndingsTriggerLight;


void settings::readSettings(string silent)
{
	json settingsForm;
	try {
		std::ifstream stream("settings.json");
		stream >> settingsForm;
		peopleSettings = settingsForm["people"];
		generalSettings = settingsForm["General Settings"];

		u_lightURL = generalSettings["lightURL"];
		u_minsBefore = generalSettings["minsBeforeTrigger"];
		u_minsAfter = generalSettings["minsAfterTrigger"];
		u_hoursFutureLookAhead = generalSettings["hoursFutureLookAhead"];
		u_shiftEndingsTriggerLight = generalSettings["shiftEndingsTriggerLight"];
		string api = generalSettings["apiPort"];
		u_apiPort = std::stoi(api);

		// Figure out how many people we have
		int peopleFound = 0;
		for (json person : peopleSettings)
		{
			peopleFound++;
		}

		json activeJsonPerson;


		for (int personNumJson = 0; personNumJson < peopleFound; personNumJson++)
		{
			settings activePerson;
			activeJsonPerson = peopleSettings["person" + std::to_string(personNumJson + 1)];
			activePerson.u_calendarURL = activeJsonPerson["calendarURL"];
			activePerson.u_name = activeJsonPerson["name"];
			activePerson.u_shiftStartBias = activeJsonPerson["shiftStartBias"];
			activePerson.intshiftStartBias = std::stoi(activePerson.u_shiftStartBias);
			activePerson.u_shiftEndBias = activeJsonPerson["shiftEndBias"];
			activePerson.intshiftEndBias = std::stoi(activePerson.u_shiftEndBias);
			activePerson.u_commuteTime = activeJsonPerson["commuteTime"];
			activePerson.intcommuteTime = std::stoi(activePerson.u_commuteTime);
			activeJsonPerson["wordsToIgnore"].get_to(activePerson.u_wordsToIgnore);
			activePerson.lightShouldBeOn = false;
			lg.i("Processed ", activePerson.u_name, ", person #", personNumJson + 1, "/", peopleFound, ".");
			settings::peopleActualInstances.push_back(activePerson);
		}

		if (!u_shiftEndingsTriggerLight) {
			lg.i("NOTE: Shift endings will not trigger lights.");
		}


		for (settings& peopleInstances : settings::peopleActualInstances)
		{
			settings::people.push_back(&peopleInstances);
		}


		lg.d(settings::people.size(), " people have been initialized, settings file successfully read.");
	}
	catch (nlohmann::detail::parse_error)
	{
		lg.en("readSettings json parsing error, verify that settings.json exists in same directory as binary");
		throw string("settings.json parse_error");
	}
	/*catch (nlohmann::detail::type_error)
	{
		lg.en("readSettings json type error, settings.json most likely corrupt, verify example");
		throw string("settings.json type_error");
	}*/

	// Print this unless reading settings silently
	if (silent != "silent")
	{
		for (settings* person : people)
		{
			lg.b();
			lg.i(person->u_name);
			if (person->ignoredWordsExist(person))
			{
				string ignoredString = person->ignoredWordsPrint();
				lg.b("Cal: Word(s) to ignore events in calendar (", person->u_wordsToIgnore.size(), "): " + ignoredString);
			}
			else {
				lg.b("No ignored words were specified for this calendar.");
			}
			int eventStart = abs(-person->intcommuteTime + person->intshiftStartBias);
			int eventEnd = abs(person->intcommuteTime + person->intshiftEndBias);
			string wordStart = ((-person->intcommuteTime + person->intshiftStartBias) > 0) ? "after" : "before";
			string wordEnd = ((person->intcommuteTime + person->intshiftEndBias) > 0) ? "after" : "before";
			lg.b("Cal: Departure from home time: ", eventStart, " minutes ", wordStart, " event start time.");
			lg.b("Cal: Arrival at home time: ", eventEnd, " minutes ", wordEnd, " event end time.");
		}
	}
	return;
}

bool settings::ignoredWordsExist(settings* person)
{
	return (person->u_wordsToIgnore.empty()) ? false : true;
}

string settings::ignoredWordsPrint()
{
	auto* comma = ", ";
	auto* sep = "";
	std::ostringstream stream;
	for (string word : u_wordsToIgnore)
	{
		stream << sep << word;
		sep = comma;
	}
	return stream.str();
}

int settings::getOperationShiftStart(settings::calEvent eventToCalc) {
	int calculatedShiftStart = eventToCalc.startTimer - this->intcommuteTime + this->intshiftStartBias;
	return calculatedShiftStart;
}

int settings::getOperationShiftEnd(settings::calEvent eventToCalc) {
	int calculatedShiftEnd = eventToCalc.endTimer + this->intcommuteTime + this->intshiftEndBias;
	return calculatedShiftEnd;
}

bool settings::settingsMutexUnlockSuccess() {
	int counter = 0;
	while (!settings::settingsMutex.try_lock()) {
		lg.d("Mutex locked, WAITING FOR UNLOCK, have looped ", counter, " times.");
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		counter++;
		// Enter seconds*5
		if (counter > 5*5) {
			lg.e("Mutex timer overlimit, settingsMutexUnlockSuccess returning FALSE");
			return false;
		}
	}
	return true;
}
