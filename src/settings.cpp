#include "utils.h"
#include "settings.h"
#include <vector>
#include <nlohmann/json.hpp>

static Log lg("Settings", Log::LogLevel::Debug);



json settings::peopleSettings;
std::vector<settings*> settings::people;
std::vector<settings> settings::peopleActualInstances;



void settings::readSettings(string silent)
{
	json settingsForm;
	try {
		std::ifstream stream("settings.json");
		stream >> settingsForm;
		peopleSettings = settingsForm["people"];

		// Figure out how many people we have
		int peopleFound = 0;
		for (json person : peopleSettings)
		{
			peopleFound++;
		}

		lg.i("There are ", peopleFound, " people to initialize from settings.json.");


		json activeJsonPerson;

		for (int personNumJson = 0; personNumJson < peopleFound; personNumJson++)
		{
			lg.d("Processing person #", personNumJson + 1, ".");
			settings activePerson;
			activeJsonPerson = peopleSettings["person" + std::to_string(personNumJson + 1)];
			activePerson.u_calendarURL = activeJsonPerson["calendarURL"];
			activePerson.u_shiftStartBias = activeJsonPerson["shiftStartBias"];
			activePerson.intshiftStartBias = std::stoi(activePerson.u_shiftStartBias);
			activePerson.u_shiftEndBias = activeJsonPerson["shiftEndBias"];
			activePerson.intshiftEndBias = std::stoi(activePerson.u_shiftEndBias);
			activePerson.u_commuteTime = activeJsonPerson["commuteTime"];
			activePerson.intcommuteTime = std::stoi(activePerson.u_commuteTime);
			activeJsonPerson["wordsToIgnore"].get_to(activePerson.u_wordsToIgnore);
			settings::peopleActualInstances.push_back(activePerson);

		}

		
		for (settings &peopleInstances : settings::peopleActualInstances)
		{
			settings::people.push_back(&peopleInstances);
		}


		lg.b();
		lg.d("Settings file settings.json successfully read.");
		lg.i(settings::people.size(), " people have been initialized.");
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
			lg.i("commute time: ", person->intcommuteTime);
			/*cout << person->u_wordsToIgnore[0] << endl;
			cout << person->u_wordsToIgnore[1] << endl;*/

			if (person->ignoredWordsExist(person))
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
