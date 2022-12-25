/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2020 Serge Zaitsev
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef BEEP_H
#define BEEP_H

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
/* On Windows use the built-in Beep() function from <utilapiset.h> */
#include <Windows.h>
int beep(int freq, int ms)
{
	return Beep(freq, ms);
}
#elif __linux__
/* On Linux use alsa in synchronous mode, open "default" device in signed 8-bit
 * mode at 8kHz, mono, request for 20ms latency. Device is opened on first call
 * and never closed. */
#include <unistd.h>
#include "vendor/abeep/abeep.h"
#include <alsa/asoundlib.h>

abeep::beep_parms_t *beep_parms = nullptr;

int beep(int freq, int ms)
{
	abeep::init();
	if (beep_parms == nullptr)
		beep_parms = (abeep::beep_parms_t *)malloc(
			sizeof(abeep::beep_parms_t));

	beep_parms->freq = freq;
	beep_parms->length = ms;
	beep_parms->reps = 1;
	beep_parms->delay = 0;
	beep_parms->end_delay = 0;
	beep_parms->next = NULL;

	abeep::play_blocks(beep_parms);
	abeep::cleanup();
	return 0;
}
#elif __APPLE__
#include <AudioUnit/AudioUnit.h>

static dispatch_semaphore_t stopped, playing, done;

static int beep_freq;
static int beep_samples;
static int counter = 0;

static int initialized = 0;
static unsigned char theta = 0;

static OSStatus tone_cb(void *inRefCon,
			AudioUnitRenderActionFlags *ioActionFlags,
			const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
			UInt32 inNumberFrames, AudioBufferList *ioData)
{
	unsigned int frame;
	unsigned char *buf = (unsigned char*)ioData->mBuffers[0].mData;
	unsigned long i = 0;

	for (i = 0; i < inNumberFrames; i++) {
		while (counter == 0) {
			dispatch_semaphore_wait(playing, DISPATCH_TIME_FOREVER);
			counter = beep_samples;
		}
		buf[i] = beep_freq > 0 ? (255 * theta * beep_freq / 8000) : 0;
		theta++;
		counter--;
		if (counter == 0) {
			dispatch_semaphore_signal(done);
			dispatch_semaphore_signal(stopped);
		}
	}
    UNUSED_PARAMETER(inRefCon);
    UNUSED_PARAMETER(ioActionFlags);
    UNUSED_PARAMETER(inTimeStamp);
    UNUSED_PARAMETER(inBusNumber);
    UNUSED_PARAMETER(frame);
	return 0;
}

int beep(int freq, int ms)
{
	if (!initialized) {
		AudioComponent output;
		AudioUnit unit;
		AudioComponentDescription descr;
		AURenderCallbackStruct cb;
		AudioStreamBasicDescription stream;

		initialized = 1;

		stopped = dispatch_semaphore_create(1);
		playing = dispatch_semaphore_create(0);
		done = dispatch_semaphore_create(0);

		descr.componentType = kAudioUnitType_Output,
		descr.componentSubType = kAudioUnitSubType_DefaultOutput,
		descr.componentManufacturer = kAudioUnitManufacturer_Apple,

		cb.inputProc = tone_cb;

		stream.mFormatID = kAudioFormatLinearPCM;
		stream.mFormatFlags = 0;
		stream.mSampleRate = 8000;
		stream.mBitsPerChannel = 8;
		stream.mChannelsPerFrame = 1;
		stream.mFramesPerPacket = 1;
		stream.mBytesPerFrame = 1;
		stream.mBytesPerPacket = 1;

		output = AudioComponentFindNext(NULL, &descr);
		AudioComponentInstanceNew(output, &unit);
		AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback,
				     kAudioUnitScope_Input, 0, &cb, sizeof(cb));
		AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat,
				     kAudioUnitScope_Input, 0, &stream,
				     sizeof(stream));
		AudioUnitInitialize(unit);
		AudioOutputUnitStart(unit);
	}

	dispatch_semaphore_wait(stopped, DISPATCH_TIME_FOREVER);
	beep_freq = freq;
	beep_samples = ms * 8;
	dispatch_semaphore_signal(playing);
	dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
	return 0;
}
#else
#error "unknown platform"
#endif

#endif /* BEEP_H */
