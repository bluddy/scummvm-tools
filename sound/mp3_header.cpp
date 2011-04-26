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

#include "sound/mp3_header.h"
#include "common/endian.h"
#include <iostream>

namespace Audio {

static const short mp3BitrateTableVer1[][15] = { // For version 1
	{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 }, // Layer1
	{ 0, 32, 48, 56, 64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 384 }, // Layer2
	{ 0, 32, 40, 48, 56,  64,  80,  96,  112, 128, 160, 192, 224, 256, 320 }
	};
static const short mp3BitrateTableVer2[][15] = { // For version 2/2.5
	{ 0, 32, 48, 56, 64,  80,  96,  112, 128, 144, 160, 176, 192, 224, 256 }, // Layer 1
	{ 0, 8,  16, 24, 32,  40,  48,  56,  64,  80,  96,  112, 128, 144, 160 }  // Layer 2/3
	};

Mp3HeaderReader::Mp3HeaderReader(Common::File &stream) : 
	_header(0), _stream(&stream), 
	_frameSize(0), _samplingRate(0), 
	_samplesPerFrame(0), _bitRate(0),
	_offset(0), _badFrame(false) {}

bool Mp3HeaderReader::load() {
	assert(_stream);
	_offset = 0;
	_badFrame = false;
	_header = 0;

	//std::cout << "1st at " << std::hex << _stream->pos() << std::endl;

	try {
		while (true) {
			_header = _stream->readUint32BE();

			//std::cout << "at " << std::hex << _stream->pos() << std::endl; //debug

			if (GETVAL(FRAME_SYNC) == FRAME_SYNC_VAL) {
				_stream->seek(-4, SEEK_CUR);	// return to start of header 
				//std::cout << "Found good header[" << std::hex << _header << "] at " << _stream->pos() 
				//		<< std::dec << std::endl; //debug
				calcValues();
				if (_offset) {
					std::cout << "Header found at offset of " << _offset << " from expected point @"
						<< std::hex << _stream->pos() << std::dec << std::endl;
				}
				return true;
			} else {
				_stream->seek(-3, SEEK_CUR);
				_offset++;
			}
		}
	}
	catch (Common::FileException e) {
		if (!_stream->eos()) // check for EOS
			throw e;
	}
	return false;		// didn't find a header
}				

// Called upon init to read the MP3 values
// 
bool Mp3HeaderReader::calcValues() {
	calcSamplesPerFrame();
	//std::cout << "Header " << std::hex << _header << std::dec << std::endl;
	//std::cout << "Version bits" << GETVAL(VERSION) << " Layer bits" << GETVAL(LAYER) << std::endl;
	//std::cout << "SamplesPerFrame " << _samplesPerFrame << "\n";
	calcSamplingRate();
	//std::cout << "SamplingRate " << _samplingRate << "\n";
	_bitRate = calcBitRate(GETVAL(BITRATE_INDEX));
	//std::cout << "Bitrate " << _bitRate << std::endl;
	_frameSize = calcFrameSizeForBitrate(getBitRate(), GETVAL(PADDING));
	//std::cout << "Frame Size " << _frameSize << std::endl;
	return _badFrame;
}

void Mp3HeaderReader::calcSamplesPerFrame() {
	uint32 samplesPerFrame = 0;
	
	switch (GETVAL(LAYER)) {
	case kMp3Layer1:
		samplesPerFrame = 384;
		break;
	case kMp3Layer2:
		samplesPerFrame = 1152;
		break;
	case kMp3Layer3:
		if (GETVAL(VERSION) == kMp3Version1)
			samplesPerFrame = 1152;
		else
			samplesPerFrame = 576;
		break;
	}
	_samplesPerFrame = samplesPerFrame;
}

void Mp3HeaderReader::calcSamplingRate() {
	uint32 rate = 0;
	//std::cout << "samplingRateIndex " << GETVAL(SAMPLE_RATE_IDX) << "\n";
	
	switch (GETVAL(SAMPLE_RATE_IDX)) {
	case 0:
		rate = 44100;
		break;
	case 1:
		rate = 48000;
		break;
	case 2:
		rate = 32000;
		break;
	}
	if (GETVAL(VERSION) == kMp3Version2)
		rate >>= 1;		// 1/2
	else if (GETVAL(VERSION) == kMp3Version25)
		rate >>= 2;		// 1/4
	_samplingRate = rate;
}

uint32 Mp3HeaderReader::calcBitRate(uint32 bitrateIndex) {
	uint32 layer;

	if (bitrateIndex >= 15) {
		_badFrame = true;
		return 0;
	}

	switch (GETVAL(LAYER)) {
		case kMp3Layer1:
		layer = 0;
		break;
		case kMp3Layer2:
		layer = 1;
		break;
		case kMp3Layer3:
		layer = 2;
		break;
	}
	const short (*table)[15];		
	if (GETVAL(VERSION) == kMp3Version1)
		table = mp3BitrateTableVer1;
	else {	// version 2/2.5
		table = mp3BitrateTableVer2;
		if (layer == 2)
			layer = 1;				// combined table for this layer 
	}
	return 1000 * table[layer][bitrateIndex];
}

uint32 Mp3HeaderReader::calcFrameSizeForBitrate(uint32 bitrate, bool padding) {
	if (!getBitRate() || !getSamplingRate() || _badFrame)
		return 0;

	uint32 constant;
	
	if (GETVAL(LAYER) == kMp3Layer1)
		constant = 12;
	else if (GETVAL(LAYER) == kMp3Layer3 && GETVAL(VERSION) == kMp3Version2)
		// Undocumented constant needed for this case becase MPEG2 Layer 3 has
		// only 576 samples, not 1152
		constant = 72;
	else
		constant = 144;
	
	uint32 numOfSlots = (constant * getBitRate()) / getSamplingRate();
	
	if (padding)
		numOfSlots++;
		
	return (numOfSlots * getSlotSizeInBytes());// + sizeof(_header);
}

// Give us the size of the biggest frame we can make with this layer and version
// The frame size is increased by setting the maximum available bitrate
// for the frame
//
uint32 Mp3HeaderReader::getMaxFrameSize() {
	return calcFrameSizeForBitrate(getMaxBitrate(), true);
}

// Get the highest bitrate possible for this layer and version
//
uint32 Mp3HeaderReader::getMaxBitrate() {
	std::cout << "max bitrate["<<calcBitRate(14)<<"]\n"; //debug
	return calcBitRate(14);			// the highest value in the table
}

// Class VbriHeaderReader ----------------------------------------
bool VbriHeaderReader::load() {		// assuming we are at a valid MP3 header
	_stream->seek(Mp3HeaderReader::HEADER_SIZE + START_OFFSET, SEEK_CUR);
		
	_stream->read_throwsOnError(&_header.id, sizeof(_header.id));

	if (_header.id[0] != 'V' || _header.id[1] != 'B' || _header.id[2] != 'R' || _header.id[3] != 'I')
		return false;
	
	_header.versionId = _stream->readUint16BE();
	_header.quality = _stream->readUint16BE();
	_header.numOfBytes = _stream->readUint32BE();
	_header.numOfFrames = _stream->readUint32BE();
	_header.numOfTableEntries = _stream->readUint16BE();
	_header.scaleFactor = _stream->readUint16BE();
	_header.tableEntrySizeInBytes = _stream->readUint16BE();		
	_header.framesPerTableEntry = _stream->readUint16BE();
	
	// read the table
	uint32 tableSize = _header.numOfTableEntries * _header.tableEntrySizeInBytes;
	_table = new uint32[_header.numOfTableEntries];
	
	for (int i = 0; i < _header.numOfTableEntries; i++) {
		switch (_header.tableEntrySizeInBytes) {
		case 4:
			_table[i] = _stream->readUint32BE();
			break;
		case 2:
			_table[i] = _stream->readUint16BE();
			break;
		case 1:
			_table[i] = _stream->readByte();
			break;
		}
		
		_table[i] *= _header.scaleFactor;
	}
	
	if (_stream->eos() || _stream->err()) {
		delete[] _table;
		return false;
	}
	
	calculateTimes();
	return true;
}

void VbriHeaderReader::calculateTimes() {
	float total = 
				(_header.numOfFrames * _mp3Header->getSamplesPerFrame()) / 
				_mp3Header->getSamplingRate();	// samples per second
	
	_totalPlayTime = ((float)_header.numOfFrames * (float)_mp3Header->getSamplesPerFrame() / (float)_mp3Header->getSamplingRate());
	_timePerTableEntry = _totalPlayTime / (_header.numOfTableEntries + 1);
}

bool VbriHeaderReader::getClosestOffsetBefore(const float &inTime, uint32 &offset, float &offsetTime) {
	assert(_table);
	uint32 totalOffset = 0;
	float totalTime = 0.0f;
	
	for (int i=0; i < _header.numOfTableEntries; i++) {
		if (totalTime + _timePerTableEntry > inTime)
			break;
		totalTime += _timePerTableEntry;
		totalOffset += _table[i];
	}
	offset = totalOffset;
	offsetTime = totalTime;
	return true;
}

} /* namespace Audio */
