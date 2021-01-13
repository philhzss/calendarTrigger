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

int initAll()
{
	try {
		InternetConnected();
		settings::readSettings();
		nowTime_secs = time(&nowTime_secs); // update to current time
		lg.i("\n\nCalendarTrigger init");
		initiateCal();
	}
	catch (string e) {
		lg.e("Error: ", e);
		return 1;
	}
	return 0;
}

int main()
{
	while (true)
	{
		if (initAll() != 0) {
			lg.e("Stopping, error in initAll");
			return EXIT_FAILURE;
		}
		sleep(15);

		settings::calEventGroup::cleanup(); // always cleanup
	}
}