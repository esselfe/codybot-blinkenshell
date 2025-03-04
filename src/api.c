#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <curl/curl.h>
#include <json-c/json.h>

#include "codybot.h"

static void APIKeyStripNewLine(char *key) {
	char *cp = key;
	while (*cp != '\0') {
		if (*cp == '\n') {
			*cp = '\0';
			break;
		}
		++cp;
	}
}

static char *APIGetKey(void) {
	FILE *fp = fopen("api.key", "r");
	if (fp == NULL) {
		sprintf(buffer, "codybot error: Cannot open api.key: %s",
			strerror(errno));
		Msg(buffer);
		return NULL;
	}
	char *key = malloc(512);
	memset(key, 0, 512);
	fgets(key, 511, fp);
	fclose(fp);

	APIKeyStripNewLine(key);
	
	return key;
}

void APIFetchAstro(char *city) {
	// Retrieve API key
	///////////////////
	char *key = APIGetKey();
	if (key == NULL)
		return;

	// Perform curl request
	///////////////////////
	CURL *handle = curl_easy_init();
	
	char *datestr = malloc(128);
	memset(datestr, 0, 128);
	t0 = time(NULL);
	struct tm *tm1 = malloc(sizeof(struct tm));
	localtime_r(&t0, tm1);
	sprintf(datestr, "%d-%02d-%02d", tm1->tm_year+1900,
		tm1->tm_mon+1, tm1->tm_mday);
	free(tm1);
	
	char *url = malloc(4096);
	memset(url, 0, 4096);
	char *city_conv = curl_easy_escape(handle, city, 128);
	sprintf(url, "https://api.weatherapi.com/v1/astronomy.json"
		"?key=%s&q=%s&dt=%s", key, city_conv, datestr);
	curl_free(city_conv);
	free(datestr);
	free(key);
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
	free(url);
	
	FILE *fp = fopen("cmd.output", "w");
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
	char *value = NULL;
	if (name != NULL) {
		value = (char *)json_object_get_string(name);
		if (value != NULL)
			sprintf(str, "%s, ", value);
	}
	if (region != NULL) {
		value = (char *)json_object_get_string(region);
		if (value != NULL)
			strcat(str, value);
		strcat(str, ", ");
	}
	if (country != NULL) {
		value = (char *)json_object_get_string(country);
		if (value != NULL)
			strcat(str, value);
	}
	strcat(str, ": ");
	if (sunrise != NULL) {
		strcat(str, "Sunrise ");
		value = (char *)json_object_get_string(sunrise);
		if (value != NULL)
			strcat(str, value);
		strcat(str, ", ");
	}
	if (sunset != NULL) {
		strcat(str, "sunset ");
		value = (char *)json_object_get_string(sunset);
		if (value != NULL)
			strcat(str, value);
		strcat(str, ", ");
	}
	if (moonrise != NULL) {
		strcat(str, "moonrise ");
		value = (char *)json_object_get_string(moonrise);
		if (value != NULL)
			strcat(str, value);
		strcat(str, ", ");
	}
	if (moonset != NULL) {
		strcat(str, "moonset ");
		value = (char *)json_object_get_string(moonset);
		if (value != NULL)
			strcat(str, value);
		strcat(str, ", ");
	}
	if (moonphase != NULL) {
		strcat(str, "phase ");
		value = (char *)json_object_get_string(moonphase);
		if (value != NULL)
			strcat(str, value);
		strcat(str, ", ");
	}
	if (moonillum != NULL) {
		strcat(str, "illumination ");
		value = (char *)json_object_get_string(moonillum);
		if (value != NULL)
			strcat(str, value);
		strcat(str, "%");
	}
	
	json_object_put(root);
	
	// Send results to chat
	///////////////////////
	Msg(str);
	free(str);
}

void APIFetchForecast(char *city) {
	// Retrieve API key
	///////////////////
	char *key = APIGetKey();
	if (key == NULL)
		return;
	
	// Perform curl request
	///////////////////////
	CURL *handle = curl_easy_init();
	
	char url[4096];
	memset(url, 0, 4096);
	char *city_conv = curl_easy_escape(handle, city, 128);
	sprintf(url, "https://api.weatherapi.com/v1/forecast.json"
		"?key=%s&q=%s&days=3&aqi=no&alerts=no", key, city_conv);
	curl_free(city_conv);
	free(key);
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
	
	FILE *fp = fopen("cmd.output", "w");
	if (fp == NULL) {
		printf("codybot error in api.c: Cannot open cmd.output: %s\n",
			strerror(errno));
		curl_easy_cleanup(handle);
		return;
	}
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)fp);
	
	CURLcode ret = curl_easy_perform(handle);
	if (ret != CURLE_OK) {
		printf("codybot error in api.c (curl): %s\n", curl_easy_strerror(ret));
		curl_easy_cleanup(handle);
		fclose(fp);
		return;
	}
	fclose(fp);
	curl_easy_cleanup(handle);

	// Parse the json results
	/////////////////////////
	json_object *root = json_object_from_file("cmd.output");
	if (root == NULL) {
		Msg("No results.");
		return;
	}
	
	json_object *location = json_object_object_get(root, "location");
	if (location == NULL) {
		json_object *error = json_object_object_get(root, "error");
		if (error == NULL) {
			json_object_put(root);
			Msg("No location found in response.");
			return;
		}
		else {
			json_object *message = json_object_object_get(error, "message");
			char *val = strdup(json_object_get_string(message));
			json_object_put(root);
			Msg(val);
			free(val);
			return;
		}
	}
	json_object *name = json_object_object_get(location, "name");
	json_object *region = json_object_object_get(location, "region");
	json_object *country = json_object_object_get(location, "country");
	
	json_object *forecast = json_object_object_get(root, "forecast");
	json_object *forecastday = json_object_object_get(forecast, "forecastday");
	
	json_object *item1 = json_object_array_get_idx(forecastday, 0);
	json_object *date1 = json_object_object_get(item1, "date");
	json_object *day1 = json_object_object_get(item1, "day");
	json_object *condition1 = json_object_object_get(day1, "condition");
	json_object *text1 = json_object_object_get(condition1, "text");
	json_object *mintemp_c1 = json_object_object_get(day1, "mintemp_c");
	json_object *mintemp_f1 = json_object_object_get(day1, "mintemp_f");
	json_object *maxtemp_c1 = json_object_object_get(day1, "maxtemp_c");
	json_object *maxtemp_f1 = json_object_object_get(day1, "maxtemp_f");
	json_object *totalprecip_mm1 = json_object_object_get(day1, "totalprecip_mm");
        json_object *totalprecip_in1 = json_object_object_get(day1, "totalprecip_in");
        json_object *totalsnow_cm1 = json_object_object_get(day1, "totalsnow_cm");
	
	json_object *item2 = json_object_array_get_idx(forecastday, 1);
	json_object *date2 = json_object_object_get(item2, "date");
	json_object *day2 = json_object_object_get(item2, "day");
	json_object *condition2 = json_object_object_get(day2, "condition");
	json_object *text2 = json_object_object_get(condition2, "text");
	json_object *mintemp_c2 = json_object_object_get(day2, "mintemp_c");
	json_object *mintemp_f2 = json_object_object_get(day2, "mintemp_f");
	json_object *maxtemp_c2 = json_object_object_get(day2, "maxtemp_c");
	json_object *maxtemp_f2 = json_object_object_get(day2, "maxtemp_f");
	json_object *totalprecip_mm2 = json_object_object_get(day2, "totalprecip_mm");
        json_object *totalprecip_in2 = json_object_object_get(day2, "totalprecip_in");
        json_object *totalsnow_cm2 = json_object_object_get(day2, "totalsnow_cm");
        
	json_object *item3 = json_object_array_get_idx(forecastday, 2);
	json_object *date3 = json_object_object_get(item3, "date");
	json_object *day3 = json_object_object_get(item3, "day");
	json_object *condition3 = json_object_object_get(day3, "condition");
	json_object *text3 = json_object_object_get(condition3, "text");
	json_object *mintemp_c3 = json_object_object_get(day3, "mintemp_c");
	json_object *mintemp_f3 = json_object_object_get(day3, "mintemp_f");
	json_object *maxtemp_c3 = json_object_object_get(day3, "maxtemp_c");
	json_object *maxtemp_f3 = json_object_object_get(day3, "maxtemp_f");
	json_object *totalprecip_mm3 = json_object_object_get(day3, "totalprecip_mm");
        json_object *totalprecip_in3 = json_object_object_get(day3, "totalprecip_in");
        json_object *totalsnow_cm3 = json_object_object_get(day3, "totalsnow_cm");
        
	// Create final output string
	/////////////////////////////
	char *str = malloc(4096);
	memset(str, 0, 4096);
	if (name != NULL)
		sprintf(str, "%s, ", (char *)json_object_get_string(name));
	if (region != NULL) {
		strcat(str, (char *)json_object_get_string(region));
		strcat(str, ", ");
	}
	if (country != NULL) {
		strcat(str, (char *)json_object_get_string(country));
		strcat(str, ": ");
	}
	if (date1 != NULL) {
		strcat(str, (char *)json_object_get_string(date1));
		strcat(str, ": ");
	}
	if (text1 != NULL) {
		strcat(str, (char *)json_object_get_string(text1));
		strcat(str, ", ");
	}
	if (mintemp_c1 != NULL) {
		strcat(str, "minimum ");
		strcat(str, (char *)json_object_get_string(mintemp_c1));
		strcat(str, "C/");
	}
	if (mintemp_f1 != NULL) {
		strcat(str, (char *)json_object_get_string(mintemp_f1));
		strcat(str, "F, ");
	}
	if (maxtemp_c1 != NULL) {
		strcat(str, "maximum ");
		strcat(str, (char *)json_object_get_string(maxtemp_c1));
		strcat(str, "C/");
	}
	if (maxtemp_f1 != NULL) {
		strcat(str, (char *)json_object_get_string(maxtemp_f1));
		strcat(str, "F ");
	}
	if (totalprecip_mm1 != NULL) {
		strcat(str, "total precip. ");
		strcat(str, (char *)json_object_get_string(totalprecip_mm1));
		strcat(str, "mm/");
	}
	if (totalprecip_in1 != NULL) {
		strcat(str, (char *)json_object_get_string(totalprecip_in1));
		strcat(str, "in, ");
	}
	if (totalsnow_cm1 != NULL) {
		strcat(str, "total snow ");
		strcat(str, (char *)json_object_get_string(totalsnow_cm1));
		strcat(str, "cm; ");
	}
	
	if (date2 != NULL) {
		strcat(str, (char *)json_object_get_string(date2));
		strcat(str, ": ");
	}
	if (text2 != NULL) {
		strcat(str, (char *)json_object_get_string(text2));
		strcat(str, ", ");
	}
	if (mintemp_c2 != NULL) {
		strcat(str, "minimum ");
		strcat(str, (char *)json_object_get_string(mintemp_c2));
		strcat(str, "C/");
	}
	if (mintemp_f2 != NULL) {
		strcat(str, (char *)json_object_get_string(mintemp_f2));
		strcat(str, "F, ");
	}
	if (maxtemp_c2 != NULL) {
		strcat(str, "maximum ");
		strcat(str, (char *)json_object_get_string(maxtemp_c2));
		strcat(str, "C/");
	}
	if (maxtemp_f2 != NULL) {
		strcat(str, (char *)json_object_get_string(maxtemp_f2));
		strcat(str, "F ");
	}
	if (totalprecip_mm2 != NULL) {
		strcat(str, "total precip. ");
		strcat(str, (char *)json_object_get_string(totalprecip_mm2));
		strcat(str, "mm/");
	}
	if (totalprecip_in2 != NULL) {
		strcat(str, (char *)json_object_get_string(totalprecip_in2));
		strcat(str, "in, ");
	}
	if (totalsnow_cm2 != NULL) {
		strcat(str, "total snow ");
		strcat(str, (char *)json_object_get_string(totalsnow_cm2));
		strcat(str, "cm ");
	}
	
	if (date3 != NULL) {
		strcat(str, (char *)json_object_get_string(date3));
		strcat(str, ": ");
	}
	if (text3 != NULL) {
		strcat(str, (char *)json_object_get_string(text3));
		strcat(str, ", ");
	}
	if (mintemp_c3 != NULL) {
		strcat(str, "minimum ");
		strcat(str, (char *)json_object_get_string(mintemp_c3));
		strcat(str, "C/");
	}
	if (mintemp_f3 != NULL) {
		strcat(str, (char *)json_object_get_string(mintemp_f3));
		strcat(str, "F, ");
	}
	if (maxtemp_c3 != NULL) {
		strcat(str, "maximum ");
		strcat(str, (char *)json_object_get_string(maxtemp_c3));
		strcat(str, "C/");
	}
	if (maxtemp_f3 != NULL) {
		strcat(str, (char *)json_object_get_string(maxtemp_f3));
		strcat(str, "F ");
	}
	if (totalprecip_mm3 != NULL) {
		strcat(str, "total precip. ");
		strcat(str, (char *)json_object_get_string(totalprecip_mm3));
		strcat(str, "mm/");
	}
	if (totalprecip_in3 != NULL) {
		strcat(str, (char *)json_object_get_string(totalprecip_in3));
		strcat(str, "in, ");
	}
	if (totalsnow_cm3 != NULL) {
		strcat(str, "total snow ");
		strcat(str, (char *)json_object_get_string(totalsnow_cm3));
		strcat(str, "cm");
	}
	
	json_object_put(root);
	
	// Send results to chat
	///////////////////////
	Msg(str);
	free(str);
}

void APIFetchTime(char *city) {
	// Retrieve API key
	///////////////////
	char *key = APIGetKey();
	if (key == NULL)
		return;

	// Perform curl request
	///////////////////////
	CURL *handle = curl_easy_init();
	
	char *url = malloc(4096);
	memset(url, 0, 4096);
	char *city_conv = curl_easy_escape(handle, city, 128);
	sprintf(url, "https://api.weatherapi.com/v1/timezone.json"
		"?key=%s&q=%s", key, city_conv);
	curl_free(city_conv);
	free(key);
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
	free(url);
	
	FILE *fp = fopen("cmd.output", "w");
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
	json_object *tz_id = json_object_object_get(location, "tz_id");
	json_object *time_string = json_object_object_get(location, "localtime");
	
	// Create output string
	///////////////////////
	char *str = malloc(4096);
	memset(str, 0, 4096);
	char *value;
	if (name) {
		value = (char *)json_object_get_string(name);
		if (value != NULL)
			sprintf(str, "%s, ", value);
	}
	if (region) {
		value = (char *)json_object_get_string(region);
		if (value != NULL)
			strcat(str, value);
		strcat(str, ", ");
	}
	if (country) {
		value = (char *)json_object_get_string(country);
		if (value != NULL)
			strcat(str, value);
	}
	if (tz_id) {
		strcat(str, ": Timezone ");
		value = (char *)json_object_get_string(tz_id);
		if (value != NULL)
			strcat(str, value);
		strcat(str, ", ");
	}
	if (time_string) {
		value = (char *)json_object_get_string(time_string);
		if (value != NULL)
			strcat(str, value);
	}
	
	json_object_put(root);
	
	// Send results to chat
	///////////////////////
	Msg(str);
	free(str);
}

void APIFetchWeather(char *city) {
	// Retrieve API key
	///////////////////
	char *key = APIGetKey();
	if (key == NULL)
		return;

	// Perform curl request
	///////////////////////
	CURL *handle = curl_easy_init();
	
	char url[4096];
	memset(url, 0, 4096);
	char *city_conv = curl_easy_escape(handle, city, 128);
	sprintf(url, "https://api.weatherapi.com/v1/current.json"
		"?key=%s&q=%s", key, city_conv);
	curl_free(city_conv);
	free(key);
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
	
	FILE *fp = fopen("cmd.output", "w");
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
	char *value;
	if (name != NULL) {
		value = (char *)json_object_get_string(name);
		if (value != NULL)
			sprintf(str, "%s, ", value);
	}
	if (region != NULL) {
		value = (char *)json_object_get_string(region);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, ", ");
	}
	if (country != NULL) {
		value = (char *)json_object_get_string(country);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, ": ");
	}
	if (text != NULL) {
		value = (char *)json_object_get_string(text);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, ", ");
	}
	if (temp_c != NULL) {
		value = (char *)json_object_get_string(temp_c);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "C/");
	}
	if (temp_f != NULL) {
		value = (char *)json_object_get_string(temp_f);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "F ");
	}
	strcat(str, "feels like ");
	if (feels_c != NULL) {
		value = (char *)json_object_get_string(feels_c);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "C/");
	}
	if (feels_f != NULL) {
		value = (char *)json_object_get_string(feels_f);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "F, ");
	}
	strcat(str, "wind ");
	if (wind_k != NULL) {
		value = (char *)json_object_get_string(wind_k);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "kmh/");
	}
	if (wind_m != NULL) {
		value = (char *)json_object_get_string(wind_m);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "mph, ");
	}
	strcat(str, "gust ");
	if (gust_k != NULL) {
		value = (char *)json_object_get_string(gust_k);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "kmh/");
	}
	if (gust_m != NULL) {
		value = (char *)json_object_get_string(gust_m);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "mph, ");
	}
	strcat(str, "precip. ");
	if (precip != NULL) {
		value = (char *)json_object_get_string(precip);
		if (value != NULL)
			strcat(str, value);
		
		strcat(str, "mm");
	}
	
	json_object_put(root);
	
	// Send results to chat
	///////////////////////
	Msg(str);
	free(str);
}

