/* ScummVM Tools
 * Copyright (C) 2002-2009 The ScummVM project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL: https://scummvm.svn.sourceforge.net/svnroot/scummvm/tools/trunk/common/endian.h $
 * $Id: endian.h 46362 2009-12-13 20:23:42Z fingolfin $
 *
 */

#include <sstream>
#include <iostream>
#include "add_header.h"

char g_message[100] = {0};

// Not sure about the tool type
AddMp3Header::AddMp3Header(const std::string &name) : 
	Tool(name, TOOLTYPE_COMPRESSION), 
	_inFileSize(0),
	_inFileFrames(0), _inFileLength(0.0f),
	_maxFrameSize(0), _minTableSize(0),
	_tableSize(0), _framesPerTableEntry(0),
	_outFileFrames(0), _outDelayFrames(0) {

	_supportsProgressBar = true;

	// Create our input file type
	ToolInput input1;
	input1.format = "*.mp3";
	input1.description = "Target mp3 file to add VBR headers to";
	_inputPaths.push_back(input1);
	
	_shorthelp = "Used to add a seek header to MP3 files.";
	_helptext = "\nUsage: " + getName() + " [-o outfile.mp3] <infile.mp3>\n";
}

std::string AddMp3Header::getHelp() const {
	std::ostringstream os;

	// Standard help text
	os << Tool::getHelp();
	return os.str();
}

void AddMp3Header::execute() {
	// Get the input mp3 file
	Common::Filename inpath(_inputPaths[0].path);
	Common::Filename outpath(_outputPath);

	if (outpath.empty()) {
		// If empty, our outpath will have a .out.mp3 extension
		outpath = inpath.getPath() + inpath.getName() + "-out." + inpath.getExtension();
	}

	// debug
	std::cout<< "Inpath: [" << inpath.getFullPath() << "] Outpath: [" << outpath.getFullPath() << "]\n";

	// open files
	Common::File inFile(inpath, "rb");	// input mp3 file

	// parse the input mp3 file
	getFileData(inFile);

	// open output file
	Common::File outFile(outpath, "wb");

	// create output file 
	createOutFile(outFile);
}

void AddMp3Header::getFileData(Common::File &inFile) {
	Audio::Mp3HeaderReader mp3Header(inFile);

	_inFileSize = inFile.size();

    if (!mp3Header.load()) {
		throw Mp3Exception("File is not a proper MP3 file");
	}

	// Calculate the file length
	calcFileLength(mp3Header, inFile);

	// debug
	std::cout << "In-file frames[" << _inFileFrames << "] length [" << _inFileLength << " sec]\n";

	// Calculate the needed parameters for the VBRI header
	inFile.seek(0, SEEK_SET);

    if (!mp3Header.load()) {
		throw Mp3Exception("File is not a proper MP3 file");
	}

	calcVbriParams(mp3Header);
}

void AddMp3Header::calcFileLength(Audio::Mp3HeaderReader &mp3Header, Common::File &file) {
	uint32 samplingRate = mp3Header.getSamplingRate();
	uint32 samplesPerFrame = mp3Header.getSamplesPerFrame();

	// get number of frames
	while (!file.eos()) {
		if (mp3Header.getFrameSize() <= 0) { 
			print("Found a bad frame at frame %d, byte %d\n", _inFileFrames, file.pos());
			file.seek(1, SEEK_CUR); // move ahead so we don't keep getting the same frame
		}
		_inFileFrames++;
		//print("got frameSize[%d] pos[%x]\n", mp3Header.getFrameSize(), file.pos());
		file.seek(mp3Header.getFrameSize(), SEEK_CUR);
		//print("post pos[%x]\n", file.pos());
		
		if (!mp3Header.load() && !file.eos()) {
			char string[100];
			sprintf(string, "Failed to parse mp3 file at point %d", file.pos());
			throw Mp3Exception(string);
		}

	}

	_inFileLength = ((float)samplesPerFrame / (float)samplingRate) * _inFileFrames;
}

void AddMp3Header::calcVbriParams(Audio::Mp3HeaderReader &mp3Header) {
	// First, get the maximum size of a frame which limits the size
	// of the table
	
	_maxFrameSize = mp3Header.getMaxFrameSize();
	std::cout << "Max framesize[" << _maxFrameSize << "]\n";
	_maxTableSize = Audio::VbriHeaderReader::getMaxTableSize(_maxFrameSize);
	std::cout << "Max tablesize[" << _maxTableSize << "]\n";

	// We use 16-bit offsets since they're not wasteful and give us sufficient precision
	// in the data domain
	_maxTableSize /= 2;

	// Check the number of frames relative to the possible size of our table
	// Add 1 frame at least for the VBRI header
	_outFileFrames = _inFileFrames;
	_outDelayFrames = 1;	// 1 frame's delay for VBRI header

	// Determine minimum number of table entries by the average amount of
	// space each entry will need to cover. On average, seeking 50K should be fine
	_minTableSize = _inFileSize / 50000;

	// Small MP3 frame size can cause our max to be smaller than our min. If so, reset
	// min size to 0
	if (_minTableSize >= _maxTableSize)
		_minTableSize = 0;
	std::cout << "Table size: min[" << _minTableSize << "] max[" << _maxTableSize << "]\n";
	
	// We need to figure out how big our table will be
	if (_outFileFrames + _outDelayFrames <= _maxTableSize) {
		_tableSize = _outFileFrames + _outDelayFrames;		// no need to go higher
		_framesPerTableEntry = 1;
	} else {
		// We have more frames than we can fit in the table. 
		// Our goal is to divide the number of frames by factors until we get it
		// down to fit in the table. If no factors exist that can get it to fit in the
		// table, we add frames until we find a number that can be fit in the table
		bool found = false;

		while (_outDelayFrames <= 10) {
			for (int i = 2; i < (int)_outFileFrames; i++) {
				if (( (_outFileFrames + _outDelayFrames) % i) == 0) { 
					_tableSize = (_outFileFrames + _outDelayFrames) / i;

					if (_tableSize <= _maxTableSize &&
						_tableSize >= _minTableSize) {
						_framesPerTableEntry = i;
						found = true;
						break;
					}
				}
			}	
			if (found) 
				break;

			_outDelayFrames++;
		}

		if (!found) {
			throw Mp3Exception("Failed to find proper number of frames for file.");
		}

		_outFileFrames += _outDelayFrames; // ultimately we have to include the delay frames
	}

	std::cout << "tableSize [" << _tableSize << "] outFrames[" << _outFileFrames << 
		"] delayFrames [" << _outDelayFrames << "] framesPerEntry [" << 
		_framesPerTableEntry << "]\n";
}

void AddMp3Header::createOutFile(Common::File &outFile) {
}
