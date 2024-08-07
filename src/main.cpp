#include "utils.h"
#include "settings.h"
#include <curl/curl.h>
#include <unistd.h>

#define CROW_MAIN
#include <crow.h>

using std::string;

static Log lg("Main", Log::LogLevel::Debug);
static Log lgC("Crow", Log::LogLevel::Debug);


time_t nowTime_secs = time(&nowTime_secs);
std::mutex settings::settingsMutex;

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
const string string_time_and_date(tm tstruct, bool printLocal)
{
	time_t tempSeconds = mktime(&tstruct) - timezone;
	tm localStruct = *localtime(&tempSeconds);
	char buf[80];
	const char* printString = (printLocal) ? "%Y-%m-%d %R Local" : "%Y-%m-%d %R";
	strftime(buf, sizeof(buf), printString, &localStruct);
	return buf;
}

void initAll()
{
	settings::calEventGroup::cleanup(); // always cleanup before anything
	try {
		InternetConnected();
		settings::readSettings();
		nowTime_secs = time(&nowTime_secs); // update to current time
		lg.b();
		initiateCal();

		// Must run eventTimeCheck to update future events, result (actionToDo) is ignored though)
		settings::calEventGroup::eventTimeCheck(stoi(settings::u_minsBefore), stoi(settings::u_minsAfter), stoi(settings::u_hoursFutureLookAhead), true);
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

void DoCrowAPI(std::vector<settings*>* people, string* minsBeforeTriggerOn,
	string* minsAfterTriggerOff, bool* doShiftEndings) {
	crow::SimpleApp app; //define your crow application

	//define your endpoint at the root directory
	CROW_ROUTE(app, "/api")([&people, &minsBeforeTriggerOn, &minsAfterTriggerOff, &doShiftEndings]() {
		lgC.d("///////////Start of Crow request");
		crow::json::wvalue json;

		if (!settings::settingsMutexLockSuccess("crow request")) {
			return json["app"] = "ERROR";
		}
		lgC.d("!!!CROW: MUTEX LOCKED!!!");

		lgC.d("Crow HTTP request; Crow thread has ", people->size(), " people.");
		json["app"]["minsBeforeTriggerOn"] = lg.prepareOnly(*minsBeforeTriggerOn);
		json["app"]["minsAfterTriggerOff"] = lg.prepareOnly(*minsAfterTriggerOff);
		json["app"]["shiftEndingsTriggerLight"] = lg.prepareOnly(*doShiftEndings);
		for (settings* person : *people) {
			json[lg.prepareOnly(person->u_name)]
				["lightShouldBeOn"] = lg.prepareOnly(person->lightShouldBeOn);
			json[lg.prepareOnly(person->u_name)]["nextEvent"]
				["time"] = lg.prepareOnly(person->allEvents.nextFutureEvent);
			json[lg.prepareOnly(person->u_name)]["nextEvent"]
				["type"] = lg.prepareOnly(person->allEvents.nextFutureEventType);
			json[lg.prepareOnly(person->u_name)]["nextEvent"]
				["triggerOn"] = lg.prepareOnly(person->allEvents.nextFutureTriggerON);
			json[lg.prepareOnly(person->u_name)]["nextEvent"]
				["triggerOff"] = lg.prepareOnly(person->allEvents.nextFutureTriggerOFF);
			json[lg.prepareOnly(person->u_name)]
				["shiftStartBias"] = lg.prepareOnly(person->u_shiftStartBias);
			json[lg.prepareOnly(person->u_name)]
				["shiftEndBias"] = lg.prepareOnly(person->u_shiftEndBias);
			json[lg.prepareOnly(person->u_name)]
				["commuteTime"] = lg.prepareOnly(person->u_commuteTime);
			json[lg.prepareOnly(person->u_name)]
				["wordsToIgnore"] = person->ignoredWordsPrint();
			lgC.d("json return next event for ", person->u_name, ": ", person->allEvents.nextFutureEvent);
		}
		settings::settingsMutex.unlock();
		lgC.d("CROW: MUTEX UNLOCKED");
		lgC.d("///////////End of Crow request -- returning");
		return json;

		});

	//set the port, set the app to run on multiple threads, and run the app
	app.port(settings::u_apiPort).multithreaded().run();
	// 3112 main port, 4112 test port to not interfere with running version
}

int main()
{
	settings::readSettings(); // Must get API port before Crow API
	int mainLoopCounter = 1;
	time_t launchTime = time(&nowTime_secs);
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
					if (!settings::settingsMutexLockSuccess("before initAll")) {
						throw "Mutex timeout in main thread (before initAll)";
					}
					lg.d("!!!MAIN: MUTEX LOCKED (before initAll)!!!");
					// Thread safe
					initAll();
					// Thread safe
					settings::settingsMutex.unlock();
					lg.d("MAIN: MUTEX UNLOCKED (after initAll)");
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					do // Trigger loop
					{
						nowTime_secs = time(&nowTime_secs); // update to current time

						if (!settings::settingsMutexLockSuccess("before updateValidEventTimers")) {
							throw "Mutex timeout in main thread (before updateValidEventTimers)";
						}
						lg.d("!!!MAIN: MUTEX LOCKED (before updateValidEventTimers)!!!");
						// Thread safe
						settings::calEvent::updateValidEventTimers();
						// Thread safe
						settings::settingsMutex.unlock();
						lg.d("MAIN: MUTEX UNLOCKED (after updateValidEventTimers)");
						std::this_thread::sleep_for(std::chrono::milliseconds(100));


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

						if (!settings::settingsMutexLockSuccess("before eventTimeCheck")) {
							throw "Mutex timeout in main thread (inside triggerLoop)";
						}
						lg.d("!!!MAIN: MUTEX LOCKED (before eventTimeCheck)!!!");
						// Thread safe
						actionToDo = settings::calEventGroup::eventTimeCheck(stoi(settings::u_minsBefore), stoi(settings::u_minsAfter), stoi(settings::u_hoursFutureLookAhead));
						// Thread safe
						settings::settingsMutex.unlock();
						lg.d("MAIN: MUTEX UNLOCKED (after eventTimeCheck)");


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
					// This is to make sure the mutex doesn't stay locked forever. "Chances are" it was locked when we caught the exception
					settings::settingsMutex.unlock();

					bool internetConnectedAfterError = InternetConnected();
					lg.e("Critical failure: ", e);
					lg.e("Failure #", count, ", waiting 10 secs and retrying.");
					lg.i("Is internet connected? Will wait more if false -> ", internetConnectedAfterError);
					if (!internetConnectedAfterError) {
						std::this_thread::sleep_for(std::chrono::seconds(20));
					}
					std::this_thread::sleep_for(std::chrono::seconds(10));
					if (++count == maxTries)
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