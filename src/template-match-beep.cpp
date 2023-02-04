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
#include <QFileDialog>
#include <QStandardPaths>
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
#include "CustomBeepSettings.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#ifndef SEC_TO_NSEC
#define SEC_TO_NSEC 1000000000ULL
#endif

#ifndef MSEC_TO_SEC
#define MSEC_TO_SEC 0.001
#endif

#define SETTING_AUTO_ROI "auto_roi"
#define SETTING_COOLDOWN_MS "cooldown_ms"
#define SETTING_PATH "template_path"
#define SETTING_SAVE_FRAME "save_frame"
#define SETTING_BEEP_SETTINGS "beep_settings"
#define SETTING_DBUG_VIEW "debug_view"
#define SETTING_XYGROUP "xygroup"
#define SETTING_XYGROUP_X1 "xygroup_x1"
#define SETTING_XYGROUP_X2 "xygroup_x2"
#define SETTING_XYGROUP_Y1 "xygroup_y1"
#define SETTING_XYGROUP_Y2 "xygroup_y2"

#define TEXT_AUTO_ROI obs_module_text("Automatic ROI on next detection")
#define TEXT_COOLDOWN_MS obs_module_text("Cooldown timer")
#define TEXT_PATH obs_module_text("Template image path")
#define TEXT_SAVE_FRAME obs_module_text("Save frame")
#define TEXT_BEEP_SETTINGS obs_module_text("Beep settings")
#define TEXT_DBUG_VIEW obs_module_text("Debug view")
#define TEXT_XYGROUP obs_module_text("Region of interest")
#define TEXT_XYGROUP_X1 obs_module_text("Top left X")
#define TEXT_XYGROUP_X2 obs_module_text("Bottom right X")
#define TEXT_XYGROUP_Y1 obs_module_text("Top left Y")
#define TEXT_XYGROUP_Y2 obs_module_text("Bottom right Y")

struct template_match_beep_data {
	obs_source_t *context;
	obs_source_t *source;

	obs_data_t *settings;

	obs_source_frame *current_frame;
	cv::UMat current_cv_frame;

	std::thread thread;
	bool thread_active;
	// Check if we need to destroy the debug window
	bool debug_view_active;

	// LiveVisionKit, OBS Frame -> OpenCV frame
	std::unique_ptr<lvk::FrameIngest> frame_ingest;

	// Plugin settings
	uint64_t cooldown_timer;
	const char *path;
	bool debug_view;

	cv::Mat template_image;

	cv::Rect roi;
	bool auto_roi;
	int xygroup_x1, xygroup_y1;
	int xygroup_x2, xygroup_y2;

	CustomBeepSettings *custom_settings;

	signal_handler_t *signal_handler;
};

static const char *template_match_beep_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Template Match Timer");
}

// Filters settings were updated
static void template_match_beep_filter_update(void *data, obs_data_t *settings)
{
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	uint64_t new_cooldown = (uint64_t)obs_data_get_int(settings, SETTING_COOLDOWN_MS);

	const char *new_path = (const char *)obs_data_get_string(settings, SETTING_PATH);

	bool new_view = (bool)obs_data_get_bool(settings, SETTING_DBUG_VIEW);

	if (filter->custom_settings == nullptr)
		filter->custom_settings = new CustomBeepSettings();

	filter->custom_settings->CreateOBSSettings(settings);

	// ROI group
	if (obs_data_get_bool(settings, SETTING_XYGROUP)) {
		filter->xygroup_x1 = (int)obs_data_get_int(settings, SETTING_XYGROUP_X1);
		filter->xygroup_y1 = (int)obs_data_get_int(settings, SETTING_XYGROUP_Y1);
		filter->xygroup_x2 = (int)obs_data_get_int(settings, SETTING_XYGROUP_X2);
		filter->xygroup_y2 = (int)obs_data_get_int(settings, SETTING_XYGROUP_Y2);
		filter->auto_roi = obs_data_get_bool(settings, SETTING_AUTO_ROI);
	} else {
		filter->xygroup_x1 = 0;
		filter->xygroup_y1 = 0;
		filter->xygroup_x2 = 0;
		filter->xygroup_y2 = 0;
		filter->auto_roi = false;
	}

	filter->roi = cv::Rect(cv::Point(filter->xygroup_x1, filter->xygroup_y1),
			       cv::Point(filter->xygroup_x2, filter->xygroup_y2));

	filter->cooldown_timer = new_cooldown;
	// Update template image on new path
	if (filter->path != new_path) {
		filter->path = new_path;
		filter->template_image = cv::imread(new_path, 0);
	}
	if (filter->debug_view != new_view) {
		filter->debug_view = new_view;
		if (!new_view && filter->debug_view_active) {
			cv::destroyWindow(SETTING_DBUG_VIEW);
			filter->debug_view_active = false;
		}
	}
}

void thread_loop(void *data);

void start_thread(void *data)
{
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	if (filter->thread_active || !obs_source_enabled(filter->context))
		return;

	filter->thread_active = true;

	filter->thread = std::thread(thread_loop, (void *)filter);
}

void end_thread(void *data)
{
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	if (!filter->thread_active)
		return;

	filter->thread_active = false;

	filter->thread.join();
	filter->current_frame = nullptr;
}

static void template_match_beep_filter_enabled(void *data, calldata_t *calldata)
{
	bool enabled = calldata_bool(calldata, "enabled");

	if (enabled)
		start_thread(data);
	else
		end_thread(data);
}

static void *template_match_beep_filter_create(obs_data_t *settings, obs_source_t *context)
{
	struct template_match_beep_data *filter =
		(template_match_beep_data *)bzalloc(sizeof(*filter));

	filter->context = context;
	template_match_beep_filter_update(filter, settings);
	filter->current_frame = nullptr;

	start_thread((void *)filter);

	filter->source = nullptr;

	filter->settings = settings;
	filter->debug_view_active = false;

	filter->signal_handler = obs_source_get_signal_handler(context);
	signal_handler_connect(filter->signal_handler, "enable", template_match_beep_filter_enabled,
			       filter);

	return filter;
}

static void template_match_beep_filter_destroy(void *data)
{
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	signal_handler_disconnect(filter->signal_handler, "enable",
				  template_match_beep_filter_enabled, filter);

	end_thread(data);
	bfree(data);
}

bool template_match_beep_save_frame(obs_properties_t *props, obs_property_t *property, void *data)
{
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	cv::UMat umat, umat2;
	filter->frame_ingest->upload(filter->current_frame, umat);
	// Changing color format, this may become issue
	cv::cvtColor(umat, umat2, cv::COLOR_YUV2BGR, 4);
	// Save image dialog
	QString filename = QFileDialog::getSaveFileName(
		nullptr, "Save frame",
		QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), "*.png");

	if (!filename.isNull())
		cv::imwrite(filename.toStdString(), umat2);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return true;
}

bool template_match_beep_settings(obs_properties_t *props, obs_property_t *property, void *data)
{
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	filter->custom_settings->ShowSettingsWindow();

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return true;
}

static obs_properties_t *template_match_beep_filter_properties(void *data)
{
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	obs_properties_t *props = obs_properties_create();

	obs_property_t *c = obs_properties_add_int(props, SETTING_COOLDOWN_MS, TEXT_COOLDOWN_MS,
						   100, INT_MAX, 1);
	obs_property_int_set_suffix(c, " ms");

	// Template Image path
	obs_properties_add_path(props, SETTING_PATH, TEXT_PATH, OBS_PATH_FILE, "*.png", NULL);

	obs_properties_add_button(props, SETTING_SAVE_FRAME, TEXT_SAVE_FRAME,
				  template_match_beep_save_frame);

	obs_properties_add_button(props, SETTING_BEEP_SETTINGS, TEXT_BEEP_SETTINGS,
				  template_match_beep_settings);

	obs_properties_add_bool(props, SETTING_DBUG_VIEW, TEXT_DBUG_VIEW);

	// Region of interest setting group
	obs_properties_t *xygroup = obs_properties_create();
	obs_properties_add_group(props, SETTING_XYGROUP, TEXT_XYGROUP, OBS_GROUP_CHECKABLE,
				 xygroup);

	obs_properties_add_bool(xygroup, SETTING_AUTO_ROI, TEXT_AUTO_ROI);
	// Default limits for ROI
	int width = 640;
	int height = 480;
	// Add limits from frame if it exist
	if (filter->current_frame) {
		width = filter->current_frame->width;
		height = filter->current_frame->height;
	}
	obs_properties_add_int(xygroup, SETTING_XYGROUP_X1, TEXT_XYGROUP_X1, 0, width, 1);
	obs_properties_add_int(xygroup, SETTING_XYGROUP_Y1, TEXT_XYGROUP_Y1, 0, height, 1);

	obs_properties_add_int(xygroup, SETTING_XYGROUP_X2, TEXT_XYGROUP_X2, 0, width, 1);
	obs_properties_add_int(xygroup, SETTING_XYGROUP_Y2, TEXT_XYGROUP_Y2, 0, height, 1);

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
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	uint64_t frame_ts = 0;

	while (filter->thread_active) {
		// Fixes crashes on media source when enabling/disabling the source
		if (!obs_source_active(filter->source))
			filter->current_frame = nullptr;

		if (filter->current_frame != nullptr &&
		    frame_ts != filter->current_frame->timestamp &&
		    !filter->template_image.empty() && filter->frame_ingest) {
			frame_ts = filter->current_frame->timestamp;
			cv::UMat umatroi, umat, umat2, umat3;
			filter->frame_ingest->upload(filter->current_frame, umat);
			if (!filter->roi.empty() && !filter->auto_roi &&
			    (filter->roi.width >= filter->template_image.cols &&
			     filter->roi.height >= filter->template_image.rows)) {
				umat(filter->roi).copyTo(umat);
			}
			// Changing color format, this may become issue
			cv::cvtColor(umat, umat2, cv::COLOR_YUV2BGR, 4);

			// Gray
			cv::cvtColor(umat2, umat3, cv::COLOR_BGR2GRAY);
			cv::Mat result;
			int result_cols = umat3.cols - filter->template_image.cols + 1;
			int result_rows = umat3.rows - filter->template_image.rows + 1;
			result.create(result_rows, result_cols, CV_32FC1);

			cv::matchTemplate(umat3, filter->template_image, result,
					  cv::TM_CCOEFF_NORMED);

			double minVal, maxVal;
			cv::Point minLoc, maxLoc, matchLoc;
			cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc, cv::Mat());
			matchLoc = maxLoc;

			cv::threshold(result, result, 0.80, 1.0, cv::THRESH_TOZERO);
			umat3.copyTo(filter->current_cv_frame);
			// Detected template image!
			if (maxVal > 0.8) {
				filter->xygroup_x1 = matchLoc.x;
				filter->xygroup_y1 = matchLoc.y;
				filter->xygroup_x2 = matchLoc.x + filter->template_image.cols;
				filter->xygroup_y2 = matchLoc.y + filter->template_image.rows;
				cv::rectangle(umat3, matchLoc,
					      cv::Point(filter->xygroup_x2, filter->xygroup_y2),
					      cv::Scalar(255), 2, 8, 0);
				if (filter->auto_roi) {
					obs_data_set_bool(filter->settings, SETTING_AUTO_ROI,
							  false);
					obs_data_set_int(filter->settings, SETTING_XYGROUP_X1,
							 matchLoc.x);
					obs_data_set_int(filter->settings, SETTING_XYGROUP_Y1,
							 matchLoc.y);
					obs_data_set_int(filter->settings, SETTING_XYGROUP_X2,
							 filter->xygroup_x2);
					obs_data_set_int(filter->settings, SETTING_XYGROUP_Y2,
							 filter->xygroup_y2);
					filter->auto_roi = false;

					filter->roi = cv::Rect(
						matchLoc,
						cv::Point(matchLoc.x + filter->template_image.cols,
							  matchLoc.y +
								  filter->template_image.rows));
				}

				umat3.copyTo(filter->current_cv_frame);

				// NOTE: the beep is synchronous!
				// Beep and wait according to settings
				for (Event e : filter->custom_settings->GetEvents()) {
					if (e.type == EventType::Beep) {
						beep(e.frequency, e.length);
					} else {
						preciseSleep(e.length * MSEC_TO_SEC);
					}
				}
				// Cooldown time until next template match
				preciseSleep(filter->cooldown_timer * MSEC_TO_SEC);
			}
		} else {
			// chill for a bit
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

// Filter gets a frame
static struct obs_source_frame *template_match_beep_filter_video(void *data,
								 struct obs_source_frame *frame)
{
	struct template_match_beep_data *filter = (template_match_beep_data *)data;

	if (filter->source == nullptr)
		filter->source = obs_filter_get_parent(filter->context);

	if (filter->frame_ingest && lvk::FrameIngest::test_obs_frame(frame))
		filter->current_frame = frame;

	if (!filter->frame_ingest || filter->frame_ingest->format() != frame->format)
		filter->frame_ingest = lvk::FrameIngest::Select(frame->format);

	// Debug view
	if (!filter->current_cv_frame.empty() && filter->debug_view) {
		cv::imshow(SETTING_DBUG_VIEW, filter->current_cv_frame);
		if (!filter->debug_view_active)
			filter->debug_view_active = true;
	}

	return frame;
}

// De/activate are for the parent source (i.e. capture card source) so not the filter it self.
static void template_match_beep_filter_activate(void *data)
{
	start_thread(data);
}

static void template_match_beep_filter_deactivate(void *data)
{
	end_thread(data);
}

bool obs_module_load(void)
{
	struct obs_source_info template_match_beep_filter = {};
	template_match_beep_filter.id = "template_match_beep_filter";
	template_match_beep_filter.type = OBS_SOURCE_TYPE_FILTER,
	template_match_beep_filter.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC;
	template_match_beep_filter.get_name = template_match_beep_filter_name;
	template_match_beep_filter.create = template_match_beep_filter_create;
	template_match_beep_filter.destroy = template_match_beep_filter_destroy;
	template_match_beep_filter.update = template_match_beep_filter_update,
	template_match_beep_filter.get_properties = template_match_beep_filter_properties;
	template_match_beep_filter.filter_video = template_match_beep_filter_video;
	template_match_beep_filter.filter_remove = template_match_beep_filter_remove;
	template_match_beep_filter.activate = template_match_beep_filter_activate;
	template_match_beep_filter.deactivate = template_match_beep_filter_deactivate;

	obs_register_source(&template_match_beep_filter);
	blog(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "plugin unloaded");
}
