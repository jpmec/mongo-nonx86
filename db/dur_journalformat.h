// @file dur_journalformat.h The format of our journal files.

/**
*    Copyright (C) 2010 10gen Inc.
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

#include "../util/md5.hpp"

namespace mongo {

    namespace dur {

#pragma pack(1)
        /** beginning header for a journal/j._<n> file
            there is nothing important int this header at this time.  except perhaps version #.
        */
        struct JHeader : public endian_aware {
            JHeader() { }
            JHeader(string fname);

            char magic[2]; // "j\n". j means journal, then a linefeed, fwiw if you were to run "less" on the file or something...

            // x4142 is asci--readable if you look at the file with head/less -- thus the starting values were near
            // that.  simply incrementing the version # is safe on a fwd basis.
            enum { CurrentVersion = 0x4147 };
            little<unsigned short> _version;

            // these are just for diagnostic ease (make header more useful as plain text)
            char n1;          // '\n'
            char ts[20];      // ascii timestamp of file generation.  for user reading, not used by code.
            char n2;          // '\n'
            char dbpath[128]; // path/filename of this file for human reading and diagnostics.  not used by code.
            char n3, n4;      // '\n', '\n'

            little<unsigned long long> fileId; // unique identifier that will be in each JSectHeader. important as we recycle prealloced files

            char reserved3[8026]; // 8KB total for the file header
            char txt2[2];         // "\n\n" at the end

            bool versionOk() const { return _version == CurrentVersion; }
            bool valid() const { return magic[0] == 'j' && txt2[1] == '\n' && fileId; }
        };

        /** "Section" header.  A section corresponds to a group commit.
            len is length of the entire section including header and footer.
        */
        struct JSectHeader : public endian_aware {
            little<unsigned> len;                  // length in bytes of the whole section
            little<unsigned long long> seqNumber;  // sequence number that can be used on recovery to not do too much work
            little<unsigned long long> fileId;     // matches JHeader::fileId
        };

        /** an individual write operation within a group commit section.  Either the entire section should
            be applied, or nothing.  (We check the md5 for the whole section before doing anything on recovery.)
        */
        struct JEntry : public endian_aware {
            enum OpCodes {
                OpCode_Footer      = 0xffffffff,
                OpCode_DbContext   = 0xfffffffe,
                OpCode_FileCreated = 0xfffffffd,
                OpCode_DropDb      = 0xfffffffc,
                OpCode_Min         = 0xfffff000
            };
            union {
                little_pod<OpCodes> opcode;
                little_pod<unsigned> len;    // length in bytes of the data of the JEntry. does not include the JEntry header
            };

            little<unsigned> ofs;  // offset in file

            // sentinel and masks for _fileNo
            enum {
                DotNsSuffix = 0x7fffffff, // ".ns" file
                LocalDbBit  = 0x80000000  // assuming "local" db instead of using the JDbContext
            };
            little<int> _fileNo;   // high bit is set to indicate it should be the <dbpath>/local database
            // char data[len] follows

            const char * srcData() const {
                return (const char *) (&_fileNo + 1 );
            }

            int getFileNo() const { return _fileNo & (~LocalDbBit); }
            void setFileNo(int f) { _fileNo = f; }
            bool isNsSuffix() const { return getFileNo() == DotNsSuffix; }

            void setLocalDbContextBit() { _fileNo |= LocalDbBit; }
            bool isLocalDbContext() const { return _fileNo & LocalDbBit; }
            void clearLocalDbContextBit() { _fileNo = getFileNo(); }

            static string suffix(int fileno) {
                if( fileno == DotNsSuffix ) return "ns";
                stringstream ss;
                ss << fileno;
                return ss.str();
            }
        };

        /** group commit section footer. md5 is a key field. */
        struct JSectFooter : public endian_aware {
            JSectFooter(const void* begin, int len) { // needs buffer to compute hash
                sentinel = JEntry::OpCode_Footer;
                reserved = 0;
                magic[0] = magic[1] = magic[2] = magic[3] = '\n';

                // skip section header since size modified after hashing
                (const char*&)begin += sizeof(JSectHeader);
                len                 -= sizeof(JSectHeader);

                md5(begin, len, hash);
            }
            little<unsigned> sentinel;
            md5digest hash; // unsigned char[16]
            little<unsigned long long> reserved;
            char magic[4]; // "\n\n\n\n"

            bool checkHash(const void* begin, int len) const {
                // skip section header since size modified after hashing
                (const char*&)begin += sizeof(JSectHeader);
                len                 -= sizeof(JSectHeader);
                md5digest current;
                md5(begin, len, current);
                DEV log() << "checkHash len:" << len << " hash:" << toHex(hash, 16) << " current:" << toHex(current, 16) << endl;
                return (memcmp(hash, current, sizeof(hash)) == 0);
            }
        };

        /** declares "the next entry(s) are for this database / file path prefix" */
        struct JDbContext : public endian_aware {
            JDbContext() : sentinel(JEntry::OpCode_DbContext) { }
            const little<unsigned> sentinel;   // compare to JEntry::len -- zero is our sentinel
            //char dbname[];
        };

        /** "last sequence number" */
        struct LSNFile : public endian_aware {
            little<unsigned> ver;
            little<unsigned> reserved2;
            little<unsigned long long> lsn;
            little<unsigned long long> checkbytes;
            little<unsigned long long> reserved[8];

            void set(unsigned long long lsn);
            unsigned long long get();
        };

#pragma pack()

    }

}
