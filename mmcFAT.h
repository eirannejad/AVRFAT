/*! \file mmcFAT.h \brief MultiMedia and SD Flash Card FAT Functions */
//*****************************************************************************
//
// File Name	: 'mmcFAT.h'
// Title		: MultiMedia and SD Flash Card FAT functions
// Author		: Ehsan Iran Nejad - Copyright (C) 2006
// Created		: 2006.05.21
// Revised		: 2006.05.21
// Version		: 0.1
// Target MCU	: Atmel AVR Series
//*****************************************************************************

#ifndef mmcFAT_H
#define mmcFAT_H

#define MMC_FAT_CREATEFILE		//Creates file and folders if not found on Open
#define MMC_FAT_COMPLETE		// + mmcRead	+ mmcDeleteEntry	+ mmcRemoveEntry 	almost:1900 bytes
#define MMC_FAT_FAST			// Fast Read and Write	almost: 400 bytes

#include "global.h"
#include "avrlibtypes.h"
#include "avrlibdefs.h"

// Some useful cluster numbers
#define MSDOSFSROOT     0               // cluster 0 means the root dir
#define CLUST_FREE      0               // cluster 0 also means a free cluster
#define MSDOSFSFREE     CLUST_FREE
#define CLUST_FIRST     2               // first legal cluster number
#define CLUST_RSRVD     0xfffffff6      // reserved cluster range
#define CLUST_BAD       0xfffffff7      // a cluster with a defect
#define CLUST_EOFS      0xfffffff8      // start of eof cluster range
#define CLUST_EOFE      0xffffffff      // end of eof cluster range

#define FAT12_MASK      0x00000fff      // mask for 12 bit cluster numbers
#define FAT16_MASK      0x0000ffff      // mask for 16 bit cluster numbers
#define FAT32_MASK      0x0fffffff      // mask for FAT32 cluster numbers

struct mmcBootSec {
//	u08	bsJump[3];					// jump inst E9xxxx or EBxx90
//	u08	bsOEMName[8];				// OEM name and version
// BIOS Parameter Block for DOS 7.10 (FAT32)*********************
//		u16	BytesPerSec;	// bytes per sector
		u08	SecPerClust;	// sectors per cluster
		u16	ResSectors;		// number of reserved sectors
//		u08	FATs;			// number of FATs
		u16	RootDirEnts;	// number of root directory entries
//		u16	totalSectors;	// total number of sectors
//		u08	bpbMedia;		// media descriptor
//		u16	FATsecs;		// number of sectors per FAT
//		u16	bpbSecPerTrack;	// sectors per track
//		u16	bpbHeads;		// number of heads
//		u32	bpbHiddenSecs;	// # of hidden sectors
// 3.3 compat ends here
		u32	totalSectors;	// # of sectors if bpbSectors == 0
// 5.0 compat ends here
		u32 FATsecs;		// like bpbFATsecs for FAT32
//		u16      bpbExtFlags;	// extended flags:
//#define FATNUM    0xf			// mask for numbering active FAT
//#define FATMIRROR 0x80			// FAT is mirrored (like it always was)
//		u16      bpbFSVers;	// filesystem version
//#define FSVERS    0				// currently only 0 is understood
//		u32 RootClust;	// start cluster for root directory
//		u16      bpbFSInfo;	// filesystem info structure sector
//		u16      bpbBackup;	// backup boot sector
// There is a 12 byte filler here, but we ignore it
//***********************************************************************
//	u08	bsExt[26];					// Bootsector Extension
//	u08	bsBootCode[418];			// pad so structure is 512b
//	u08	bsBootSectSig2;				// 2 & 3 are only defined for FAT32?
//	u08	bsBootSectSig3;
//	u08	bsBootSectSig0;				// boot sector signature byte 0x55
//	u08	bsBootSectSig1;				// boot sector signature byte 0xAA
//#define BOOTSIG0        0x55
//#define BOOTSIG1        0xaa
//#define BOOTSIG2        0
//#define BOOTSIG3        0
};
// Structure of a dos directory entry.
struct mmcDir {
		u08		deName[8];      // filename, blank filled
#define SLOT_EMPTY      0x00            // slot has never been used
#define SLOT_E5         0x05            // the real value is 0xe5
#define SLOT_DELETED    0xe5            // file in this slot deleted
		u08		deExtension[3]; // extension, blank filled
		u08		deAttributes;   // file attributes
#define ATTR_NORMAL     0x00            // normal file
#define ATTR_READONLY   0x01            // file is readonly
#define ATTR_HIDDEN     0x02            // file is hidden
#define ATTR_SYSTEM     0x04            // file is a system file
#define ATTR_VOLUME     0x08            // entry is a volume label
#define ATTR_LONG_FILENAME	0x0f		// this is a long filename entry			    
#define ATTR_DIRECTORY  0x10            // entry is a directory name
#define ATTR_ARCHIVE    0x20            // file is new or modified
		u08        deLowerCase;    // NT VFAT lower case flags
#define LCASE_BASE      0x08            // filename base in lower case
#define LCASE_EXT       0x10            // filename extension in lower case
		u08        deCHundredth;   // hundredth of seconds in CTime
		u08        deCTime[2];     // create time
		u08        deCDate[2];     // create date
		u08        deADate[2];     // access date
		u16        deHighClust; 	// high bytes of cluster number
		u08        deMTime[2];     // last update time
		u08        deMDate[2];     // last update date
		u16        deStartCluster; // starting cluster of file
		u32       deFileSize;  	// size of file in bytes
};

struct fileHandler {
	char filename[8];
	char fileext[3];
	u32 cluster;
	u32 filesize;
	u32 fileInfoSector;
	u08 fileInfoOffset;
#ifdef MMC_FAT_FAST	//-------------------
	u32 lastAccessCluster;
	u08 lastAccessSector;
	u16 lastAccessOffset;
#endif					//-------------------
};

struct driveInfo {
	u08 fs;		// File system 2=Fat16	4=Fat32
	u32 fat1;
	u32 fat2;
	u32 rootCluster;
	u16 rootSize;
	u08 spc;	// sectors per cluster
};

typedef struct mmcBootSec BOOTSEC;
typedef struct mmcDir DIRENTRY;
typedef struct fileHandler FILEHANDLE;
typedef struct driveInfo DRIVEINFO;


u08 fatRegister;
// bit0 = FAT16 found
// bit1 = FAT32 found
// bit2 = '1' write was last
//		  '0' Read was last


//User Functions---------------------------------------------------------
#define MMC_FAT16 			2
#define MMC_FAT32 			4
#define MMC_FATFAILED		0
// Reads Boot Sector and Initialize DriveInfo Global Structure
// returns MMC_FAT32 if FAT32 and MMC_FAT16 if FAT16 found
u08 mmcFatInit(void);

#define MMC_OK				0x10
#define MMC_FILECREATED		0xD0
#define MMC_FOLDERCREATED	0x0E
#define MMC_FILEOPENED		0xD1
#define MMC_FOLDEROPENED	0x0F

#define MMC_FILENOTFOUND	0xE0
#define MMC_FOLDERNOTFOUND	0xE1

#define ERROR_DRIVEFULL 	0xFF

//opens an existing file or create one. sets handler info.
//sample: mmcFileOpen("folder:filename.ext");
//use ':' as folder seprators '\' or '/'
//use '::' to indicate you are opening a folder
u08 mmcFileOpen(char* filename);

//Writes Data according to Given Data Size to End of The Current File
u08 mmcWriteFile(u08* outData,u32 dataSize);

#define MMC_WRONGOFFSET		0xF0
#define MMC_FILESIZE_ZERO	0xF1
#define MMC_WRONGFILE		0xF2
#define MMC_ENDFILE			0xF8
//Reads Data according to Given Data Size from offset point
u08 mmcReadFile(u08* inData,u32 dataSize,u32 offset);
//deletes a file from disk...file space NOT released.
u08 mmcDeleteEntry();
//Removes a file from disk...file space Released.
u08 mmcRemoveEntry();
#endif
