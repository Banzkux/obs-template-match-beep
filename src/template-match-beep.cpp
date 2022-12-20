/*
OBS Template Match Beep
Copyright (C) 2022 Janne Pitkanen <acebanzkux@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
// Remember to bundle with the opencv binaries!
#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>
#include <thread>
#include <math.h>
#include "FrameIngest.hpp"

#include "template-match-beep.generated.h"

void beep(int freq, int duration);
#ifdef _WIN32
#include <Windows.h>
void beep(int freq, int duration)
{
	Beep(freq, duration);
}
#elif __linux__
#include <iostream>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/kd.h> 
#include <unistd.h>
void beep(int freq, int duration)
{
	int fd = open("/dev/console", O_WRONLY);
	ioctl(fd, KIOCSOUND, (int)(1193180/freq));
	usleep(1000*duration);
	ioctl(fd, KIOCSOUND, 0);
	close(fd);
}
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#ifndef SEC_TO_NSEC
#define SEC_TO_NSEC 1000000000ULL
#endif

#ifndef MSEC_TO_SEC
#define MSEC_TO_SEC 0.001
#endif

#define SETTING_DELAY_MS "delay_ms"
#define SETTING_COOLDOWN_MS "cooldown_ms"
#define SETTING_PATH "template_path"
#define SETTING_STATUS "status"

#define TEXT_DELAY_MS obs_module_text("Timerr")
#define TEXT_COOLDOWN_MS obs_module_text("Cooldown timer")
#define TEXT_PATH obs_module_text("Template image path")
#define TEXT_STATUS std::string("Status: ")

struct template_match_beep_data {
	obs_source_t *context;

	/* contains struct obs_source_frame* */
	obs_source_frame *current_frame;

	// Rest of the needed variables here (I guess)
	uint64_t last_video_ts;
	uint64_t timer;
	uint64_t cooldown_timer;
	const char *path;
	std::string state;
	bool timer_active;
	uint64_t timer_video_ts;
	std::thread thread;
	bool thread_active;
	cv::Mat template_image;
	cv::UMat current_cv_frame;

	std::unique_ptr<lvk::FrameIngest> frame_ingest;
};

static const char *template_match_beep_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Template Match Timer");
}

// Filters settings were updated
static void template_match_beep_filter_update(void *data, obs_data_t *settings)
{
	struct template_match_beep_data *filter = (template_match_beep_data*)data;
	uint64_t new_timer =
		(uint64_t)obs_data_get_int(settings, SETTING_DELAY_MS);

	uint64_t new_cooldown =
		(uint64_t)obs_data_get_int(settings, SETTING_COOLDOWN_MS);

	const char *new_path =
		(const char *)obs_data_get_string(settings, SETTING_PATH);

	filter->timer = new_timer;
	filter->cooldown_timer = new_cooldown;
	// Update template image on new path
	if (filter->path != new_path) {
		filter->path = new_path;
		filter->template_image = cv::imread(new_path, 0);
	}

	blog(LOG_INFO, "asd");
}

void thread_loop(void *data);

static void *template_match_beep_filter_create(obs_data_t *settings,
				       obs_source_t *context)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)bzalloc(sizeof(*filter));

	filter->context = context;
	template_match_beep_filter_update(filter, settings);

	filter->current_frame = nullptr;
	filter->state = TEXT_STATUS + "Created";
	filter->timer_video_ts = 0;
	filter->thread_active = true;
	filter->thread = std::thread(thread_loop, (void *)filter);
	return filter;
}

static void template_match_beep_filter_destroy(void *data)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	filter->thread_active = false;

	bfree(data);
}

// calibrate button was clicked
bool template_match_beep_calibrate(obs_properties_t *props,
				    obs_property*, void* data)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	// Do calibration
	filter->state = TEXT_STATUS + "Calibration";
	obs_property_set_description(obs_properties_get(props, SETTING_STATUS),
				     filter->state.c_str());
	// Check if the filter is active
	if (obs_source_enabled(filter->context))
		blog(LOG_INFO, "calibrate pressed");

	return true;
}

// reset calibration button was clicked
bool template_match_beep_reset_calibration(obs_properties_t *props, obs_property *,
				    void *data)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	// Reset calibration
	filter->state = TEXT_STATUS + "Reset calibration";
	obs_property_set_description(obs_properties_get(props, SETTING_STATUS),
				     filter->state.c_str());
	// Check if the filter is active
	if (obs_source_enabled(filter->context))
		blog(LOG_INFO, "reset pressed");

	return true;
}

static obs_properties_t *template_match_beep_filter_properties(void *data)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_text(props, SETTING_STATUS, filter->state.c_str(),
				OBS_TEXT_INFO);

	obs_property_t *p = obs_properties_add_int(props, SETTING_DELAY_MS,
						   TEXT_DELAY_MS, 0, 20000, 1);
	obs_property_int_set_suffix(p, " ms");

	obs_property_t *c = obs_properties_add_int(props, SETTING_COOLDOWN_MS,
						   TEXT_COOLDOWN_MS, 0, 20000, 1);
	obs_property_int_set_suffix(c, " ms");

	// obs_properties_add_path
	// obs_properties_add_button

	obs_properties_add_path(props, SETTING_PATH, TEXT_PATH, OBS_PATH_FILE,
				"*.png", NULL);

	obs_properties_add_button(props, "calibrate", "Calibrate",
					  template_match_beep_calibrate);
	obs_properties_add_button(props, "reset_calibration", "Reset Calibration",
				  template_match_beep_reset_calibration);

	return props;
}

static void template_match_beep_filter_remove(void *, obs_source_t *)
{
	blog(LOG_INFO, "filter removed");
}

// https://blat-blatnik.github.io/computerBear/making-accurate-sleep-function/
// ^^ also has windows only implementation for better cpu usage with slightly lower accuracy
void preciseSleep(double seconds)
{
	using namespace std;
	using namespace std::chrono;

	static double estimate = 5e-3;
	static double mean = 5e-3;
	static double m2 = 0;
	static int64_t count = 1;

	while (seconds > estimate) {
		auto start = high_resolution_clock::now();
		this_thread::sleep_for(milliseconds(1));
		auto end = high_resolution_clock::now();

		double observed = (end - start).count() / 1e9;
		seconds -= observed;

		++count;
		double delta = observed - mean;
		mean += delta / count;
		m2 += delta * (observed - mean);
		double stddev = sqrt(m2 / (count - 1));
		estimate = mean + stddev;
	}

	// spin lock
	auto start = high_resolution_clock::now();
	while ((high_resolution_clock::now() - start).count() / 1e9 < seconds)
		;
}

// Thread for template matching and beeping
void thread_loop(void* data) {
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	obs_source_frame *frame = nullptr;

	while (filter->thread_active) {
		if (frame != filter->current_frame && !filter->template_image.empty() && filter->frame_ingest) {
			frame = filter->current_frame;
			cv::UMat umat, umat2;
			filter->frame_ingest->upload(frame, umat);
			// Changing color format, this may become issue
			cv::cvtColor(umat, umat2, cv::COLOR_YUV2BGR, 4);
			// Gray
			cv::cvtColor(umat2, filter->current_cv_frame,
				     cv::COLOR_BGR2GRAY);
			cv::Mat result;
			int result_cols = filter->current_cv_frame.cols -
					  filter->template_image.cols + 1;
			int result_rows = filter->current_cv_frame.rows -
					  filter->template_image.rows + 1;
			result.create(result_rows, result_cols, CV_32FC1);

			cv::matchTemplate(filter->current_cv_frame,
					  filter->template_image, result,
					  cv::TM_CCOEFF_NORMED);

			double minVal, maxVal;
			cv::Point minLoc, maxLoc, matchLoc;
			cv::minMaxLoc(result, &minVal, &maxVal, &minLoc,
				      &maxLoc, cv::Mat());
			matchLoc = maxLoc;

			cv::threshold(result, result, 0.80, 1.0,
				      cv::THRESH_TOZERO);
			// Detected template image!
			if (maxVal > 0.8) {
				cv::rectangle(
					filter->current_cv_frame, matchLoc,
					cv::Point(matchLoc.x +
							  filter->template_image
								  .cols,
						  matchLoc.y +
							  filter->template_image
								  .rows),
					cv::Scalar(255), 2, 8, 0);

				// Detection beep
				// NOTE: the beep is synchronous!
				beep(880, 100);
				preciseSleep(filter->timer * MSEC_TO_SEC);
				// Beep after timer
				beep(440, 100);
				// Cooldown time until next template match
				preciseSleep(filter->cooldown_timer *
					     MSEC_TO_SEC);
			}
		}  else {
			// chill for a bit
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

// Filter gets a frame
static struct obs_source_frame *
template_match_beep_filter_video(void *data, struct obs_source_frame *frame)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	filter->current_frame = frame;

	if (!filter->frame_ingest ||
	    filter->frame_ingest->format() != frame->format)
		filter->frame_ingest = lvk::FrameIngest::Select(frame->format);

	// Add property for debug or somehting
	if (!filter->current_cv_frame.empty())
		cv::imshow("TEST", filter->current_cv_frame);

	return frame;
}

bool obs_module_load(void)
{
	struct obs_source_info template_match_beep_filter = {};
	template_match_beep_filter.id = "template_match_beep_filter";
	template_match_beep_filter.type = OBS_SOURCE_TYPE_FILTER,
	template_match_beep_filter.output_flags = OBS_SOURCE_VIDEO |
						   OBS_SOURCE_ASYNC;
	template_match_beep_filter.get_name = template_match_beep_filter_name;
	template_match_beep_filter.create = template_match_beep_filter_create;
	template_match_beep_filter.destroy =
		template_match_beep_filter_destroy;
	template_match_beep_filter.update = template_match_beep_filter_update,
	template_match_beep_filter.get_properties =
		template_match_beep_filter_properties;
	template_match_beep_filter.filter_video =
		template_match_beep_filter_video;
	template_match_beep_filter.filter_remove =
		template_match_beep_filter_remove;

	obs_register_source(&template_match_beep_filter);
	blog(LOG_INFO, "plugin loaded successfully (version %s)",
	     PLUGIN_VERSION);
	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "plugin unloaded");
}
