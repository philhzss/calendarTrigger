#include "utils.h"
#include <curl/curl.h>
#include <fstream>


using std::cout;
using std::endl;
using std::cin;
using std::string;

// CURL stuff
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}
string curl_GET(string url)
{
	const char* const url_to_use = url.c_str();
	CURL* curl;
	CURLcode res;
	// Buffer to store result temporarily:
	string readBuffer;
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url_to_use);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		/* Check for errors */
		if (res != CURLE_OK)
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
			throw "curl_easy_perform() failed: " + std::to_string(res);
		}

		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
	return readBuffer;
}


/* Constructor that'll take a string for parameter,
	will be inserted before each log message to make sure we know what file its from
	Also requires logging level per file now*/
Log::Log(string sourceFile, int level)
{
	source_file = sourceFile;

	string tempLevelVar;
	fileLogLevel = level;
	if (level == 0)
		tempLevelVar = "[Errors only]";
	else if (level == 1)
		tempLevelVar = "[Normal - Info & errors]";
	else if (level == 2)
		tempLevelVar = "[Debug - all messages]";
	else if (level == 3)
		tempLevelVar = "[Programming - High debugging level messages]";
	cout << "Log level for " + source_file + " is: " << tempLevelVar << endl;
}


int Log::ReadLevel()
{
	return fileLogLevel;
}


string Log::curl_POST_slack(string url, string message)
{
	const char* const url_to_use = url.c_str();
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
		headers = curl_slist_append(headers, "Content-Type: application/json");

		string data = "{\"text\":\"" + message + "\"}";
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
		}

		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
	return std::to_string(response_code);
}



void Log::write(string message, string sourceFileWrite, string levelPrint, bool notification)
{
	string toWrite;
	if ((sourceFileWrite == "") && (levelPrint == ""))
	{
		toWrite = message;
	}
	else {
		toWrite = "[" + sourceFileWrite + " " + levelPrint + "] " + message;
	}
	cout << toWrite << endl;
	if ("true" == "true")
		toFile(toWrite);
}

// Logging to file functions
inline string Log::getCurrentDateTime(string s) {
	time_t now = time(0);
	struct tm  tstruct;
	char  buf[80];
	tstruct = *localtime(&now);
	if (s == "now")
		strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
	else if (s == "date")
		strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
	return string(buf);
};

inline void Log::toFile(string message) {

	string filePath = "log_" + Log::getCurrentDateTime("date") + ".txt";
	string now = Log::getCurrentDateTime("now");
	std::ofstream ofs(filePath.c_str(), std::ios_base::out | std::ios_base::app);
	ofs << now << '\t' << message << '\n';
	ofs.close();
}

void AddTime(int minutes, tm* date) {
	if (date == NULL) return;
	date->tm_min += minutes;
	mktime(date);
}