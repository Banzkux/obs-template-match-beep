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

#include "audio.h"

SineOscillator::SineOscillator(float freq, float amp) : frequency(freq), amplitude(amp)
{
	offset = 2 * (float)M_PI * frequency / sampleRate;
}

float SineOscillator::process() {
	auto sample = amplitude * sin(angle);
	angle += offset;
	return sample;
}

std::vector<uint8_t> PcmToWave(std::vector<uint16_t> pcm)
{
	static_assert(sizeof(wav_hdr) == 44, "");

	uint32_t fsize = (uint32_t)pcm.size() * 2;

	wav_hdr wav;
	wav.ChunkSize = fsize + sizeof(wav_hdr) - 8;
	wav.Subchunk2Size = fsize + sizeof(wav_hdr) - 44;

	std::vector<uint8_t> output;
	output.resize(sizeof(wav) + pcm.size() * sizeof(pcm[0]));
	memcpy(output.data(), &wav, sizeof(wav));
	memcpy(output.data() + sizeof(wav), pcm.data(), pcm.size() * sizeof(pcm[0]));
	return output;
}

std::map<std::pair<float, float>, std::vector<uint8_t>> beepCache;

std::vector<uint8_t> CreateBeep(float duration, float freq, float amp)
{
	if (auto search = beepCache.find(std::make_pair(duration, freq));
	    search != beepCache.end()) {
		return search->second;
	}

	SineOscillator sineOscillator(freq, amp);
	auto maxAmplitude = pow(2, bitDepth - 1) - 1;
	std::vector<uint16_t> input;
	for (int i = 0; i < sampleRate * duration; i++) {
		auto sample = sineOscillator.process();
		input.push_back(static_cast<uint16_t>(sample * maxAmplitude));
	}
	auto result = PcmToWave(input);
	beepCache[std::make_pair(duration, freq)] = result;
	return result;
}