/*
 *
  Copyright (c) Dialogic(R), 2010

 *
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 *
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*! \file
 * \brief Declares interface to access diva management file system
 */

#ifndef __DIVA_STATUS_H__
#define __DIVA_STATUS_H__

typedef enum _diva_status_ifc_type {
	DivaStatusIfcNotPri = 0,
	DivaStatusIfcPri,
	DivaStatusIfcBri,
	DivaStatusIfcAnalog
} diva_status_ifc_type_t;

typedef struct _diva_status_ifc_alarms {
	int Red;
	int Blue;
	int Yellow;
} diva_status_ifc_alarms_t;

typedef enum _diva_status_ifc_l1_state {
	DivaStatusIfcL1DoNotApply = 0,
	DivaStatusIfcL1OK,
	DivaStatusIfcL1Error
} diva_status_ifc_l1_state_t;

typedef enum _diva_status_ifc_l2_state {
	DivaStatusIfcL2DoNotApply = 0,
	DivaStatusIfcL2OK,
	DivaStatusIfcL2Error
} diva_status_ifc_l2_state_t;

typedef enum _diva_status_ifc_hw_state {
	DivaStatusHwStateUnknown = 0,
	DivaStateHwStateActive   = 1,
	DivaStateHwStateInactive = 2
} diva_status_ifc_hw_state_t;

typedef struct _diva_status_ifc_statistics {
	unsigned int Frames;
	unsigned int Bytes;
	unsigned int Errors;
} diva_status_ifc_statistics_t;

typedef struct _diva_status_ifc_state {
	diva_status_ifc_type_t     ifcType;
	diva_status_ifc_hw_state_t hwState;
	diva_status_ifc_alarms_t   ifcAlarms;
	diva_status_ifc_l1_state_t ifcL1State;
	diva_status_ifc_l2_state_t ifcL2State;
	diva_status_ifc_l1_state_t ifcL1VisualState;
	diva_status_ifc_l2_state_t ifcL2VisualState;
	diva_status_ifc_statistics_t ifcRxDStatistics;
	diva_status_ifc_statistics_t ifcTxDStatistics;
	unsigned int serialNumber;
	unsigned int maxTemperature;
	unsigned int currentTemperature;
} diva_status_ifc_state_t;

#define DIVA_STATUS_EVENT_L1         0x00000001U
#define DIVA_STATUS_EVENT_L2         0x00000002U
#define DIVA_STATUS_EVENT_ALARMS     0x00000004U
#define DIVA_STATUS_EVENT_STATISTICS 0x00000008U

#endif


