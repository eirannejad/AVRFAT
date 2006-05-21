/*! \file mmcFAT.c \brief MultiMedia and SD Flash Card FAT Functions */
//*****************************************************************************
//
// File Name	: 'mmcFAT.c'
// Title		: MultiMedia and SD Flash Card FAT functions
// Author		: Ehsan Iran Nejad - Copyright (C) 2006
// Created		: 2006.05.21
// Revised		: 2006.05.21
// Version		: 0.1
// Target MCU	: Atmel AVR Series
//*****************************************************************************

#include "mmc.h"
#include "mmcFAT.h"

FILEHANDLE currentFile;
DRIVEINFO currentDrive;

/* ********************** Implementation **************************/
/******************************************************************************* Get Absolute Offset */
//Gets Relative Cluster code and Convert it to Absolute
u32 getAbsOffset(u32 cluster) {
	return ( currentDrive.rootCluster + currentDrive.rootSize + ((cluster-2)*currentDrive.spc) );
}
/*******************************************************************************  Get Relative Offset */
//Gets Absolute Cluster code and Convert it to Relative
u32 getRelOffset(u32 cluster) {
	return ( ( (cluster - currentDrive.rootCluster - currentDrive.rootSize)/currentDrive.spc) + 2 );
}
/******************************************************************************* Upgrade File Size  */
//updates Filsesize and Writes Cluster
void updateFileSize(u08 *buffer) {
	mmcRead(currentFile.fileInfoSector,buffer);
	u08 *ptr;
	ptr=(u08 *)&currentFile.filesize;
	buffer[(currentFile.fileInfoOffset*32)+ 28] =*(ptr);
	buffer[(currentFile.fileInfoOffset*32)+ 29] =*(ptr+1);
	buffer[(currentFile.fileInfoOffset*32)+ 30] =*(ptr+2);
	buffer[(currentFile.fileInfoOffset*32)+ 31] =*(ptr+3);
	mmcWrite(currentFile.fileInfoSector,buffer);
}
/******************************************************************************* Set Zero Cluster */
//Writes Zero to cluster
void setZeroCluster(u32 clustercode,u08 *buffer) {
	u08 k=0;
	clustercode=getAbsOffset(clustercode);
	do {
		buffer[k*2]=0;
		buffer[(k*2)+1]=0;
		if(k==255) break;
		k++;
	} while(1);
	mmcWrite(clustercode,buffer);
}
/******************************************************************************* Get NEXT Cluster  */
//scans fat1 to find cluster ofsset and next cluster code
//returns Absolute cluster no.
u32 getNextCluster(u32 clustercode, u08 *buffer) {
	u32 offsetcode=0;
	u08 *ptr;
	while(clustercode >= (512/currentDrive.fs))	{
		offsetcode++;
		clustercode-=(512/currentDrive.fs);
	}
	mmcRead(currentDrive.fat1 + offsetcode,buffer);
	offsetcode=clustercode*currentDrive.fs;
	ptr=(u08 *)&clustercode;
	*ptr = *(buffer+offsetcode);
	*(ptr+1) = *(buffer+offsetcode+1);
	if(currentDrive.fs==4) {
		*(ptr+2) = *(buffer+offsetcode+2);
		*(ptr+3) = *(buffer+offsetcode+3);
	}
	else {
		*(ptr+2) =0;
		*(ptr+3) =0;
	}
	if( clustercode >= ((currentDrive.fs==MMC_FAT32)?0x0FFFFFF8:0xFFF8) )
		return 0;
	return getAbsOffset(clustercode);
}
/******************************************************************************* Set Cluster Code */
//scans fat1 to find cluster ans sets its value to new one
//returns stat
u08 setClusterCode(u32 clustercode,u32 code,u08 *buffer) {
	u32 offsetcode=0;
	u08 *ptr;
	while(clustercode >= (512/currentDrive.fs))	{
		offsetcode++;
		clustercode-=(512/currentDrive.fs);
	}
	mmcRead(currentDrive.fat1 + offsetcode,buffer);
	ptr=(u08 *)&code;
	clustercode*=currentDrive.fs;
	*(buffer+clustercode)=*ptr;
	*(buffer+clustercode+1)=*(ptr+1);
	if(currentDrive.fs==MMC_FAT32) {
		*(buffer+clustercode+2)=*(ptr+2);
		*(buffer+clustercode+3)=*(ptr+3);
	}
	mmcWrite(currentDrive.fat1 + offsetcode,buffer);
	mmcWrite(currentDrive.fat2 + offsetcode,buffer);
	return MMC_OK;
}
/******************************************************************************* Get NEW Cluster */
//scans fat1 to find an Empty cluster
//returns Relative Cluster Code
u32 getNewClusterCode(u08 *buffer) {
	u16 i;
	u32 k=0;
	do {
		mmcRead(currentDrive.fat1 + k , buffer);
		for(i=0;i<(512/currentDrive.fs);i++) {
			if(i<=2 && k==0) continue;
			if(buffer[i*currentDrive.fs]==0x00) {
		/*	*/	if(buffer[i*currentDrive.fs+1]==0x00) {
		/*	*/		if(currentDrive.fs==MMC_FAT32) {
		/*	*/			if(buffer[i*currentDrive.fs+2]==0x00) {
		/*	*/				if(buffer[i*currentDrive.fs+3]==0x00) {
		/*	*/					break;
		/*	*/				}
		/*	*/			}
		/*	*/		}
		/*	*/		else {
		/*	*/			break;
		/*	*/		}
		/*	*/	}
		/*	*/}
		}
		if(i<(512/currentDrive.fs)) {
			buffer[i*currentDrive.fs]=0xF8;
			buffer[i*currentDrive.fs+1]=0xFF;
			if(currentDrive.fs==MMC_FAT32) {
				buffer[i*currentDrive.fs+2]=0xFF;
				buffer[i*currentDrive.fs+3]=0xFF;
			}
			mmcWrite(currentDrive.fat1 + k,buffer);
			mmcWrite(currentDrive.fat2 + k,buffer);
			setZeroCluster((i+(k*(512/currentDrive.fs))),buffer);
			return (i+(k*(512/currentDrive.fs)));
		}
		else {
			k++;
		}
	} while((currentDrive.fat1 + k) < currentDrive.fat2 );
	return 0;
}
/******************************************************************************* Make Entry */
//creates an entry in Recieved Cluster Absolute Offset
u08 makeEntry(u32 cluster,char* entryname,u08 *buffer){
	u08 i=0,
		j=0,
		secOffset=0;
	u32 newClusterCode;	
	do {
		for(secOffset=0;secOffset<currentDrive.spc;secOffset++) {
			mmcRead( cluster + secOffset , buffer);
			// Finding Empty Entry in FAT buffer
			for(i=0;i<16;i++) {
				if(buffer[i*32]==0x00) break;
			}
			if(i<16) break;
		}
		if(i<16) {
			if((newClusterCode=getNewClusterCode(buffer))==0)
				return ERROR_DRIVEFULL;
			mmcRead(cluster  + secOffset ,buffer);
			for(j=0;j<8;j++) buffer[i*32+j]=currentFile.filename[j]=*(entryname+j);
			for(j=8;j<11;j++) buffer[i*32+j]=currentFile.fileext[j]=*(entryname+j);
			for(j=13;j<32;j++) buffer[i*32+j]=0x00;
			if(*(entryname+8)==0x20) { buffer[i*32+11]=0x10; j=0xFF; }
			else buffer[i*32+11]=0x00;
			buffer[i*32+12]=0x18;

			entryname=(char *)&newClusterCode;
			buffer[i*32+26]=*entryname;
			buffer[i*32+27]=*(entryname+1);
			buffer[i*32+20]=*(entryname+2);
			buffer[i*32+21]=*(entryname+3);

			currentFile.cluster=getAbsOffset(newClusterCode);
			currentFile.filesize=0;
			currentFile.fileInfoSector=cluster+secOffset;
			currentFile.fileInfoOffset=i;

			mmcWrite(cluster+secOffset,buffer);
			if(j==0xFF) {
				return MMC_FOLDERCREATED;
			}
			else return MMC_FILECREATED;
		}
		else {
			u32 tmp;
			tmp=cluster;
			if( (cluster=getNextCluster(getRelOffset(cluster),buffer)) == 0 ) {
				if((newClusterCode=getNewClusterCode(buffer))==0)
					return ERROR_DRIVEFULL;
				setClusterCode(getRelOffset(tmp),newClusterCode,buffer);
				cluster=getAbsOffset(newClusterCode);
			}
		}
	} while(1);
}
/******************************************************************************* Read Entry */
//searches for a given entry in current folder.
//if not exist calls makeEntry() to create it.
u08 ReadEntry(char* entryname) {
	u08 mmcFATbuffer[512];
	u08 i=0,
		j=0,
		secOffset=0;
	u08 *handleptr;
	u32 currentCluster;
	currentCluster=currentFile.cluster;
	do {
		for(secOffset=0;secOffset<currentDrive.spc;secOffset++) {		
			mmcRead(currentCluster + secOffset,mmcFATbuffer);
			// Finding entry name in FAT buffer
			for(i=0;i<16;i++) {
				if(mmcFATbuffer[i*32]==0x00) {i=18; break; }
				if(mmcFATbuffer[i*32]==0xE5) {continue; }
				for(j=0;j<11;j++) {
					if(mmcFATbuffer[i*32+j]!=*(entryname+j)) break;
				}
				if(j==11) break;
			}
			if(i<16 || i==18 || j==11) break;
		}
		// if found i contains entry code in fat
		if(i<16) {
			for(j=0;j<7;j++)
				currentFile.filename[j]=*(entryname+j);
			for(j=0;j<2;j++)
				currentFile.fileext[j]=*(entryname+8+j);
			handleptr=(u08 *)&currentFile.cluster;
			*handleptr=mmcFATbuffer[i*32+26];	handleptr++;
			*handleptr=mmcFATbuffer[i*32+27];	handleptr++;
			*handleptr=mmcFATbuffer[i*32+20];	handleptr++;
			*handleptr=mmcFATbuffer[i*32+21];
			currentFile.cluster=getAbsOffset(currentFile.cluster);

			handleptr=(u08 *)&currentFile.filesize;
			*handleptr=mmcFATbuffer[i*32+28];	handleptr++;
			*handleptr=mmcFATbuffer[i*32+29];	handleptr++;
			*handleptr=mmcFATbuffer[i*32+30];	handleptr++;
			*handleptr=mmcFATbuffer[i*32+31];

			currentFile.fileInfoSector=currentCluster;
			currentFile.fileInfoOffset=i;

			if(mmcFATbuffer[i*32+11]==0x10) return MMC_FOLDEROPENED;
			return MMC_FILEOPENED;
		}
		//if not found accourding to FS type choose next Cluster or Create File
		else {
			if(	currentDrive.fs==MMC_FAT16 && currentFile.cluster==currentDrive.rootCluster ) {
				if((currentCluster==(currentDrive.rootSize+currentDrive.rootCluster)-1)) return ERROR_DRIVEFULL;
				else
					if(i==18) {
#ifdef MMC_FAT_CREATEFILE		//----------------------------------------------------------
						return makeEntry(currentFile.cluster,entryname,mmcFATbuffer);
#else	//----------------------------------------------------------------------------------
						if(entryname[8]==' ') 
								return MMC_FOLDERNOTFOUND;
						else 
								return MMC_FILENOTFOUND; 
#endif	//----------------------------------------------------------------------------------
				}else { 
					currentCluster++;
					continue;
				}
			}
			else {
				if((currentCluster=getNextCluster(getRelOffset(currentCluster),mmcFATbuffer)) == 0 ) {
#ifdef MMC_FAT_CREATEFILE		//----------------------------------------------------------
						return makeEntry(currentFile.cluster,entryname,mmcFATbuffer);
#else	//----------------------------------------------------------------------------------
						if(entryname[8]==' ') 
								return MMC_FOLDERNOTFOUND;
						else 
								return MMC_FILENOTFOUND; 
#endif	//----------------------------------------------------------------------------------
				}
				continue;
			}
		}
	}while(1);
}
/******************************************************************************* mmc FAT Init */
// Reads Boot Sector and Initialize DriveInfo Global Structure
// returns MMC_FAT32 if FAT32 and MMC_FAT16 if FAT16 found
u08 mmcFatInit() {

	u08 mmcFATbuffer[512];
	BOOTSEC bootSectorData;
	fatRegister=0;

#ifdef MMC_FAT_FAST		//------------------------------
	currentFile.lastAccessCluster=0;
	currentFile.lastAccessSector=0;
	currentFile.lastAccessOffset=0;
#endif	

	mmcReset();
	mmcRead(0,mmcFATbuffer);

	if(mmcFATbuffer[0x0C]!=0x02) return MMC_FATFAILED;

	u08 *bootptr;
//Sector per Cluster----------------------------
	bootptr=(u08*)&bootSectorData.SecPerClust;
	*bootptr=mmcFATbuffer[0x0D];
//Reserved Sectors------------------------------
	bootptr=(u08*)&bootSectorData.ResSectors;
	*bootptr=mmcFATbuffer[0x0E];	bootptr++;
	*bootptr=mmcFATbuffer[0x0F];
//Root Dir Entries------------------------------
	bootptr=(u08*)&bootSectorData.RootDirEnts;
	*bootptr=mmcFATbuffer[0x11];	bootptr++;
	*bootptr=mmcFATbuffer[0x12];
//Total Sectors	4bytes--------------------------
	bootptr=(u08*)&bootSectorData.totalSectors;
	*bootptr=mmcFATbuffer[0x13];	bootptr++;
	*bootptr=mmcFATbuffer[0x14];	bootptr++;
	*bootptr=0;						bootptr++;
	*bootptr=0;
	if(bootSectorData.totalSectors==0) {
		bootptr=(u08*)&bootSectorData.totalSectors;
		*bootptr=mmcFATbuffer[0x20];		bootptr++;
		*bootptr=mmcFATbuffer[0x21];		bootptr++;
		*bootptr=mmcFATbuffer[0x22];		bootptr++;
		*bootptr=mmcFATbuffer[0x23];
	}

//FAT sectors	4bytes--------------------------
	bootptr=(u08*)&bootSectorData.FATsecs;
	*bootptr=mmcFATbuffer[0x16];	bootptr++;
	*bootptr=mmcFATbuffer[0x17];	bootptr++;
	*bootptr=0;						bootptr++;
	*bootptr=0;						
	if(bootSectorData.FATsecs==0) {
		bootptr=(u08*)&bootSectorData.FATsecs;
		*bootptr=mmcFATbuffer[0x24];		bootptr++;
		*bootptr=mmcFATbuffer[0x25];		bootptr++;
		*bootptr=mmcFATbuffer[0x26];		bootptr++;
		*bootptr=mmcFATbuffer[0x27];
	}

	currentDrive.fat1=bootSectorData.ResSectors;
	currentDrive.fat2=currentDrive.fat1 + bootSectorData.FATsecs;
	currentDrive.rootCluster=currentDrive.fat2 + bootSectorData.FATsecs;
	currentDrive.rootSize=(bootSectorData.RootDirEnts*32)/512;
	currentDrive.spc=bootSectorData.SecPerClust;

	if(bootSectorData.RootDirEnts==0) {
		currentDrive.fs = MMC_FAT32;
		fatRegister|=0x02;
		return MMC_FAT32;
	}
	else {
		currentDrive.fs = MMC_FAT16;
		fatRegister|=0x01;
		return MMC_FAT16;
	}
}
/******************************************************************************* mmc File Open */
//opens an existing file or create one. sets handler info.
//sample: mmcFileOpen("folder:filename.ext");
//use ':' as folder seprators '\' or '/'
u08 mmcFileOpen(char* filename) {
	char entryname[11];
	u08	i=0;
	currentFile.cluster = currentDrive.rootCluster;
	do {
		if(*filename==':') filename++;
		if(*filename==':') break;
		for(i=0;i<11;i++) {
			if(*filename==':')
				entryname[i]=' ';
			else if(*filename=='.') {
						if(i<8) entryname[i]=' ';
						else { filename++; i--; }
					}
			else if(*filename==0x00)
					entryname[i]=' ';
			else {
				entryname[i]=*filename;
				filename++;
			}
		}
	} while((i=ReadEntry(entryname))<0x10);
	return i;
}
/******************************************************************************* mmc Write File */
//Writes Data according to Given Data Size to End of The Current File
u08 mmcWriteFile(u08* outData,u32 dataSize) {
	u08 buffer[512];
	u32 lastCluster=currentFile.cluster;
	u08 sectNumber=0;
	u32 k;
#ifdef MMC_FAT_FAST		//------------------------------
	if( (fatRegister&0x04)==0x04 && currentFile.lastAccessCluster!=0) {
		lastCluster=currentFile.lastAccessCluster;
		sectNumber=currentFile.lastAccessSector;
		if(currentFile.lastAccessOffset==512) { k=0; sectNumber++; }
		else k=currentFile.lastAccessOffset;
	}
	else {
		while((k=getNextCluster(getRelOffset(lastCluster),buffer)) != 0 )
			lastCluster=k;
		if(currentFile.filesize!=0) {
			if((k=currentFile.filesize % (512*currentDrive.spc))==0) {
				k=getNewClusterCode(buffer);
				setClusterCode(getRelOffset(lastCluster),k,buffer);
				lastCluster=getAbsOffset(k);
				k=0;
			}
			while(k>=512) {
				sectNumber++;
				k-=512;
			}
		}
		else k=0;
	}
#else
	while((k=getNextCluster(getRelOffset(lastCluster),buffer)) != 0 )
		lastCluster=k;
	if(currentFile.filesize!=0) {
		if((k=currentFile.filesize % (512*currentDrive.spc))==0) {
			k=getNewClusterCode(buffer);
			setClusterCode(getRelOffset(lastCluster),k,buffer);
			lastCluster=getAbsOffset(k);
			k=0;
		}
		while(k>=512) {
			sectNumber++;
			k-=512;
		}
	}
	else k=0;
#endif

	fatRegister|=0x04;
	do {
		mmcRead(lastCluster + sectNumber,buffer);
		do {
			buffer[k]=*outData;
			outData++;
			dataSize--;
			currentFile.filesize++;
			k++;
		} while(dataSize!=0 && k<512);
#ifdef MMC_FAT_FAST		//------------------------------
		currentFile.lastAccessCluster=lastCluster;
		currentFile.lastAccessSector=sectNumber;
		currentFile.lastAccessOffset=k;
#endif					//------------------------------

		if(dataSize==0) {
			mmcWrite(lastCluster + sectNumber,buffer);
			updateFileSize(buffer);
			return MMC_OK;
		}
		else {
			mmcWrite(lastCluster + sectNumber,buffer);
			updateFileSize(buffer);
			if( currentDrive.spc!=1 && sectNumber< (currentDrive.spc-1) ) {
				sectNumber++;
			}
			else {
				k=getNewClusterCode(buffer);
				setClusterCode(getRelOffset(lastCluster),k,buffer);
				lastCluster=getAbsOffset(k);
				sectNumber=0;
			}
			k=0;
		}
	} while(1);
}
#ifdef MMC_FAT_COMPLETE
/******************************************************************************* mmc Read File */
//Reads Data according to Given Data Size from offset point
u08 mmcReadFile(u08* inData,u32 dataSize,u32 offset) {
	u08 buffer[512];
	u32 lastCluster=currentFile.cluster;
	u08 sectNumber=0;
	u32 k;
	if(offset>=currentFile.filesize) return MMC_WRONGOFFSET;
	if(currentFile.filesize==0) return MMC_FILESIZE_ZERO;

#ifdef MMC_FAT_FAST		//------------------------------
	if((fatRegister&0x04)==0x00 && currentFile.lastAccessCluster!=0) {
		lastCluster=currentFile.lastAccessCluster;
		sectNumber=currentFile.lastAccessSector;
		if(currentFile.lastAccessOffset==512) { k=0; sectNumber++; }
		else k=currentFile.lastAccessOffset;
	}
	else {
		while(offset>=(512*currentDrive.spc)) {
			if((k=getNextCluster(getRelOffset(lastCluster),buffer)) == 0 ) 
				return MMC_WRONGFILE;
			else {
				lastCluster=k;
				offset-=(512*currentDrive.spc);
			}
		}
		while(offset>=512) {
			sectNumber++;
			offset-=512;
		}
	}
#else
	while(offset>=(512*currentDrive.spc)) {
		if((k=getNextCluster(getRelOffset(lastCluster),buffer)) == 0) 
			return MMC_WRONGFILE;
		else {
			lastCluster=k;
			offset-=(512*currentDrive.spc);
		}
	}
	while(offset>=512) {
		sectNumber++;
		offset-=512;
	}
#endif

	fatRegister&=0xFB;

	do {
		mmcRead(lastCluster+sectNumber,buffer);
		do {
			*inData=buffer[offset];
			inData++;
			offset++;
			dataSize--;
		} while(dataSize!=0 && offset<512);
#ifdef MMC_FAT_FAST		//------------------------------
		currentFile.lastAccessCluster=lastCluster;
		currentFile.lastAccessSector=sectNumber;
		currentFile.lastAccessOffset=offset;
#endif					//------------------------------
		if(dataSize==0)
			return MMC_OK;
		else {
			if(currentDrive.spc!=1 && sectNumber<currentDrive.spc ) {
					sectNumber++;
			}
			else {
				if((k=getNextCluster(getRelOffset(lastCluster),buffer)) == 0) 
					return MMC_ENDFILE;
				lastCluster=k;
				sectNumber=0;
			}
			offset=0;
		}
	} while(1);
}
/******************************************************************************* mmc Delete Entry */
//deletes a file from disk...file space NOT released.
u08 mmcDeleteEntry() {
	u08 buffer[512];
	mmcRead(currentFile.fileInfoSector,buffer);
	buffer[currentFile.fileInfoOffset*32]=0xE5;
	mmcWrite(currentFile.fileInfoSector,buffer);
	return MMC_OK;
}
/******************************************************************************* mmc Removes Entry */
//Removes a file from disk...file space Released.
u08 mmcRemoveEntry() {
	u08 buffer[512];
	mmcRead(currentFile.fileInfoSector,buffer);
	buffer[currentFile.fileInfoOffset*32]=0xE5;
	mmcWrite(currentFile.fileInfoSector,buffer);

	u32 nextCluster;
	do {
		nextCluster=getNextCluster(getRelOffset(currentFile.cluster),buffer);
		setClusterCode(getRelOffset(currentFile.cluster),CLUST_FREE,buffer);
		if(nextCluster  == 0 ) break;
		currentFile.cluster=nextCluster;
	} while(1);
	currentFile.cluster=0;
	currentFile.filesize=0;
	return MMC_OK;
}
#endif //MMC_FAT_COMPLETE
