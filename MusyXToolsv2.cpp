// MusyXToolsv2.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <direct.h>
#include <math.h>

using namespace std;

/* Structures for Sound Archives and their sub-sections */

typedef   signed char      s8;
typedef unsigned char      u8;
typedef   signed short     s16;
typedef unsigned short     u16;
typedef   signed int       s32;
typedef unsigned int       u32;


static inline u32 ReadBE(FILE *f, s32 b) {
	u32 v = 0;
	for (s32 i = b - 8; i >= 0; i -= 8) v |= fgetc(f) << i;
	return v;
}

typedef struct {
	u32 id;
	u32 offset;
	u32 size;
	u32 sampOffset;
	u32 coeffOffset;
	u8 baseNote;
	u8 loopFlag;
	u32 sampleRate;
	u32 sampleCount;
	u32 loopStart;
	u32 loopLength;
	u32 infoOffset;
	u32 channels;
	u16 adpcmCoeff[16];
} dsp;

typedef struct {
	u32 size;
	u16 id;	// 
	u16 sampleID;
	u16 adsrID;
} macro;

typedef struct {
	u16 id;
	bool exists;
	u16 sampleID;
	u8 baseNote;
	u8 loopFlag;
	u8 startNote;
	u8 endNote;
	s8 transpose;
	u8 volume;
	u8 pan;
	u8 sourr;
	u8 prioOfs;
} noteRegion; // noteRegion

typedef struct {
	bool exists;
	u32 size;
	u16 id;	// 
	u16 dummyA;
	u32 noteCount;
	vector<noteRegion> notes;
} instrument;



double getPan(double pan) {
	double realPan = (pan - 64) * 7.936507937;
	if (realPan < 0)
		return realPan + 65536;
	else
		return realPan;
}

int main(int argc, const char* argv[])
{
	FILE *proj, *pool, *sdir;
	int i, j, k, instCount, actualInstCount = 0, drumCount, actualDrumCount = 0, curInstrument = 0;
	instrument instruments[128];
	instrument drums[128];
	u8 tempChar;
	u16 tempID;
	u32 tempSize, tempOffset, nextOffset, poolSize;

	if (argc < 4)
		printf("Usage:\n%s inst.proj inst.pool inst.sdir inst.samp", argv[0]);
	else {


		// Getting our instrument info from proj
		proj = fopen(argv[1], "rb");
		fseek(proj, 0x1c, SEEK_SET);
		u32 projInstOffset = ReadBE(proj, 32);
		u32 projDrumOffset = ReadBE(proj, 32);
		u32 projFinalOffset = ReadBE(proj, 32);
		int instCount = (projDrumOffset - projInstOffset) / 6;
		int drumCount = (projFinalOffset - projDrumOffset) / 6;
		fseek(proj, projInstOffset, SEEK_SET);

		for (i = 0; i < 128; i++) {
			instruments[i].exists = false;
			instruments[i].noteCount = 0;
			drums[i].exists = false;
			drums[i].noteCount = 0;
		}
		for (i = 0; i < instCount; i++) {
			fseek(proj, projInstOffset + i * 6, SEEK_SET);
			tempID = ReadBE(proj, 16);
			if (tempID == 0xffff)
				continue;
			else {
				fseek(proj, 2, SEEK_CUR);
				tempChar = ReadBE(proj, 8);
				instruments[(int)tempChar].exists = true;
				actualInstCount++;
				instruments[(int)tempChar].id = tempID;
				instruments[(int)tempChar].notes.resize(128);
				printf("Instrument %d exists\n", tempChar);
			}
		}

		fseek(proj, projDrumOffset, SEEK_SET);
		for (i = 0; i < drumCount; i++) {
			fseek(proj, projDrumOffset + i * 6, SEEK_SET);
			tempID = ReadBE(proj, 16);
			if (tempID == 0xffff) {
				continue;
			}
			else {
				fseek(proj, 2, SEEK_CUR);
				tempChar = ReadBE(proj, 8);
				drums[(int)tempChar].exists = true;
				actualDrumCount++;
				drums[(int)tempChar].id = tempID;
				drums[(int)tempChar].notes.resize(128);
				printf("Drumkit %d exists\n", tempChar);
			}
		}
		fclose(proj);

		// Reading pool
		pool = fopen(argv[2], "rb");
		fseek(pool, 0, SEEK_END);
		poolSize = ftell(pool);
		fseek(pool, 0, SEEK_SET);
		u32 macroOffset = ReadBE(pool, 32);
		u32 adsrOffset = ReadBE(pool, 32);
		u32 keymapOffset = ReadBE(pool, 32);
		u32 layerOffset = ReadBE(pool, 32);
		
		// Should be at macro offset, but let's seek to be sure
		fseek(pool, macroOffset, SEEK_SET);
		nextOffset = tempOffset = ftell(pool);
		vector<macro> macros;
		int macroCount = 0;
		while (ftell(pool) < adsrOffset) {
			tempSize = ReadBE(pool, 32);
			nextOffset += tempSize;
			tempID = ReadBE(pool, 16);
			if (tempID != 0xffff) {
				macroCount++;
				macros.resize(macroCount);
				macros[macroCount - 1].id = tempID;

				fseek(pool, 2, SEEK_CUR);
				tempOffset = ftell(pool);
				// Looking for sample info
				while (ftell(pool) < nextOffset) {
					fseek(pool, 3, SEEK_CUR);
					tempChar = ReadBE(pool, 8);
					if (tempChar == 0x10) {
						fseek(pool, -3, SEEK_CUR);
						macros[macroCount - 1].sampleID = ReadBE(pool, 16);
						printf("Macro %X uses sample # %X\n", macroCount - 1, macros[macroCount - 1].sampleID);
						fseek(pool, 5, SEEK_CUR);
					}
					else if (tempChar == 0xc) {
						fseek(pool, -3, SEEK_CUR);
						macros[macroCount - 1].adsrID = ReadBE(pool, 16);
						fseek(pool, 5, SEEK_CUR);
					}
					else
						fseek(pool, 4, SEEK_CUR);
				}
			}
			else
				fseek(pool, adsrOffset, SEEK_SET);
		}

		// Reserved for ADSR

		// Checking drum tables
		printf("Checking drum tables\n");
		fseek(pool, keymapOffset, SEEK_SET);
		for (i = 0; i < actualDrumCount; i++) {
			fseek(pool, 4, SEEK_CUR);
			tempID = ReadBE(pool, 16);
			fseek(pool, 2, SEEK_CUR);
			for (j = 0; j < 128; j++) {
				if (drums[j].exists && drums[j].id == tempID) {
					curInstrument = j;
					drums[curInstrument].noteCount = 128;
					drums[curInstrument].notes.resize(128);
					break;
				}
				else
					continue;
			}
			
			// Now to try and map all note regions
			for (j = 0; j < 128; j++) {
				printf("Reading Drum %d: Note Region %d\n", curInstrument, j);
				drums[curInstrument].notes[j].startNote = j;
				drums[curInstrument].notes[j].endNote = j;
				tempID = ReadBE(pool, 16);
				if (tempID == 0xffff) {
					fseek(pool, 6, SEEK_CUR);
					drums[curInstrument].notes[j].exists = false;
				}
				else {
					for (k = 0; k < macroCount; k++) {
						if (macros[k].id == tempID) {
							printf("Drumkit %d note %d uses macro %X\n", i, j, k);
							drums[curInstrument].notes[j].sampleID = macros[k].sampleID;
							drums[curInstrument].notes[j].exists = true;
							drums[curInstrument].notes[j].transpose = ReadBE(pool, 8);
							drums[curInstrument].notes[j].pan = ReadBE(pool, 8);
							fseek(pool, 4, SEEK_CUR);
							break;
						}
					}
				}
			}
		}

		// Checking remaining instrument layers
		printf("Checking instrument layers\n");
		fseek(pool, layerOffset, SEEK_SET);
		nextOffset = tempOffset = ftell(pool);
		while (ftell(pool) < poolSize - 12) {
			tempSize = ReadBE(pool, 32);
			nextOffset += tempSize;
			tempID = ReadBE(pool, 16);
			fseek(pool, 2, SEEK_CUR);
			for (j = 0; j < 128; j++) {
				if (instruments[j].exists && instruments[j].id == tempID) {
					curInstrument = j;
					break;
				}
				else
					continue;
			}
			instruments[curInstrument].noteCount = ReadBE(pool, 32);
			instruments[curInstrument].notes.resize((int)instruments[curInstrument].noteCount);
			for (j = 0; j < instruments[curInstrument].noteCount; j++) {
				tempID = ReadBE(pool, 16);
				if (tempID == 0xffff) {
					fseek(pool, 10, SEEK_CUR);
					instruments[curInstrument].notes[j].exists = false;
				}
				else {
					printf("Instrument %d at 0x%X\n", curInstrument, ftell(pool));
					for (k = 0; k < macroCount; k++) {
						if (macros[k].id == tempID) {
							printf("\tNote region %d uses macro %X\n", j, k);
							instruments[curInstrument].notes[j].sampleID = macros[k].sampleID;
							instruments[curInstrument].notes[j].exists = true;
							instruments[curInstrument].notes[j].startNote = ReadBE(pool, 8);
							instruments[curInstrument].notes[j].endNote = ReadBE(pool, 8);
							instruments[curInstrument].notes[j].transpose = ReadBE(pool, 8);
							instruments[curInstrument].notes[j].volume = ReadBE(pool, 8);
							fseek(pool, 2, SEEK_CUR);
							instruments[curInstrument].notes[j].pan = ReadBE(pool, 8);
							fseek(pool, 3, SEEK_CUR);
							break;
						}
						else
							fseek(pool, 10, SEEK_CUR);
					}
				}
			}
		}

		fclose(pool);

		// Now getting sample info
		sdir = fopen(argv[3], "rb");
		vector<dsp> dsps;
		fseek(sdir, 0, SEEK_END);
		u32 sdirSize = ftell(sdir);
		fseek(sdir, 0, SEEK_SET);
		int dspCount = (sdirSize - 4) / (32 + 40);
		dsps.resize(dspCount);
		for (i = 0; i < dspCount; i++) {
			dsps[i].id = ReadBE(sdir, 16);
			printf("Reading sample %X\n", dsps[i].id);
			fseek(sdir, 2, SEEK_CUR);
			dsps[i].sampOffset = ReadBE(sdir, 32); 
			fseek(sdir, 4, SEEK_CUR);
			dsps[i].baseNote = ReadBE(sdir, 8);
			fseek(sdir, 1, SEEK_CUR);
			dsps[i].sampleRate = ReadBE(sdir, 16);
			dsps[i].sampleCount = ReadBE(sdir, 32);
			dsps[i].loopStart = ReadBE(sdir, 32);
			dsps[i].loopLength = ReadBE(sdir, 32);
			if (dsps[i].loopLength > 0)
				dsps[i].loopFlag = 1;
			else
				dsps[i].loopFlag = 0;
			dsps[i].infoOffset = ReadBE(sdir, 32);
		}

		fclose(sdir);

		ofstream bankTemplate("soundfontBuild.txt");
		stringstream bankTemplateText;
		string bankText;
		bankTemplateText << "[Samples]\n";

		for (i = 0; i < dspCount; i++) {
			bankTemplateText << "\n    SampleName=" << hex << dsps[i].id << "\n        SampleRate=" << to_string(dsps[i].sampleRate) << "\n        Key=" << to_string(dsps[i].baseNote) << "\n        FineTune=0\n        Type=1\n";
		}
		bankTemplateText << "\n\n[Instruments]\n";

		
		for (i = 0; i < 128; i++) {
			if (drums[i].exists) {

				bankTemplateText << "\n    InstrumentName=Drum" << i << "\n";
				for (j = 0; j < 128; j++) {

					if (drums[i].notes[j].exists) {
						printf("Printing Drum %d: Note Region %d\n", i, j);
						for (k = 0; k < dspCount; k++) {
							if (dsps[k].id == drums[i].notes[j].sampleID) {
								drums[i].notes[j].loopFlag = dsps[k].loopFlag;
								drums[i].notes[j].baseNote = dsps[k].baseNote;
								break;
							}
							else
								continue;
						}

						bankTemplateText << "\n        Sample=" << hex << drums[i].notes[j].sampleID << "\n";
						bankTemplateText << "            Z_LowKey=" << to_string(drums[i].notes[j].startNote) << "\n";
						bankTemplateText << "            Z_HighKey=" << to_string(drums[i].notes[j].endNote) << "\n";
						bankTemplateText << "            Z_LowVelocity=0\n";
						bankTemplateText << "            Z_HighVelocity=127\n";
						bankTemplateText << "            Z_overridingRootKey=" << to_string(drums[i].notes[j].baseNote) << "\n";
						//						bankTemplateText << "            Z_initialAttenuation=" << to_string((int)floor(instruments[j].notes[k].getVolume())) << "\n";
						bankTemplateText << "            Z_pan=" << to_string((int)floor(getPan(drums[i].notes[j].pan))) << "\n";
						/*						bankTemplateText << "            Z_attackVolEnv=" << to_string((int)instruments[j].notes[k].getAttack()) << "\n";
												bankTemplateText << "            Z_holdVolEnv=" << to_string((int)instruments[j].notes[k].getHold()) << "\n";
												bankTemplateText << "            Z_decayVolEnv=" << to_string((int)floor(instruments[j].notes[k].getDecay())) << "\n";
												bankTemplateText << "            Z_releaseVolEnv=" << to_string((int)floor(instruments[j].notes[k].getRelease())) << "\n";
												bankTemplateText << "            Z_sustainVolEnv=" << to_string((int)floor(instruments[j].notes[k].getSustain())) << "\n";
												*/
						bankTemplateText << "            Z_sampleModes=" << dec << (int)(drums[i].notes[j].loopFlag) << "\n";
					}

					else
						continue;


			}
			else
				continue;
		}


		for (i = 0; i < 128; i++) {
			if (instruments[i].exists) {
				printf("Printing Instrument %d:\n", i);

				bankTemplateText << "\n    InstrumentName=Instrument" << i << "\n";
				for (j = 0; j < instruments[i].noteCount; j++) {

					if (instruments[i].notes[j].exists) {
						printf("\tNote Region %d\n", j);
						for (k = 0; k < dspCount; k++) {
							if (dsps[k].id == instruments[i].notes[j].sampleID) {
								instruments[i].notes[j].loopFlag = dsps[k].loopFlag;
								instruments[i].notes[j].baseNote = dsps[k].baseNote;
								break;
							}
							else
								continue;
						}

						bankTemplateText << "\n        Sample=" << hex << instruments[i].notes[j].sampleID << "\n";
						bankTemplateText << "            Z_LowKey=" << to_string(instruments[i].notes[j].startNote) << "\n";
						bankTemplateText << "            Z_HighKey=" << to_string(instruments[i].notes[j].endNote) << "\n";
						bankTemplateText << "            Z_LowVelocity=0\n";
						bankTemplateText << "            Z_HighVelocity=127\n";
						bankTemplateText << "            Z_overridingRootKey=" << to_string(instruments[i].notes[j].baseNote) << "\n";
						//						bankTemplateText << "            Z_initialAttenuation=" << to_string((int)floor(instruments[j].notes[k].getVolume())) << "\n";
						bankTemplateText << "            Z_pan=" << to_string((int)floor(getPan(instruments[i].notes[j].pan))) << "\n";
						/*						bankTemplateText << "            Z_attackVolEnv=" << to_string((int)instruments[j].notes[k].getAttack()) << "\n";
						bankTemplateText << "            Z_holdVolEnv=" << to_string((int)instruments[j].notes[k].getHold()) << "\n";
						bankTemplateText << "            Z_decayVolEnv=" << to_string((int)floor(instruments[j].notes[k].getDecay())) << "\n";
						bankTemplateText << "            Z_releaseVolEnv=" << to_string((int)floor(instruments[j].notes[k].getRelease())) << "\n";
						bankTemplateText << "            Z_sustainVolEnv=" << to_string((int)floor(instruments[j].notes[k].getSustain())) << "\n";
						*/
						bankTemplateText << "            Z_sampleModes=" << dec << (int)(instruments[i].notes[j].loopFlag) << "\n";
					}

					else
						continue;
				}



			}
			else
				continue;
		}


		bankTemplateText << "\n\n[Presets]\n";

		for (i = 0; i < 128; i++) {
			if (drums[i].exists) {
				bankTemplateText << "\n    PresetName=Program" << i << "Drum\n        Bank=128\n        Program=" << i << "\n";
				bankTemplateText << "\n        Instrument=Drum" << i << "\n            L_LowKey=0\n            L_HighKey=127\n            L_LowVelocity=0\n            L_HighVelocity=127\n\n";
			}
			else
				continue;
		}

		for (i = 0; i < 128; i++) {
			if (instruments[i].exists) {
				bankTemplateText << "\n    PresetName=Program" << i << "Instrument\n        Bank=0\n        Program=" << i << "\n";
				bankTemplateText << "\n        Instrument=Instrument" << i << "\n            L_LowKey=0\n            L_HighKey=127\n            L_LowVelocity=0\n            L_HighVelocity=127\n\n";
			}
			else
				continue;
		}

		bankTemplateText << "\n[Info]\nVersion=2.1\nEngine=EMU8000 \nName=" << "golf" << "\nROMName=\nROMVersion=0.0\nDate=\nDesigner=\nProduct=\nCopyright=\nEditor=Awave Studio v10.6  \nComments=\n";

		

		bankText = bankTemplateText.str();
		bankTemplate << bankText;
		bankTemplate.close();
	}
	return 0;
}
