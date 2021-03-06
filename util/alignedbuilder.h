// @file alignedbuilder.h

/**
*    Copyright (C) 2009 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "../bson/stringdata.h"

namespace mongo {

    /** a page-aligned BufBuilder. */
    class AlignedBuilder {
        template<class T> void append(T j) {
            copyLE( grow( sizeof(T) ), j );
        }
    public:
        AlignedBuilder(unsigned init_size);
        ~AlignedBuilder() { kill(); }

        /** reset for a re-use. shrinks if > 128MB */
        void reset() {
            _len = 0;
            const unsigned sizeCap = 128*1024*1024;
            if (_p._size > sizeCap)
                _realloc(sizeCap, _len);
        }

        /** note this may be deallocated (realloced) if you keep writing or reset(). */
        const char* buf() const { return _p._data; }

        /** leave room for some stuff later
            @return offset in the buffer that was our current position
        */
        size_t skip(unsigned n) {
            unsigned l = len();
            grow(n);
            return l;
        }

        char* atOfs(unsigned ofs) { return _p._data + ofs; }

        void appendChar(char j) {
           append<char>( j );
        }
        void appendNum(char j) {
           append<char>( j );
        }
        void appendNum(short j) {
           append<short>( j );
        }
        void appendNum(int j) {
           append<int>( j );
        }
        void appendNum(unsigned j) {
           append<unsigned>( j );
        }
        void appendNum(bool j) {
           append<bool>( j );
        }
        void appendNum(double j) {
           append<double>( j );
        }
        void appendNum(long long j) {
           append<long long>( j );
        }
        void appendNum(unsigned long long j) {
           append<unsigned long long>( j );
        }

        void appendBuf(const void *src, size_t len) { memcpy(grow((unsigned) len), src, len); }

        template<class T>
        void appendStruct(const T& s) { STATIC_ASSERT_HAS_ENDIAN_AWARE_MARKER(T); appendBuf(&s, sizeof(T)); }

        void appendStr(const StringData &str , bool includeEOO = true ) {
            const unsigned len = str.size() + ( includeEOO ? 1 : 0 );
            assert( len < (unsigned) BSONObjMaxUserSize );
            memcpy(grow(len), str.data(), len);
        }

        /** @return the in-use length */
        unsigned len() const { return _len; }

    private:
        static const unsigned Alignment = 8192;

        /** returns the pre-grow write position */
        inline char* grow(unsigned by) {
            unsigned oldlen = _len;
            _len += by;
            if ( _len > _p._size ) {
                growReallocate(oldlen);
            }
            return _p._data + oldlen;
        }

        void growReallocate(unsigned oldLenInUse);
        void kill();
        void mallocSelfAligned(unsigned sz);
        void _malloc(unsigned sz);
        void _realloc(unsigned newSize, unsigned oldLenInUse);
        void _free(void*);

        struct AllocationInfo {
            char *_data;
            void *_allocationAddress;
            unsigned _size;
        } _p;
        unsigned _len;  // bytes in use
    };

}
