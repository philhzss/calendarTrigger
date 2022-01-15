#include "calendar.h"
#include "utils.h"
#include "settings.h"
#include <curl/curl.h>
#include <unistd.h>
#include <atomic>

#define CROW_MAIN
#include <crow.h>

using std::string;

static Log lg("Main", Log::LogLevel::Debug);

time_t nowTime_secs = time(&nowTime_secs);

// Make sure internet connection works
bool InternetConnected() {
	bool internet_res;
	CURL* curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "http://www.google.com/");
		/* Disable the console output of the HTTP request data as we don't care: */
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		int curlRes = curl_easy_perform(curl);
		if (curlRes == 0) {
			internet_res = true;
		}
		else {
			internet_res = false;
			lg.e("***** INTERNET DOWN *****");
			lg.e("***** INTERNET DOWN *****");
			lg.e("***** INTERNET DOWN *****");
			long response_code;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
			lg.e("CURL Error Code: ", curlRes);
			lg.e("HTTP Error Code (will be 0 if absolutely can't connect): ", response_code);
			lg.e("See https://curl.haxx.se/libcurl/c/libcurl-errors.html for details.");
		}
		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();

	return internet_res;
}

// Will return current time if no parameter, or parameter tstruct time
const string return_current_time_and_date(time_t& time_to_return)
{
	struct tm tstruct;
	char buf[80];
	tstruct = *localtime(&time_to_return);
	// VERIFY LOGGER HERE
	if (lg.ReadLevel() == Log::Programming)
	{
		cout << "TEST NOW TIME STRUCT" << endl;
		cout << "nowTime_secs before conversions, should be the same after:" << endl;
		cout << time_to_return << endl;
		lg.d
		(
			"::TEST NOW TIME STRUCT::"
			"\nYear=" + (std::to_string(tstruct.tm_year)) +
			"\nMonth=" + (std::to_string(tstruct.tm_mon)) +
			"\nDay=" + (std::to_string(tstruct.tm_mday)) +
			"\nTime=" + (std::to_string(tstruct.tm_hour)) + ":" + (std::to_string(tstruct.tm_min)) + "\n"
		);
		time_t time_since_epoch = mktime(&tstruct);
		cout << "nowTime in seconds:" << endl;
		cout << time_since_epoch << endl << endl;
	}
	strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
	return buf;
}
const string string_time_and_date(tm tstruct)
{
	time_t tempSeconds = mktime(&tstruct) - timezone;
	tm localStruct = *localtime(&tempSeconds);
	char buf[80];
	strftime(buf, sizeof(buf), "%Y-%m-%d %R Local", &localStruct);
	return buf;
}

void initAll()
{
	try {
		InternetConnected();
		settings::readSettings();
		nowTime_secs = time(&nowTime_secs); // update to current time
		lg.b();
		initiateCal();
	}
	catch (string e) {
		lg.e("Error: ", e);
		throw e;
	}
	return;
}

string garageLightCommand(string command)
{
	string response;
	bool response_code_ok;
	do
	{
		string fullUrl = settings::u_lightURL; // dev API for now
		const char* const url_to_use = fullUrl.c_str();
		// lg.d("teslaPOSTing to this URL: " + fullUrl); // disabled for clutter

		CURL* curl;
		CURLcode res;
		// Buffer to store result temporarily:
		string readBuffer;
		long response_code;

		/* In windows, this will init the winsock stuff */
		curl_global_init(CURL_GLOBAL_ALL);

		/* get a curl handle */
		curl = curl_easy_init();

		if (curl) {
			/* First set the URL that is about to receive our POST. This URL can
			   just as well be a https:// URL if that is what should receive the
			   data. */
			curl_easy_setopt(curl, CURLOPT_URL, url_to_use);
			/* Now specify the POST data */
			struct curl_slist* headers = nullptr;
			headers = curl_slist_append(headers, "User-Agent: calendarTrigger");




			// Serialize the package json to string
			string data = "command=" + command;
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length());
			curl_easy_setopt(curl, CURLOPT_POST, 1);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

			/* Perform the request, res will get the return code */
			res = curl_easy_perform(curl);
			/* Check for errors */
			if (res != CURLE_OK)
				fprintf(stderr, "curl_easy_perform() failed: %s\n",
					curl_easy_strerror(res));
			if (res == CURLE_OK) {
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				lg.i(response_code, "=response code for ", fullUrl);
				lg.p("readBuffer (before jsonify): " + readBuffer);

				if (response_code != 200)
				{
					lg.e("Abnormal server response (", response_code, ") for ", fullUrl);
					lg.d("readBuffer for incorrect: " + readBuffer);
					response_code_ok = false;
					lg.i("Waiting 30 secs and retrying");
					sleep(30); // wait a little before redoing the curl request
					continue;
				}
				else {
					response_code_ok = true;
				}
			}

			/* always cleanup */
			curl_easy_cleanup(curl);
		}
		curl_global_cleanup();
		response = readBuffer;
	} while (!response_code_ok);
	return response;
}

void DoCrowAPI(std::vector<settings*>* people, string *minsBeforeTriggerOn, 
	string *minsAfterTriggerOff, bool *doShiftEndings) {
	crow::SimpleApp app; //define your crow application

	//define your endpoint at the root directory
	CROW_ROUTE(app, "/api")([&people, &minsBeforeTriggerOn, &minsAfterTriggerOff, &doShiftEndings]() {

		crow::json::wvalue json;
		lg.d("Crow HTTP request; Crow thread has ", people->size(), " people.");
		json["app"]["minsBeforeTriggerOn"] = lg.prepareOnly(*minsBeforeTriggerOn);
		json["app"]["minsAfterTriggerOff"] = lg.prepareOnly(*minsAfterTriggerOff);
		json["app"]["shiftEndingsTriggerLight"] = lg.prepareOnly(*doShiftEndings);
		for (settings* person : *people) {
			json[lg.prepareOnly(person->u_name)]
				["lightShouldBeOn"] = lg.prepareOnly(person->lightShouldBeOn);
			json[lg.prepareOnly(person->u_name)]
				["shiftStartBias"] = lg.prepareOnly(person->u_shiftStartBias);
			json[lg.prepareOnly(person->u_name)]
				["shiftEndBias"] = lg.prepareOnly(person->u_shiftEndBias);
			json[lg.prepareOnly(person->u_name)]
				["commuteTime"] = lg.prepareOnly(person->u_commuteTime);
			json[lg.prepareOnly(person->u_name)]
				["wordsToIgnore"] = person->ignoredWordsPrint();
			
		}

		return json;

		});

	//set the port, set the app to run on multiple threads, and run the app
	app.port(3112).multithreaded().run();
	// 3112 main port, 4112 test port to not interfere with running version
}

int main()
{
	int mainLoopCounter = 1;
	time_t launchTime = time(&nowTime_secs);
	std::atomic <bool> canLightBeTurnedOff(true);
	std::thread worker(DoCrowAPI, 
		&settings::people, 
		&settings::u_minsBefore,
		&settings::u_minsAfter,
		&settings::u_shiftEndingsTriggerLight);
	while (true)
	{
		lg.b("\n\n>>>>>>>------------------------------PROGRAM STARTS HERE (loop #", mainLoopCounter, ")----------------------------<<<<<<<");
		nowTime_secs = time(&nowTime_secs); // update to current time
		lg.i("Runtime date-time (this loop): " + return_current_time_and_date() + " LOCAL");
		lg.i("Launch date-time (first loop): " + return_current_time_and_date(launchTime) + " LOCAL\n");
		string actionToDo;
		int count = 0;
		int maxTries = 10;
		if (InternetConnected())
		{
			while (true) // max tries loop
			{
				try {
					settings::calEventGroup::cleanup(); // always cleanup
					initAll();
					lg.b();
					do // Trigger loop
					{
						nowTime_secs = time(&nowTime_secs); // update to current time
						settings::calEvent::updateValidEventTimers();

						if (actionToDo == "startOn" || actionToDo == "endOn")
						{
							string result = garageLightCommand("poweron");
							lg.i("Command poweron sent to device, result: ", result);
						}
						else if (actionToDo == "startOff" || actionToDo == "endOff")
						{
							string result = garageLightCommand("poweroff");
							lg.i("Command poweroff sent to device, result: ", result);
						}

						actionToDo = settings::calEventGroup::eventTimeCheck(stoi(settings::u_minsBefore), stoi(settings::u_minsAfter));
						// Once eventTimeCheck has run, person->lightShouldBeOn has run so we can update the API
						canLightBeTurnedOff = settings::calEventGroup::updateCanLightTurnOffBool();
						if (actionToDo != "")
						{
							lg.i("End of trigger loop, actionToBeDone is: ", actionToDo, " // waiting 10 secs (", return_current_time_and_date(), ")");
							sleep(10);
						}
						for (settings* person : settings::people) {
							if (person->lightShouldBeOn)
							{
								lg.d(person->u_name, "'s light should be ON right now. (lightShouldBeOn: ", person->lightShouldBeOn, ")");
							}
							else
							{
								lg.d(person->u_name, "'s light should be off right now. (lightShouldBeOn: ", person->lightShouldBeOn, ")");
							}
						}
					} while (actionToDo != "");
					break; // Must exit maxTries loop if no error caught
				}
				catch (string e) {
					bool internetConnectedAfterError = InternetConnected();
					lg.e("Critical failure: ", e);
					lg.e("Failure #", count, ", waiting 1 min and retrying.");
					lg.i("Is internet connected? Will stop if false -> ", internetConnectedAfterError);
					sleep(60);
					if (++count == maxTries || !internetConnectedAfterError)
					{
						lg.e("ERROR ", count, " out of max ", maxTries, "!!! Stopping, reason ->\n", e);
						return EXIT_FAILURE;
					}
				}
			} // max tries loop
		} // internet connected
		else {
			lg.i("\nProgram requires internet to run, will keep retrying.");
		}
		lg.b("\n<<<<<<<---------------------------PROGRAM TERMINATES HERE (loop #", mainLoopCounter, ")--------------------------->>>>>>>");

		mainLoopCounter++;
		lg.b("Waiting for 30 seconds... (now -> ", return_current_time_and_date(), " LOCAL)\n\n\n\n\n\n\n\n\n");
		sleep(30);
	}
}