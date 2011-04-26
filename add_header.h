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

#ifndef ADD_MP3_HEADER_H
#define ADD_MP3_HEADER_H

#include "tool.h"
#include "common/file.h"
#include "sound/mp3_header.h"

class Mp3Exception : public ToolException {
public:
	Mp3Exception(std::string error, int retcode = -1) : ToolException(error, retcode) {}
};

class AddMp3Header : public Tool {
public:
	AddMp3Header(const std::string &name="add_mp3_header");
	
	virtual std::string getHelp() const;
	virtual void execute();

protected:
	void getFileData(Common::File &inFile); 
	void calcFileLength(Audio::Mp3HeaderReader &mp3Header, Common::File &file);
	void calcVbriParams(Audio::Mp3HeaderReader &mp3Header); 
	void createOutFile(Common::File &outFile);

	uint32 _inFileSize;
	uint32 _inFileFrames;
	float _inFileLength;

	uint32 _maxFrameSize;

	uint32 _minTableSize;
	uint32 _maxTableSize;
	uint32 _tableSize;
	uint32 _framesPerTableEntry;

	uint32 _outFileFrames;
	uint32 _outDelayFrames;
};

#endif /* ADD_MP3_HEADER_H */
