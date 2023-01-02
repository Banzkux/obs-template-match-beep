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

// Remember to bundle with the opencv binaries!
#ifdef __cplusplus
#undef NO
#undef YES
#include <opencv2/opencv.hpp>
#include "vendor/LiveVisionKit/FrameIngest.hpp"
#endif

#include <obs-module.h>
#include <string>
#include <chrono>
#include <thread>
#include <math.h>

#include "vendor/beep/beep.h"

#ifdef __OBJC__
#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#import <Availability.h>
#endif

#include "template-match-beep.generated.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#ifndef SEC_TO_NSEC
#define SEC_TO_NSEC 1000000000ULL
#endif

#ifndef MSEC_TO_SEC
#define MSEC_TO_SEC 0.001
#endif

#define SETTING_AUTO_ROI "auto_roi"
#define SETTING_DELAY_MS "delay_ms"
#define SETTING_COOLDOWN_MS "cooldown_ms"
#define SETTING_PATH "template_path"
#define SETTING_XYGROUP "xygroup"
#define SETTING_XYGROUP_X1 "xygroup_x1"
#define SETTING_XYGROUP_X2 "xygroup_x2"
#define SETTING_XYGROUP_Y1 "xygroup_y1"
#define SETTING_XYGROUP_Y2 "xygroup_y2"

#define TEXT_AUTO_ROI obs_module_text("Automatic ROI on next detection")
#define TEXT_DELAY_MS obs_module_text("Timer")
#define TEXT_COOLDOWN_MS obs_module_text("Cooldown timer")
#define TEXT_PATH obs_module_text("Template image path")
#define TEXT_XYGROUP obs_module_text("Region of interest")
#define TEXT_XYGROUP_X1 obs_module_text("Top left X")
#define TEXT_XYGROUP_X2 obs_module_text("Bottom right X")
#define TEXT_XYGROUP_Y1 obs_module_text("Top left Y")
#define TEXT_XYGROUP_Y2 obs_module_text("Bottom right Y")

struct template_match_beep_data {
	obs_source_t *context;

	obs_data_t *settings;

	obs_source_frame *current_frame;
	cv::UMat current_cv_frame;

	std::thread thread;
	bool thread_active;

	// LiveVisionKit, OBS Frame -> OpenCV frame
	std::unique_ptr<lvk::FrameIngest> frame_ingest;

	// Plugin settings
	uint64_t timer;
	uint64_t cooldown_timer;
	const char *path;

	cv::Mat template_image;

	cv::Rect roi;
	bool auto_roi;
	int xygroup_x1, xygroup_y1;
	int xygroup_x2, xygroup_y2;
};

static const char *template_match_beep_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Template Match Timer");
}

// Filters settings were updated
static void template_match_beep_filter_update(void *data, obs_data_t *settings)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;
	uint64_t new_timer =
		(uint64_t)obs_data_get_int(settings, SETTING_DELAY_MS);

	uint64_t new_cooldown =
		(uint64_t)obs_data_get_int(settings, SETTING_COOLDOWN_MS);

	const char *new_path =
		(const char *)obs_data_get_string(settings, SETTING_PATH);

	filter->xygroup_x1 =
		(int)obs_data_get_int(settings, SETTING_XYGROUP_X1);
	filter->xygroup_y1 =
		(int)obs_data_get_int(settings, SETTING_XYGROUP_Y1);
	filter->xygroup_x2 =
		(int)obs_data_get_int(settings, SETTING_XYGROUP_X2);
	filter->xygroup_y2 =
		(int)obs_data_get_int(settings, SETTING_XYGROUP_Y2);

	filter->roi =
		cv::Rect(cv::Point(filter->xygroup_x1, filter->xygroup_y1),
			 cv::Point(filter->xygroup_x2, filter->xygroup_y2));

	filter->auto_roi = obs_data_get_bool(settings, SETTING_AUTO_ROI);

	filter->timer = new_timer;
	filter->cooldown_timer = new_cooldown;
	// Update template image on new path
	if (filter->path != new_path) {
		filter->path = new_path;
		filter->template_image = cv::imread(new_path, 0);
	}
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
	filter->thread_active = true;
	filter->thread = std::thread(thread_loop, (void *)filter);
	filter->settings = settings;
	return filter;
}

static void template_match_beep_filter_destroy(void *data)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	filter->thread_active = false;
	bfree(data);
}

static obs_properties_t *template_match_beep_filter_properties(void *data)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	obs_properties_t *props = obs_properties_create();

	obs_property_t *p = obs_properties_add_int(props, SETTING_DELAY_MS,
						   TEXT_DELAY_MS, 100, INT_MAX, 1);
	obs_property_int_set_suffix(p, " ms");

	obs_property_t *c = obs_properties_add_int(
		props, SETTING_COOLDOWN_MS, TEXT_COOLDOWN_MS, 100, INT_MAX, 1);
	obs_property_int_set_suffix(c, " ms");

	// Template Image path
	obs_properties_add_path(props, SETTING_PATH, TEXT_PATH, OBS_PATH_FILE,
				"*.png", NULL);

	// Region of interest setting group
	obs_properties_t *xygroup = obs_properties_create();
	obs_properties_add_group(props, SETTING_XYGROUP, TEXT_XYGROUP,
				 OBS_GROUP_CHECKABLE, xygroup);

	obs_properties_add_bool(xygroup, SETTING_AUTO_ROI, TEXT_AUTO_ROI);
	// Default limits for ROI
	int width = 640;
	int height = 480;
	// Add limits from frame if it exist
	if (filter->current_frame) {
		width = filter->current_frame->width;
		height = filter->current_frame->height;
	}
	obs_properties_add_int(xygroup, SETTING_XYGROUP_X1, TEXT_XYGROUP_X1, 0,
			       width, 1);
	obs_properties_add_int(xygroup, SETTING_XYGROUP_Y1, TEXT_XYGROUP_Y1, 0,
			       height, 1);

	obs_properties_add_int(xygroup, SETTING_XYGROUP_X2, TEXT_XYGROUP_X2, 0,
			       width, 1);
	obs_properties_add_int(xygroup, SETTING_XYGROUP_Y2, TEXT_XYGROUP_Y2, 0,
			       height, 1);

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
void thread_loop(void *data)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)data;

	obs_source_frame *frame = nullptr;

	while (filter->thread_active) {
		if (frame != filter->current_frame &&
		    !filter->template_image.empty() && filter->frame_ingest) {
			frame = filter->current_frame;
			cv::UMat umatroi, umat, umat2, umat3;
			filter->frame_ingest->upload(frame, umat);
			if (!filter->roi.empty() && !filter->auto_roi &&
			    (filter->roi.width >= filter->template_image.cols &&
			     filter->roi.height >=
				     filter->template_image.rows)) {
				umat(filter->roi).copyTo(umat);
			}
			// Changing color format, this may become issue
			cv::cvtColor(umat, umat2, cv::COLOR_YUV2BGR, 4);

			// Gray
			cv::cvtColor(umat2, umat3, cv::COLOR_BGR2GRAY);
			cv::Mat result;
			int result_cols =
				umat3.cols - filter->template_image.cols + 1;
			int result_rows =
				umat3.rows - filter->template_image.rows + 1;
			result.create(result_rows, result_cols, CV_32FC1);

			cv::matchTemplate(umat3, filter->template_image, result,
					  cv::TM_CCOEFF_NORMED);

			double minVal, maxVal;
			cv::Point minLoc, maxLoc, matchLoc;
			cv::minMaxLoc(result, &minVal, &maxVal, &minLoc,
				      &maxLoc, cv::Mat());
			matchLoc = maxLoc;

			cv::threshold(result, result, 0.80, 1.0,
				      cv::THRESH_TOZERO);
			umat3.copyTo(filter->current_cv_frame);
			// Detected template image!
			if (maxVal > 0.8) {
				filter->xygroup_x1 = matchLoc.x;
				filter->xygroup_y1 = matchLoc.y;
				filter->xygroup_x2 =
					matchLoc.x +
					filter->template_image.cols;
				filter->xygroup_y2 =
					matchLoc.y +
					filter->template_image.rows;
				cv::rectangle(umat3, matchLoc,
					      cv::Point(filter->xygroup_x2,
							filter->xygroup_y2),
					      cv::Scalar(255), 2, 8, 0);
				if (filter->auto_roi) {
					obs_data_set_bool(filter->settings,
							  SETTING_AUTO_ROI,
							  false);
					obs_data_set_int(filter->settings,
							 SETTING_XYGROUP_X1,
							 matchLoc.x);
					obs_data_set_int(filter->settings,
							 SETTING_XYGROUP_Y1,
							 matchLoc.y);
					obs_data_set_int(filter->settings,
							 SETTING_XYGROUP_X2,
							 filter->xygroup_x2);
					obs_data_set_int(filter->settings,
							 SETTING_XYGROUP_Y2,
							 filter->xygroup_y2);
					filter->auto_roi = false;

					filter->roi = cv::Rect(
						matchLoc,
						cv::Point(
							matchLoc.x +
								filter->template_image
									.cols,
							matchLoc.y +
								filter->template_image
									.rows));
				}

				umat3.copyTo(filter->current_cv_frame);
				// Detection beep
				// NOTE: the beep is synchronous!
				beep(880, 100);
				preciseSleep((filter->timer - 100) * MSEC_TO_SEC);
				// Beep after timer
				beep(440, 100);
				// Cooldown time until next template match
				preciseSleep((filter->cooldown_timer - 100) *
					     MSEC_TO_SEC);
					     
			}
		} else {
			// chill for a bit
			std::this_thread::sleep_for(
				std::chrono::milliseconds(1));
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
	template_match_beep_filter.destroy = template_match_beep_filter_destroy;
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
