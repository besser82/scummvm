/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef ULTIMA8_AUDIO_RAWAUDIOSAMPLE_H
#define ULTIMA8_AUDIO_RAWAUDIOSAMPLE_H

#include "ultima8/audio/audio_sample.h"

namespace Ultima8 {
namespace Pentagram {

class RawAudioSample : public AudioSample {
public:
	RawAudioSample(uint8 *buffer, uint32 size,
	               uint32 rate, bool signeddata, bool stereo);
	virtual ~RawAudioSample();

	virtual void initDecompressor(void *DecompData) const;
	virtual uint32 decompressFrame(void *DecompData, void *samples) const;
	virtual void rewind(void *DecompData) const;

protected:

	struct RawDecompData {
		uint32 pos;
	};

	bool signeddata;

};

} // End of namespace Pentagram
} // End of namespace Ultima8

#endif
