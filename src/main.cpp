#include "calendar.h"
#include "utils.h"
#include "settings.h"
#include <curl/curl.h>
#include <unistd.h>

using std::string;

static Log lg("Main", Log::LogLevel::Debug);

time_t nowTime_secs = time(&nowTime_secs);

// Make sure internet connection works
bool InternetConnected() {
	lg.p("Running internet_connected() function");
	bool internet_res;
	CURL* curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "http://www.google.com/");
		/* Disable the console output of the HTTP request data as we don't care: */
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		int curlRes = curl_easy_perform(curl);
		if (curlRes == 0) {
			internet_res = true;
			lg.i("~Internet connection check successful~");
		}
		else {
			internet_res = false;
			lg.e("***** INTERNET DOWN *****");
			lg.e("***** INTERNET DOWN *****");
			lg.e("***** INTERNET DOWN *****");
			long response_code;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
			lg.e("CURL Error Code: ", curlRes);
			lg.d("HTTP Error Code (will be 0 if absolutely can't connect): ", response_code);
			lg.d("See https://curl.haxx.se/libcurl/c/libcurl-errors.html for details.");
		}
		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();

	lg.p("End of internet_connected() function\n");
	return internet_res;
}

// Time functions
const string return_current_time_and_date()
{
	struct tm tstruct;
	char buf[80];
	tstruct = *localtime(&nowTime_secs);
	// VERIFY LOGGER HERE
	if (lg.ReadLevel() == Log::Programming)
	{
		cout << "TEST NOW TIME STRUCT" << endl;
		cout << "nowTime_secs before conversions, should be the same after:" << endl;
		cout << nowTime_secs << endl;
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
			//headers = curl_slist_append(headers, "Content-Type: application/json");




			// Serialize the package json to string
			string data = "command=" + command;
			lg.i(data);
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

int main()
{
	int mainLoopCounter = 1;
	while (true)
	{
		lg.b("\n\n>>>>>>>------------------------------PROGRAM STARTS HERE----------------------------<<<<<<<\n");
		nowTime_secs = time(&nowTime_secs); // update to current time
		lg.i("Runtime date-time (this loop): " + return_current_time_and_date() + " LOCAL\n");
		lg.d("Loop run number since program start: ", mainLoopCounter);
		string actionToDo;
		int count = 0;
		int maxTries = 10;
		lg.in("test");
		if (InternetConnected())
		{
			while (true) // max tries loop
			{
				try {
					initAll();
					lg.b();
					do
					{
						nowTime_secs = time(&nowTime_secs); // update to current time
						settings::calEvent::updateValidEventTimers();

						if (actionToDo == "startOn" || actionToDo == "endOn")
						{
							string result = garageLightCommand("poweron");
							lg.i(result);
						}
						else if (actionToDo == "startOff" || actionToDo == "endOff")
						{
							string result = garageLightCommand("poweroff");
							lg.i(result);
						}

						actionToDo = settings::calEventGroup::eventTimeCheck(10, 10);
						if (actionToDo != "")
						{
							lg.i("End of wakeLoop, actionToBeDone is: ", actionToDo);
							lg.i("Waiting ", 10, " seconds and re-running wakeLoop");
							sleep(10);
						}
						settings::calEventGroup::confirmDuplicateProtect(actionToDo);
					} while (actionToDo != "");
					break; // Must exit maxTries loop if no error caught
				}
				catch (string e) {
					lg.e("Critical failure: ", e, "\nFailure #", count, ", waiting 1 min and retrying.");
					lg.i("Is internet connected?", InternetConnected());
					sleep(60);
					if (++count == maxTries)
					{
						lg.e("ERROR ", count, " out of max ", maxTries, "!!! Stopping, reason ->\n", e);
						return EXIT_FAILURE;
					}
				}
			} // max tries loop
			settings::calEventGroup::cleanup(); // always cleanup, except if internet not working
		} // internet connected
		else {
			lg.i("\nProgram requires internet to run, will keep retrying.");
		}
		lg.b("\n<<<<<<<---------------------------PROGRAM TERMINATES HERE--------------------------->>>>>>>\n");

		mainLoopCounter++;
		lg.b("Waiting for 30 seconds... (now -> ", return_current_time_and_date(), " LOCAL)\n\n\n\n\n\n\n\n\n");
		sleep(30);
	}
}