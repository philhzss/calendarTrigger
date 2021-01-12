#include "utils.h"
#include "settings.h"
#include <vector>
#include <nlohmann/json.hpp>

static Log lg("Settings", Log::LogLevel::Debug);



// Settings definitions
json settings::calendarSettings;
settings person1;
settings person2;

std::vector<settings*> settings::people;



void settings::readSettings(string silent)
{
	json settingsForm;
	try {
		std::ifstream stream("settings.json");
		stream >> settingsForm;

		// Initially do person1
		calendarSettings = settingsForm["Person1 Settings"];
		for (settings* person : people)
		{
			person->u_calendarURL = calendarSettings["calendarURL"];
			person->u_shiftStartBias = calendarSettings["shiftStartBias"];
			person->intshiftStartBias = std::stoi(person->u_shiftStartBias);
			person->u_shiftEndBias = calendarSettings["shiftEndBias"];
			person->intshiftEndBias = std::stoi(person->u_shiftEndBias);
			person->u_commuteTime = calendarSettings["commuteTime"];
			person->intcommuteTime = std::stoi(person->u_commuteTime);
			person->calendarSettings["wordsToIgnore"].get_to(person->u_wordsToIgnore);
			// Switch to person2
			calendarSettings = settingsForm["Person2 Settings"];
			// This is a horrible way to do it but it might work. With exactly 2 people
		}

		lg.b();
		lg.d("Settings file settings.json successfully read.");
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
		lg.b();
		for (settings* person : people)
		{
			lg.i("INFO on person::");
			if (person->ignoredWordsExist())
			{
				string ignoredString = person->ignoredWordsPrint();
				lg.b("Cal: Calendar URL: " + person->u_calendarURL +
					"\nCal: Word(s) to ignore events in calendar (", person->u_wordsToIgnore.size(), "): " + ignoredString);
			}
			else {
				lg.b("Cal: Calendar URL: " + person->u_calendarURL);
				lg.b("No ignored words were specified.");
			}
			lg.b("Cal: Commute time setting: " + person->u_commuteTime + " minutes.");
		}
	}
	return;
}

bool settings::ignoredWordsExist()
{
	return (u_wordsToIgnore.empty()) ? false : true;
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

