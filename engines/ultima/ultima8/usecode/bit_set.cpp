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

#include "ultima/ultima8/misc/pent_include.h"
#include "ultima/ultima8/usecode/bit_set.h"

#include "ultima/ultima8/filesys/idata_source.h"
#include "ultima/ultima8/filesys/odata_source.h"

namespace Ultima {
namespace Ultima8 {

BitSet::BitSet() : _size(0), _bytes(0), _data(0) {
}


BitSet::BitSet(unsigned int size) {
	_data = 0;
	setSize(size);
}

BitSet::~BitSet() {
	delete[] _data;
}

void BitSet::setSize(unsigned int size) {
	if (_data) delete[] _data;

	_size = size;
	_bytes = 0;
	_bytes = _size / 8;
	if (_size % 8 != 0) _bytes++;

	_data = new uint8[_bytes];
	for (unsigned int i = 0; i < _bytes; ++i)
		_data[i] = 0;
}

uint32 BitSet::getBits(unsigned int pos, unsigned int n) {
	assert(n <= 32);
	assert(pos + n <= _size);
	if (n == 0) return 0;

	unsigned int firstbyte = pos / 8;
	unsigned int lastbyte = (pos + n - 1) / 8;

	if (firstbyte == lastbyte) {
		return ((_data[firstbyte] >> (pos % 8)) & ((1 << n) - 1));
	}

	unsigned int firstbits = 8 - (pos % 8);
	unsigned int lastbits = ((pos + n - 1) % 8) + 1;

	unsigned int firstmask = ((1 << firstbits) - 1) << (8 - firstbits);
	unsigned int lastmask = ((1 << lastbits) - 1);

	uint32 ret = 0;

	ret |= (_data[firstbyte] & firstmask) >> (8 - firstbits);
	unsigned int shift = firstbits;
	for (unsigned int i = firstbyte + 1; i < lastbyte; ++i) {
		ret |= (_data[i] << shift);
		shift += 8;
	}
	ret |= (_data[lastbyte] & lastmask) << shift;

	return ret;
}

void BitSet::setBits(unsigned int pos, unsigned int n, uint32 bits) {
	assert(n <= 32);
	assert(pos + n <= _size);
	if (n == 0) return;

	unsigned int firstbyte = pos / 8;
	unsigned int lastbyte = (pos + n - 1) / 8;

	if (firstbyte == lastbyte) {
		_data[firstbyte] &= ~(((1 << n) - 1) << (pos % 8));
		_data[firstbyte] |= (bits & ((1 << n) - 1)) << (pos % 8);
		return;
	}

	unsigned int firstbits = 8 - (pos % 8);
	unsigned int lastbits = ((pos + n - 1) % 8) + 1;

	unsigned int firstmask = ((1 << firstbits) - 1) << (8 - firstbits);
	unsigned int lastmask = ((1 << lastbits) - 1);

	_data[firstbyte] &= ~firstmask;
	_data[firstbyte] |= (bits << (8 - firstbits)) & firstmask;
	unsigned int shift = firstbits;
	for (unsigned int i = firstbyte + 1; i < lastbyte; ++i) {
		_data[i] = (bits >> shift);
		shift += 8;
	}
	_data[lastbyte] &= ~lastmask;
	_data[lastbyte] |= (bits >> shift) & lastmask;
}

void BitSet::save(ODataSource *ods) {
	ods->write4(_size);
	ods->write(_data, _bytes);
}

bool BitSet::load(IDataSource *ids, uint32 version) {
	uint32 s = ids->read4();
	setSize(s);
	ids->read(_data, _bytes);

	return true;
}

} // End of namespace Ultima8
} // End of namespace Ultima
