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
 * \brief Implements to access Diva management file system
 *
 * \par Access to Diva hardware state
 * divalogd captures information from Diva management and exports
 * it CVS file format.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "divastreaming/platform.h"
#include "chan_capi_platform.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "dlist.h"
#include "divastatus_parameters.h"
#include "divastatus_ifc.h"
#include "divastatus.h"
#ifdef CC_USE_INOTIFY
#include <fcntl.h>
#include <sys/inotify.h>
#endif

typedef const char* pcchar;

static pcchar DIVA_STATUS_PATH       = "/usr/lib/eicon/divas/registry/ifc";
static pcchar DIVA_STATUS_FILE       = "ifcstate";
static pcchar DIVA_INFO_FILE         = "info";
static pcchar DIVA_CONFIG_FILE       = "info/Config";
static pcchar DIVA_SERIAL_FILE       = "serial";
static pcchar DIVA_READ_ALARM_FILE   = "info/Red Alarm";
static pcchar DIVA_YELLOW_ALARM_FILE = "info/Yellow Alarm";
static pcchar DIVA_BLUE_ALARM_FILE   = "info/Blue Alarm";

/*
	LOCALS
	*/
struct _diva_status_ifc;
static int diva_status_active(void);
static int diva_status_get_controller_state(int controller, diva_status_ifc_state_t *state);
static char* diva_status_read_file(unsigned int controller, const char* fileName);
static void diva_status_create_wd(int* wd, int controller, const char* fileName, int isDir);
static diva_status_interface_state_t diva_status_get_interface_state_from_idi_state (const diva_status_ifc_state_t* state);
static diva_status_hardware_state_t diva_status_get_hw_state_from_idi_state (const diva_status_ifc_state_t* state);
static int diva_status_map_CAPI2XDI(int capiController);
static void diva_status_process_event(struct _diva_status_ifc *controllerState, int initialStateIndex, int newStateIndex);
#ifdef CC_USE_INOTIFY
static pcchar DIVA_DIVA_FS_PATH       = "/usr/lib/eicon/divas/registry/ifc";
static void diva_status_cleanup_wd(int wd);
static int divaFsWd  = -1; /*! \brief Diva fs state */
#endif

#ifdef CC_AST_HAS_VERSION_1_6
static
#endif
char diva_status_ifc_state_usage[] =
"Usage: " CC_MESSAGE_NAME " ifcstate\n"
"       Show info about interfaces.\n";


static int inotifyFd = -1; /*! \brief inotify descriptor */
static diva_entity_queue_t controller_q; /*! \brief Active controllers. \note List changed only while CAPI thread is not running */

typedef struct _diva_status_ifc {
	diva_entity_link_t link;
	int capiController;
	int idiController;
	diva_status_ifc_state_t state[2];
	int currentState;
	int ifstateWd;
	int infoWd;
	diva_status_changed_cb_proc_t   status_changed_notify_proc; /*! \brief Notify about interface state change */
	diva_hwstatus_changed_cb_proc_t hw_status_changed_notify_proc; /*! \brief Notify about hardware state change */
	time_t changeTime; /*! \brief Time interface state changed */
	time_t unavailableChangeTime; /*! \brief Time interface state changed to unavailable */
	time_t hwChangeTime; /*! \brief Time hardware state changed */
	time_t unavailableHwChangeTime; /*! \brief Time hardware state changed to unavailable */
} diva_status_ifc_t;

diva_status_interface_state_t diva_status_init_interface(int controller,
																												 diva_status_hardware_state_t* hwState,
																												 diva_status_changed_cb_proc_t fn,
																												 diva_hwstatus_changed_cb_proc_t hwfn)
{
	int idiController = diva_status_map_CAPI2XDI(controller);
	diva_status_ifc_t* controllerState = idiController > 0 ? (ast_malloc(sizeof(*controllerState))) : 0;
	diva_status_interface_state_t ret = DivaStatusInterfaceStateNotAvailable;

  if (controllerState != 0) {
		controllerState->capiController = controller;
		controllerState->idiController  = idiController;
		controllerState->status_changed_notify_proc    = fn;
		controllerState->hw_status_changed_notify_proc = hwfn;
		controllerState->ifstateWd = -1;
		controllerState->infoWd    = -1;
		diva_status_create_wd(&controllerState->ifstateWd, idiController, DIVA_STATUS_FILE, 0);
		diva_status_create_wd(&controllerState->infoWd, idiController, DIVA_INFO_FILE, 1);
		controllerState->currentState = 0;
		controllerState->changeTime = time(NULL);
		diva_status_get_controller_state(idiController, &controllerState->state[controllerState->currentState]);
		diva_q_add_tail(&controller_q, &controllerState->link);
		ret = diva_status_get_interface_state_from_idi_state(&controllerState->state[controllerState->currentState]);
		*hwState = diva_status_get_hw_state_from_idi_state(&controllerState->state[controllerState->currentState]);
	}

	return ret;
}

void diva_status_cleanup_interface (int controller)
{
	diva_entity_link_t* link;

	for (link = diva_q_get_head(&controller_q); link != 0; link = diva_q_get_next(link)) {
		diva_status_ifc_t *controllerState = DIVAS_CONTAINING_RECORD(link, diva_status_ifc_t, link);

		if (controllerState->capiController == controller) {
			diva_q_remove(&controller_q, link);
#ifdef CC_USE_INOTIFY
			if (controllerState->ifstateWd >= 0)
				inotify_rm_watch(inotifyFd, controllerState->ifstateWd);
			if (controllerState->infoWd >= 0)
				inotify_rm_watch(inotifyFd, controllerState->infoWd);
#endif
			ast_free (controllerState);
			break;
		}
	}

#ifdef CC_USE_INOTIFY
	if (diva_q_get_head(&controller_q) == 0) {
		if (divaFsWd >= 0) {
			inotify_rm_watch(inotifyFd, divaFsWd);
			divaFsWd = -1;
		}
		if (inotifyFd >= 0) {
			close(inotifyFd);
			inotifyFd = -1;
		}
	}
#endif
}

int diva_status_get_waitable_object(void)
{
	return inotifyFd;
}

void diva_status_process_events(void)
{
	diva_entity_link_t* link;

	/*
		Polling
		*/
	for (link = diva_q_get_head(&controller_q); link != 0; link = diva_q_get_next(link)) {
		diva_status_ifc_t *controllerState = DIVAS_CONTAINING_RECORD(link, diva_status_ifc_t, link);
		if ((controllerState->ifstateWd < 0) || (controllerState->infoWd < 0)) {
			int currentState = controllerState->currentState;
			diva_status_create_wd(&controllerState->ifstateWd, controllerState->idiController, DIVA_STATUS_FILE, 0);
			diva_status_create_wd(&controllerState->infoWd, controllerState->idiController, DIVA_INFO_FILE, 1);
			controllerState->currentState = (controllerState->currentState + 1) % 2;
			diva_status_get_controller_state(controllerState->idiController, &controllerState->state[controllerState->currentState]);
			diva_status_process_event(controllerState, currentState, controllerState->currentState);
		}
	}

	/*
		Events
		*/
#ifdef CC_USE_INOTIFY
	if (inotifyFd >= 0 && divaFsWd >= 0) {
		unsigned char buffer[1024];
		int length;
		int i;

		while ((length = read(inotifyFd, buffer, sizeof(buffer))) > 0) {
			i = 0;
			while (i < length) {
				struct inotify_event *event = (struct inotify_event*)&buffer[i];

				if ((event->mask & (IN_IGNORED|IN_DELETE_SELF|IN_UNMOUNT|IN_Q_OVERFLOW)) != 0) {
					diva_status_cleanup_wd((event->mask & IN_IGNORED) == 0 ? event->wd : -1);
					break;
				}
				if (event->wd != divaFsWd) {
					for (link = diva_q_get_head(&controller_q); link != 0; link = diva_q_get_next(link)) {
						diva_status_ifc_t *controllerState = DIVAS_CONTAINING_RECORD(link, diva_status_ifc_t, link);
						if ((controllerState->ifstateWd == event->wd) || (controllerState->infoWd == event->wd)) {
							int currentState = controllerState->currentState;
							controllerState->currentState = (controllerState->currentState + 1) % 2;
							diva_status_get_controller_state(controllerState->idiController, &controllerState->state[controllerState->currentState]);
							diva_status_process_event(controllerState, currentState, controllerState->currentState);
						}
					}
				}

				i += sizeof(*event) + event->len;
			}
		}
	}
#endif
}

#ifdef CC_USE_INOTIFY
static void diva_status_cleanup_wd(int wd)
{
	diva_entity_link_t* link;

	for (link = diva_q_get_head(&controller_q); link != 0; link = diva_q_get_next(link)) {
		diva_status_ifc_t *controllerState = DIVAS_CONTAINING_RECORD(link, diva_status_ifc_t, link);

		if ((controllerState->ifstateWd >= 0) && (wd != controllerState->ifstateWd)) {
			inotify_rm_watch(inotifyFd, controllerState->ifstateWd);
		}
		controllerState->ifstateWd = -1;
		if ((controllerState->infoWd >= 0) && (wd != controllerState->infoWd)) {
			inotify_rm_watch(inotifyFd, controllerState->infoWd);
		}
		controllerState->infoWd = -1;
	}

	if ((divaFsWd >= 0) && (wd != divaFsWd)) {
		inotify_rm_watch(inotifyFd, divaFsWd);
	}
	divaFsWd = -1;
}
#endif

static void diva_status_process_event(diva_status_ifc_t *controllerState, int initialStateIndex, int newStateIndex)
{
	diva_status_ifc_state_t* initialState = &controllerState->state[initialStateIndex];
	diva_status_ifc_state_t* newState     = &controllerState->state[newStateIndex];
	diva_status_interface_state_t initialIfcState = diva_status_get_interface_state_from_idi_state (initialState);
	diva_status_interface_state_t newIfcState     = diva_status_get_interface_state_from_idi_state (newState);
	diva_status_hardware_state_t initalHwState    = diva_status_get_hw_state_from_idi_state(initialState);
	diva_status_hardware_state_t newHwState       = diva_status_get_hw_state_from_idi_state(newState);
	time_t t = time(NULL);

	if (initialIfcState != newIfcState) {
		controllerState->changeTime = t;

		if (newIfcState == DivaStatusInterfaceStateERROR)
			controllerState->unavailableChangeTime = t;

		controllerState->status_changed_notify_proc(controllerState->capiController, newIfcState);
	}

	if (initalHwState != newHwState) {
		controllerState->hwChangeTime = t;

		if (newHwState == DivaStatusHardwareStateERROR)
			controllerState->unavailableHwChangeTime = t;

		controllerState->hw_status_changed_notify_proc(controllerState->capiController, newHwState);
	}
}

static void diva_status_create_wd(int* wd, int controller, const char* fileName, int isDir)
{
#ifdef CC_USE_INOTIFY
	if (inotifyFd < 0) {
		inotifyFd = inotify_init();
		if (inotifyFd >= 0)
			fcntl(inotifyFd, F_SETFL, O_NONBLOCK);
	}
	if (inotifyFd >= 0 && divaFsWd < 0) {
		divaFsWd = inotify_add_watch (inotifyFd, DIVA_DIVA_FS_PATH, IN_DELETE_SELF | IN_UNMOUNT | IN_IGNORED);
	}

	if (*wd >= 0)
		return;

	if (inotifyFd >= 0 && divaFsWd >= 0) {
		int name_len = strlen(DIVA_STATUS_PATH) + strlen(fileName) + 32;
		char name[name_len];

		snprintf(name, name_len, "%s/adapter%u/%s", DIVA_STATUS_PATH, controller, fileName);
		name[name_len-1] = 0;

		*wd = inotify_add_watch (inotifyFd, name,
					 IN_CLOSE_WRITE | IN_DELETE_SELF | IN_IGNORED | ((isDir != 0) ? IN_DELETE : 0));

		return;
	}
#else
	*wd = -1;
#endif
}

/*!
	\brief Check divalogd is available
	*/
static int diva_status_active(void)
{
	struct stat v;

	return ((stat(DIVA_STATUS_PATH, &v) == 0 && S_ISDIR(v.st_mode) != 0) ? 0 : -1);
}

static char* diva_status_read_file(unsigned int controller, const char* fileName)
{
	int name_len = strlen(DIVA_STATUS_PATH) + strlen(fileName) + 32;
	char name[name_len];
	struct stat v;
	int fd;
	int length;
	char *data, *p;

	snprintf(name, name_len, "%s/adapter%u/%s", DIVA_STATUS_PATH, controller, fileName);
	name[name_len-1] = 0;

	fd = open(name, O_RDONLY);
	if (fd < 0)
		return 0;

	if (fstat(fd, &v) != 0 || v.st_size == 0) {
    close(fd);
    return 0;
  }

	length = MIN(v.st_size, 16U*1024U);

	data = ast_malloc(length+1);
	if (data == 0) {
		close (fd);
		return 0;
	}

	if (read(fd, data, length) != length) {
		ast_free(data);
		close(fd);
		return (0);
	}

	close (fd);

	data[length] = 0;

	while (((p = strchr(data, '\n')) != 0) || ((p = strchr(data, '\r')))) {
		*p = 0;
	}

	return (data);
}

static int diva_status_get_controller_state(int controller, diva_status_ifc_state_t *state)
{
	char *data, *p;
	int i, pri, bri;
	const char* v;

	memset (state, 0x00, sizeof(*state));
	state->ifcType    = DivaStatusIfcNotPri;
	state->hwState    = DivaStatusHwStateUnknown;
	state->ifcL1State = DivaStatusIfcL2DoNotApply;
	state->ifcL2State = DivaStatusIfcL2DoNotApply;
	state->ifcL1VisualState = DivaStatusIfcL2DoNotApply;
	state->ifcL2VisualState = DivaStatusIfcL2DoNotApply;

	if (diva_status_active() != 0)
		return -1;

	if ((data = diva_status_read_file(controller, DIVA_CONFIG_FILE)) == 0)
		return -1;

	for (i = 0, pri = 0, bri = 0, p = data, v = strsep(&p, ",");
			 v != 0 && i < DivaStateIfcConfig_Max;
			 v = strsep(&p, ","), i++) {
		if (v[0] == '\'' && v[strlen(v)-1] != '\'') {
			const char *tmp;
			do {
				if ((p != 0) && (*p != 0))
					p[-1] = ',';
				tmp = strsep (&p, ",");
			} while ((tmp != 0) && (tmp[strlen(tmp)-1] != '\''));
		}

		switch ((diva_state_ifc_config_parameters_t)i) {
			case DivaStateIfcConfig_TYPE:
				pri += (strcmp ("PRI", v) == 0);
				bri += (strcmp ("BRI", v) == 0);
				break;

			case DivaStateIfcConfig_PRI:
				pri += (strcmp ("'YES'", v) == 0);
				bri += (strcmp ("'NO'", v) == 0);
				break;

			default:
				break;
		}
	}
	ast_free(data);

	state->ifcType = (pri == 2) ? DivaStatusIfcPri : DivaStatusIfcNotPri;
	if (bri == 2) {
		state->ifcType = DivaStatusIfcBri;
	}

	if ((data = diva_status_read_file(controller, DIVA_STATUS_FILE)) == 0)
		return (-1);

	for (i = 0, p = data, v = strsep (&p, ","); v != 0 && i < (int)DivaStateIfcState_Max; v = strsep (&p, ","), i++) {
		if (v[0] == '\'' && v[strlen(v)-1] != '\'') {
			const char *tmp;
			do {
				if ((p != 0) && (*p != 0))
					p[-1] = ',';
				tmp = strsep (&p, ",");
			} while ((tmp != 0) && (tmp[strlen(tmp)-1] != '\''));
		}

		switch ((diva_state_ifcstate_parameters_t)i) {
			case DivaStateIfcState_LAYER1_STATE:
				state->ifcL1VisualState = (strcmp ("'Activated'", v) == 0) ? DivaStatusIfcL1OK : DivaStatusIfcL1Error;
				if (state->ifcType == DivaStatusIfcPri) {
					state->ifcL1State = state->ifcL1VisualState;
				}
				break;

			case DivaStateIfcState_LAYER2_STATE:
				state->ifcL2VisualState = (strcmp ("'Layer2 UP'", v) == 0) ? DivaStatusIfcL2OK : DivaStatusIfcL2Error;
				if (state->ifcType == DivaStatusIfcPri) {
					state->ifcL2State = state->ifcL2VisualState;
				}
				break;

			case DivaStateIfcState_D1_X_FRAMES:
				state->ifcTxDStatistics.Frames = (unsigned int)atol(v);
				break;
			case DivaStateIfcState_D1_X_BYTES:
				state->ifcTxDStatistics.Bytes = (unsigned int)atol(v);
				break;
			case DivaStateIfcState_D1_X_ERRORS:
				state->ifcTxDStatistics.Errors = (unsigned int)atol(v);
				break;
			case DivaStateIfcState_D1_R_FRAMES:
				state->ifcRxDStatistics.Frames = (unsigned int)atol(v);
				break;
			case DivaStateIfcState_D1_R_BYTES:
				state->ifcRxDStatistics.Bytes = (unsigned int)atol(v);
				break;
			case DivaStateIfcState_D1_R_ERRORS:
				state->ifcRxDStatistics.Errors = (unsigned int)atol(v);
				break;

			case DivaStateIfcState_MAX_TEMPERATURE:
				state->maxTemperature = (unsigned int)atoi(v);
				break;
			case DivaStateIfcState_TEMPERATURE:
				state->currentTemperature = (unsigned int)atoi(v);
				break;

			case DivaStateIfcState_HARDWARE_STATE:
				if (strcmp("'Active'", v) == 0)
					state->hwState = DivaStateHwStateActive;
				else if (strcmp("'Inactive'", v) == 0)
					state->hwState = DivaStateHwStateInactive;
				break;

			default:
				break;
		}
	}
	ast_free (data);

	if ((data = diva_status_read_file(controller, DIVA_READ_ALARM_FILE)) != 0) {
		state->ifcAlarms.Red = strcmp("TRUE", data) == 0;
		ast_free(data);
	}
	if ((data = diva_status_read_file(controller, DIVA_YELLOW_ALARM_FILE)) != 0) {
		state->ifcAlarms.Yellow = strcmp("TRUE", data) == 0;
		ast_free(data);
	}
	if ((data = diva_status_read_file(controller, DIVA_BLUE_ALARM_FILE)) != 0) {
		state->ifcAlarms.Blue = strcmp("TRUE", data) == 0;
		ast_free(data);
	}
	if ((data = diva_status_read_file(controller, DIVA_SERIAL_FILE)) != 0) {
		state->serialNumber = (unsigned int)(atol(data)) & 0x00ffffff;
		ast_free(data);
	}

	return (0);
}

/*
	chan_capi interface
	*/
diva_status_interface_state_t diva_status_get_interface_state(int controller)
{
	diva_status_ifc_state_t state;
	int ret;

	ret = diva_status_get_controller_state(controller, &state);

	if ((ret != 0) ||
			(state.ifcType != DivaStatusIfcPri) ||
			(state.ifcL1State == DivaStatusIfcL2DoNotApply) ||
			(state.hwState    == DivaStatusHwStateUnknown)) {
		return (DivaStatusInterfaceStateNotAvailable);
	}

	if ((state.ifcAlarms.Red    == 0) &&
			(state.ifcAlarms.Yellow == 0) &&
			(state.ifcAlarms.Blue   == 0) &&
			(state.hwState == DivaStateHwStateActive) &&
			(state.ifcL1State == DivaStatusIfcL1OK)) {
		return DivaStatusInterfaceStateOK;
	}

	return DivaStatusInterfaceStateERROR;
}

static diva_status_interface_state_t diva_status_get_interface_state_from_idi_state(const diva_status_ifc_state_t* state)
{
	if ((state->ifcType    != DivaStatusIfcPri)          ||
			(state->ifcL1State == DivaStatusIfcL2DoNotApply) ||
			(state->hwState    == DivaStatusHwStateUnknown)) {
		return (DivaStatusInterfaceStateNotAvailable);
	}

	if ((state->ifcAlarms.Red    == 0) &&
			(state->ifcAlarms.Yellow == 0) &&
			(state->ifcAlarms.Blue   == 0) &&
			(state->hwState == DivaStateHwStateActive) &&
			(state->ifcL1State == DivaStatusIfcL1OK)) {
		return DivaStatusInterfaceStateOK;
	}

	return DivaStatusInterfaceStateERROR;
}

static diva_status_hardware_state_t diva_status_get_hw_state_from_idi_state(const diva_status_ifc_state_t* state)
{
	switch(state->hwState) {
		case DivaStateHwStateActive:
			return (DivaStatusHardwareStateOK);

		case DivaStateHwStateInactive:
			return (DivaStatusHardwareStateERROR);

		case DivaStatusHwStateUnknown:
		default:
			return (DivaStatusHardwareStateUnknown);
	}
}

/*!
	\brief Map CAPI controller to Diva hardware

	\note In most cases chan_capi uses 1:1 mappimg
        between CAPI controller and Diva hardware

	\todo Read CAPI2DIva mapping table from CAPI
	*/
static int diva_status_map_CAPI2XDI(int capiController)
{
	return ((capiController > 0) ? capiController : -1);
}

int diva_status_available(void)
{
	return (diva_status_active());
}

const char* diva_status_interface_state_name(diva_status_interface_state_t state)
{
	switch(state) {
		case DivaStatusInterfaceStateOK:
			return "active";

		case DivaStatusInterfaceStateERROR:
			return "inactive";

		case DivaStatusInterfaceStateNotAvailable:
		default:
			return "unknown";
	}
}

const char* diva_status_hw_state_name(diva_status_hardware_state_t hwState)
{
	switch(hwState) {
		case DivaStatusHardwareStateOK:
			return "active";

		case DivaStatusHardwareStateERROR:
			return "not running";

		case DivaStatusHardwareStateUnknown:
		default:
			return "unknown";
	}
}

static const char* pbxcli_get_visual_ifc_state(const diva_status_ifc_state_t* state)
{
	diva_status_interface_state_t ifcState;

	switch (state->ifcType) {
		case DivaStatusIfcBri: /* Functional state is always unknown, probably L1/L2 activated on demand, \todo read L2 config */
			if (state->hwState == DivaStateHwStateActive     &&
					state->ifcL1VisualState == DivaStatusIfcL1OK &&
					state->ifcL2VisualState == DivaStatusIfcL2OK) {
				ifcState = DivaStatusInterfaceStateOK;
			} else {
				ifcState = diva_status_get_interface_state_from_idi_state (state);
			}
			break;

		case DivaStatusIfcAnalog: /* \todo implementation */
			ifcState = diva_status_get_interface_state_from_idi_state (state);
			break;

		case DivaStatusIfcPri: /* Functional state == visual state */
		default:
			ifcState = diva_status_get_interface_state_from_idi_state (state);
			break;
	}

	return (diva_status_interface_state_name(ifcState));
}

static const char* pbxcli_get_visual_ifc_l1_state(const diva_status_ifc_state_t* state)
{
	const char* ret = "-";

	if (state->hwState == DivaStatusHardwareStateOK) {
		switch (state->ifcType) {
			case DivaStatusIfcPri: /* Functional state == visual state */
				ret = (state->ifcL1State == DivaStatusIfcL1OK) ? "On" : "Off";
				break;
			case DivaStatusIfcBri:
				ret = (state->ifcL1VisualState == DivaStatusIfcL1OK) ? "On" : "Off"; /* Functional state != visual state */
				break;
			case DivaStatusIfcAnalog: /* \todo implementation */
				break;
			default:
				break;
		}
	}

	return (ret);
}

static const char* pbxcli_get_visual_ifc_l2_state(const diva_status_ifc_state_t* state)
{
	const char* ret = "-";

	if (state->hwState == DivaStatusHardwareStateOK) {
		switch (state->ifcType) {
			case DivaStatusIfcPri: /* Functional state == visual state */
				ret = (state->ifcL2State == DivaStatusIfcL2OK) ? "On" : "Off";
				break;
			case DivaStatusIfcBri:
				ret = (state->ifcL2VisualState == DivaStatusIfcL2OK) ? "On" : "Off"; /* Functional state != visual state */
				break;
			case DivaStatusIfcAnalog: /* \todo implementation */
				break;
			default:
				break;
		}
	}

	return (ret);
}

#ifdef CC_AST_HAS_VERSION_1_6
char *pbxcli_capi_ifc_status(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
#else
int pbxcli_capi_ifc_status(int fd, int argc, char *argv[])
#endif
{
	diva_entity_link_t* link;
#ifdef CC_AST_HAS_VERSION_1_6
	int fd = a->fd;

	if (cmd == CLI_INIT) {
		e->command = CC_MESSAGE_NAME " ifcstate";
		e->usage = diva_status_ifc_state_usage;
		return NULL;
	} else if (cmd == CLI_GENERATE)
		return NULL;
	if (a->argc != e->args)
		return CLI_SHOWUSAGE;
#else
	
	if (argc != 2)
		return RESULT_SHOWUSAGE;
#endif

	if ( diva_q_get_head(&controller_q) == NULL) {
		ast_cli(fd, "There are no interraces in " CC_MESSAGE_NAME " instance.\n");
		return RESULT_SUCCESS;
	}

	ast_cli(fd, CC_MESSAGE_NAME " interfaces\n");
	ast_cli(fd, "%s %-9s%-4s%-4s%-4s%-7s%-5s%8s%12s%12s%12s%12s\n",
							"INTERFACE", "State", "L1", "L2", "RED", "YELLOW", "BLUE", "D-Rx-Frames", "D-Rx-Bytes","D-Tx-Frames", "D-Tx-Bytes", "D-Errors");

	for (link = diva_q_get_head(&controller_q); link != 0; link = diva_q_get_next(link)) {
		diva_status_ifc_t *controllerState = DIVAS_CONTAINING_RECORD(link, diva_status_ifc_t, link);
		diva_status_ifc_state_t* state = &controllerState->state[controllerState->currentState];
		ast_cli(fd, "ISDN%-3d%-2s %-9s%-4s%-4s%-4s%-7s%-3s %12d %11d %11d %11d %11d\n",
						controllerState->capiController, "",
						pbxcli_get_visual_ifc_state(state),
						pbxcli_get_visual_ifc_l1_state(state),
						pbxcli_get_visual_ifc_l2_state(state),
						((state->ifcType == DivaStatusIfcPri) && (state->hwState == DivaStatusHardwareStateOK)) ?
												(state->ifcAlarms.Red    != 0 ? "On" : "Off") : "-",
						((state->ifcType == DivaStatusIfcPri) && (state->hwState == DivaStatusHardwareStateOK)) ?
												(state->ifcAlarms.Yellow != 0 ? "On" : "Off") : "-",
						((state->ifcType == DivaStatusIfcPri) && (state->hwState == DivaStatusHardwareStateOK)) ?
												(state->ifcAlarms.Blue   != 0 ? "On" : "Off") : "-",
						state->ifcRxDStatistics.Frames, state->ifcRxDStatistics.Bytes,
						state->ifcTxDStatistics.Frames, state->ifcTxDStatistics.Bytes,
						state->ifcRxDStatistics.Errors + state->ifcTxDStatistics.Errors
						);
	}

#ifdef CC_AST_HAS_VERSION_1_6
	return CLI_SUCCESS;
#else
	return RESULT_SUCCESS;
#endif
}
