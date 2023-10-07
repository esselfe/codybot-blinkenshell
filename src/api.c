#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>

#include "codybot.h"

void APIFetchAstro(char *city) {
	// Retrieve API key
	///////////////////
	FILE *fp = fopen("api.key", "r");
	if (fp == NULL) {
		sprintf(buffer, "codybot error: Cannot open cmd.output: %s",
			strerror(errno));
		Msg(buffer);
		return;
	}
	char *key = malloc(512);
	memset(key, 0, 512);
	fgets(key, 511, fp);
	fclose(fp);

	// Remove newline from key string
	/////////////////////////////////
	if (*(key+strlen(key)-1) == '\n')
		*(key+strlen(key)-1) = '\0';

	// Perform curl request
	///////////////////////
	CURL *handle = curl_easy_init();
	
	char *datestr = malloc(128);
	memset(datestr, 0, 128);
	time_t t0 = time(NULL);
	struct tm *tm0 = localtime(&t0);
	sprintf(datestr, "%d-%02d-%02d", tm0->tm_year+1900,
		tm0->tm_mon+1, tm0->tm_mday);
	
	char *url = malloc(4096);
	memset(url, 0, 4096);
	sprintf(url, "https://api.weatherapi.com/v1/astronomy.json"
		"?key=%s&q=%s&dt=%s", key, city, datestr);
	free(datestr);
	free(key);
	curl_easy_setopt(handle, CURLOPT_URL, url);
	free(url);
	
	fp = fopen("cmd.output", "w");
	if (fp == NULL) {
		sprintf(buffer, "codybot error: Cannot open cmd.output: %s",
			strerror(errno));
		Msg(buffer);
		curl_easy_cleanup(handle);
		return;
	}
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)fp);
	
	CURLcode ret = curl_easy_perform(handle);
	if (ret != CURLE_OK) {
		sprintf(buffer, "codybot error (curl): %s\n", curl_easy_strerror(ret));
		Msg(buffer);
		curl_easy_cleanup(handle);
		return;
	}
	fclose(fp);
	curl_easy_cleanup(handle);

	// Parse json-formatted response
	////////////////////////////////
	json_object *root = json_object_from_file("cmd.output");
	if (root == NULL) {
		Msg("No results.");
		return;
	}
	
	json_object *location = json_object_object_get(root, "location");
	if (location == NULL) {
		json_object *error = json_object_object_get(root, "error");
		if (error == NULL)
			Msg("No location found in response.");
		else {
			json_object *message = json_object_object_get(error, "message");
			Msg((char *)json_object_get_string(message));
		}
		
		json_object_put(root);
		return;
	}
	json_object *name = json_object_object_get(location, "name");
	json_object *region = json_object_object_get(location, "region");
	json_object *country = json_object_object_get(location, "country");
	
	json_object *current = json_object_object_get(root, "astronomy");
	json_object *astro = json_object_object_get(current, "astro");
	json_object *sunrise = json_object_object_get(astro, "sunrise");
	json_object *sunset = json_object_object_get(astro, "sunset");
	json_object *moonrise = json_object_object_get(astro, "moonrise");
	json_object *moonset = json_object_object_get(astro, "moonset");
	json_object *moonphase = json_object_object_get(astro, "moon_phase");
	json_object *moonillum = json_object_object_get(astro, "moon_illumination");
	
	// Create output string
	///////////////////////
	char *str = malloc(4096);
	memset(str, 0, 4096);
	char *value = (char *)json_object_get_string(name);
	sprintf(str, "%s, ", value);
	value = (char *)json_object_get_string(region);
	strcat(str, value);
	strcat(str, ", ");
	value = (char *)json_object_get_string(country);
	strcat(str, value);
	strcat(str, ": Sunrise ");
	value = (char *)json_object_get_string(sunrise);
	strcat(str, value);
	strcat(str, ", sunset ");
	value = (char *)json_object_get_string(sunset);
	strcat(str, value);
	strcat(str, ", moonrise ");
	value = (char *)json_object_get_string(moonrise);
	strcat(str, value);
	strcat(str, ", moonset ");
	value = (char *)json_object_get_string(moonset);
	strcat(str, value);
	strcat(str, ", phase ");
	value = (char *)json_object_get_string(moonphase);
	strcat(str, value);
	strcat(str, ", illumination ");
	value = (char *)json_object_get_string(moonillum);
	strcat(str, value);
	strcat(str, "%");
	
	json_object_put(root);
	
	// Send results to chat
	///////////////////////
	Msg(str);
	free(str);
}

void APIFetchWeather(char *city) {
	// Retrieve API key
	///////////////////
	FILE *fp = fopen("api.key", "r");
	if (fp == NULL) {
		sprintf(buffer, "codybot error: Cannot open cmd.output: %s",
			strerror(errno));
		Msg(buffer);
		return;
	}
	char *key = malloc(512);
	memset(key, 0, 512);
	fgets(key, 511, fp);
	fclose(fp);

	// Remove newline from key string
	/////////////////////////////////
	if (*(key+strlen(key)-1) == '\n')
		*(key+strlen(key)-1) = '\0';

	// Perform curl request
	///////////////////////
	CURL *handle = curl_easy_init();
	
	char url[4096];
	memset(url, 0, 4096);
	sprintf(url, "https://api.weatherapi.com/v1/current.json"
		"?key=%s&q=%s", key, city);
	free(key);
	curl_easy_setopt(handle, CURLOPT_URL, url);
	
	fp = fopen("cmd.output", "w");
	if (fp == NULL) {
		sprintf(buffer, "codybot error: Cannot open cmd.output: %s",
			strerror(errno));
		Msg(buffer);
		curl_easy_cleanup(handle);
		return;
	}
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)fp);
	
	CURLcode ret = curl_easy_perform(handle);
	if (ret != CURLE_OK) {
		sprintf(buffer, "codybot error (curl): %s\n", curl_easy_strerror(ret));
		Msg(buffer);
		curl_easy_cleanup(handle);
		return;
	}
	fclose(fp);
	curl_easy_cleanup(handle);

	// Parse json-formatted response
	////////////////////////////////
	json_object *root = json_object_from_file("cmd.output");
	if (root == NULL) {
		Msg("No results.");
		return;
	}
	
	json_object *location = json_object_object_get(root, "location");
	if (location == NULL) {
		json_object *error = json_object_object_get(root, "error");
		if (error == NULL)
			Msg("No location found in response.");
		else {
			json_object *message = json_object_object_get(error, "message");
			Msg((char *)json_object_get_string(message));
		}
		
		json_object_put(root);
		return;
	}
	json_object *name = json_object_object_get(location, "name");
	json_object *region = json_object_object_get(location, "region");
	json_object *country = json_object_object_get(location, "country");
	
	json_object *current = json_object_object_get(root, "current");
	json_object *condition = json_object_object_get(current, "condition");
	json_object *text = json_object_object_get(condition, "text");
	json_object *temp_c = json_object_object_get(current, "temp_c");
	json_object *temp_f = json_object_object_get(current, "temp_f");
	json_object *feels_c = json_object_object_get(current, "feelslike_c");
	json_object *feels_f = json_object_object_get(current, "feelslike_f");
	json_object *wind_k = json_object_object_get(current, "wind_kph");
	json_object *wind_m = json_object_object_get(current, "wind_mph");
	json_object *gust_k = json_object_object_get(current, "gust_kph");
	json_object *gust_m = json_object_object_get(current, "gust_mph");
	json_object *precip = json_object_object_get(current, "precip_mm");
	
	// Create output string
	///////////////////////
	char *str = malloc(4096);
	memset(str, 0, 4096);
	char *value = (char *)json_object_get_string(name);
	sprintf(str, "%s, ", value);
	value = (char *)json_object_get_string(region);
	strcat(str, value);
	strcat(str, ", ");
	value = (char *)json_object_get_string(country);
	strcat(str, value);
	strcat(str, ": ");
	value = (char *)json_object_get_string(text);
	strcat(str, value);
	strcat(str, ", ");
	value = (char *)json_object_get_string(temp_c);
	strcat(str, value);
	strcat(str, "C/");
	value = (char *)json_object_get_string(temp_f);
	strcat(str, value);
	strcat(str, "F feels like ");
	value = (char *)json_object_get_string(feels_c);
	strcat(str, value);
	strcat(str, "C/");
	value = (char *)json_object_get_string(feels_f);
	strcat(str, value);
	strcat(str, "F, wind ");
	value = (char *)json_object_get_string(wind_k);
	strcat(str, value);
	strcat(str, "kmh/");
	value = (char *)json_object_get_string(wind_m);
	strcat(str, value);
	strcat(str, "mph, gust ");
	value = (char *)json_object_get_string(gust_k);
	strcat(str, value);
	strcat(str, "kmh/");
	value = (char *)json_object_get_string(gust_m);
	strcat(str, value);
	strcat(str, "mph, precip. ");
	value = (char *)json_object_get_string(precip);
	strcat(str, value);
	strcat(str, "mm");
	
	json_object_put(root);
	
	// Send results to chat
	///////////////////////
	Msg(str);
	free(str);
}

