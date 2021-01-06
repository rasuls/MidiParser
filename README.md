                                        MIDI File Parser in C++ - Rasul Silva

I did this project to better understand MIDI, and also get some practice with file parsing in C++.
Knowing how to understand a format, procedure, or technology (based on its specification) is a valuable 
engineering skill and this project provided some good practice in that regard.

MIDI is a file format which is used to communicate musical information between devices. I recently picked 
up Music Production as a hobby, and i'm using MIDI all the time. I was curious to see how this worked, so 
I found the MIDI spec online and got to work!

This project provides a C++ class, which takes a MIDI file path as an input and prints the note contents
of that file to the console in a readable format. To clarify, all the MIDI interpetation is done from 
scratch, no third party libraries used. All standard C++! 

In addition to printing the note information from the MIDI file, the class will also store these in a vector 
which the user can process in anyway they please.

Code is built for the following specifications:
Based on RP-001_v1-0_Standard_MIDI_Files_Specification_96-1-4 and
https://web.archive.org/web/20141227205754/http://www.sonicspot.com:80/guide/midifiles.html
