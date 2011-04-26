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
 * $URL$
 * $Id$
 */

#ifndef SOUND_MP3_HEADERS_H
#define SOUND_MP3_HEADERS_H

#include "common/scummsys.h"
#include "common/file.h"
#include "tool.h"

namespace Audio {

class Mp3Exception : public ToolException {
public:
	Mp3Exception(std::string error, int retcode = -1) : ToolException(error, retcode) {}
};

class Mp3HeaderReader {
public:
	enum { 
		FRAME_SYNC = 0x7FF,
		HEADER_SIZE = 4
	};

protected:
	enum Mp3Layer {
		kMp3LayerReserved = 0,
		kMp3Layer3 = 1,
		kMp3Layer2 = 2,
		kMp3Layer1 = 3
	};

	enum Mp3Version {
		kMp3Version25 = 0,
		kMp3VersionReserved = 1,
		kMp3Version2 = 2,
		kMp3Version1 = 3
	};

	enum Mp3Channels {
		kMp3Stereo = 0,
		kMp3JointStereo = 1,
		kMp3DualMonoChannels = 2,
		kMp3Mono = 3
	};
		
	enum {
		FRAME_SYNC_VAL = 0x7FF,
		FRAME_SYNC_SHIFT = 21,
		FRAME_SYNC_BITS = 11,
		VERSION_SHIFT = 19,
		VERSION_BITS = 2,
		LAYER_SHIFT = 17,
		LAYER_BITS = 2,
		PROTECTION_SHIFT = 16,
		PROTECTION_BITS = 1,
		BITRATE_INDEX_SHIFT = 12,
		BITRATE_INDEX_BITS = 4,
		SAMPLE_RATE_IDX_SHIFT = 10,
		SAMPLE_RATE_IDX_BITS = 2,
		PADDING_SHIFT = 9,
		PADDING_BITS = 1,
		PRIVATE_SHIFT = 8,
		PRIVATE_BITS = 1,
		CHANNEL_MODE_SHIFT = 6,
		CHANNEL_MODE_BITS = 2,
		MODE_EX_SHIFT = 4,
		MODE_EX_BITS = 2,
		COPYRIGHT_SHIFT = 3,
		COPYRIGHT_BITS = 1,
		ORIGINAL_SHIFT = 2,
		ORIGINAL_BITS = 1,
		EMPHASIS_SHIFT = 0,
		EMPHASIS_BITS = 2
	};

#define MASK(x)		((1 << x) - 1)
#define GETVAL(x)	((_header >> x ##_SHIFT) & MASK(x ##_BITS))

public:	
	Mp3HeaderReader(Common::File &stream);

	uint32 getSamplesPerFrame() const { return _samplesPerFrame; }
	uint32 getSamplingRate() const { return _samplingRate; }
	uint32 getBitRate() const { return _bitRate; }
	uint32 getFrameSize() const { return _frameSize; }

	// Get max size of frame possible for this stream
	uint32 getMaxFrameSize();
	
	// Load the header from the stream
	bool load();

private:

	// Calculate all the values in the header
	bool calcValues();
	
	// Functions to calculate different aspects
	void calcSamplesPerFrame();
	void calcSamplingRate();
	uint32 calcBitRate(uint32 bitrateIndex);
	uint32 calcFrameSizeForBitrate(uint32 bitrate, bool padding); 
	void calcFrameSize();

	uint32 getMaxBitrate();
	
	uint32 getSlotSizeInBytes() {
		if (GETVAL(LAYER) == kMp3Layer1)
			return 4;
		return 1;
	}

	uint32 _header;
	Common::File *_stream;
	uint32 _frameSize;
	uint32 _samplingRate;
	uint32 _samplesPerFrame;
	uint32 _bitRate;
	uint32 _offset;		// offset from where we expected the header
	bool _badFrame;		// if we've come to the conclusion the frame is bad
};

/* VBRI Header ------------------------------------------------- */


class VbriHeaderReader {
protected:
	enum { 
		START_OFFSET = 32
	};
#include "common/pack-start.h"
	struct Header {
		byte id[4];
		uint16 versionId;
		uint16 delay;					// in num of samples
		uint16 quality;
		uint32 numOfBytes;
		uint32 numOfFrames;
		uint16 numOfTableEntries;
		uint16 scaleFactor;				// multiply entry value with this
		uint16 tableEntrySizeInBytes;	// bytes per entry
		uint16 framesPerTableEntry;		// frames per entry
	};
#include "common/pack-end.h"

	uint32 *_table;
	Header _header;
	Mp3HeaderReader *_mp3Header;
	Common::File *_stream;
	float _totalPlayTime;
	float _timePerTableEntry;
		
public:	
	VbriHeaderReader(Common::File &stream, Mp3HeaderReader &mp3Header) : 
		_table(0), _mp3Header(&mp3Header), 
		_stream(&stream), _totalPlayTime(0) {}
	~VbriHeaderReader() { delete _table; }
	
	bool load();
	void getTotalPlayTime(float &t) { t = _totalPlayTime; }
	bool getClosestOffsetBefore(const float &inTime, uint32 &offset, float &offsetTime);  // returns the nearest time and offset
	void calculateTimes();
	static uint32 getMaxTableSize(uint32 frameSize) {
		return frameSize - START_OFFSET + 1 - sizeof(Header);
	}
};	

} // End of namespace Audio

#endif /* SOUND_MP3_HEADERS_H */
