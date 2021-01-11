#include "utils.h"
#include "settings.h"
#include <vector>
#include <nlohmann/json.hpp>

static Log lg("Settings", Log::LogLevel::Debug);



// Settings definitions
json settings::calendarSettings;


// Cal
string settings::u_calendarURL;
string settings::u_shiftStartBias;
int settings::intshiftStartBias;
string settings::u_shiftEndBias;
int settings::intshiftEndBias;
std::vector<string> settings::u_wordsToIgnore;

string settings::u_commuteTime;
int settings::intcommuteTime;



void settings::readSettings(string silent)
{
	json settingsForm;
	try {
		std::ifstream stream("settings.json");
		stream >> settingsForm;

		// Get the settings cubcategories
		calendarSettings = settingsForm["Calendar Settings"];

		//Save the data from settings in the program variables
		{
			// CALENDAR SETTINGS
			u_calendarURL = calendarSettings["calendarURL"];
			u_shiftStartBias = calendarSettings["shiftStartBias"];
			intshiftStartBias = std::stoi(u_shiftStartBias);
			u_shiftEndBias = calendarSettings["shiftEndBias"];
			intshiftEndBias = std::stoi(u_shiftEndBias);
			u_commuteTime = calendarSettings["commuteTime"];
			intcommuteTime = std::stoi(u_commuteTime);
			calendarSettings["wordsToIgnore"].get_to(u_wordsToIgnore);


			lg.b();
			lg.d("Settings file settings.json successfully read.");
		}
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

		if (settings::ignoredWordsExist())
		{
			string ignoredString = settings::ignoredWordsPrint();
			lg.b("Cal: Calendar URL: " + u_calendarURL +
				"\nCal: Word(s) to ignore events in calendar (", u_wordsToIgnore.size(), "): " + ignoredString);
		}
		else {
			lg.b("Cal: Calendar URL: " + u_calendarURL);
			lg.b("No ignored words were specified.");
		}
		lg.b("Cal: Commute time setting: " + u_commuteTime + " minutes.");
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

