# OBS Template Match Beep
## Introduction
**OBS Studio 28 or higher required.**

This plugin is an OBS filter which does the following:
- Template matches each frame
- If match is found:
    - Waits and beeps as many times as user has set it to
    - Waits user selected cooldown to ran out, so we avoid multiple detections at once
- Debug view
- Automatic and manual region of interest

## Installation

### Windows
Executable installer is the easy way to install, just make sure that the installation path is the same as OBS installation path. With the zip file, you just extract the files to OBS installation folder and the files go to correct locations.

## Usage
1. Apply Audio/Video filter to your source.
2. Set beep settings by clicking 'Beep settings'.
3. Set cooldown, so you don't get multiple detections at once.
4. If you don't have template image ready, create template image by screenshotting it using Save frame button or by right clicking the OBS source -> Screenshot (Source), then make unneeded pixels transparent using image editor of your choice and save as .png.
5. Set the template image path (NOTE: .png only).
6. If the template image appears at same position always, enable Automatic ROI.

If the template image has an area in which it appears (not same position always) then select the region of interest manually (Debug view shows the area).

## Dependencies
- [OpenCV](https://github.com/opencv/opencv) 4.6.0, used components: core, highgui, imgcodecs, imgproc
- [LiveVisionKit](https://github.com/Crowsinc/LiveVisionKit) (Conversion from obs_source_frame -> OpenCV UMat, FrameIngest class)
- [abeep](https://github.com/Hawk777/abeep) beep for linux using [ALSA](https://github.com/alsa-project).
- [beep](https://github.com/zserge/beep) base for cross-platform beep file.
- [Qt](https://www.qt.io/) 6 for the settings UI (https://github.com/obsproject/obs-deps)
- [OBS](https://github.com/obsproject/obs-studio)