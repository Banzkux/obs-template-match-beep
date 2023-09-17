/*
OBS Template Match Beep
Copyright (C) 2022 - 2023 Janne Pitkänen <acebanzkux@gmail.com>

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

#ifndef AUDIO_H
#define AUDIO_H
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <math.h>
#include <vector>

const int sampleRate = 44100;
const int bitDepth = 16;
const int numberOfChannels = 1;

class SineOscillator {
	float frequency, amplitude, angle = 0.0f, offset = 0.0f;

public:
	SineOscillator(float freq, float amp);
	float process();
};

typedef struct WAVE_HDR {
	/* RIFF Chunk Descriptor */
	uint8_t RIFF[4] = {'R', 'I', 'F', 'F'}; // RIFF Header Magic header
	uint32_t ChunkSize = 0;                 // RIFF Chunk Size
	uint8_t WAVE[4] = {'W', 'A', 'V', 'E'}; // WAVE Header
	/* "fmt" sub-chunk */
	uint8_t fmt[4] = {'f', 'm', 't', ' '}; // FMT header
	uint32_t Subchunk1Size = 16;           // Size of the fmt chunk
	uint16_t AudioFormat = 1;              // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM
					       // Mu-Law, 258=IBM A-Law, 259=ADPCM
	uint16_t NumOfChan = numberOfChannels; // Number of channels 1=Mono 2=Sterio
	uint32_t SamplesPerSec = sampleRate;   // Sampling Frequency in Hz
	uint32_t bytesPerSec = SamplesPerSec * NumOfChan; // bytes per second
	uint16_t blockAlign = bitDepth / 8 * NumOfChan;   // 2=16-bit mono, 4=16-bit stereo
	uint16_t bitsPerSample = bitDepth;                // Number of bits per sample
	/* "data" sub-chunk */
	uint8_t Subchunk2ID[4] = {'d', 'a', 't', 'a'}; // "data"  string
	uint32_t Subchunk2Size = 0;                    // Sampled data length
} wav_hdr;

std::vector<uint8_t> PcmToWave(std::vector<uint16_t> pcm);

std::vector<uint8_t> CreateBeep(float duration, float freq, float amp);

#endif