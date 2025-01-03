/*
  Constants for ProjectorStepper project

*/

// -------------------------------------------------------------------------------------------------

// Digital PIN definitions
const int PIN_CLUTCH = 2;
const int PIN_MOTORON = 3;
const int PIN_MOTORDIRECTION1 = 4;
const int PIN_MOTORDIRECTION2 = 5;
const int PIN_FILMINTERRUPT = 6;
const int PIN_OPTICALSENSOR = 7;

// The below are likely not needed. We'd only use them if we use a Raspberry PI instead of a laptop
// to control the Arduino functions.
// I'm leaving them disabled for the time being, unless I find that I do need them.
/*
//const int runSwitch = 7;
//const int rewindFilm = 8;
//const int readyOut = 9;
//const int readyIn = 10;
*/

// -------------------------------------------------------------------------------------------------

// String commands to send or receive from the USB controller (e.g. laptop)
const String CMD_SEND = "CTS:";     // Prefix, indicates a command or acknowledgment to send to the laptop
const String CMD_RECEIVE = "STC:";  // Prefix, indicates a command or ack received from the laptop

const String CMD_OK = "OK";             // Acknowledgement. To be sent as a respond to each command
const String RSP_ERR = "ERROR: ";       // Error response. Will be followed by an error code or text
const String CMD_NEXTCELL = "NEXTCELL"; // Instruction to move to the next film cell
const String CMD_REWIND = "REWIND";     // Instruction to rewind the film
const String CMD_MOTORON = "MOTORON";   // Instruction to turn the main motor on
const String CMD_MOTOROFF = "MOTOROFF"; // Instruction to turn the main motor off
const String CMD_READY = "READY";       // Sent to the laptop to indicate we're ready to receive instructions
const String CMD_PING = "PING";         // Connection check sent from the laptop to test if the Arduino is online
const String CMD_ATCELL = "ATCELL";     // The film has been positioned and is ready for a photo capture
const String CMD_TESTOPTO = "OPTIC";    // Run a test function to report the state of the optic sensor

/*
char *myStrings[] = {"This is string 1", "This is string 2", "This is string 3",
                     "This is string 4", "This is string 5", "This is string 6"
                    };

*/

// -------------------------------------------------------------------------------------------------

const int FORWARD = 1;
const int BACKWARD = 0;
const int SWITCHON = 1;
const int SWITCHOFF = 0;

// -------------------------------------------------------------------------------------------------

// --- Globals --- 
int debugMode = 0;    // if set, then we're sending debug log messages to the laptop
int inPriorityCommand = 0;  // if set, then we're currently executing a command that can't be interrupted
