/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __TAS3004_HW__
#define __TAS3004_HW__

#include "AppleDBDMAAudio.h"
#include <IOKit/IOTypes.h>

#define		kDontRestoreOnNormal		0
#define		kRestoreOnNormal			1

#define		kNumberOfBiquadsPerChannel						6
#define		kNumberOfTAS3004BiquadsPerChannel				7
#define		kNumberOfCoefficientsPerBiquad					5
#define		kNumberOfBiquadCoefficientsPerChannel			( kNumberOfBiquadsPerChannel * kNumberOfCoefficientsPerBiquad )
#define		kNumberOfBiquadCoefficients						( kNumberOfBiquadCoefficientsPerChannel * kTAS3004MaxStreamCnt )
#define		kNumberOfTAS3004BiquadCoefficientsPerChannel	( kNumberOfTAS3004BiquadsPerChannel * kNumberOfCoefficientsPerBiquad )
#define		kNumberOfTAS3004BiquadCoefficients				( kNumberOfTAS3004BiquadCoefficientsPerChannel * kTAS3004MaxStreamCnt )
/*
 * Status register:
 */
#define		kHeadphoneBit		0x02

typedef UInt8	biquadParams[15];

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  CONSTANTS

#if 0 // plese don't delete, these may be used in the future
static UInt8	kTrebleRegValues[] = {
	0x01,	0x09,	0x10,	0x16,	0x1C,	0x22,					//	+18.0, +17.5, +17.0, +16.5, +16.0, +15.5	[dB]
	0x28,	0x2D,	0x32,	0x36,	0x3A,	0x3E,					//	+15.0, +14.5, +14.0, +13.5, +13.0, +12.5	[dB]
	0x42,	0x45,	0x49,	0x4C,	0x4F,	0x52,					//	+12.0, +11.5, +11.0, +10.5, +10.0, + 9.5	[dB]
	0x55,	0x57,	0x5A,	0x5C,	0x5E,	0x60,					//	+ 9.0, + 8.5, + 8.0, + 7.5, + 7.0, + 6.5	[dB]
	0x62,	0x63,	0x65,	0x66,	0x68,	0x69,					//	+ 6.0, + 5.5, + 5.0, + 4.5, + 4.0, + 3.5	[dB]
	0x6B,	0x6C,	0x6D,	0x6E,	0x70,	0x71,					//	+ 3.0, + 2.5, + 2.0, + 1.5, + 1.0, + 0.5	[dB]
	0x72,	0x73,	0x74,	0x75,	0x76,	0x77,					//	  0.0, - 0.5, - 1.0, - 1.5, - 2.0, - 2.5	[dB]
	0x78,	0x79,	0x7A,	0x7B,	0x7C,	0x7D,					//	- 3.0, - 3.5, - 4.0, - 4.5, - 5.0, - 5.5	[dB]
	0x7E,	0x7F,	0x80,	0x81,	0x82,	0x83,					//	- 6.0, - 6.5, - 7.0, - 7.5, - 8.0, - 8.5	[dB]
	0x84,	0x85,	0x86,	0x87,	0x88,	0x89,					//	- 9.0, - 9.5, -10.0, -10.5, -11.0, -11.5	[dB]
	0x8A,	0x8B,	0x8C,	0x8D,	0x8E,	0x8F,					//	-12.0, -12.5, -13.0, -13.5, -14.0, -14.5	[dB]
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,					//	-15.0, -15.5, -16.0, -16.5, -17.0, -17.5	[dB]
	0x96															//	-18.0										[dB]
};

static UInt8	kBassRegValues[] = {
	0x01,	0x03,	0x06,	0x08,	0x0A,	0x0B,					//	+18.0, +17.5, +17.0, +16.5, +16.0, +15.5	[dB]
	0x0D,	0x0F,	0x10,	0x12,	0x13,	0x14,					//	+15.0, +14.5, +14.0, +13.5, +13.0, +12.5	[dB]
	0x16,	0x17,	0x18,	0x19,	0x1C,	0x1F,					//	+12.0, +11.5, +11.0, +10.5, +10.0, + 9.5	[dB]
	0x21,	0x23,	0x25,	0x26,	0x28,	0x29,					//	+ 9.0, + 8.5, + 8.0, + 7.5, + 7.0, + 6.5	[dB]
	0x2B,	0x2C,	0x2E,	0x30,	0x31,	0x33,					//	+ 6.0, + 5.5, + 5.0, + 4.5, + 4.0, + 3.5	[dB]
	0x35,	0x36,	0x28,	0x39,	0x3B,	0x3C,					//	+ 3.0, + 2.5, + 2.0, + 1.5, + 1.0, + 0.5	[dB]
	0x3E,	0x40,	0x42,	0x44,	0x46,	0x49,					//	  0.0, - 0.5, - 1.0, - 1.5, - 2.0, - 2.5	[dB]
	0x4B,	0x4D,	0x4F,	0x51,	0x53,	0x54,					//	- 3.0, - 3.5, - 4.0, - 4.5, - 5.0, - 5.5	[dB]
	0x55,	0x56,	0x58,	0x59,	0x5A,	0x5C,					//	- 6.0, - 6.5, - 7.0, - 7.5, - 8.0, - 8.5	[dB]
	0x5D,	0x5F,	0x61,	0x64,	0x66,	0x69,					//	- 9.0, - 9.5, -10.0, -10.5, -11.0, -11.5	[dB]
	0x6B,	0x6D,	0x6E,	0x70,	0x72,	0x74,					//	-12.0, -12.5, -13.0, -13.5, -14.0, -14.5	[dB]
	0x76,	0x78,	0x7A,	0x7D,	0x7F,	0x82,					//	-15.0, -15.5, -16.0, -16.5, -17.0, -17.5	[dB]
	0x86															//	-18.0										[dB]
};
#endif

//#define kUSE_DRC		/*	when defined, enable DRC at -30.0 dB	*/

#if 0
static UInt32	gainHWTable[] = {									// added for [3134221] rbm, aml
	0x000000, 0x00014B, 0x00015F, 0x000174, 0x00018A, 0x0001A1, 0x0001BA, 0x0001D4,
	0x0001F0, 0x00020D, 0x00022C, 0x00024D, 0x000270, 0x000295, 0x0002BC, 0x0002E6,
	0x000312, 0x000340, 0x000372, 0x0003A6, 0x0003DD, 0x000418, 0x000456, 0x000498,
	0x0004DE, 0x000528, 0x000576, 0x0005C9, 0x000620, 0x00067D, 0x0006E0, 0x000748,
	0x0007B7, 0x00082C, 0x0008A8, 0x00092B, 0x0009B6, 0x000A49, 0x000AE5, 0x000B8B,
	0x000C3A, 0x000CF3, 0x000DB8, 0x000E88, 0x000F64, 0x00104E, 0x001145, 0x00124B,
	0x001361, 0x001487, 0x0015BE, 0x001708, 0x001865, 0x0019D8, 0x001B60, 0x001CFF,
	0x001EB7, 0x002089, 0x002276, 0x002481, 0x0026AB, 0x0028F5, 0x002B63, 0x002DF5,
	0x0030AE, 0x003390, 0x00369E, 0x0039DB, 0x003D49, 0x0040EA, 0x0044C3, 0x0048D6,
	0x004D27, 0x0051B9, 0x005691, 0x005BB2, 0x006121, 0x0066E3, 0x006CFB, 0x007370,
	0x007A48, 0x008186, 0x008933, 0x009154, 0x0099F1, 0x00A310, 0x00ACBA, 0x00B6F6,
	0x00C1CD, 0x00CD49, 0x00D973, 0x00E655, 0x00F3FB, 0x010270, 0x0111C0, 0x0121F9,
	0x013328, 0x01455B, 0x0158A2, 0x016D0E, 0x0182AF, 0x019999, 0x01B1DE, 0x01CB94,
	0x01E6CF, 0x0203A7, 0x022235, 0x024293, 0x0264DB, 0x02892C, 0x02AFA3, 0x02D862,
	0x03038A, 0x033142, 0x0361AF, 0x0394FA, 0x03CB50, 0x0404DE, 0x0441D5, 0x048268,
	0x04C6D0, 0x050F44, 0x055C04, 0x05AD50, 0x06036E, 0x065EA5, 0x06BF44, 0x07259D,
	0x079207, 0x0804DC, 0x087E80, 0x08FF59, 0x0987D5, 0x0A1866, 0x0AB189, 0x0B53BE,
	0x0BFF91, 0x0CB591, 0x0D765A, 0x0E4290, 0x0F1ADF, 0x100000, 0x10F2B4, 0x11F3C9,
	0x13041A, 0x14248E, 0x15561A, 0x1699C0, 0x17F094, 0x195BB8, 0x1ADC61, 0x1C73D5,
	0x1E236D, 0x1FEC98, 0x21D0D9, 0x23D1CD, 0x25F125, 0x2830AF, 0x2A9254, 0x2D1818,
	0x2FC420, 0x3298B0, 0x35982F, 0x38C528, 0x3C224C, 0x3FB278, 0x4378B0, 0x477828,
	0x4BB446, 0x5030A1, 0x54F106, 0x59F980, 0x5F4E32, 0x64F403, 0x6AEF5D, 0x714575,
	0x77FBAA, 0x7F17AF
};
#endif

#pragma mark -
#pragma mark •••••••• TAS3001C Registers ••••••••
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
enum TAS3004Registers {
	kTAS3004MainCtrl1Reg					=	0x01,	//	 1		C(7-0)
	kFL									=	7,		//	N.......	[bit address]	Load Mode:		0 = normal,		1 = Fast Load
	kSC									=	6,		//	.N......	[bit address]	SCLK frequency:	0 = 32 fs,		1 = 64 fs
	kE0									=	4,		//	..NN....	[bit address]	Output Serial Mode
	kF0									=	2,		//	....10..	[bit address]	
	kW0									=	0,		//	......NN	[bit address]	Serial Port Word Length

	kNormalLoad							=	0,		//	use:	( kNormalLoad << kFL )
	kFastLoad							=	1,		//	use:	( kFastLoad << kFL )
	
	kSerialModeLeftJust					=	0,		//	use:	( kSerialModeLeftJust << kEO ) …OR… ( kSerialModeLeftJust << kFO )
	kSerialModeRightJust				=	1,		//	use:	( kSerialModeRightJust << kEO ) …OR… ( kSerialModeRightJust << kFO )
	kSerialModeI2S						=	2,		//	use:	( kSerialModeI2S << kEO ) …OR… ( kSerialModeI2S << kFO )
	
	kSerialWordLength16					=	0,		//	use:	( kSerialWordLength16 << kWO )
	kSerialWordLength18					=	1,		//	use:	( kSerialWordLength18 << kWO )
	kSerialWordLength20					=	2,		//	use:	( kSerialWordLength20 << kWO )
	kSerialWordLength24					=	3,		//	use:	( kSerialWordLength24 << kWO )
	kSerialWordLengthMASK				=	3,		//	use:	( kSerialWordLengthMASK << kWO )
	
	k32fs								=	0,		//	use:	( k32fs << kSC )
	k64fs								=	1,		//	use:	( k64fs << kSC )
	
	kI2SMode							=	( kSerialModeI2S << kE0 ) | ( 2 << kF0 ),
	kLeftJustMode						=	( kSerialModeLeftJust << kE0 ) | ( kSerialModeLeftJust << kF0 ),
	kRightJustMode						=	( kSerialModeRightJust << kE0 ) | ( kSerialModeRightJust << kF0 ),
	
	kTAS3004DynamicRangeCtrlReg			=	0x02,	//	 5		RATIO(7-0), THRESHOLD(7-0), ENERGY(7-0), ATTACK(7-0), DECAY(7-0)
	kTAS3004VolumeCtrlReg				=	0x04,	//	 6		VL(23-16), VL(15-8), VL(7-0), VR(23-16), VR(15-8), VR(7-0)
	kTAS3004TrebleCtrlReg				=	0x05,	//	 1		T(7-0)
	kTAS3004BassCtrlReg					=	0x06,	//	 1		B(7-0)
	kTAS3004MixerLeftGainReg			=	0x07,	//	 9		S1L(23-16), S1L(15-8), S1L(7-0)
													//			S2L(23-16), S2L(15-8), S2L(7-0)
													//			AIL(23-16), AIL(15-8), AIL(7-0)
	kTAS3004MixerRightGainReg			=	0x08,	//	 9		S1R(23-16), S1R(15-8), S1R(7-0)
													//			S2R(23-16), S2R(15-8), S2R(7-0)
													//			AIR(23-16), AIR(15-8), AIR(7-0)
	kTAS3004LeftBiquad0CtrlReg			=	0x0A,	//	15		B0(23-16), B0(15-8), B0(7-0), 
													//			B1(23-16), B1(15-8), B1(7-0), 
													//			B2(23-16), B2(15-8), B2(7-0), 
													//			A1(23-16), A1(15-8), A1(7-0), 
													//			A2(23-16), A2(15-8), A2(7-0)
	kTAS3004LeftBiquad1CtrlReg			=	0x0B,	//	15		(same format as kLeftBiquad0CtrlReg)
	kTAS3004LeftBiquad2CtrlReg			=	0x0C,	//	15		(same format as kLeftBiquad0CtrlReg)
	kTAS3004LeftBiquad3CtrlReg			=	0x0D,	//	15		(same format as kLeftBiquad0CtrlReg)
	kTAS3004LeftBiquad4CtrlReg			=	0x0E,	//	15		(same format as kLeftBiquad0CtrlReg)
	kTAS3004LeftBiquad5CtrlReg			=	0x0F,	//	15		(same format as kLeftBiquad0CtrlReg)
	kTAS3004LeftBiquad6CtrlReg			=	0x10,	//	15		(same format as kLeftBiquad0CtrlReg)
	
	kTAS3004RightBiquad0CtrlReg			=	0x13,	//	15		(same format as kLeftBiquad0CtrlReg)
	kTAS3004RightBiquad1CtrlReg			=	0x14,	//	15		(same format as kRightBiquad0CtrlReg)
	kTAS3004RightBiquad2CtrlReg			=	0x15,	//	15		(same format as kRightBiquad0CtrlReg)
	kTAS3004RightBiquad3CtrlReg			=	0x16,	//	15		(same format as kRightBiquad0CtrlReg)
	kTAS3004RightBiquad4CtrlReg			=	0x17,	//	15		(same format as kRightBiquad0CtrlReg)
	kTAS3004RightBiquad5CtrlReg			=	0x18,	//	15		(same format as kRightBiquad0CtrlReg)
	kTAS3004RightBiquad6CtrlReg			=	0x19,	//	15		(same format as kRightBiquad0CtrlReg)
	kTAS3004LeftLoudnessBiquadReg		=	0x21,	//	15		(same format as kRightBiquad0CtrlReg)
	kTAS3004RightLoudnessBiquadReg		=	0x22,	//	15		(same format as kRightBiquad0CtrlReg)
	kTAS3004LeftLoudnessBiquadGainReg	=	0x23,	//	 3		LBG(23-16), LBG(15-8), LBG(7-0)
	kTAS3004RightLoudnessBiquadGainReg	=	0x24,	//	 3		RBG(23-16), RBG(15-8), RBG(7-0)
	kTAS3004TestReg						=	0x29,	//	10		RESERVED
	kTAS3004AnalogControlReg			=	0x40,	//	 1		Anal_ctrl(7-0)
	kTAS3004Test0x41Reg					=	0x41,	//	 1
	kTAS3004Text0x42Reg					=	0x42,	//	 1
	kTAS3004MainCtrl2Reg				=	0x43	//	 1		MCR2(7-0)
};

enum TAS3004RegistersSize {
	kTAS3004MainCtrl1Size				=	1,
	kTAS3004DynamicRangeCtrlSize		=	6,
	kTAS3004VolumeCtrlSize				=	6,
	kTAS3004TrebleCtrlSize				=	1,
	kTAS3004BassCtrlSize				=	1,
	kTAS3004MixerLeftGainSize			=	9,
	kTAS3004MixerRightGainSize			=	9,
	kTAS3004LeftBiquad0CtrlSize			=	15,
	kTAS3004LeftBiquad1CtrlSize			=	15,
	kTAS3004LeftBiquad2CtrlSize			=	15,
	kTAS3004LeftBiquad3CtrlSize			=	15,
	kTAS3004LeftBiquad4CtrlSize			=	15,
	kTAS3004LeftBiquad5CtrlSize			=	15,
	kTAS3004LeftBiquad6CtrlSize			=	15,
	kTAS3004RightBiquad0CtrlSize		=	15,
	kTAS3004RightBiquad1CtrlSize		=	15,
	kTAS3004RightBiquad2CtrlSize		=	15,
	kTAS3004RightBiquad3CtrlSize		=	15,
	kTAS3004RightBiquad4CtrlSize		=	15,
	kTAS3004RightBiquad5CtrlSize		=	15,
	kTAS3004RightBiquad6CtrlSize		=	15,
	kTAS3004LeftLoudnessBiquadSize		=	15,
	kTAS3004RightLoudnessBiquadSize		=	15,
	kTAS3004LeftLoudnessBiquadGainSize	=	3,
	kTAS3004RightLoudnessBiquadGainSize	=	3,
	kTAS3004TestSize					=	10,
	kTAS3004AnalogControlSize			=	1,
	kTAS3004Test0x41Size				=	1,
	kTAS3004Text0x42Size				=	1,
	kTAS3004MainCtrl2Size				=	1
};

enum AnalogControlReg {
	kADM					=	7,
	kLRB					=	6,
	kDM						=	2,
	kDM0					=	2,
	kDM1					=	3,
	kINP					=	1,
	kAPD					=	0,
	
	kADMNormal				=	0,		//	use:	( kADMNormal << kADM )
	kADMBInputsMonaural		=	1,		//	use:	( kADMBInputsMonaural << kADM )
	
	kLeftInputForMonaural	=	0,		//	use:	( kLeftInputForMonaural << kLRB )
	kRightInputForMonaural	=	1,		//	use:	( kRightInputForMonaural << kLRB )
	
	kDeEmphasisOFF			=	0,		//	use:	( kDeEmphasisOFF << kDM )
	kDeEmphasis48KHz		=	1,		//	use:	( kDeEmphasis48KHz << kDM )
	kDeEmphasis44KHz		=	2,		//	use:	( kDeEmphasis44KHz << kDM )
	
	kAnalogInputA			=	0,		//	use:	( kAnalogInputA << kINP )
	kAnalogInputB			=	1,		//	use:	( kAnalogInputB << kINP )
	
	kPowerNormalAnalog		=	0,		//	use:	( kPowerNormalAnalog << kAPD )
	kPowerDownAnalog		=	1,		//	use:	( kPowerDownAnalog << kAPD )
	kAPD_MASK				=	1		//	use:	( kAPD_MASK << kAPD )
};

enum MainCtrl2Reg {
	kDL						=	7,		//	use:	( kNormalBassTreble << kDL ) OR ( kLoadBassTreble << kDL )
	kNormalBassTreble		=	0,
	kLoadBassTreble			=	1,
	kAP						=	1,		//	use:	( kNormalFilter << kAP ) OR ( kAllPassFilter << kAP )
	kNormalFilter			=	0,
	kAllPassFilter			=	1,
	kFilter_MASK			=	1
};

enum DRCRegisterByteIndex {
		DRC_AboveThreshold,
		DRC_BelowThreshold,
		DRC_Threshold,
		DRC_Integration,
		DRC_Attack,
		DRC_Decay
};

enum TAS3004_DRC_Constants {
		kDisableDRC						=	0x01,	/*	Disable the DRC by setting above threshold to an odd value	*/
		kDRC_Integration_6point7ms		=	0x60
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
enum biquadInformation{
	kBiquadRefNum_0						=	 0,
	kBiquadRefNum_1						=	 1,
	kBiquadRefNum_2						=	 2,
	kBiquadRefNum_3						=	 3,
	kBiquadRefNum_4						=	 4,
	kBiquadRefNum_5						=	 5,
	kBiquadRefNum_6						=	 6,
	kTAS3004MaxBiquadRefNum				=	 6,
	kTAS3004NumBiquads					=	 7,
	kTAS3004CoefficientsPerBiquad		=	 5,
	kTAS3004CoefficientBitWidth			=	24,
	kTAS3004CoefficientIntegerBitWidth	=	 4,
	kTAS3004CoefficientFractionBitWidth	=	20
};

#define	TAS_I2S_MODE		kI2SMode
#define	TAS_WORD_LENGTH		kSerialWordLength20

#define	ASSERT_GPIO( x )					( 0 == x ? 0 : 1 )
#define	NEGATE_GPIO( x )					( 0 == x ? 1 : 0 )

enum TAS3004_registerWidths{
	kTAS3004MC1Rwidth					=	1,
	kTAS3004DRCwidth						=	6,
	kTAS3004VOLwidth						=	6,
	kTAS3004TREwidth						=	1,
	kTAS3004BASwidth						=	1,
	kTAS3004MIXERGAINwidth				=	9,
	kTAS3004BIQwidth						=	( 3 * 5 ),
	kTAS3004LoudnessBIQwidth				=	( 3 * 5 ),
	kTAS3004LOUDNESSBIQUADGAINwidth		=	3,
	kTAS3004ANALOGCTRLREGwidth			=	1,
	kTAS3004MC2Rwidth					=	1,
	kTAS3004MaximumRegisterWidth			=	( 3 * 5 )
};

enum mixerType{
	kMixerNone,
	kMixerTAS3001C
};

enum TAS3001Constants {
	kMixMute				= 0x00000000,	//	4.N two's complement with 1 sign bit, 3 integer bits & N bits of fraction
	kMix0dB					= 0x00100000	//	4.N two's complement with 1 sign bit, 3 integer bits & N bits of fraction
};

enum {
	kStreamCountMono			= 1,
	kStreamCountStereo			= 2
};

enum GeneralTAS3004HardwareAttributeConstants {
	kSampleRatesCount			=	2,
	kFrontLeftOFFSET			=	0,
	kFrontRightOFFSET			=	1,
	kTAS3004MaxStreamCnt		=	kStreamCountStereo,	//	Two streams are kStreamFrontLeft and kStreamFrontRight (see SoundHardwarePriv.h)
	kTAS3004MaxSndSystem		=	2,					//	TAS3004 supports kSndHWSystemBuiltIn and kSndHWSystemTelephony
	k16BitsPerChannel			=	16,
	kTAS3004InputChannelDepth	=	kStreamCountMono,
	kTAS3004InputFrameSize		=	16,
	kTouchBiquad				=	1,
	kBiquadUntouched			=	0
};

enum {
	// Regular, actually addressable streams (i.e. data can be streamed to and/or from these).
	// As such, these streams can be used with get and set current stream map;
	// get and set relative volume min, max, and current; and 
	// get hardware stream map
#if 0		/*		THESE ENUMERATIONS NOW APPEAR IN 'AudioHardwareConstants.h'		rbm		*/
	kStreamFrontLeft			= 'fntl',	// front left speaker
	kStreamFrontRight			= 'fntr',	// front right speaker
	kStreamSurroundLeft			= 'surl',	// surround left speaker
	kStreamSurroundRight		= 'surr',	// surround right speaker
	kStreamCenter				= 'cntr',	// center channel speaker
	kStreamLFE					= 'lfe ',	// low frequency effects speaker
	kStreamHeadphoneLeft		= 'hplf',	// left headphone
	kStreamHeadphoneRight		= 'hprt',	// right headphone
	kStreamLeftOfCenter			= 'loc ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[in front]
	kStreamRightOfCenter		= 'roc ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[in front]
	kStreamSurround				= 'sur ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[rear]
	kStreamSideLeft				= 'sidl',	//	see usb audio class spec. v1.0, section 3.7.2.3	[left wall]
	kStreamSideRight			= 'sidr',	//	see usb audio class spec. v1.0, section 3.7.2.3	[right wall]
	kStreamTop					= 'top ',	//	see usb audio class spec. v1.0, section 3.7.2.3	[overhead]
	kStreamMono					= 'mono',	//	for usb devices with a spatial configuration of %0000000000000000
#endif

	// Virtual streams. These are separately addressable streams as a "regular" stream; however, they are mutually
	// exclusive with one or more "regular" streams because they are not actually separate on the hardware device.
	// These selectors may be used with get and set current stream map; the relative volume calls; and get the
	// hardware stream map. However, using these in set current stream map will return an error if any of the 
	// exclusive relationships are violated by the request. 
	kStreamVirtualHPLeft		= 'vhpl',	// virtual headphone left - exclusive with front left speaker
	kStreamVirtualHPRight		= 'vhpr',	// virtual headphone right 

	// Embeddeded streams. These are not separately addressable streams and may never be used in the current stream 
	// map calls. Their presence in the hardware stream map indicates that there is a port present that can be 
	// volume controlled and thus these may also be used with the relative volume calls.
	kStreamEmbeddedSubwoofer	= 'lfei',	// embedded subwoofer
	kStreamBitBucketLeft		= 'sbbl',	//	VirtualHAL stream bit bucket left channel
	kStreamBitBucketRight		= 'sbbr',	//	VirtualHAL stream bit bucket right channel
	kMAX_STREAM_COUNT			= 20,		//	total number of unique stream identifiers defined in this enumeration list
	
	//	The following enumerations are related to the stream descriptions provided above but are not unique.
	//	They are intented to include multiple selections of the individual stream descriptions and as such
	//	are not to be included within the tally for kMAX_STREAM_COUNT.
	
	kStreamStereo				= 'flfr'	//	implies both 'fntl' and 'fntr' streams
};

#define kSetBass				1
#define kSetTreble				0
#define kToneGainToRegIndex		0x38E

#define kDrcThresholdMin			-35.9375				/*	minimum DRC threshold dB (i.e. -36.000 dB + 0.375 dB)	*/
#define	kDrcThresholdMax			/*0.0*/ 0				/*	maximum DRC threshold dB								*/

// For Mac OS X we can't use floats, so multiply by 1000 to make a normal interger
#define	kDrcThresholdStepSize		/*0.375*/	375			/*	dB per increment										*/
#define	kDrcUnityThresholdHW		(15 << 4 )				/*	hardware value for 0.0 dB DRC threshold					*/
#define	kDrcRatioNumerator			/*3.0*/		3
#define	kDrcRationDenominator		/*1.0*/		1
#define	kDefaultMaximumVolume		/*0.0*/		0
#define	kTAS3004VolumeStepSize		/*0.5*/		1
#define	kTAS3004MinVolume			/*-70.0*/	-70
#define	kTAS3004AbsMaxVolume		/*+18.0*/	18
/*#define kTAS3004VolToVolSteps	  1.82*/
#define	kTAS3004MaxIntVolume			256
// 225 was picked over 10 because it allows the part to come fully to volume after mute so that we don't loose the start of a sound
#define	kAmpRecoveryMuteDuration		225				/* expressed in milliseconds	*/
#define	kCodecResetMakeBreakDuration	10				/* expressed in milliseconds	*/
#define	kCodec_RESET_SETUP_TIME			 5				/* expressed in milliseconds	*/
#define	kCodec_RESET_HOLD_TIME			20				/* expressed in milliseconds	*/
#define	kCodec_RESET_RELEASE_TIME		10				/* expressed in milliseconds	*/

#define kTAS3004OutputSampleLatency	31
#define kTAS3004InputSampleLatency	32

#define kHeadphoneAmpEntry			"headphone-mute"
#define kSpeakerIDEntry				"speaker-id"
#define kAmpEntry					"amp-mute"
#define kLineOutAmpEntry			"line-output-mute"
#define kMasterAmpEntry				"master-mute"
#define kHWResetEntry				"audio-hw-reset"
#define kHeadphoneDetectInt			"headphone-detect"
#define kLineInDetectInt			"line-input-detect"
#define kLineOutDetectInt			"line-output-detect"
#define	kKWHeadphoneDetectInt		"keywest-gpio15"
#define kDallasDetectInt			"extint-gpio16"
#define kKWDallasDetectInt			"keywest-gpio16"
#define kVideoPropertyEntry			"video"
#define kSerialPropertyEntry		"headphone-serial"

#define kI2CDTEntry					"i2c"
#define kDigitalEQDTEntry			"deq"

#define kNumInputs					"#-inputs"
#define kCompatible					"compatible"
#define kI2CAddress					"i2c-address"
#define kAudioGPIO					"audio-gpio"
#define kAudioGPIOActiveState		"audio-gpio-active-state"
#define kIOInterruptControllers		"IOInterruptControllers"

#define	kOneWireBusPropName			"one-wire-bus"
#define	kSpeakerIDPropValue			"speaker-id"

#define		kMinimumTAS3004SampleRate		31200
#define		kMaximumTAS3004SampleRate		49200

enum UFixedPointGain{
	kMinSoftwareGain				=	0x00008000,
	kUnitySoftwareGain				=	0x00010000,
	kMaxSoftwareGain				=	0x00018000,
	kSoftwareGainMask				=	0x0001F000,
	kTAS3004_Plus_12dB				=	0x000C0000,
	kTAS3004_Minus_12dB				=	0xFFF40000,
	kTAS3004_MaxGainSteps			=	48
};

enum TAS3001C_ResetFlags{
	kNO_FORCE_RESET_SETUP_TIME		=	0,
	kFORCE_RESET_SETUP_TIME			=	1
};

enum loadMode {
		kSetNormalLoadMode			=	0,
		kSetFastLoadMode			=	1
};

enum semaphores{
	kResetSemaphoreBit				=	0,							//	bit address:	1 = reset in progress
	kResetSemaphoreMask				=	( 1 << 0 )					//	bit address:	1 = reset in progress
};

enum writeMode{
	kUPDATE_SHADOW					=	0,
	kUPDATE_HW						=	1,
	kUPDATE_ALL						=	2,
	kFORCE_UPDATE_ALL				=	3
};

enum resetRetryCount{
	kTAS3004_MAX_RETRY_COUNT		=	5
};

enum eqPrefsVersion{
	kCurrentEQPrefsVersion			=	1
};

enum muteSelectors{
	kHEADPHONE_AMP					=	0,
	kSPEAKER_AMP					=	1,
    kLINEOUT_AMP					=	2,
	kMASTER_AMP						=	3
};

enum TAS3004delays {
	kMAX_VOLUME_RAMP_DELAY			=	50				/*	50 milliseconds	*/
};

#define kHeadphoneBitPolarity		1
#define kHeadphoneBitAddr			0
#define kHeadphoneBitMask			(1 << kHeadphoneBitAddr)
#define kHeadphoneBitMatch			(kHeadphoneBitPolarity << kHeadphoneBitAddr)
#define kHeadphoneDetect			1

#define kSpeakerBit					1

#pragma mark -
#pragma mark •••••••• Structures ••••••••
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STRUCTURES

#ifndef GpioActiveState
typedef Boolean	GpioActiveState;
#endif

typedef struct{
	UInt8		sMC1R[kTAS3004MC1Rwidth];					//	main control 1 register
	UInt8		sDRC[kTAS3004DRCwidth];						//	dynamic range compression
	UInt8		sVOL[kTAS3004VOLwidth];						//	volume
	UInt8		sTRE[kTAS3004TREwidth];						//	treble
	UInt8		sBAS[kTAS3004BASwidth];						//	bass
	UInt8		sMXL[kTAS3004MIXERGAINwidth];				//	mixer left
	UInt8		sMXR[kTAS3004MIXERGAINwidth];				//	mixer right
	UInt8		sLB0[kTAS3004BIQwidth];						//	left biquad 0
	UInt8		sLB1[kTAS3004BIQwidth];						//	left biquad 1
	UInt8		sLB2[kTAS3004BIQwidth];						//	left biquad 2
	UInt8		sLB3[kTAS3004BIQwidth];						//	left biquad 3
	UInt8		sLB4[kTAS3004BIQwidth];						//	left biquad 4
	UInt8		sLB5[kTAS3004BIQwidth];						//	left biquad 5
	UInt8		sLB6[kTAS3004BIQwidth];						//	left biquad 6
	UInt8		sRB0[kTAS3004BIQwidth];						//	right biquad 0
	UInt8		sRB1[kTAS3004BIQwidth];						//	right biquad 1
	UInt8		sRB2[kTAS3004BIQwidth];						//	right biquad 2
	UInt8		sRB3[kTAS3004BIQwidth];						//	right biquad 3
	UInt8		sRB4[kTAS3004BIQwidth];						//	right biquad 4
	UInt8		sRB5[kTAS3004BIQwidth];						//	right biquad 5
	UInt8		sRB6[kTAS3004BIQwidth];						//	right biquad 6
	UInt8		sLLB[kTAS3004LoudnessBIQwidth];				//	left loudness biquad
	UInt8		sRLB[kTAS3004LoudnessBIQwidth];				//	right loudness biquad
	UInt8		sLLBG[kTAS3004LOUDNESSBIQUADGAINwidth];		//	left loudness biquad gain
	UInt8		sRLBG[kTAS3004LOUDNESSBIQUADGAINwidth];		//	right loudness biquad gain
	UInt8		sACR[kTAS3004ANALOGCTRLREGwidth];			//	analog control register
	UInt8		sMC2R[kTAS3004MC2Rwidth];					//	main control 2 register
}TAS3004_ShadowReg;
typedef TAS3004_ShadowReg TAS3004_ShadowReg;
typedef TAS3004_ShadowReg *TAS3004_ShadowRegPtr;

#define	kTAS3004_NumberOfRegisters			27

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct HiLevelFilterCoefficients {
	float			filterFrequency;
	float			filterGain;
	float			filterQ;
	UInt32			filterType;	
};
typedef HiLevelFilterCoefficients * HiLevelFilterCoefficientsPtr;

union EQFilterCoefficients {
		FourDotTwenty				coefficient[kNumberOfCoefficientsPerBiquad];	//	Coefficient[] is b0, b1, b2, a1 & a2 
//		HiLevelFilterCoefficients	hlFilter;
};
typedef EQFilterCoefficients *EQFilterCoefficientsPtr;

struct EQPrefsElement {
	UInt32					filterSampleRate;		
	UInt32					drcCompressionRatioNumerator;
	UInt32					drcCompressionRatioDenominator;
	SInt32					drcThreshold;
	SInt32					drcMaximumVolume;
	UInt32					drcEnable;
	UInt32					layoutID;				//	what cpu we're running on
	UInt32					deviceID;				//	i.e. internal spkr, external spkr, h.p.
	UInt32					speakerID;				//	what typeof external speaker is connected.
	UInt32					reserved;
	UInt32					filterCount;			//	number of biquad filters (total number : channels X ( biquads per channel )
	EQFilterCoefficients	filter[14];				//	an array of filter coefficient equal in length to filter count * sizeof(EQFilterCoefficients)
};
typedef EQPrefsElement *EQPrefsElementPtr;

#define	kCurrentEQPrefVersion	1
struct EQPrefs {
	UInt32					structVersionNumber;	//	identify what we're parsing
	UInt32					genreType;				//	'jazz', 'clas', etc...
	UInt32					eqCount;				//	number of eq[n] array elements
	UInt32					nameID;					//	resource id of STR identifying the filter genre
	EQPrefsElement			eq[0x2];				//	'n' sized based on number of devicID/speakerID/layoutID combinations...
};
typedef EQPrefs *EQPrefsPtr;

struct DRCInfo {
	UInt8		ratio;
	UInt8		ratioBelow;
	UInt8		threshold;
	UInt8		integrationInterval;
	UInt8		attack;
	UInt8		decay;
};
typedef DRCInfo *DRCInfoPtr;

enum extInt_gpio{
		intEdgeSEL				=	7,		//	bit address:	R/W Enable Dual Edge
		positiveEdge			=	0,		//		0 = positive edge detect for ExtInt interrupt sources (default)
		dualEdge				=	1		//		1 = enable both edges
};

enum gpio{
		gpioOS					=	4,		//	bit address:	output select
		gpioBit0isOutput		=	0,		//		use gpio bit 0 as output (default)
		gpioMediaBayIsOutput	=	1,		//		use media bay power
		gpioReservedOutputSel	=	2,		//		reserved
		gpioMPICopenCollector	=	3,		//		MPIC CPUInt2_1 (open collector)
		
		gpioAltOE				=	3,		//	bit address:	alternate output enable
		gpioOE_DDR				=	0,		//		use DDR for output enable
		gpioOE_Use_OS			=	1,		//		use gpioOS for output enable
		
		gpioDDR					=	2,		//	bit address:	r/w data direction
		gpioDDR_INPUT			=	0,		//		use for input (default)
		gpioDDR_OUTPUT			=	1,		//		use for output
		
		gpioPIN_RO				=	1,		//	bit address:	read only level on pin
		
		gpioDATA				=	0,		//	bit address:	the gpio itself
		
		gpioBIT_MASK			=	1		//	value shifted by bit position to be used to determine a GPIO bit state
};




#endif	// __TAS3004_HW__
