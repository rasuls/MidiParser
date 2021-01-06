/*
MIDI File Note Extractor - Takes a midi file and processes it in 2 useful ways:
	1. The program will print out all the noteOn/noteOff contents of the midi file in a readable format
	2. The program will place the Midi track note data into a c++ vector of vectors, which contains noteOn and noteOff events
	   only, other MIDI channel events are currently not added to the vector, but they may be added in the future

All code written by Rasul Silva,
Based on RP-001_v1-0_Standard_MIDI_Files_Specification_96-1-4 and
https://web.archive.org/web/20141227205754/http://www.sonicspot.com:80/guide/midifiles.html
*/
#include "pch.h"
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
using namespace std;

/*EventType enum holds values for Event types in Midi track
chunks. Inconsistency in naming convention is purposeful
in order to remain consistent with midi spec used.*/

enum EventType : uint8_t {
	noteOff = 0x8,
	noteOn = 0x9,
	noteAfterTouch = 0xA,
	controller = 0xB,
	programChange = 0xC,
	channelAfterTouch = 0xD,
	pitchBend = 0xE,
	metaEvent = 0xF
};

enum MetaEventType : uint8_t {
	sequenceNumber = 0x00,
	textEvent = 0x01,
	copyrightNotice = 0x02,
	sequenceTrackName = 0x03,
	instrumentName = 0x04,
	lyrics = 0x05,
	marker = 0x06,
	cuePoint = 0x07,
	midiChannelPrefix = 0x20,
	endOfTrack = 0x2F,
	setTempo = 0x51,
	smpteOffset = 0x54,
	timeSignature = 0x58,
	keySignature = 0x59,
	sequencerSpecific = 0x7F
};

struct Note {
	uint8_t noteNumber;
	bool on;
};

class MidiFileParser {
	public:
		MidiFileParser();
		MidiFileParser(const string& midiFileName);
		~MidiFileParser();
		vector <vector <Note>> getTrackNotes();
	private:
		struct Header;
		struct Track;
		struct Event;
		int swapEndianess32(uint32_t input);
		int swapEndianess16(uint16_t input);
		Header acquireHeaderData(ifstream& stream_object);
		bool isMSBHigh(uint8_t input);
		uint32_t readVariableLengthData(ifstream& stream_object);
		string readDefinedLengthData(ifstream& stream_object, uint32_t length);
		void doWork(const string& midiFileName);
		vector <vector <Note>> trackNotes;

};

MidiFileParser::MidiFileParser(){
	//no default constructor required
};
	
MidiFileParser::MidiFileParser(const string& midiFileName){
	doWork(midiFileName);
};

MidiFileParser::~MidiFileParser() {
	//nothing needed in destructor, stream will be closed after final read
};

struct MidiFileParser::Header {
	uint32_t chunk_type;
	uint32_t length;
	uint16_t format;
	uint16_t ntrks;
	uint16_t division;
};

struct MidiFileParser::Track {
	uint32_t chunk_type;
	uint32_t length;
};


int MidiFileParser::swapEndianess32(uint32_t input) {
	//performing operations individually for readability
	int byte0 = (input >> 24) & 0x000000ff;
	int byte1 = (input >> 8) & 0x0000ff00;
	int byte2 = (input << 8) & 0x00ff0000;
	int byte3 = (input << 24) & 0xff000000;
	return byte0 | byte1 | byte2 | byte3;
}

int MidiFileParser::swapEndianess16(uint16_t input) {
	//performing operations individually for readability
	int byte0 = (input >> 8) & 0x00ff;
	int byte1 = (input << 8) & 0xff00;
	return byte0 | byte1;
}

MidiFileParser::Header MidiFileParser::acquireHeaderData(ifstream& stream_object) {
	struct Header header_data;
	int header_data_size = 14;//hardcoding Header size for now because because byte padding causes sizeof() incorrect return value
	stream_object.read((char *)&header_data, header_data_size);

	//go through and swap Endianess of each item in header_data struct
	header_data.chunk_type = swapEndianess32(header_data.chunk_type);
	header_data.length = swapEndianess32(header_data.length);
	header_data.format = swapEndianess16(header_data.format);
	header_data.ntrks = swapEndianess16(header_data.ntrks);
	header_data.division = swapEndianess16(header_data.division);

	return header_data;
}

bool MidiFileParser::isMSBHigh(uint8_t input) {
	//return: True if Bit 8 is low, False if Bit 8 is high
	return ((input & 0x80) != 0);
}

uint32_t MidiFileParser::readVariableLengthData(ifstream& stream_object) {
	uint32_t result = 0;
	uint8_t temp;
	bool in_progress;

	stream_object.read((char *)&temp, sizeof(char));
	in_progress = isMSBHigh(temp);
	result = temp & 0x7F;

	while (in_progress) {
		stream_object.read((char *)&temp, sizeof(char));
		in_progress = isMSBHigh(temp);

		result = result << 7; //first shift result to the left by 7 bits, to make room in bottom 7 bits
		result = result | (temp & 0x7f); // then OR the temp value (with a masked 8th bit) into the bottom 7 bits 
	}

	return result;
}

string MidiFileParser::readDefinedLengthData(ifstream& stream_object, uint32_t length) {
	string value;
	char temp;
	for (uint32_t i = 0; i < length; i++) {
		stream_object.read((char *)&temp, sizeof(char));
		value += temp;
	}
	return value;
}

vector <vector <Note>> MidiFileParser::getTrackNotes(){
	return trackNotes;
}

void MidiFileParser::doWork(const string& midiFileName) {
	ifstream file(midiFileName , std::ios::in | std::ios::binary);
	if (!file) {
		cout << "-E- file read is not working!" << endl;
		//throw exception
	};

	struct Header header_chunk;
	header_chunk = acquireHeaderData(file);

	//some variables for Track chunk data reading
	struct Track track_chunk;

	uint32_t deltaTime = 0;
	uint8_t status = 0;
	uint8_t prevStatus = 0;//used for running status
	uint8_t statusUpper4Bits = 0;
	Note tempNote;
	bool reachedEndOfTrack = false;

	cout << "------------------- MIDI File parser -------------------" << endl;
	cout <<  "                " << header_chunk.ntrks << " MIDI tracks were found" << endl;
	cout <<  "                " <<"beginning processing now ..." << endl << endl << dec;

	for (uint16_t track_num = 0; track_num < header_chunk.ntrks; track_num++) {
		reachedEndOfTrack = false;
		vector <Note> notesVector;
		trackNotes.push_back(notesVector);

		cout << "------------------- TRACK NUMBER " << track_num << " -------------------" << endl;
		file.read((char *)&track_chunk, sizeof(track_chunk));
		track_chunk.chunk_type = swapEndianess32(track_chunk.chunk_type);
		track_chunk.length = swapEndianess32(track_chunk.length);

		/*ntrk structure = <delta-time><event>
		<event> = <MIDI event> | <sysex event> | <meta-event>
		first event will be status byte*/

		while (!reachedEndOfTrack) {

			deltaTime = readVariableLengthData(file);

			file.read((char *)&status, sizeof(char));
			statusUpper4Bits = (status >> 4); //Shift top 4 bits of byte to the bottom

			if (status < 0x80) {
				/*if status byte is less that 0x80 then it is not a status byte,
				but rather it is data, the stream pointer has still moved however,
				so to decorrupt our stream, we move our pointer backwards by 1*/
				status = prevStatus;
				statusUpper4Bits = (status >> 4);
				file.seekg(-1, std::ios_base::cur);
			}

			switch (statusUpper4Bits) {
			case (EventType::noteOff):
			{
				prevStatus = status;
				uint8_t midiChannel = 0, noteNumber = 0, velocity = 0;
				midiChannel = (status & 0x0F);
				file.read((char *)&noteNumber, sizeof(char));
				file.read((char *)&velocity, sizeof(char));
				cout << "noteOff -> noteNumber: " << int(noteNumber) << " velocity: " << velocity << " delta: " << deltaTime << endl;
				tempNote.noteNumber = noteNumber;
				tempNote.on = false;
				trackNotes[track_num].push_back(tempNote);
				break;
			}
			case (EventType::noteOn):
			{
				prevStatus = status;
				uint8_t midiChannel = 0, noteNumber = 0, velocity = 0;
				midiChannel = (status & 0x0F);
				file.read((char *)&noteNumber, sizeof(char));
				file.read((char *)&velocity, sizeof(char));
				cout << "noteOn -> noteNumber: " << int(noteNumber) << " velocity: " <<  velocity << " delta: " << deltaTime << endl;
				tempNote.noteNumber = noteNumber;
				tempNote.on = true;
				trackNotes[track_num].push_back(tempNote);
				break;
			}
			case (EventType::noteAfterTouch):
			{
				prevStatus = status;
				uint8_t midiChannel = 0, noteNumber = 0, amount = 0;
				midiChannel = (status & 0x0F);
				file.read((char *)&noteNumber, sizeof(char));
				file.read((char *)&amount, sizeof(char));
				cout << "noteAftertouch -> noteNumber: " << noteNumber << " amount: " << amount << endl;
				break;
			}
			case (EventType::controller):
			{
				prevStatus = status;
				uint8_t midiChannel = 0, controllerType = 0, value = 0;
				midiChannel = (status & 0x0F);
				file.read((char *)&controllerType, sizeof(char));
				file.read((char *)&value, sizeof(char));
				cout << "controller -> controllerType: " << controllerType << " value: " << value << endl;
				break;
			}
			case (EventType::programChange):
			{
				prevStatus = status;
				uint8_t midiChannel = 0, programNumber = 0;
				midiChannel = (status & 0x0F);
				file.read((char *)&programNumber, sizeof(char));
				cout << "programChange -> programNumber: " << programNumber << endl;
				break;
			}
			case (EventType::channelAfterTouch):
			{
				prevStatus = status;
				uint8_t midiChannel = 0, amount = 0;
				midiChannel = (status & 0x0F);
				file.read((char *)&amount, sizeof(char));
				cout << "channelAfterTouch -> amount: " << hex << amount << endl;
				break;
			}
			case (EventType::pitchBend):
			{
				prevStatus = status;
				uint8_t midiChannel = 0, valueLSB = 0, valueMSB = 0;
				midiChannel = (status & 0x0F);
				file.read((char *)&valueLSB, sizeof(char));
				file.read((char *)&valueMSB, sizeof(char));
				cout << "pitchBend -> valueLSB: " << valueLSB << " valueMSB: " << valueMSB << endl;
				break;
			}
			case (EventType::metaEvent):
			{
				prevStatus = status;
				uint8_t type = 0;
				uint32_t length = 0;

				if (status == 0xFF) {

					file.read((char *)&type, sizeof(char));
					length = readVariableLengthData(file);

					switch (type){
						case (MetaEventType::sequenceNumber):
						{
							uint8_t msb = file.get();
							uint8_t lsb = file.get();
							cout << "Sequence Number     MSB: " << msb << "   LSB: " << lsb << endl;
							break;
						}
						case (MetaEventType::textEvent):
						{
							string text = readDefinedLengthData(file, length);
							cout << "Text Event        Text: " << text << endl;
							break;
						}
						case (MetaEventType::copyrightNotice):
						{
							string text = readDefinedLengthData(file, length);
							cout << "Copyright       Text: " << text << endl;
							break;
						}
						case (MetaEventType::sequenceTrackName):
						{
							string text = readDefinedLengthData(file, length);
							cout << "SequenceTrack/Name       Text: " << text << endl;
							break;
						}
						case (MetaEventType::instrumentName):
						{
							string text = readDefinedLengthData(file, length);
							cout << "Instrument Name       Text: " << text << endl;
							break;
						}
						case (MetaEventType::lyrics):
						{
							string text = readDefinedLengthData(file, length);
							cout << "Lyrics       Text: " << text << endl;
							break;
						}
						case (MetaEventType::marker):
						{
							string text = readDefinedLengthData(file, length);
							cout << "Marker       Text: " << text << endl;
							break;
						}
						case (MetaEventType::cuePoint):
						{
							string text = readDefinedLengthData(file, length);
							cout << "Cue Point       Text: " << text << endl;
							break;
						}
						case (MetaEventType::midiChannelPrefix):
						{
							uint8_t channel = 0;
							file.read((char *)&channel, sizeof(char));
							cout << "MIDI Channel Prefix     Channel: " << channel << endl;
							break;
						}
						case (MetaEventType::endOfTrack): 
						{
							reachedEndOfTrack = true;
							cout << "End of Track has been reached " << endl << endl;
							break;
						}
						case (MetaEventType::setTempo): 
						{
							uint32_t bpm = 0, mspm = 0, byte0 = 0, byte1 = 0, byte2 = 0;
							file.read((char *)&byte0, sizeof(char));
							file.read((char *)&byte1, sizeof(char));
							file.read((char *)&byte2, sizeof(char));
							mspm = (byte0 << 16) | (byte1 << 8) | (byte0);
							bpm = 60000000 / mspm;
							cout << "SetTempo     MSPM: " << mspm << "   BPM: " << bpm << endl;
							break;
						}
						case (MetaEventType::smpteOffset): 
						{
							uint32_t hour = 0, min = 0, sec = 0, fr = 0, subFr = 0;
							file.read((char *)&hour, sizeof(char));
							file.read((char *)&min, sizeof(char));
							file.read((char *)&sec, sizeof(char));
							file.read((char *)&fr, sizeof(char));
							file.read((char *)&subFr, sizeof(char));
							cout << "SMPTE    (hour,min,sec,fr,subFr):(" << hour << "," << min << "," << sec << "," << subFr << endl;
							break;
						}
						case (MetaEventType::timeSignature):
						{
							uint8_t number = 0, denom = 0, metro = 0, thirtysecondnotes = 0;
							file.read((char *)&number, sizeof(char));
							file.read((char *)&denom, sizeof(char));
							file.read((char *)&metro, sizeof(char));
							file.read((char *)&thirtysecondnotes, sizeof(char));
							cout << "TimeSignature     number: " << number << "  denom: " << denom << "  metro: " << metro << " 32nd: " << thirtysecondnotes << endl;
							break;
						}
						case (MetaEventType::keySignature): 
						{
							uint8_t key = 0, scale = 0;
							file.read((char *)&key, sizeof(char));
							file.read((char *)&scale, sizeof(char));
							cout << "KeySignature     key: " << key << "  scale: " << scale << endl;
							break;
						}
						case (MetaEventType::sequencerSpecific): 
						{
							string text = readDefinedLengthData(file, length);
							break;
						}
					}
				}
				else if (status == 0xF0) {
					//sysex begin
					string text;
					file.read((char *)&type, sizeof(char));
					length = readVariableLengthData(file);
					text = readDefinedLengthData(file, length);
					cout << "Sysex Begin" << endl;
				}
				else if (status == 0xF7) {
					//sysex end
					string text;
					file.read((char *)&type, sizeof(char));
					length = readVariableLengthData(file);
					text = readDefinedLengthData(file, length);
					cout << "Sysex End" << endl;
				}
				else {
					cout << "STATUS BYTE ERROR    status = " << status << endl;
				}
				break;
			}
			};
		}
	}
	
	cout << "All tracks have been processed, closing file stream" << endl;
	file.close();//at this point we have processed all tracks, so close the stream
}


int main()
{
	MidiFileParser parser("my_midi_file.mid");
	vector <vector <Note>> notes = parser.getTrackNotes();
	return 0;
}


