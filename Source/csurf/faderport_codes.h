#define SYSEX_HDR			   			   0xF0000106
#define FP16							   0x16
#define FP8								   0x02
#define FP16_SIZE						   16
#define FP8_SIZE						   8
#define FUNC_DC							   NULL

#define END								   0xF7

#define DEFAULT_TRACK_COLOUR			   0x00CCCC

#define RED_CHANNEL						   0x91
#define GREEN_CHANNEL					   0x92
#define BLUE_CHANNEL					   0x93

#define STATE_OFF						   0x00
#define STATE_BLINK						   0x01
#define STATE_ON						   0x7F	
#define PEAK_METER_1_8					   0xD0
#define PEAK_METER_9_16					   0xC0
#define REDUCTION_METER_1_8				   0xD9
#define REDUCTION_METER_9_16			   0xC9
#define FADER  							   0xE0
#define BTN  							   0x90
#define ENCODER  						   0xB0
#define VALUEBAR						   0xB0
#define VALUEBAR_VALUE_1_8				   0x30
#define VALUEBAR_VALUE_9_16				   0x40
#define VALUEBAR_MODE_OFFSET_1_8		   0x38
#define VALUEBAR_MODE_OFFSET_9_16		   0x48
#define MODE_NORMAL						   0x00
#define MODE_BIPOLAR					   0x01
#define MODE_FILL						   0x02
#define MODE_SPREAD						   0x03
#define MODE_OFF						   0X04	

#define SCRIBBLE_SEND_STRING			   0x12
#define SCRIBBLE						   0x13 
#define SCRIBBLE_CLEAR					   0x10 
#define SCRIBBLE_DEFAULT				   0x00
#define SCRIBBLE_ALT_DEFAULT		       0x01
#define SCRIBBLE_SMALL_TEXT	               0x02
#define SCRIBBLE_LARGE_TEXT                0x03
#define SCRIBBLE_LARGE_TEXT_METERING       0x04
#define SCRIBBLE_DEFAULT_TEXT_METERING     0x05
#define SCRIBBLE_MIXED_TEXT		           0x06
#define SCRIBBLE_ALT_TEXT_METERING		   0x07
#define SCRIBBLE_MIXED_TEXT_METERING	   0x08
#define SCRIBBLE_MENU		               0x09

#define RUNNING_STATUS		               0x0A0000