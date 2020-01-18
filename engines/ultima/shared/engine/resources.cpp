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

#include "ultima/shared/engine/resources.h"
#include "ultima/shared/early/font_resources.h"

namespace Ultima {
namespace Shared {

ResourceFile::ResourceFile(Common::ReadStream *in) : _inStream(in), _bufferP(_buffer) {
	Common::fill(_buffer, _buffer + STRING_BUFFER_SIZE, 0);
}

void ResourceFile::syncString(const char *&str) {
	str = _bufferP;
	while ((*_bufferP = _inStream->readByte()) != '\0')
		++_bufferP;

	assert(_bufferP < (_buffer + STRING_BUFFER_SIZE));
}

void ResourceFile::syncStrings(const char **str, size_t count) {
	uint tag = _inStream->readUint32LE();
	assert(tag == MKTAG(count, 0, 0, 0));

	for (size_t idx = 0; idx < count; ++idx)
		syncString(str[idx]);
}

void ResourceFile::syncStrings2D(const char **str, size_t count1, size_t count2) {
	uint tag = _inStream->readUint32LE();
	assert(tag == MKTAG(count1, count2, 0, 0));

	for (size_t idx = 0; idx < count1 * count2; ++idx)
		syncString(str[idx]);
}

void ResourceFile::syncNumber(int &val) {
	val = _inStream->readSint32LE();
}

void ResourceFile::syncNumbers(int *vals, size_t count) {
	uint tag = _inStream->readUint32LE();
	assert(tag == MKTAG(count, 0, 0, 0));
	for (size_t idx = 0; idx < count; ++idx)
		vals[idx] = _inStream->readSint32LE();
}

void ResourceFile::syncNumbers2D(int *vals, size_t count1, size_t count2) {
	uint tag = _inStream->readUint32LE();
	assert(tag == MKTAG(count1, count2, 0, 0));
	for (size_t idx = 0; idx < count1 * count2; ++idx)
		vals[idx] = _inStream->readSint32LE();
}

void ResourceFile::syncNumbers3D(int *vals, size_t count1, size_t count2, size_t count3) {
	uint tag = _inStream->readUint32LE();
	assert(tag == MKTAG(count1, count2, count3, 0));
	for (size_t idx = 0; idx < count1 * count2 * count3; ++idx)
		vals[idx] = _inStream->readSint32LE();
}

void ResourceFile::syncBytes(byte *vals, size_t count) {
	uint tag = _inStream->readUint32LE();
	assert(tag == MKTAG(count, 0, 0, 0));
	_inStream->read(vals, count);
}

void ResourceFile::syncBytes2D(byte *vals, size_t count1, size_t count2) {
	uint tag = _inStream->readUint32LE();
	assert(tag == MKTAG(count1, count2, 0, 0));
	_inStream->read(vals, count1 * count2);
}

/*-------------------------------------------------------------------*/

LocalResourceFile::LocalResourceFile(Common::WriteStream *out) : ResourceFile(nullptr), _outStream(out) {
}

void LocalResourceFile::syncString(const char *&str) {
	if (!_outStream) {
		ResourceFile::syncString(str);
	} else {
		_outStream->writeString(str);
	}
}

void LocalResourceFile::syncStrings(const char **str, size_t count) {
	if (!_outStream) {
		ResourceFile::syncStrings(str, count);
	} else {
		_outStream->writeUint32LE(MKTAG(count, 0, 0, 0));
		for (size_t idx = 0; idx < count; ++idx)
			syncString(str[idx]);
	}
}

void LocalResourceFile::syncStrings2D(const char **str, size_t count1, size_t count2) {
	if (!_outStream) {
		ResourceFile::syncStrings2D(str, count1, count2);
	} else {
		_outStream->writeUint32LE(MKTAG(count1, count2, 0, 0));
		for (size_t idx = 0; idx < count1 * count2; ++idx)
			syncString(str[idx]);
	}
}

void LocalResourceFile::syncNumber(int &val) {
	if (!_outStream)
		ResourceFile::syncNumber(val);
	else
		_outStream->writeUint32LE(val);
}

void LocalResourceFile::syncNumbers(int *vals, size_t count) {
	if (!_outStream) {
		ResourceFile::syncNumbers(vals, count);
	} else {
		_outStream->writeUint32LE(MKTAG(count, 0, 0, 0));
		for (size_t idx = 0; idx < count; ++idx)
			_outStream->writeUint32LE(vals[idx]);
	}
}

void LocalResourceFile::syncNumbers2D(int *vals, size_t count1, size_t count2) {
	if (!_outStream) {
		ResourceFile::syncNumbers2D(vals, count1, count2);
	} else {
		_outStream->writeUint32LE(MKTAG(count1, count2, 0, 0));
		for (size_t idx = 0; idx < count1 * count2; ++idx)
			_outStream->writeUint32LE(vals[idx]);
	}
}

void LocalResourceFile::syncNumbers3D(int *vals, size_t count1, size_t count2, size_t count3) {
	if (!_outStream) {
		ResourceFile::syncNumbers3D(vals, count1, count2, count3);
	} else {
		_outStream->writeUint32LE(MKTAG(count1, count2, count3, 0));
		for (size_t idx = 0; idx < count1 * count2 * count3; ++idx)
			_outStream->writeUint32LE(vals[idx]);
	}
}

void LocalResourceFile::syncBytes(byte *vals, size_t count) {
	if (!_outStream) {
		ResourceFile::syncBytes(vals, count);
	} else {
		_outStream->writeUint32LE(MKTAG(count, 0, 0, 0));
		_outStream->write(vals, count);
	}
}

void LocalResourceFile::syncBytes2D(byte *vals, size_t count1, size_t count2) {
	if (!_outStream) {
		ResourceFile::syncBytes2D(vals, count1, count2);
	} else {
		_outStream->writeUint32LE(MKTAG(count1, count2, 0, 0));
		_outStream->write(vals, count1 * count2);
	}
}

/*-------------------------------------------------------------------*/

bool Resources::setup() {
	// Save locally constructred resources to the archive manager for access
	Shared::FontResources sharedFonts;
	sharedFonts.save();

	SearchMan.add("ultima", this);
	return true;
}

void Resources::addResource(const Common::String &name, const byte *data, size_t size) {
	// Add a new entry to the local resources list for the passed data
	_localResources.push_back(LocalResource());
	LocalResource &lr = _localResources[_localResources.size() - 1];

	lr._name = name;
	lr._data.resize(size);
	Common::copy(data, data + size, &lr._data[0]);
}

const Resources::LocalResource *Resources::getResource(const Common::String &name) const {
	for (uint idx = 0; idx < _localResources.size(); ++idx) {
		if (!_localResources[idx]._name.compareToIgnoreCase(name))
			return &_localResources[idx];
	}

	return nullptr;
}

bool Resources::hasFile(const Common::String &name) const {
	return getResource(name) != nullptr;
}

int Resources::listMembers(Common::ArchiveMemberList &list) const {
	for (uint idx = 0; idx < _localResources.size(); ++idx) {
		list.push_back(Common::ArchiveMemberPtr(new Common::GenericArchiveMember(_localResources[idx]._name, this)));
	}

	return _localResources.size();
}

const Common::ArchiveMemberPtr Resources::getMember(const Common::String &name) const {
	if (!hasFile(name))
		return Common::ArchiveMemberPtr();

	return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(name, this));
}

Common::SeekableReadStream *Resources::createReadStreamForMember(const Common::String &name) const {
	const LocalResource *lr = getResource(name);
	if (!lr)
		return nullptr;

	return new Common::MemoryReadStream(&lr->_data[0], lr->_data.size());
}

} // End of namespace Shared
} // End of namespace Ultima
