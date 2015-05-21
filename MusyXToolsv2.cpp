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
	u8 rootKey;
	u8 loopFlag;
	u16 adsrID;
	int adsrIndex;
} macro;

typedef struct {
	u16 id;
	bool exists;
	u16 sampleID;
	u8 baseNote;
	u8 loopFlag;
	u8 startNote;
	u8 endNote;
	bool adsr;
	double attack;
	double decay;
	double sustain;
	double release;
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

typedef struct {
	bool exists;
	u32 size;
	u16 id;	// 
	u16 dummyA;
	u32 noteCount;
	vector<noteRegion> notes;
} layer;

typedef struct {
	u32 size;
	u16 id;
	u8 attack;
	u8 attackDecimal;
	u8 decay;
	u8 decayDecimal;
	u8 sustain;
	u8 sustainDecimal;
	u8 release;
	u8 releaseDecimal;
} table;

double LogB(double n, double b) {
	// log(n)/log(2) is log2.  
	return log(n) / log(b);
}
double getPan(double pan) {
	double realPan = (pan - 64) * 7.936507937;
	if (realPan < 0)
		return realPan + 65536;
	else
		return realPan;
}

double getVolume(double volume) {
	return 200 * abs(LogB(pow((volume / 127), 2), 10));
}

float getSustain(float sustain) {
	if (sustain == 0)
		return 900;
	else
		return 200 * abs(LogB(pow(((float)sustain / 255), 2), 10));
}

float timeToTimecents(float time){
	float timeCent = floor(1200 * LogB(time, 2));
	if (timeCent > -12000 && timeCent < 0)
		return timeCent + 65536;
	else if (timeCent < -12000)
		return -12000;
	else
		return timeCent;
}

// General MIDI instrument names
static const char *const general_MIDI_instr_names[128] =
{
	"Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano", "Rhodes Piano", "Chorused Piano",
	"Harpsichord", "Clavinet", "Celesta", "Glockenspiel", "Music Box", "Vibraphone", "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
	"Hammond Organ", "Percussive Organ", "Rock Organ", "Church Organ", "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
	"Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)", "Electric Guitar (muted)",
	"Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics", "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)",
	"Fretless Bass", "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2", "Violin", "Viola", "Cello", "Contrabass",
	"Tremelo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani", "String Ensemble 1", "String Ensemble 2", "SynthStrings 1",
	"SynthStrings 2", "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit", "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
	"French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2", "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
	"Oboe", "English Horn", "Bassoon", "Clarinet", "Piccolo", "Flute", "Recorder", "Pan Flute", "Bottle Blow", "Shakuhachi", "Whistle",
	"Ocarina", "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope lead)", "Lead 4 (chiff lead)", "Lead 5 (charang)",
	"Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)", "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
	"Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)", "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)",
	"FX 4 (atmosphere)", "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)", "Sitar", "Banjo", "Shamisen", "Koto",
	"Kalimba", "Bagpipe", "Fiddle", "Shanai", "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock", "Taiko Drum", "Melodic Tom",
	"Synth Drum", "Reverse Cymbal", "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet", "Telephone Ring", "Helicopter",
	"Applause", "Gunshot"
};

int main(int argc, const char* argv[])
{
	FILE *proj, *pool, *sdir;
	int i, j, k, instCount, actualInstCount = 0, drumCount, actualDrumCount = 0, curInstrument = 0;
	instrument instruments[128];
	instrument drums[128];
	vector<instrument> layers;
	u8 tempChar;
	u16 tempID;
	u32 tempSize, tempOffset, nextOffset, poolSize;

	if (argc < 4)
		printf("Usage:\n%s inst.proj inst.pool inst.sdir inst.samp", argv[0]);
	else {

		// Now getting sample info
		sdir = fopen(argv[3], "rb");
		vector<dsp> dsps;
		fseek(sdir, 0, SEEK_END);
		u32 sdirSize = ftell(sdir);
		fseek(sdir, 0, SEEK_SET);
		int dspCount = (sdirSize - 4) / (0x20 + 0x28);
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

		// Reading pool
		pool = fopen(argv[2], "rb");
		fseek(pool, 0, SEEK_END);
		poolSize = ftell(pool);
		fseek(pool, 0, SEEK_SET);
		u32 macroOffset = ReadBE(pool, 32);
		u32 adsrOffset = ReadBE(pool, 32);
		u32 keymapOffset = ReadBE(pool, 32);
		u32 layerOffset = ReadBE(pool, 32);
		
		// Checking ADSR first
		fseek(pool, adsrOffset, SEEK_SET);
		nextOffset = tempOffset = ftell(pool);
		vector<table> tables;
		int tableCount = 0;
		while (ftell(pool) < keymapOffset) {
			tempSize = ReadBE(pool, 32);
			nextOffset += tempSize;
//			if (tempSize == 0x10) {	// Only know how to read these tables so far
				tempID = ReadBE(pool, 16);
				if (tempID != 0xffff) {
					tableCount++;
					tables.resize(tableCount);
					tables[tableCount - 1].size = tempSize;
					tables[tableCount - 1].id = tempID;
					
					fseek(pool, tempSize - 0xe, SEEK_CUR);
					tables[tableCount - 1].attack = ReadBE(pool, 8);
					tables[tableCount - 1].attackDecimal = ReadBE(pool, 8);
					tables[tableCount - 1].decay = ReadBE(pool, 8);
					tables[tableCount - 1].decayDecimal = ReadBE(pool, 8);
					tables[tableCount - 1].sustain = ReadBE(pool, 8);
					tables[tableCount - 1].sustainDecimal = ReadBE(pool, 8);
					tables[tableCount - 1].release = ReadBE(pool, 8);
					tables[tableCount - 1].releaseDecimal = ReadBE(pool, 8);
				}
				else 
					fseek(pool, nextOffset, SEEK_SET);
//			}
//			else
//				fseek(pool, nextOffset, SEEK_SET);
		}

		
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
						for (i = 0; i < dspCount; i++) {
							if (dsps[i].id == macros[macroCount - 1].sampleID) {
								macros[macroCount - 1].rootKey = dsps[i].baseNote;
								macros[macroCount - 1].loopFlag = dsps[i].loopFlag;
							}
						}
						printf("Macro %X uses sample # %X\n", macros[macroCount - 1].id, macros[macroCount - 1].sampleID);
						fseek(pool, 5, SEEK_CUR);
					}
					else if (tempChar == 0xc) {
						fseek(pool, -3, SEEK_CUR);
						macros[macroCount - 1].adsrID = ReadBE(pool, 16);
						for (i = 0; i < tableCount; i++) {
							if (tables[i].id == macros[macroCount - 1].adsrID) {
								macros[macroCount - 1].adsrIndex = i;
							}
						}
						fseek(pool, 5, SEEK_CUR);
					}
					else
						fseek(pool, 4, SEEK_CUR);
				}
			}
			else
				fseek(pool, nextOffset, SEEK_CUR);
				
		}
		
		// Checking layers
		printf("Checking instrument layers\n");
		fseek(pool, layerOffset, SEEK_SET);
		nextOffset = tempOffset = ftell(pool);
		int layerCount = 0;
		while (ftell(pool) < poolSize - 12) {
			tempSize = ReadBE(pool, 32);
			if (tempSize != 0xffffffff) {
				layerCount++;
				layers.resize(layerCount);
				nextOffset += tempSize;
				tempID = ReadBE(pool, 16);
				fseek(pool, 2, SEEK_CUR);
				layers[layerCount - 1].id = tempID;

				layers[layerCount - 1].noteCount = ReadBE(pool, 32);
				layers[layerCount - 1].notes.resize(layers[layerCount - 1].noteCount);
				printf("Layer %X at 0x%X with %d note regions\n", layers[layerCount - 1].id, ftell(pool), layers[layerCount - 1].noteCount);
				for (j = 0; j < layers[layerCount - 1].noteCount; j++) {
					tempID = ReadBE(pool, 16);
					if (tempID == 0xffff) {
						fseek(pool, 10, SEEK_CUR);
						layers[layerCount - 1].notes[j].exists = false;
					}
					else {

						for (k = 0; k < macroCount; k++) {
							if (macros[k].id == tempID) {
								printf("\tNote region %d uses macro %X\n", j, macros[k].id);
								layers[layerCount - 1].notes[j].sampleID = macros[k].sampleID;
								layers[layerCount - 1].notes[j].baseNote = macros[k].rootKey;
								layers[layerCount - 1].notes[j].loopFlag = macros[k].loopFlag;
								layers[layerCount - 1].notes[j].exists = true;
								layers[layerCount - 1].notes[j].startNote = ReadBE(pool, 8);
								layers[layerCount - 1].notes[j].endNote = ReadBE(pool, 8);
								layers[layerCount - 1].notes[j].transpose = ReadBE(pool, 8);
								layers[layerCount - 1].notes[j].volume = ReadBE(pool, 8);
								fseek(pool, 2, SEEK_CUR);
								layers[layerCount - 1].notes[j].pan = ReadBE(pool, 8);
								fseek(pool, 3, SEEK_CUR);
								if (macros[k].adsrIndex != NULL) {
									layers[layerCount - 1].notes[j].adsr = true;
									layers[layerCount - 1].notes[j].attack = (float)(tables[macros[j].adsrIndex].attack / 1e3) + (float)(tables[macros[j].adsrIndex].attackDecimal * 256 / 1e3);
									layers[layerCount - 1].notes[j].decay = (float)(tables[macros[j].adsrIndex].decay / 1e3) + (float)(tables[macros[j].adsrIndex].decayDecimal * 256 / 1e3);
									layers[layerCount - 1].notes[j].sustain = (float)(tables[macros[k].adsrIndex].sustain) * 0.0244 + (float)(tables[macros[k].adsrIndex].sustainDecimal * 6.25);
									layers[layerCount - 1].notes[j].release = (float)(tables[macros[j].adsrIndex].release / 1e3) + (float)(tables[macros[j].adsrIndex].releaseDecimal * 256 / 1e3);
								}
								break;
							}
							//						else
							//							fseek(pool, 10, SEEK_CUR);
						}
					}
				}
			}
		}

		// Checking drum tables
		printf("Checking keymaps\n");
		fseek(pool, keymapOffset, SEEK_SET);
		int keymapCount = (layerOffset - keymapOffset - 4) / 0x408;
		layerCount += keymapCount;
		layers.resize(layerCount);
		for (i = layerCount - keymapCount; i < layerCount; i++) {
			printf("Reading keymap at 0x%X\n", ftell(pool));
			fseek(pool, 4, SEEK_CUR);
			tempID = ReadBE(pool, 16);
			fseek(pool, 2, SEEK_CUR);
			layers[i].id = tempID;
			layers[i].noteCount = 128;
			layers[i].notes.resize(128);
			
			// Now to try and map all note regions
			for (j = 0; j < 128; j++) {
				layers[i].notes[j].exists = false;
//				printf("Reading Keymap %d: Note Region %d\n", curInstrument, j);
				tempID = ReadBE(pool, 16);
				if (tempID == 0xffff) {
					fseek(pool, 6, SEEK_CUR);
				}
				else if (tempID & 0x8000) {	// Maps to layer, rather than macro						
					for (k = 0; k < layerCount - 1; k++) {	// Reading all layers prior to this one
						if (layers[k].id == tempID) {
							printf("Keymap %X note %d uses layer %X\n", layers[i].id, j, layers[k].id);
							layers[i].notes[j] = layers[k].notes[0];	// Only taking the first region for now
							layers[i].notes[j].transpose = ReadBE(pool, 8);
							layers[i].notes[j].pan = ReadBE(pool, 8);
							layers[i].notes[j].exists = true;
							fseek(pool, 4, SEEK_CUR);
							break;
						}
					}
						
				}
				else {
					for (k = 0; k < macroCount; k++) {
						if (macros[k].id == tempID) {
							printf("Keymap %X note %d uses macro %X\n", layers[i].id, j, macros[k].id);
							layers[i].notes[j].sampleID = macros[k].sampleID;
							layers[i].notes[j].baseNote = macros[k].rootKey;
							layers[i].notes[j].loopFlag = macros[k].loopFlag;
							if (macros[k].adsrIndex != NULL) {
								layers[i].notes[j].adsr = true;
								layers[i].notes[j].attack = (float)(tables[macros[j].adsrIndex].attack / 1e3) + (float)(tables[macros[j].adsrIndex].attackDecimal * 256 / 1e3);
								layers[i].notes[j].decay = (float)(tables[macros[j].adsrIndex].decay / 1e3) + (float)(tables[macros[j].adsrIndex].decayDecimal * 256 / 1e3);
								layers[i].notes[j].sustain = (float)(tables[macros[k].adsrIndex].sustain) * 0.0244 + (float)(tables[macros[k].adsrIndex].sustainDecimal * 6.25);
								layers[i].notes[j].release = (float)(tables[macros[j].adsrIndex].release / 1e3) + (float)(tables[macros[j].adsrIndex].releaseDecimal * 256 / 1e3);
							}
							layers[i].notes[j].exists = true;
							layers[i].notes[j].transpose = ReadBE(pool, 8);
							layers[i].notes[j].pan = ReadBE(pool, 8);
							fseek(pool, 4, SEEK_CUR);
							break;
						}
					}
				}
				layers[i].notes[j].startNote = j;
				layers[i].notes[j].endNote = j;
			}
		}


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
			instruments[i].notes.resize(1);
			instruments[i].notes[0].sampleID == NULL;
			drums[i].exists = false;
			drums[i].noteCount = 0;
			drums[i].notes.resize(1);
			drums[i].notes[0].sampleID == NULL;
		}
		for (i = 0; i < instCount; i++) {
			fseek(proj, projInstOffset + i * 6, SEEK_SET);
			tempID = ReadBE(proj, 16);
			if (tempID == 0xffff)
				continue;
			else if (tempID & 0x8000) {	// Normal layer section
				fseek(proj, 2, SEEK_CUR);
				tempChar = ReadBE(proj, 8);

				for (j = 0; j < layerCount; j++) {
					if (layers[j].id == tempID) {
						instruments[(int)tempChar] = layers[j];
						break;
					}
				}
				instruments[(int)tempChar].exists = true;
//				printf("Instrument %d exists\n", tempChar);
				printf("%s exists\n", general_MIDI_instr_names[tempChar]);
			}

			else if (tempID & 0x4000) {	// Keymap section 
				fseek(proj, 2, SEEK_CUR);
				tempChar = ReadBE(proj, 8);
				

				for (j = 0; j < layerCount; j++) {
					if (layers[j].id == tempID) {
						instruments[(int)tempChar] = layers[j];
						break;
					}
				}
				instruments[(int)tempChar].exists = true;
//				printf("Instrument %d exists as a keymap\n", tempChar);
				printf("%s exists as a keymap\n", general_MIDI_instr_names[tempChar]);
			}

			else {	// Instrument just has info at macro
				fseek(proj, 2, SEEK_CUR);
				tempChar = ReadBE(proj, 8);
				instruments[(int)tempChar].exists = true;

				instruments[(int)tempChar].id = tempID;
				instruments[(int)tempChar].notes.resize(1);
				instruments[(int)tempChar].notes[0].startNote = 0;
				instruments[(int)tempChar].notes[0].endNote = 127;
//				printf("Instrument %d exists as a single macro\n", tempChar);
				printf("%s exists as a single macro\n", general_MIDI_instr_names[tempChar]);
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

				for (j = 0; j < layerCount; j++) {
					if (layers[j].id == tempID) {
						drums[(int)tempChar] = layers[j];
						break;
					}
				}
				drums[(int)tempChar].exists = true;
				printf("Drumkit %d exists\n", tempChar);
			}
		}
		fclose(proj);

		printf("Taking care of instruments with only macros\n");
		for (i = 0; i < 128; i++) {
			if (instruments[i].exists && instruments[i].notes[0].sampleID == NULL) {
				printf("Looking for instrument %d macro\n", i);
				for (j = 0; j < macroCount; j++) {
					if (macros[j].id == instruments[i].id) {
						printf("\tMacro %X\n", macros[j].id);
						instruments[i].notes[0].sampleID = macros[j].sampleID;
						instruments[i].notes[0].baseNote = macros[j].rootKey;
						
						if (macros[j].adsrIndex != NULL) {
							instruments[i].notes[0].adsr = true;
							/*
							instruments[i].notes[0].attack = (float)(tables[macros[j].adsrIndex].attack / 1e3) + (float)(tables[macros[j].adsrIndex].attackDecimal / 1e6);
							instruments[i].notes[0].decay = (float)(tables[macros[j].adsrIndex].decay / 1e3) + (float)(tables[macros[j].adsrIndex].decayDecimal / 1e6);
							instruments[i].notes[0].sustain = (float)(tables[macros[k].adsrIndex].sustain) + (float)(tables[macros[k].adsrIndex].sustainDecimal / 1e3);
							instruments[i].notes[0].release = (float)(tables[macros[j].adsrIndex].release / 1e3) + (float)(tables[macros[j].adsrIndex].releaseDecimal / 1e6);
							*/
							instruments[i].notes[0].attack = (float)(tables[macros[j].adsrIndex].attack / 1e3) + (float)(tables[macros[j].adsrIndex].attackDecimal * 256 / 1e3);
							instruments[i].notes[0].decay = (float)(tables[macros[j].adsrIndex].decay / 1e3) + (float)(tables[macros[j].adsrIndex].decayDecimal * 256 / 1e3);
							instruments[i].notes[0].sustain = (float)(tables[macros[k].adsrIndex].sustain) * 0.0244 + (float)(tables[macros[k].adsrIndex].sustainDecimal * 6.25);
							instruments[i].notes[0].release = (float)(tables[macros[j].adsrIndex].release / 1e3) + (float)(tables[macros[j].adsrIndex].releaseDecimal * 256 / 1e3);
						}
						
						printf("\tRoot Key %X\n", macros[j].rootKey);
						instruments[i].notes[0].exists = true;
						instruments[i].notes[0].pan = 64;	// Assuming this sample is centered
						instruments[i].noteCount = 1;
						
						// Reserved for ADSR
					}
				}
			}
		}
		fclose(pool);

		ofstream bankTemplate("soundfontBuild.txt");
		stringstream bankTemplateText;
		string bankText;
		bankTemplateText << "[Samples]\n";

		printf("Writing samples\n");
		for (i = 0; i < dspCount; i++) {
			bankTemplateText << "\n    SampleName=" << hex << dsps[i].id << "\n        SampleRate=" << to_string(dsps[i].sampleRate) << "\n        Key=" << to_string(dsps[i].baseNote) << "\n        FineTune=0\n        Type=1\n";
		}
		bankTemplateText << "\n\n[Instruments]\n";

		
		for (i = 0; i < 128; i++) {
			if (drums[i].exists && drums[i].noteCount) {

				bankTemplateText << "\n    InstrumentName=Drum" << i << "\n";
				for (j = 0; j < drums[i].noteCount; j++) {

					if (drums[i].notes[j].exists) {
						printf("Printing Drum %d: Note Region %d\n", i, j);

						bankTemplateText << "\n        Sample=" << hex << drums[i].notes[j].sampleID << "\n";
						bankTemplateText << "            Z_LowKey=" << to_string(drums[i].notes[j].startNote) << "\n";
						bankTemplateText << "            Z_HighKey=" << to_string(drums[i].notes[j].endNote) << "\n";
						bankTemplateText << "            Z_LowVelocity=0\n";
						bankTemplateText << "            Z_HighVelocity=127\n";
						bankTemplateText << "            Z_overridingRootKey=" << to_string(drums[i].notes[j].baseNote - drums[i].notes[j].transpose) << "\n"; // + drums[i].notes[j].transpose
//						bankTemplateText << "            Z_initialAttenuation=" << to_string((int)floor(getVolume(drums[i].notes[j].volume))) << "\n";
						bankTemplateText << "            Z_pan=" << to_string((int)floor(getPan(drums[i].notes[j].pan))) << "\n";					
						/*
						if (drums[i].notes[j].adsr) {
							bankTemplateText << "            Z_attackVolEnv=" << to_string((int)timeToTimecents(drums[i].notes[j].attack)) << "\n";
							bankTemplateText << "            Z_decayVolEnv=" << to_string((int)timeToTimecents(drums[i].notes[j].decay)) << "\n";
//							bankTemplateText << "            Z_sustainVolEnv=" << to_string((int)floor(getSustain(drums[i].notes[j].sustain))) << "\n";
//							bankTemplateText << "            Z_sustainVolEnv=" << to_string((int)floor(drums[i].notes[j].sustain)) << "\n";
							bankTemplateText << "            Z_releaseVolEnv=" << to_string((int)timeToTimecents(drums[i].notes[j].release)) << "\n";
						}
						*/
						/*
						bankTemplateText << "            Z_holdVolEnv=" << to_string((int)instruments[j].notes[k].getHold()) << "\n";
						*/
						bankTemplateText << "            Z_sampleModes=" << dec << (int)(drums[i].notes[j].loopFlag) << "\n";
					}

					else
						continue;


				}
			}
			else
				continue;
		}

		for (i = 0; i < 128; i++) {
			if (instruments[i].exists && instruments[i].noteCount) {
				printf("Printing Instrument %d:\n", i);

				bankTemplateText << "\n    InstrumentName=Instrument" << i << "\n";
				for (j = 0; j < instruments[i].noteCount; j++) {

					if (instruments[i].notes[j].exists) {
						printf("\tNote Region %d\n", j);

						bankTemplateText << "\n        Sample=" << hex << instruments[i].notes[j].sampleID << "\n";
						bankTemplateText << "            Z_LowKey=" << to_string(instruments[i].notes[j].startNote) << "\n";
						bankTemplateText << "            Z_HighKey=" << to_string(instruments[i].notes[j].endNote) << "\n";
						bankTemplateText << "            Z_LowVelocity=0\n";
						bankTemplateText << "            Z_HighVelocity=127\n";
						bankTemplateText << "            Z_overridingRootKey=" << to_string(instruments[i].notes[j].baseNote - instruments[i].notes[j].transpose) << "\n"; // + instruments[i].notes[j].transpose
//						bankTemplateText << "            Z_initialAttenuation=" << to_string((int)floor(getVolume(instruments[i].notes[j].volume))) << "\n";
						bankTemplateText << "            Z_pan=" << to_string((int)floor(getPan(instruments[i].notes[j].pan))) << "\n";
						/*
						if (instruments[i].notes[j].adsr) {
							bankTemplateText << "            Z_attackVolEnv=" << to_string((int)timeToTimecents(instruments[i].notes[j].attack)) << "\n";
							bankTemplateText << "            Z_decayVolEnv=" << to_string((int)timeToTimecents(instruments[i].notes[j].decay)) << "\n";
//							bankTemplateText << "            Z_sustainVolEnv=" << to_string((int)floor(getSustain(instruments[i].notes[j].sustain))) << "\n";
//							bankTemplateText << "            Z_sustainVolEnv=" << to_string((int)floor(instruments[i].notes[j].sustain)) << "\n";
							bankTemplateText << "            Z_releaseVolEnv=" << to_string((int)timeToTimecents(instruments[i].notes[j].release)) << "\n";
						}
						*/
						/*
						bankTemplateText << "            Z_holdVolEnv=" << to_string((int)instruments[j].notes[k].getHold()) << "\n";
						
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
			if (drums[i].exists && drums[i].noteCount) {
				bankTemplateText << "\n    PresetName=Program" << i << "Drum\n        Bank=128\n        Program=" << i << "\n";
				bankTemplateText << "\n        Instrument=Drum" << i << "\n            L_LowKey=0\n            L_HighKey=127\n            L_LowVelocity=0\n            L_HighVelocity=127\n\n";
			}
			else
				continue;
		}

		for (i = 0; i < 128; i++) {
			if (instruments[i].exists && instruments[i].noteCount) {
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
