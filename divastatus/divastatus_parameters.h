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

#ifndef __DIVA_STATUS_PARAMETERS_H__
#define __DIVA_STATUS_PARAMETERS_H__

/*! \brief ifcstate
	*/
typedef enum _diva_state_ifcstate_parameters {
	DivaStateIfcState_LAYER1_STATE = 0,
	DivaStateIfcState_LAYER2_STATE,
	DivaStateIfcState_INC_CALLS,
	DivaStateIfcState_INC_CONNECTED,
	DivaStateIfcState_INC_USER_BUSY,
	DivaStateIfcState_INC_CALL_REJECTED,
	DivaStateIfcState_INC_WRONG_NUMBER,
	DivaStateIfcState_INC_INCOMPATIBLE_DST,
	DivaStateIfcState_INC_OUT_OF_ORDER,
	DivaStateIfcState_INC_IGNORED,
	DivaStateIfcState_OUT_CALLS,
	DivaStateIfcState_OUT_CONNECTED,
	DivaStateIfcState_OUT_USER_BUSY,
	DivaStateIfcState_OUT_NO_ANSWER,
	DivaStateIfcState_OUT_WRONG_NUMBER,
	DivaStateIfcState_OUT_CALL_REJECTED,
	DivaStateIfcState_OUT_OTHER_FAILURES,
	DivaStateIfcState_MDM_DISC_NORMAL,
	DivaStateIfcState_MDM_DISC_UNSPECIFIED,
	DivaStateIfcState_MDM_DISC_BUSY_TONE,
	DivaStateIfcState_MDM_DISC_CONGESTION,
	DivaStateIfcState_MDM_DISC_CARR_WAIT,
	DivaStateIfcState_MDM_DISC_TRN_TIMEOUT,
	DivaStateIfcState_MDM_DISC_INCOMPAT,
	DivaStateIfcState_MDM_DISC_FRAME_REJ,
	DivaStateIfcState_MDM_DISC_V42BIS,
	DivaStateIfcState_FAX_DISC_NORMAL,
	DivaStateIfcState_FAX_DISC_NOT_IDENT,
	DivaStateIfcState_FAX_DISC_NO_RESPONSE,
	DivaStateIfcState_FAX_DISC_RETRIES,
	DivaStateIfcState_FAX_DISC_UNEXP_MSG,
	DivaStateIfcState_FAX_DISC_NO_POLLING,
	DivaStateIfcState_FAX_DISC_TRAINING,
	DivaStateIfcState_FAX_DISC_UNEXPECTED,
	DivaStateIfcState_FAX_DISC_APPLICATION,
	DivaStateIfcState_FAX_DISC_INCOMPAT,
	DivaStateIfcState_FAX_DISC_NO_COMMAND,
	DivaStateIfcState_FAX_DISC_LONG_MSG,
	DivaStateIfcState_FAX_DISC_SUPERVISOR,
	DivaStateIfcState_FAX_DISC_SUB_SEP_PWD,
	DivaStateIfcState_FAX_DISC_INVALID_MSG,
	DivaStateIfcState_FAX_DISC_PAGE_CODING,
	DivaStateIfcState_FAX_DISC_APP_TIMEOUT,
	DivaStateIfcState_FAX_DISC_UNSPECIFIED,
	DivaStateIfcState_B1_X_FRAMES,
	DivaStateIfcState_B1_X_BYTES,
	DivaStateIfcState_B1_X_ERRORS,
	DivaStateIfcState_B1_R_FRAMES,
	DivaStateIfcState_B1_R_BYTES,
	DivaStateIfcState_B1_R_ERRORS,
	DivaStateIfcState_B2_X_FRAMES,
	DivaStateIfcState_B2_X_BYTES,
	DivaStateIfcState_B2_X_ERRORS,
	DivaStateIfcState_B2_R_FRAMES,
	DivaStateIfcState_B2_R_BYTES,
	DivaStateIfcState_B2_R_ERRORS,
	DivaStateIfcState_D1_X_FRAMES,
	DivaStateIfcState_D1_X_BYTES,
	DivaStateIfcState_D1_X_ERRORS,
	DivaStateIfcState_D1_R_FRAMES,
	DivaStateIfcState_D1_R_BYTES,
	DivaStateIfcState_D1_R_ERRORS,
	DivaStateIfcState_D2_X_FRAMES,
	DivaStateIfcState_D2_X_BYTES,
	DivaStateIfcState_D2_X_ERRORS,
	DivaStateIfcState_D2_R_FRAMES,
	DivaStateIfcState_D2_R_BYTES,
	DivaStateIfcState_D2_R_ERRORS,
	DivaStateIfcState_INITIAL_TEMPERATURE,
	DivaStateIfcState_MIN_TEMPERATURE,
	DivaStateIfcState_MAX_TEMPERATURE,
	DivaStateIfcState_TEMPERATURE,
	DivaStateIfcState_DSPSTATE,
	DivaStateIfcState_FAX_TX_PAGES_TOTAL,
	DivaStateIfcState_FAX_TX_PAGES_RETRAIN,
	DivaStateIfcState_FAX_TX_PAGES_REJECT,
	DivaStateIfcState_FAX_RX_PAGES_TOTAL,
	DivaStateIfcState_FAX_RX_PAGES_RETRAIN,
	DivaStateIfcState_FAX_RX_PAGES_REJECT,
	DivaStateIfcState_OUT_ABANDONED,
	DivaStateIfcState_IN_ABANDONED,
	DivaStateIfcState_HARDWARE_STATE,
	DivaStateIfcState_Max
} diva_state_ifcstate_parameters_t;

typedef enum diva_state_ifc_config_parameters {
	DivaStateIfcConfig_TYPE = 0,
	DivaStateIfcConfig_CHANNELS,
	DivaStateIfcConfig_PROTOCOL,
	DivaStateIfcConfig_NT_MODE,
	DivaStateIfcConfig_POINTTOPOINT,
	DivaStateIfcConfig_INTERFACENR,
	DivaStateIfcConfig_BOARDREVISION,
	DivaStateIfcConfig_SUBFUNCTION,
	DivaStateIfcConfig_SUBDEVICE,
	DivaStateIfcConfig_PROTOCOLBUILD,
	DivaStateIfcConfig_DSPCODEBUILD,
	DivaStateIfcConfig_ANALOGCHANNELS,
	DivaStateIfcConfig_PRI,
	DivaStateIfcConfig_PCIDMA,
	DivaStateIfcConfig_ADAPTERTYPE,
	DivaStateIfcConfig_LAW,
	DivaStateIfcConfig_Max
} diva_state_ifc_config_parameters_t;

#endif

