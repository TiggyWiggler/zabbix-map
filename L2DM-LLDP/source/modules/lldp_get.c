/*
** L2 Discovery Module for LLDP
** Copyright (C) 2018 NTT Com Solutions Corporation
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "sysinc.h"
#include "module.h"
#include "common.h"
#include "log.h"
#include "sysinfo.h"
#include "zbxalgo.h"
#include "zbxjson.h"

#define SNMP_NO_DEBUGGING		/* disabling debugging messages from Net-SNMP library */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#ifdef ZBX_PROGRAM_TYPE_SERVER
extern unsigned char program_type ;
#else	/* ZBX_DAEMON_TYPE_SERVER (Zabbix2.x) */
extern unsigned char daemon_type ;
#endif

/******************************************************************************
 *                                                                            *
 * optional value process function prototype.                                 *
 *                                                                            *
 * Parameters: in           - [IN]  input value before process                *
 *             out          - [OUT] processed value                           *
 *             max_out_len  - [IN]  max length of the output buffer           *
 *                                                                            *
 ******************************************************************************/
typedef void (*lldpmod_optproc_func)(void *in, char *out, size_t max_out_len);

/* snmp discovery request structure */
typedef struct
{
	int	noid;				/* number of all oid (include remote) */
	int	noid_rem;			/* number of remote oid */
	int	noid_max;			/* number of all oid (allocated memory size) */

	char	*lld_key_macro;			/* lld_macro for index */
	char	*lld_remkey_macro;		/* lld_macro for remote index */
	char	*lld_optvalue_macro;		/* lld_macro for optional value */
	lldpmod_optproc_func	opt_func;	/* function pointer for process optional value */

	int	*rem_map;			/* link oid array(that mixed local and remote) to remote values array */
	int	*key_types;			/* normal or remote: normal has index only, remote has index and remote_index */
	int	*res_types;			/* string / integer: type of the result */
	int	*dump_flags;			/* standard binary dump separator is space, but when mac address, separate by coron */
	char	**oids;				/* retrieve oids */
	char	**lld_value_macros;		/* lld_macro for value */
}
lldpmod_discovery_request_t;

/*  discovered SNMP object, identified by its index */
typedef struct
{
	oid			index;		/* object index */
	char			**values;	/* an array of OID values stored in the same order as defined in OID key */
	zbx_hashset_t		*rem_objects;	/* when get remote object, local object may have some remote objects indentified by remote index */
						/* remote hash recursively have same data structure as local */
}
lldpmod_snmp_object_t;

/* helper data structure used by snmp discovery */
typedef struct
{
	int				num;		/* array index of snmp value getting then */
	zbx_hashset_t			objects;	/* local value hash identified by snmp index. it keeps snmp object */
	lldpmod_discovery_request_t	request;	/* request information */
}
lldpmod_discovery_data_t;

/* helper data structure used by snmp get */
typedef struct
{
	int		res_type;	/* string / bitstring / integer: type of the result (when object is not found, there is no hint of type) */
	int		dump_flag;	/* standard binary dump separator is space, but when mac address, separate by coron */
	oid		index;		/* required oid */
	oid		rem_index;	/* required remote index */
	AGENT_RESULT	*result;	/* snmp get result */
}
lldpmod_getrem_data_t;

#define SNMP_NAME		"lldp_get_module"
#define MODULE_NAME		"lldp_get module"
#define	SUCCEED_WITH_VALUE	(100)

/* key_type */
#define KEY_TYPE_LOC		(1)
#define KEY_TYPE_REM		(2)

/* res_type */
#define RES_TYPE_STR		(1)
#define RES_TYPE_INT		(2)
#define RES_TYPE_BSTR		(3)

/* dump_flag */
#define DUMP_NORMAL		(1)
#define DUMP_WITH_MAC		(2)

/* return value when remote object is not found */
#define NC_STRVAL		"** No Information **"
#define NC_INTVAL		"99"

/* module keys */
#define MODULE_KEY_DISCOVERY		"lldp.discovery"
#define MODULE_KEY_GET_OID		"lldp.rem.getoid"
#define MODULE_KEY_REM_PORT_TYPE	"lldp.rem.port.type"
#define MODULE_KEY_REM_PORT_ID		"lldp.rem.port.id"
#define MODULE_KEY_REM_PORTDESC		"lldp.rem.port.desc"
#define MODULE_KEY_REM_SYSNAME		"lldp.rem.sysname"
#define MODULE_KEY_REM_SYS_DESC		"lldp.rem.sys.desc"
#define MODULE_KEY_REM_CHASSIS_TYPE	"lldp.rem.chassis.type"
#define MODULE_KEY_REM_CHASSIS_ID	"lldp.rem.chassis.id"

/* retrieve oids */
#define LOC_PORTID_OID			".1.0.8802.1.1.2.1.3.7.1.3"	/* LLDP-MIB::lldpLocPortId */
#define REM_TYPE_OID			".1.0.8802.1.1.2.1.4.1.1.6"	/* LLDP-MIB::lldpRemPortIdSubtype */
#define REM_PORTID_OID			".1.0.8802.1.1.2.1.4.1.1.7"	/* LLDP-MIB::lldpRemPortId */
#define REM_PORTDESC_OID		".1.0.8802.1.1.2.1.4.1.1.8"	/* LLDP-MIB::lldpRemPortDesc */
#define REM_SYSNAME_OID			".1.0.8802.1.1.2.1.4.1.1.9"	/* LLDP-MIB::lldpRemSysName */
#define REM_SYSDESC_OID			".1.0.8802.1.1.2.1.4.1.1.10"	/* LLDP-MIB::lldpRemSysDesc */
#define REM_CHASSIS_TYPE_OID		".1.0.8802.1.1.2.1.4.1.1.4"	/* LLDP-MIB::lldpRemChassisIdSubtype */
#define REM_CHASSIS_ID_OID		".1.0.8802.1.1.2.1.4.1.1.5"	/* LLDP-MIB::lldpRemChassisId */
#define REM_CAP_SUPPORTED_OID		".1.0.8802.1.1.2.1.4.1.1.11"	/* LLDP-MIB::lldpRemSysCapSupported */
#define REM_CAP_ENABLED_OID		".1.0.8802.1.1.2.1.4.1.1.12"	/* LLDP-MIB::lldpRemSysCapEnabled */

#define IF_OID				".1.3.6.1.2.1.31.1.1.1.1"	/* IF-MIB::ifName */
#define IF_DESC_OID			".1.3.6.1.2.1.2.2.1.2"		/* IF-MIB::ifDescr */
#define IF_TYPE_OID			".1.3.6.1.2.1.2.2.1.3"		/* IF-MIB::ifType */

/* lld macros */
#define LLD_MACRO_PORT_INDEX		"{#PORT_NUM}"
#define LLD_MACRO_REM_INDEX		"{#REM_IDX}"
#define LLD_MACRO_PORT_NAME		"{#PORT_NAME}"

#define LLD_MACRO_PORT_IDENTIFIER	"{#PORT_IDX}"
#define LLD_MACRO_PORT_DESC		"{#PORT_DESC}"
#define LLD_MACRO_IF_TYPE		"{#IF_TYPE}"

/* the variable keeps timeout setting for item processing */
static int	item_timeout = 0;

/******************************************************************************
 *                                                                            *
 * This is zbx_snmp_walk() callback function prototype.                       *
 *                                                                            *
 * Parameters: arg   - [IN] an user argument passed to lldpmod_snmp_walk()    *
 *                          function                                          *
 *             var   - [IN] the variable list processing now                  *
 *                                                                            *
 ******************************************************************************/
typedef int (lldpmod_snmpwalk_cb_func)(void *arg, const netsnmp_variable_list *var);

/* function prototype */
int	lldpmod_lldp_discovery(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_common(AGENT_REQUEST *request, AGENT_RESULT *result, const char *get_mib, lldpmod_getrem_data_t *getrem_data);
int	lldpmod_getrem_port_type(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_port_id(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_port_desc(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_sysname(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_sys_desc(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_oid(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_chassis_type(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_chassis_id(AGENT_REQUEST *request, AGENT_RESULT *result);

static ZBX_METRIC keys[] =
/*	KEY				FLAG		FUNCTION			TEST PARAMETERS */
{
	{MODULE_KEY_DISCOVERY,		CF_HAVEPARAMS,	lldpmod_lldp_discovery,		"{HOST.IP},public,local"},
	{MODULE_KEY_REM_PORT_TYPE,	CF_HAVEPARAMS,	lldpmod_getrem_port_type,	"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
	{MODULE_KEY_REM_PORT_ID,	CF_HAVEPARAMS,	lldpmod_getrem_port_id,		"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
	{MODULE_KEY_REM_PORTDESC,	CF_HAVEPARAMS,	lldpmod_getrem_port_desc,	"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
	{MODULE_KEY_REM_SYSNAME,	CF_HAVEPARAMS,	lldpmod_getrem_sysname,		"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
	{MODULE_KEY_REM_SYS_DESC,	CF_HAVEPARAMS,	lldpmod_getrem_sys_desc,	"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
	{MODULE_KEY_GET_OID,		CF_HAVEPARAMS,	lldpmod_getrem_oid,		"{HOST.IP},{$SNMP_COMMUNITY},LLDP-MIB::lldpRemSysDesc,{#PORT_NUM},{#REM_IDX}"},
	{MODULE_KEY_REM_CHASSIS_TYPE,	CF_HAVEPARAMS,	lldpmod_getrem_chassis_type,	"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
	{MODULE_KEY_REM_CHASSIS_ID,	CF_HAVEPARAMS,	lldpmod_getrem_chassis_id,		"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
	{NULL}
};


/******************************************************************************
 *                                                                            *
 * Function: zbx_module_api_version                                           *
 *                                                                            *
 * Purpose: returns version number of the module interface                    *
 *                                                                            *
 * Return value: ZBX_MODULE_API_VERSION - version of module.h module is       *
 *               compiled with, in order to load module successfully Zabbix   *
 *               MUST be compiled with the same version of this header file   *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_api_version(void)
{
#ifdef ZBX_MODULE_API_VERSION
	return ZBX_MODULE_API_VERSION;
#else	/* ZBX_MODULE_API_VERSION_ONE (Zabbix2.x) */
	return ZBX_MODULE_API_VERSION_ONE;
#endif
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_timeout                                          *
 *                                                                            *
 * Purpose: set timeout value for processing of items                         *
 *                                                                            *
 * Parameters: timeout - timeout in seconds, 0 - no timeout set               *
 *                                                                            *
 ******************************************************************************/
void	zbx_module_item_timeout(int timeout)
{
	item_timeout = timeout;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_list                                             *
 *                                                                            *
 * Purpose: returns list of item keys supported by the module                 *
 *                                                                            *
 * Return value: list of item keys                                            *
 *                                                                            *
 ******************************************************************************/
ZBX_METRIC	*zbx_module_item_list(void)
{
	return keys;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_init                                                  *
 *                                                                            *
 * Purpose: the function is called on agent startup                           *
 *          It should be used to call any initialization routines             *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - module initialization failed               *
 *                                                                            *
 * Comment: the module won't be loaded in case of ZBX_MODULE_FAIL             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_init(void)
{
	int ret = ZBX_MODULE_FAIL;

#ifdef ZBX_PROGRAM_TYPE_SERVER
	switch (program_type)
	{
		case ZBX_PROGRAM_TYPE_SERVER:
			zabbix_log(LOG_LEVEL_WARNING, "%s: loaded by server process. [%d]", MODULE_NAME, program_type);
			ret = ZBX_MODULE_OK;
			break;
		case ZBX_PROGRAM_TYPE_PROXY_ACTIVE:
		case ZBX_PROGRAM_TYPE_PROXY_PASSIVE:
		case ZBX_PROGRAM_TYPE_PROXY:
			zabbix_log(LOG_LEVEL_WARNING, "%s: loaded by proxy process. [%d]", MODULE_NAME, program_type);
			ret = ZBX_MODULE_OK;
			break;
		case ZBX_PROGRAM_TYPE_AGENTD:
			zabbix_log(LOG_LEVEL_INFORMATION, "%s: This loadable-module is not runnning by agent process. [%d]", MODULE_NAME, program_type);
			break;
		default:
			zabbix_log(LOG_LEVEL_INFORMATION, "%s: unknown program_type [%d]", MODULE_NAME, program_type );
	}
#else	/* ZBX_DAEMON_TYPE_SERVER (Zabbix2.x) */
	switch (daemon_type)
	{
		case ZBX_DAEMON_TYPE_SERVER:
			zabbix_log(LOG_LEVEL_WARNING, "%s: loaded by server process. [%d]", MODULE_NAME, daemon_type);
			ret = ZBX_MODULE_OK;
			break;
		case ZBX_DAEMON_TYPE_PROXY_ACTIVE:
		case ZBX_DAEMON_TYPE_PROXY_PASSIVE:
		case ZBX_DAEMON_TYPE_PROXY:
			zabbix_log(LOG_LEVEL_WARNING, "%s: loaded by proxy process. [%d]", MODULE_NAME, daemon_type);
			ret = ZBX_MODULE_OK;
			break;
		case ZBX_DAEMON_TYPE_AGENT:
			zabbix_log(LOG_LEVEL_INFORMATION, "%s: This loadable-module is not runnning by agent process. [%d]", MODULE_NAME, daemon_type);
			break;
		default:
			zabbix_log(LOG_LEVEL_INFORMATION, "%s: unknown daemon_type [%d]", MODULE_NAME, daemon_type );
	}
#endif

	init_snmp(SNMP_NAME);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_uninit                                                *
 *                                                                            *
 * Purpose: the function is called on agent shutdown                          *
 *          It should be used to cleanup used resources if there are any      *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - function failed                            *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_uninit(void)
{
	snmp_shutdown(SNMP_NAME);

	return ZBX_MODULE_OK;
}


/******************************************************************************
 *                                                                            *
 * Function: get_if_idx                                                       *
 *                                                                            *
 * Purpose: substring lower 2 digits from oid (to link if_index to portid)    *
 *                                                                            *
 * Parameters: in          - [IN] oid                                         *
 *             out         - [OUT] extracted string buffer                    *
 *             max_out_len - [IN] buffer length                               *
 *                                                                            *
 ******************************************************************************/
static void get_if_idx(void *in, char *out, size_t max_out_len)
{
	oid *port_num = (oid *)in;

	char temp_char[MAX_ID_LEN];
	char *temp_start;

	zbx_snprintf(temp_char, sizeof(temp_char), "%lu", *port_num);
	if (strlen(temp_char) >= 2)
	{
		if (temp_char[strlen(temp_char) - 2] == '0')
		{
			temp_start = &temp_char[strlen(temp_char) - 1];
		}
		else
		{
			temp_start = &temp_char[strlen(temp_char) - 2];
		}
	}
	else
	{
		temp_start = temp_char;
	}
	zbx_strlcpy(out, temp_start, max_out_len);
}

/******************************************************************************
 *                                                                            *
 * Function: dump                                                             *
 *                                                                            *
 * Purpose: dump binary data                                                  *
 *                                                                            *
 * Parameters: separator - [IN] specify separator                             *
 *             src       - [IN] original data                                 *
 *             srclen    - [IN] original data length                          *
 *             dest      - [OUT] output string                                *
 *                                                                            *
 ******************************************************************************/
static void	dump(const char separator, const unsigned char *src, size_t srclen, char *dest)
{
	size_t i;
	char *dstp = dest;

	for (i = 0; i < srclen; i++)
	{
		zbx_snprintf(dstp, (srclen * 3) - (i * 3), "%02X%c", src[i], separator);
		dstp += 3;
	}
	*(dstp - 1) = '\0';
}

/******************************************************************************
 *                                                                            *
 * Function: isasciistr                                                       *
 *                                                                            *
 * Purpose: check data is binary or ascii string                              *
 *                                                                            *
 * Parameters: cz     - [IN] target data                                      *
 *             len    - [IN] target length                                    *
 *             srclen - [IN] original data length                             *
 *             dest   - [OUT] output string                                   *
 *                                                                            *
 * Return value: TRUE - target is ascii string                                *
 *               FALSE - target is not ascii string                           *
 *                                                                            *
 ******************************************************************************/
static int	isasciistr(unsigned char *cz, size_t len)
{
	int i;
	if (len <= 0) return TRUE;
	for (i = 0; i < len; i++)
	{
		/* depending on the implementation of the target device,  */
		/* sometimes it has '0x00' at the end of the string.      */
		/* (probably it seems to be a bug that does not consider  */
		/* the length of the terminate character.)                */
		/* In order to avoid being judged as binary at that time, */
		/* make it recognize as a character string only           */
		/* when the last character is '0x00'                      */
		if (!isprint(cz[i]) && !isspace(cz[i]))
		{
			if (i < (len - 1) || (i == (len - 1) && 0 != cz[i]))
				return FALSE;
		}
	}
	return TRUE;
}

/******************************************************************************
 *                                                                            *
 * Function: init_discovery_request                                           *
 *                                                                            *
 * Purpose: initialize the discovery_request structure                        *
 *                                                                            *
 * Parameters: request            - [OUT] pointer to the structure            *
 *             lld_key_macro      - [IN] lld_macro assigned to indexes        *
 *             lld_remkey_macro   - [IN] lld_macro assigned to remote indexes *
 *             lld_optvalue_macro - [IN] lld_macro assigned to option values  *
 *             opt_func           - [IN] function poiner for process          *
 *                                       option values                        *
 *             object_num         - [IN] number of oids to retrieve           *
 *                                                                            *
 ******************************************************************************/
static void	init_discovery_request(lldpmod_discovery_request_t *request,
			const char *lld_key_macro, const char *lld_remkey_macro,
			const char *lld_optvalue_macro, lldpmod_optproc_func opt_func, int object_num)
{
	request->noid = 0;
	request->noid_rem = 0;
	request->noid_max = object_num;

	request->lld_key_macro = (char *)lld_key_macro;
	request->lld_remkey_macro = (char *)lld_remkey_macro;
	request->lld_optvalue_macro = (char *)lld_optvalue_macro;
	request->opt_func = opt_func;
	
	request->rem_map = NULL;
	request->key_types = NULL;
	request->res_types = NULL;
	request->dump_flags = NULL;
	request->oids = NULL;
	request->lld_value_macros = NULL;

	/* memory allocation */
	request->rem_map = (int *)zbx_malloc(request->rem_map, object_num * sizeof(int));
	request->key_types = (int *)zbx_malloc(request->key_types, object_num * sizeof(int));
	request->res_types = (int *)zbx_malloc(request->res_types, object_num * sizeof(int));
	request->dump_flags = (int *)zbx_malloc(request->dump_flags, object_num * sizeof(int));
	request->oids = (char **)zbx_malloc(request->oids, object_num * sizeof(char *));
	request->lld_value_macros = (char **)zbx_malloc(request->lld_value_macros, object_num * sizeof(char *));

}

/******************************************************************************
 *                                                                            *
 * Function: free_discovery_request                                           *
 *                                                                            *
 * Purpose: free memory used by the discovery_request                         *
 *                                                                            *
 * Parameters: request - pointer to the discovery_request structure           *
 *                                                                            *
 ******************************************************************************/
static void	free_discovery_request(lldpmod_discovery_request_t *request)
{
	request->noid = 0;
	request->noid_rem = 0;
	request->noid_max = 0;

	/* strings in the request array such as                        */
	/* request->oids[i], request->lld_value_macros[i]              */
	/* are pointers to constants, so do not free. only array free. */
	zbx_free(request->rem_map);
	zbx_free(request->key_types);
	zbx_free(request->res_types);
	zbx_free(request->dump_flags);
	zbx_free(request->oids);
	zbx_free(request->lld_value_macros);

}

/******************************************************************************
 *                                                                            *
 * Function: add_discovery_request                                            *
 *                                                                            *
 * Purpose: add a new parameter                                               *
 *                                                                            *
 * Parameters: request         - [OUT] pointer to the request structure       *
 *             key_type        - [IN] KEY_TYPE_LOC or KEY_TYPE_REM            *
 *             res_type        - [IN] only RES_TYPE_STR for discovery         *
 *             dump_flag       - [IN] DUMP_NORMAL(space) or                   *
 *                                    DUMP_WITH_MAC(coron)                    *
 *             oid             - [IN] oid to retrieve                         *
 *             lld_value_macro - [IN] lld macro collesponding to oid's value  *
 *                                                                            *
 ******************************************************************************/
static void	add_discovery_request(lldpmod_discovery_request_t *request,
		int key_type, int res_type, int dump_flag, char *oid, char *lld_value_macro)
{
	if (request->noid >= request->noid_max)
		return;

	request->noid++;
	if (KEY_TYPE_REM == key_type)
	{
		request->noid_rem++;
		request->rem_map[request->noid - 1] = request->noid_rem - 1;
	}
	else
	{
		request->rem_map[request->noid - 1] = request->noid - request->noid_rem - 1;
	}

	request->res_types[request->noid - 1] = res_type;
	request->key_types[request->noid - 1] = key_type;
	request->dump_flags[request->noid - 1] = dump_flag;
	request->oids[request->noid - 1] = oid;
	request->lld_value_macros[request->noid - 1] = lld_value_macro;

}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_get_snmp_type_error                                      *
 *                                                                            *
 * Purpose: convert snmp type to string                                       *
 *                                                                            *
 * Parameters: type - snmp type                                               *
 *                                                                            *
 * Return value: string collesponding to snmp type                            *
 *                                                                            *
 ******************************************************************************/
static char	*lldpmod_get_snmp_type_error(u_char type)
{
	switch (type)
	{
		case SNMP_NOSUCHOBJECT:
			return zbx_strdup(NULL, "No Such Object available on this agent at this OID");
		case SNMP_NOSUCHINSTANCE:
			return zbx_strdup(NULL, "No Such Instance currently exists at this OID");
		case SNMP_ENDOFMIBVIEW:
			return zbx_strdup(NULL, "No more variables left in this MIB View"
					" (it is past the end of the MIB tree)");
		default:
			return zbx_dsprintf(NULL, "Value has unknown type 0x%02X", (unsigned int)type);
	}
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_get_snmp_response_error                                  *
 *                                                                            *
 * Purpose: convert snmp code to string                                       *
 *                                                                            *
 * Parameters: ss        - [IN] snmp session                                  *
 *             status    - [IN] snmp status code                              *
 *             resoponse - [IN] snmp_response structure                       *
 *             error     - [OUT] error message buffer                         *
 *             error_len - [IN] buffer length                                 *
 *                                                                            *
 * Return value: item return code                                             *
 *                                                                            *
 ******************************************************************************/
static int	lldpmod_get_snmp_response_error(const struct snmp_session *ss, int status,
		const struct snmp_pdu *response, char *error, size_t max_error_len)
{
	int	ret;

	/* error in response */
	if (STAT_SUCCESS == status)
	{
		zbx_snprintf(error, max_error_len, "SNMP error: %s", snmp_errstring(response->errstat));
		ret = NOTSUPPORTED;
	}
	else if (STAT_ERROR == status)
	{
		zbx_snprintf(error, max_error_len, "Cannot connect to \"%s\": %s.", ss->peername, snmp_api_errstring(ss->s_snmp_errno));
		ret = NETWORK_ERROR;
	}
	else if (STAT_TIMEOUT == status)
	{
		zbx_snprintf(error, max_error_len, "Timeout while connecting to \"%s\".", ss->peername);
		ret = NETWORK_ERROR;
	}
	else
	{
		zbx_snprintf(error, max_error_len, "SNMP error: [%d]", status);
		ret = NOTSUPPORTED;
	}

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmp_open_session                                        *
 *                                                                            *
 * Purpose: open snmp session                                                 *
 *                                                                            *
 * Parameters: IP        - [IN] IP address of monitoring object               *
 *             community - [IN] snmp community name                           *
 *             error     - [OUT] error message buffer                         *
 *             error_len - [IN] buffer length                                 *
 *                                                                            *
 * Return value: item return code                                             *
 *                                                                            *
 ******************************************************************************/
static netsnmp_session	*lldpmod_snmp_open_session(const char *IP, const char *community, char *error, size_t max_error_len)
{
	netsnmp_session	session, *ss = NULL;

	snmp_sess_init(&session);

	session.version = SNMP_VERSION_2c;

	session.timeout = item_timeout * 1000 * 1000;	/* timeout of one attempt in microseconds */
							/* (net-snmp default = 1 second) */
	session.retries = 0;

	session.peername = (char *)IP;

	session.community = (u_char *)community;
	
	session.community_len = strlen((char *)session.community);

	SOCK_STARTUP;

	if (NULL == (ss = snmp_open(&session)))
	{
		SOCK_CLEANUP;
		zbx_strlcpy(error, "Cannot open SNMP session", max_error_len);
	}

	return ss;
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmp_close_session                                       *
 *                                                                            *
 * Purpose: close snmp session                                                *
 *                                                                            *
 * Parameters: session - [IN] snmp session                                    *
 *                                                                            *
 ******************************************************************************/
static void	lldpmod_snmp_close_session(netsnmp_session *session)
{
	snmp_close(session);
	SOCK_CLEANUP;
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_get_octet_string                                         *
 *                                                                            *
 * Purpose: copy snmp string to buffer                                        *
 *                                                                            *
 * Parameters: var - [IN] snmp variable_list                                  *
 *                                                                            *
 * Return value: pointer to string                                            *
 *                                                                            *
 ******************************************************************************/
static char	*lldpmod_get_octet_string(const struct variable_list *var)
{
	const char	*hint;
	char		buffer[MAX_STRING_LEN];
	char		*strval_dyn = NULL;
	struct tree     *subtree;

	/* find the subtree to get display hint */
	subtree = get_tree(var->name, var->name_length, get_tree_head());
	hint = (NULL != subtree ? subtree->hint : NULL);

	/* we will decide if we want the value from var->val or what snprint_value() returned later */
	if (-1 == snprint_value(buffer, sizeof(buffer), var->name, var->name_length, var))
		goto end;

	if (0 == strncmp(buffer, "Hex-STRING: ", 12))
	{
		strval_dyn = zbx_strdup(strval_dyn, buffer + 12);
	}
	else if (NULL != hint && 0 == strncmp(buffer, "STRING: ", 8))
	{
		strval_dyn = zbx_strdup(strval_dyn, buffer + 8);
	}
	else if (0 == strncmp(buffer, "OID: ", 5))
	{
		strval_dyn = zbx_strdup(strval_dyn, buffer + 5);
	}
	else if (0 == strncmp(buffer, "BITS: ", 6))
	{
		strval_dyn = zbx_strdup(strval_dyn, buffer + 6);
	}
	else
	{
		/* snprint_value() escapes hintless ASCII strings, so */
		/* we are copying the raw unescaped value in this case */

		strval_dyn = (char *)zbx_malloc(strval_dyn, var->val_len + 1);
		memcpy(strval_dyn, var->val.string, var->val_len);
		strval_dyn[var->val_len] = '\0';
	}

end:
	return strval_dyn;
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmp_set_result                                          *
 *                                                                            *
 * Purpose: set snmp result to zabbix agent_result                            *
 *                                                                            *
 * Parameters: var       - [IN] snmp variable_list                            *
 *             result    - [OUT] zabbix agent_result                          *
 *             res_type  - [IN] RES_TYPE_STR or RES_TYPE_BSTR or RES_TYPE_INT *
 *             dump_flag - [IN] DUMP_NORMAL(space) or DUMP_WITH_MAC(coron)    *
 *                                                                            *
 * Return value: item return code                                             *
 *                                                                            *
 ******************************************************************************/
static int	lldpmod_snmp_set_result(const struct variable_list *var, AGENT_RESULT *result, int res_type, int dump_flag)
{
	char		*strval_dyn = NULL;
	int		ret = SUCCEED;
	
	if (res_type == RES_TYPE_BSTR)
	{
		/* if RES_TYPE_BSTR, use lldpmod_get_octet_string */
		if (NULL == (strval_dyn = lldpmod_get_octet_string(var)))
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Cannot receive string value: out of memory."));
			ret = NOTSUPPORTED;
		}
		else
		{
			zbx_replace_invalid_utf8(strval_dyn);
			SET_TEXT_RESULT(result, zbx_strdup(NULL, strval_dyn));
			zbx_free(strval_dyn);
		}
	}
	else if (ASN_OCTET_STR == var->type || ASN_OBJECT_ID == var->type)
	{
		/* if other res_type, and ASN_OCTET_STR or ASN_OBJECT_ID */
		if (isasciistr(var->val.string, var->val_len))
		{
			/* if ascii string, simply copy strings */
			if (NULL == (strval_dyn = (char *)zbx_malloc(strval_dyn, 1 + var->val_len)))
			{
				SET_MSG_RESULT(result, zbx_strdup(NULL, "Cannot receive string value: out of memory."));
				ret = NOTSUPPORTED;
			}
			else
			{
				memcpy(strval_dyn, var->val.string, var->val_len);
				strval_dyn[var->val_len] = '\0';

				zbx_replace_invalid_utf8(strval_dyn);
				SET_TEXT_RESULT(result, zbx_strdup(NULL, strval_dyn));
				zbx_free(strval_dyn);
			}
		}
		else
		{
			/* in other case, dump binaries */
			if (NULL == (strval_dyn = (char *)zbx_malloc(strval_dyn, 3 * (var->val_len) + 1)))
			{
				SET_MSG_RESULT(result, zbx_strdup(NULL, "Cannot receive string value: out of memory."));
				ret = NOTSUPPORTED;
			}
			else
			{
				char separator;
				if (DUMP_WITH_MAC == dump_flag)
					separator = ':';
				else
					separator = ' ';
				dump(separator, (const unsigned char *)var->val.string, var->val_len, strval_dyn);

				zbx_replace_invalid_utf8(strval_dyn);
				SET_TEXT_RESULT(result, zbx_strdup(NULL, strval_dyn));
				zbx_free(strval_dyn);
			}
		}
	}
#ifdef OPAQUE_SPECIAL_TYPES
	else if (ASN_UINTEGER == var->type || ASN_COUNTER == var->type || ASN_OPAQUE_U64 == var->type ||
			ASN_TIMETICKS == var->type || ASN_GAUGE == var->type)
#else
	else if (ASN_UINTEGER == var->type || ASN_COUNTER == var->type ||
			ASN_TIMETICKS == var->type || ASN_GAUGE == var->type)
#endif
	{
		SET_UI64_RESULT(result, (unsigned long)*var->val.integer);
	}
#ifdef OPAQUE_SPECIAL_TYPES
	else if (ASN_COUNTER64 == var->type || ASN_OPAQUE_COUNTER64 == var->type)
#else
	else if (ASN_COUNTER64 == var->type)
#endif
	{
		SET_UI64_RESULT(result, (((zbx_uint64_t)var->val.counter64->high) << 32) +
				(zbx_uint64_t)var->val.counter64->low);
	}
#ifdef OPAQUE_SPECIAL_TYPES
	else if (ASN_INTEGER == var->type || ASN_OPAQUE_I64 == var->type)
#else
	else if (ASN_INTEGER == var->type)
#endif
	{
		char	buffer[21];

		zbx_snprintf(buffer, sizeof(buffer), "%ld", *var->val.integer);

		zbx_replace_invalid_utf8(buffer);
		SET_TEXT_RESULT(result, zbx_strdup(NULL, buffer));
	}
#ifdef OPAQUE_SPECIAL_TYPES
	else if (ASN_OPAQUE_FLOAT == var->type)
	{
		SET_DBL_RESULT(result, *var->val.floatVal);
	}
	else if (ASN_OPAQUE_DOUBLE == var->type)
	{
		SET_DBL_RESULT(result, *var->val.doubleVal);
	}
#endif
	else if (ASN_IPADDRESS == var->type)
	{
		SET_STR_RESULT(result, zbx_dsprintf(NULL, "%u.%u.%u.%u",
				(unsigned int)var->val.string[0],
				(unsigned int)var->val.string[1],
				(unsigned int)var->val.string[2],
				(unsigned int)var->val.string[3]));
	}
	else
	{
		SET_MSG_RESULT(result, lldpmod_get_snmp_type_error(var->type));
		ret = NOTSUPPORTED;
	}

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmp_object_hash                                         *
 *                                                                            *
 * Purpose: return hash value for snmp oid                                    *
 *                                                                            *
 * Parameters: data - [IN] target oid                                         *
 *                                                                            *
 * Return value: hash value                                                   *
 *                                                                            *
 ******************************************************************************/
static zbx_hash_t	lldpmod_snmp_object_hash(const void *data)
{
	const oid	*key = (const oid *)data;

#ifdef ZBX_DEFAULT_HASH_ALGO
	return ZBX_DEFAULT_HASH_ALGO(key, sizeof(oid), ZBX_DEFAULT_HASH_SEED);
#else	/* ZBX_DEFAULT_UINT64_HASH_ALGO (Zabbix2.x) */
	return ZBX_DEFAULT_UINT64_HASH_ALGO(key, sizeof(oid), ZBX_DEFAULT_HASH_SEED);
#endif

}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmp_object_compare                                      *
 *                                                                            *
 * Purpose: compare funcion for oid                                           *
 *                                                                            *
 * Parameters: d1 - [IN] target oid 1                                         *
 *             d2 - [IN] target oid 2                                         *
 *                                                                            *
 * Return value: -1 if d1 < d2, 0 if d1 = d2, 1 if d1 > d2                    *
 *                                                                            *
 ******************************************************************************/
static int	lldpmod_snmp_object_compare(const void *d1, const void *d2)
{
	const oid	*k1 = (const oid *)d1;
	const oid	*k2 = (const oid *)d2;

	if (d1 == d2)
		return 0;

	return snmp_oid_compare(k1, 1, k2, 1);

}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmp_data_clean                                          *
 *                                                                            *
 * Purpose: releases data allocated by snmp walk or discovery                 *
 *                                                                            *
 * Parameters: data - [IN] snmpwalk data object                               *
 *                                                                            *
 ******************************************************************************/
static void	lldpmod_snmp_data_clean(lldpmod_discovery_data_t *data)
{
	int			i;
	lldpmod_snmp_object_t	*obj;
	lldpmod_snmp_object_t	*rem_obj;
	zbx_hashset_iter_t	iter;
	zbx_hashset_iter_t	rem_iter;

	zbx_hashset_iter_reset(&data->objects, &iter);
	while (NULL != (obj = (lldpmod_snmp_object_t *)zbx_hashset_iter_next(&iter)))
	{
		if (NULL != obj->rem_objects)
		{
			zbx_hashset_iter_reset(obj->rem_objects, &rem_iter);
			while (NULL != (rem_obj = (lldpmod_snmp_object_t *)zbx_hashset_iter_next(&rem_iter)))
			{
				for (i = 0; i < data->request.noid; i++)
				{
					if (KEY_TYPE_REM == data->request.key_types[i])
						zbx_free(rem_obj->values[data->request.rem_map[i]]);
				}
				zbx_free(rem_obj->values);
			}
			zbx_hashset_destroy(obj->rem_objects);
			zbx_free(obj->rem_objects);
		}

		for (i = 0; i < data->request.noid; i++)
			zbx_free(obj->values[i]);

		zbx_free(obj->values);
	}

	zbx_hashset_destroy(&data->objects);

	free_discovery_request(&data->request);
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmpwalk                                                 *
 *                                                                            *
 * Purpose: get data from monitoring object by snmp-walk                      *
 *                                                                            *
 * Parameters: session - [IN] snmp session                                    *
 *             OID          - target OID of snmp-walk                         *
 *             error        - [OUT] error message buffer                      *
 *             error_len    - [IN] buffer length                              *
 *             walk_cb_func - [IN] callback function to process walked OIDs   *
 *                                 and their values                           *
 *             walk_cb_arg  - [IN] argument to pass to the callback function  *
 *                                                                            *
 * Return value: item return code                                             *
 *                                                                            *
 ******************************************************************************/
static int	lldpmod_snmpwalk(struct snmp_session *ss, const char *OID, char *error,	size_t max_error_len, 
		lldpmod_snmpwalk_cb_func walk_cb_func, void *walk_cb_arg)
{
	netsnmp_pdu		*pdu, *response;
	oid			name[MAX_OID_LEN], root[MAX_OID_LEN];
	size_t			name_len = MAX_OID_LEN, root_len = MAX_OID_LEN;

	netsnmp_variable_list	*var;
	int			status, running, ret = SUCCEED;

	/* create OID from string */
	if (NULL == snmp_parse_oid(OID, root, &root_len))
	{
		zbx_snprintf(error, max_error_len, "snmp_parse_oid(): cannot parse OID \"%s\".", OID);
		ret = CONFIG_ERROR;
		goto out;
	}

	/* copy root to name */
	memcpy(name, root, root_len * sizeof(oid));
	name_len = root_len;

	/* initialize variables */
	running = 1;

	while (running)
	{
		if (NULL == (pdu = snmp_pdu_create(SNMP_MSG_GETNEXT)))
		{
			zbx_strlcpy(error, "snmp_pdu_create(): cannot create PDU object.", max_error_len);
			ret = NOTSUPPORTED;
			break;
		}

		if (NULL == snmp_add_null_var(pdu, name, name_len))	/* add OID as variable to PDU */
		{
			zbx_strlcpy(error, "snmp_add_null_var(): cannot add null variable.", max_error_len);
			ret = NOTSUPPORTED;
			snmp_free_pdu(pdu);
			break;
		}

		status = snmp_synch_response(ss, pdu, &response);

		if (STAT_SUCCESS != status || SNMP_ERR_NOERROR != response->errstat)
		{
			ret = lldpmod_get_snmp_response_error(ss, status, response, error, max_error_len);
			running = 0;
			goto next;
		}

		/* check retrieved variables */
		for (var = response->variables; var; var = var->next_variable)
		{
			if (SNMP_ENDOFMIBVIEW == var->type || var->name_length < root_len ||
				0 != memcmp(root, var->name, root_len * sizeof(oid)))
			{
				/* not part of this subtree */
				running = 0;
				break;
			}
			else if (SNMP_NOSUCHOBJECT != var->type && SNMP_NOSUCHINSTANCE != var->type)
			{
				/* callback functions */
				if (SUCCEED != (ret = walk_cb_func(walk_cb_arg, var)))
				{
					running = 0;
					break;
				}

				/* get next */
				memcpy((char *)name, (char *)var->name, var->name_length * sizeof(oid));
				name_len = var->name_length;
			}
			else
			{
				/* an exception value, so stop */
				char	*errmsg;

				errmsg = lldpmod_get_snmp_type_error(var->type);
				zbx_strlcpy(error, errmsg, max_error_len);
				zbx_free(errmsg);
				ret = NOTSUPPORTED;
				running = 0;
				break;
			}

		}	/* for (var = response->variables; var; var = var->next_variable) */

next:
		if (NULL != response)
			snmp_free_pdu(response);

	}	/* while (running) */

out:
	return ret;

}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmpwalk_getrem_cb                                       *
 *                                                                            *
 * Purpose: callback function pass to lldpmod_snmpwalk to get a remote object *
 *          walk remote objects ignore timemark while checking index and      *
 *          remote index                                                      *
 *                                                                            *
 * Parameters: arg - [IN/OUT] condition and result                            *
 *             var - [IN] snmp variable_list                                  *
 *                                                                            *
 * Return value: SUCCEED_WITH_VALUE - succeed and found a remote object       *
 *               SUCCEED - no error occured but not found any remote object   *
 *               other item return code - error occured                       *
 *                                                                            *
 ******************************************************************************/
int lldpmod_snmpwalk_getrem_cb(void *arg, const netsnmp_variable_list *var)
{
	lldpmod_getrem_data_t	*getrem_data = (lldpmod_getrem_data_t *)arg;
	int			ret = SUCCEED;
	int			err = SUCCEED;
	
	if (	var->name_length > 1 &&
		getrem_data->index == var->name[var->name_length - 2] &&
		getrem_data->rem_index == var->name[var->name_length - 1])
	{
		if (SUCCEED != (err = lldpmod_snmp_set_result(var, getrem_data->result, getrem_data->res_type, getrem_data->dump_flag)))
		{
			char	**msg;

			msg = GET_MSG_RESULT(getrem_data->result);

			zabbix_log(LOG_LEVEL_DEBUG, "%s: cannot get index='%lu', rem_index='%lu' value: %s", MODULE_NAME,
					(unsigned long)getrem_data->index, (unsigned long)getrem_data->rem_index, NULL != msg && NULL != *msg ? *msg : "(null)");
			
		}
		if (SUCCEED == err)
			ret = SUCCEED_WITH_VALUE;
		else
			ret = err;

	}
	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_snmpwalk_discovery_cb                                    *
 *                                                                            *
 * Purpose: callback function pass to lldpmod_snmpwalk to discovery items     *
 *          walk and collect objects with index as key to output lld rules    *
 *                                                                            *
 * Parameters: arg - [IN/OUT] request and result                              *
 *             var - [IN] snmp variable_list                                  *
 *                                                                            *
 * Return value: item return code                                             *
 *                                                                            *
 ******************************************************************************/
int lldpmod_snmpwalk_discovery_cb(void *arg, const netsnmp_variable_list *var)
{
	lldpmod_discovery_data_t	*data = (lldpmod_discovery_data_t *)arg;
	AGENT_RESULT			snmp_result;
	
	lldpmod_snmp_object_t	*obj;
	lldpmod_snmp_object_t	*rem_obj;

	oid			index;
	oid			rem_index;

	int			ret = SUCCEED;

	if (data->request.key_types[data->num] == KEY_TYPE_LOC && var->name_length > 0)
		index = var->name[var->name_length - 1];
	else if (var->name_length > 1)	/* KEY_TYPE_REM */
	{
		index = var->name[var->name_length - 2];
		rem_index = var->name[var->name_length - 1];
	}
	else
	{
		return ret;
	}

	init_result(&snmp_result);
	if (SUCCEED == lldpmod_snmp_set_result(var, &snmp_result, data->request.res_types[data->num], data->request.dump_flags[data->num]) &&
		NULL != GET_STR_RESULT(&snmp_result))
	{
		if (NULL == (obj = (lldpmod_snmp_object_t *)zbx_hashset_search(&data->objects, &index)))
		{
			/* if index not found, create new hash */
			lldpmod_snmp_object_t	new_obj;

			new_obj.index = index;
			new_obj.values = (char **)zbx_malloc(NULL, sizeof(char *) * data->request.noid);
			memset(new_obj.values, 0, sizeof(char *) * data->request.noid);
			new_obj.rem_objects = NULL;

			obj = (lldpmod_snmp_object_t *)zbx_hashset_insert(&data->objects, &new_obj, sizeof(new_obj));
		}

		if (NULL != obj)
		{
			if (data->request.key_types[data->num] == KEY_TYPE_REM)
			{
				/* if remote object and rem_index not found, create new hash */
				if (NULL == obj->rem_objects || NULL == (rem_obj = (lldpmod_snmp_object_t *)zbx_hashset_search(obj->rem_objects, &rem_index)))
				{
					if (NULL == obj->rem_objects)
					{
						obj->rem_objects = (zbx_hashset_t *)zbx_malloc(NULL, sizeof(zbx_hashset_t));
						zbx_hashset_create(obj->rem_objects, 10, lldpmod_snmp_object_hash, lldpmod_snmp_object_compare);
					}
					lldpmod_snmp_object_t	new_rem_obj;

					new_rem_obj.index = rem_index;
					new_rem_obj.values = (char **)zbx_malloc(NULL, sizeof(char *) * data->request.noid_rem);
					memset(new_rem_obj.values, 0, sizeof(char *) * data->request.noid_rem);
					new_rem_obj.rem_objects = NULL;

					rem_obj = (lldpmod_snmp_object_t *)zbx_hashset_insert(obj->rem_objects, &new_rem_obj, sizeof(new_rem_obj));
				}
				if (NULL != rem_obj->values[data->request.rem_map[data->num]])
				{
					zabbix_log(LOG_LEVEL_WARNING, "%s: the same index appears again. oid='%s',index=%lu, rem_index=%lu, oldvalue='%s', newvalue='%s'",
						MODULE_NAME,
						data->request.oids[data->num],
						(unsigned long)index, (unsigned long)rem_index,
						rem_obj->values[data->request.rem_map[data->num]],
						snmp_result.str);
					zbx_free(rem_obj->values[data->request.rem_map[data->num]]);
				}

				rem_obj->values[data->request.rem_map[data->num]] = zbx_strdup(NULL, snmp_result.str);

			}
			else	/* KEY_TYPE_LOC */
			{
				if (NULL != obj->values[data->num])
				{
					zabbix_log(LOG_LEVEL_WARNING, "%s: the same index appears again. oid='%s',index=%lu, oldvalue='%s', newvalue='%s'",
						MODULE_NAME,
						data->request.oids[data->num],
						(unsigned long)index,
						obj->values[data->num],
						snmp_result.str);
					zbx_free(obj->values[data->num]);
				}

				obj->values[data->num] = zbx_strdup(NULL, snmp_result.str);
			}
		}

	}
	else
	{
		char	**msg;
		msg = GET_MSG_RESULT(&snmp_result);
		if (data->request.key_types[data->num] == KEY_TYPE_LOC)
			zabbix_log(LOG_LEVEL_DEBUG, "%s: cannot get index '%lu' string value: %s", MODULE_NAME,
					(unsigned long)index, NULL != msg && NULL != *msg ? *msg : "(null)");
		else
			zabbix_log(LOG_LEVEL_DEBUG, "%s: cannot get index '%lu.%lu' string value: %s", MODULE_NAME,
					(unsigned long)index, (unsigned long)rem_index, NULL != msg && NULL != *msg ? *msg : "(null)");

	}

	free_result(&snmp_result);

	return ret;
}


/******************************************************************************
 *                                                                            *
 * Function: lldpmod_lldp_discovery                                           *
 *                                                                            *
 * Purpose: a main entry point for item discovery                             *
 *                                                                            *
 * Parameters: request - structure that contains item key and parameters      *
 *              request->key - item key without parameters                    *
 *              request->nparam - number of parameters                        *
 *              request->timeout - processing should not take longer than     *
 *                                 this number of seconds                     *
 *              request->params[N-1] - pointers to item key parameters        *
 *                                                                            *
 *             result - structure that will contain result                    *
 *                                                                            *
 * Return value: SYSINFO_RET_FAIL - function failed, item will be marked      *
 *                                  as not supported by zabbix                *
 *               SYSINFO_RET_OK - success                                     *
 *                                                                            *
 * Comment: get_rparam(request, N-1) can be used to get a pointer to the Nth  *
 *          parameter starting from 0 (first parameter). Make sure it exists  *
 *          by checking value of request->nparam.                             *
 *                                                                            *
 ******************************************************************************/
int	lldpmod_lldp_discovery(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	netsnmp_session	*ss;
	char		error[MAX_STRING_LEN];

	int		ret = SYSINFO_RET_OK;
	int		err = SUCCEED;

	char		*param_ip, *param_community, *param_func;

	struct zbx_json	js;

	int				i;
	lldpmod_discovery_data_t	data;
	lldpmod_snmp_object_t		*obj;
	lldpmod_snmp_object_t		*rem_obj;
	zbx_hashset_iter_t		iter;
	zbx_hashset_iter_t		rem_iter;

	char	oid_str_buf[MAX_ID_LEN];


	if (request->nparam < 3 || request->nparam > 3)
	{
		SET_MSG_RESULT(result, strdup("Invalid number of parameters"));
		return SYSINFO_RET_FAIL;
	}
	param_ip		= get_rparam(request, 0);
	param_community		= get_rparam(request, 1);
	param_func		= get_rparam(request, 2);

	if (NULL == param_ip || '\0' == *param_ip)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 1st parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == param_community || '\0' == *param_community)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 2nd parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == param_func || '\0' == *param_func
		|| ((0 != strcmp(param_func, "local"))
		 && (0 != strcmp(param_func, "remote"))
		 && (0 != strcmp(param_func, "interface"))))
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 3rd parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == (ss = lldpmod_snmp_open_session(param_ip, param_community, error, sizeof(error))))
	{
		err = NETWORK_ERROR;
		goto out;
	}

	zbx_hashset_create(&data.objects, 10, lldpmod_snmp_object_hash, lldpmod_snmp_object_compare);

	/* setup request parameter */
	if (0 == strcmp(param_func, "local"))
	{
		init_discovery_request(&data.request, LLD_MACRO_PORT_INDEX, NULL, NULL, NULL, 1);

		add_discovery_request(&data.request, KEY_TYPE_LOC, RES_TYPE_STR, DUMP_WITH_MAC, LOC_PORTID_OID, LLD_MACRO_PORT_NAME);
	}
	else if (0 == strcmp(param_func, "remote"))
	{
		init_discovery_request(&data.request, LLD_MACRO_PORT_INDEX, LLD_MACRO_REM_INDEX, NULL, NULL, 2);

		add_discovery_request(&data.request, KEY_TYPE_LOC, RES_TYPE_STR, DUMP_WITH_MAC, LOC_PORTID_OID, LLD_MACRO_PORT_NAME);

		add_discovery_request(&data.request, KEY_TYPE_REM, RES_TYPE_STR, DUMP_WITH_MAC, REM_PORTID_OID, NULL);
	}
	else	/* if (0 == strcmp(param_func, "interface")) */
	{
		init_discovery_request(&data.request, LLD_MACRO_PORT_INDEX, NULL, LLD_MACRO_PORT_IDENTIFIER, get_if_idx, 3);

		add_discovery_request(&data.request, KEY_TYPE_LOC, RES_TYPE_STR, DUMP_NORMAL, IF_OID, LLD_MACRO_PORT_NAME);

		add_discovery_request(&data.request, KEY_TYPE_LOC, RES_TYPE_STR, DUMP_NORMAL, IF_DESC_OID, LLD_MACRO_PORT_DESC);

		add_discovery_request(&data.request, KEY_TYPE_LOC, RES_TYPE_STR, DUMP_NORMAL, IF_TYPE_OID, LLD_MACRO_IF_TYPE);

	}

	/* call net-snmp library */
	for (data.num = 0; data.num < data.request.noid; data.num++)
	{
		if (SUCCEED != (err = lldpmod_snmpwalk(ss, data.request.oids[data.num], error, sizeof(error), lldpmod_snmpwalk_discovery_cb, &data)))
		{
			zabbix_log(LOG_LEVEL_WARNING, "%s: %s", MODULE_NAME, error);
			goto clean;
		}
	}

	/* make lld rules json from retrieved result */
	zbx_json_init(&js, ZBX_JSON_STAT_BUF_LEN);
	zbx_json_addarray(&js, ZBX_PROTO_TAG_DATA);

	zbx_hashset_iter_reset(&data.objects, &iter);
	while (NULL != (obj = (lldpmod_snmp_object_t *)zbx_hashset_iter_next(&iter)))
	{
		if (0 == strcmp(param_func, "remote"))
		{
			/* lldp remote objects */
			if (NULL == obj->rem_objects || 0 == obj->rem_objects->num_data)
			{
				/* port that can't get remote information */
				zbx_json_addobject(&js, NULL);
				zbx_snprintf(oid_str_buf, sizeof(oid_str_buf), "%lu", (unsigned long)obj->index);
				zbx_json_addstring(&js, data.request.lld_key_macro, oid_str_buf, ZBX_JSON_TYPE_STRING);
				zbx_json_addstring(&js, data.request.lld_remkey_macro, "0", ZBX_JSON_TYPE_STRING);

				for (i = 0; i < data.request.noid; i++)
				{
					if (NULL != obj->values[i] && NULL != data.request.lld_value_macros[i])
						zbx_json_addstring(&js, data.request.lld_value_macros[i], obj->values[i], ZBX_JSON_TYPE_STRING);
				}
				zbx_json_close(&js);
			}
			else
			{
				/* port that can get remote information */
				zbx_hashset_iter_reset(obj->rem_objects, &rem_iter);
				while (NULL != (rem_obj = (lldpmod_snmp_object_t *)zbx_hashset_iter_next(&rem_iter)))
				{
					zbx_json_addobject(&js, NULL);

					zbx_snprintf(oid_str_buf, sizeof(oid_str_buf), "%lu", (unsigned long)obj->index);
					zbx_json_addstring(&js, data.request.lld_key_macro, oid_str_buf, ZBX_JSON_TYPE_STRING);

					zbx_snprintf(oid_str_buf, sizeof(oid_str_buf), "%lu", (unsigned long)rem_obj->index);
					zbx_json_addstring(&js, data.request.lld_remkey_macro, oid_str_buf, ZBX_JSON_TYPE_STRING);

					for (i = 0; i < data.request.noid; i++)
					{
						if (KEY_TYPE_LOC == data.request.key_types[i])
						{
							if (NULL != obj->values[i] && NULL != data.request.lld_value_macros[i])
								zbx_json_addstring(&js, data.request.lld_value_macros[i], obj->values[i], ZBX_JSON_TYPE_STRING);
						}
						else
						{
							if (NULL != rem_obj->values[data.request.rem_map[i]] && NULL != data.request.lld_value_macros[i])
								zbx_json_addstring(&js, data.request.lld_value_macros[i], rem_obj->values[data.request.rem_map[i]], ZBX_JSON_TYPE_STRING);
						}
					}

					zbx_json_close(&js);
				}
			}
		}
		else
		{
			/* lldp local objects or interface objects */
			zbx_json_addobject(&js, NULL);

			zbx_snprintf(oid_str_buf, sizeof(oid_str_buf), "%lu", (unsigned long)obj->index);
			zbx_json_addstring(&js, data.request.lld_key_macro, oid_str_buf, ZBX_JSON_TYPE_STRING);

			if (NULL != data.request.lld_optvalue_macro && NULL != data.request.opt_func)
			{
				data.request.opt_func(&obj->index, oid_str_buf, sizeof(oid_str_buf));
				zbx_json_addstring(&js, data.request.lld_optvalue_macro, oid_str_buf, ZBX_JSON_TYPE_STRING);
			}

			for (i = 0; i < data.request.noid; i++)
			{
				if (NULL != obj->values[i] && NULL != data.request.lld_value_macros[i])
					zbx_json_addstring(&js, data.request.lld_value_macros[i], obj->values[i], ZBX_JSON_TYPE_STRING);
			}

			zbx_json_close(&js);
		}
	}

	zbx_json_close(&js);

	/* make lld rules end */


	SET_TEXT_RESULT(result, zbx_strdup(NULL, js.buffer));

	zbx_json_free(&js);

clean:
	lldpmod_snmp_close_session(ss);
	lldpmod_snmp_data_clean(&data);

out:
	if (SUCCEED != err)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, error));
		ret = SYSINFO_RET_FAIL;
	}

	return ret;

}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_getrem_common                                            *
 *                                                                            *
 * Purpose: common function of get specific remote object                     *
 *          each caller functions pass oid, dump_flag, res_type               *
 *                                                                            *
 * Parameters: request - structure that contains item key and parameters      *
 *              request->key - item key without parameters                    *
 *              request->nparam - number of parameters                        *
 *              request->timeout - processing should not take longer than     *
 *                                 this number of seconds                     *
 *              request->params[N-1] - pointers to item key parameters        *
 *                                                                            *
 *             result - structure that will contain result                    *
 *             get_mib - target mib                                           *
 *             getrem_data - structure that contains parameter                *
 *              getrem_data->dump_flag - DUMP_NORMAL(space) or                *
 *                                       DUMP_WITH_MAC(coron)                 *
 *              getrem_data->res_type - RES_TYPE_STR or RES_TYPE_BSTR or      *
 *                                      RES_TYPE_INT                          *
 *                                                                            *
 * Return value: SYSINFO_RET_FAIL - function failed, item will be marked      *
 *                                  as not supported by zabbix                *
 *               SYSINFO_RET_OK - success                                     *
 *                                                                            *
 * Comment: get_rparam(request, N-1) can be used to get a pointer to the Nth  *
 *          parameter starting from 0 (first parameter). Make sure it exists  *
 *          by checking value of request->nparam.                             *
 *                                                                            *
 ******************************************************************************/
int lldpmod_getrem_common(AGENT_REQUEST *request, AGENT_RESULT *result, const char *get_mib, lldpmod_getrem_data_t *getrem_data)
{

	netsnmp_session		*ss;
	char			error[MAX_STRING_LEN];

	int			ret = SYSINFO_RET_OK;
	int			err = SUCCEED;

	char	*param_ip, *param_community, *param_port_num, *param_rem_idx, *param_def_val;


	if (request->nparam < 4 || request->nparam > 5)
	{
		SET_MSG_RESULT(result, strdup("Invalid number of parameters"));
		return SYSINFO_RET_FAIL;
	}
	param_ip	= get_rparam(request, 0);
	param_community	= get_rparam(request, 1);
	param_port_num	= get_rparam(request, 2);
	param_rem_idx	= get_rparam(request, 3);
	param_def_val	= get_rparam(request, 4);
	

	if (NULL == param_ip || '\0' == *param_ip)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 1st parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == param_community || '\0' == *param_community)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 2nd parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == param_port_num || '\0' == *param_port_num)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 3rd parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == param_rem_idx || '\0' == *param_rem_idx)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 4th parameter"));
		return SYSINFO_RET_FAIL;
	}


	if (NULL != param_def_val &&
		(	(strlen(param_def_val) == 1) ||
			(strlen(param_def_val) > 1 && param_def_val[0] != '{' && param_def_val[1] != '$')))
	{
		/* when default value was specified, return exact same string */
		/* don't perform snmp-walk */
		SET_TEXT_RESULT(result, strdup(param_def_val));
		goto out;
	}

	if (0 == atol(param_rem_idx))
	{
		/* when remote index is 0, remote device is not connected */
		/* don't perform snmp-walk */
		if (RES_TYPE_STR == getrem_data->res_type)
			SET_TEXT_RESULT(result, strdup(NC_STRVAL));
		else	/* RES_TYPE_STR or RES_TYPE_BSTR */
			SET_TEXT_RESULT(result, strdup(NC_INTVAL));
		goto out;
	}

	if (NULL == (ss = lldpmod_snmp_open_session(param_ip, param_community, error, sizeof(error))))
	{
		err = NETWORK_ERROR;
		goto exit;
	}

	getrem_data->index = (oid)strtoul(param_port_num, NULL, 0);
	getrem_data->rem_index = (oid)strtoul(param_rem_idx, NULL, 0);
	getrem_data->result = result;

	/* call net-snmp library */
	err = lldpmod_snmpwalk(ss, get_mib, error, sizeof(error), lldpmod_snmpwalk_getrem_cb, getrem_data);

	lldpmod_snmp_close_session(ss);

exit:
	if (SUCCEED_WITH_VALUE != err)
	{
		if (SUCCEED == err)
		{
			/* specified remote index was not found  */
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Object not found. [ %s, %lu, %lu ]",
					get_mib, (unsigned long)getrem_data->index, (unsigned long)getrem_data->rem_index));
		}
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, error));
		}

		char **msg;
		msg = GET_MSG_RESULT(result);
		zabbix_log(LOG_LEVEL_DEBUG, "%s: getting SNMP values failed: %s", MODULE_NAME, NULL != msg && NULL != *msg ? *msg : "(null)");

		ret = SYSINFO_RET_FAIL;
	}

out:
	return ret;

}

/* get rem_portid_subtype */
int	lldpmod_getrem_port_type(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	lldpmod_getrem_data_t	getrem_data;

	getrem_data.dump_flag = DUMP_NORMAL;
	getrem_data.res_type = RES_TYPE_INT;

	return lldpmod_getrem_common(request, result, REM_TYPE_OID, &getrem_data);
}

/* get rem_portid */
int	lldpmod_getrem_port_id(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	lldpmod_getrem_data_t	getrem_data;

	getrem_data.dump_flag = DUMP_WITH_MAC;
	getrem_data.res_type = RES_TYPE_STR;

	return lldpmod_getrem_common(request, result, REM_PORTID_OID, &getrem_data);
}

/* get rem_port_description */
int	lldpmod_getrem_port_desc(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	lldpmod_getrem_data_t	getrem_data;

	getrem_data.dump_flag = DUMP_NORMAL;
	getrem_data.res_type = RES_TYPE_STR;

	return lldpmod_getrem_common(request, result, REM_PORTDESC_OID, &getrem_data);
}

/* get rem_sysname */
int	lldpmod_getrem_sysname(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	lldpmod_getrem_data_t	getrem_data;

	getrem_data.dump_flag = DUMP_NORMAL;
	getrem_data.res_type = RES_TYPE_STR;

	return lldpmod_getrem_common(request, result, REM_SYSNAME_OID, &getrem_data);
}

/* get rem_sysdescription */
int	lldpmod_getrem_sys_desc(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	lldpmod_getrem_data_t	getrem_data;

	getrem_data.dump_flag = DUMP_NORMAL;
	getrem_data.res_type = RES_TYPE_STR;

	return lldpmod_getrem_common(request, result, REM_SYSDESC_OID, &getrem_data);
}

/* get rem_chassisid_subtype */
int	lldpmod_getrem_chassis_type(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	lldpmod_getrem_data_t	getrem_data;

	getrem_data.dump_flag = DUMP_NORMAL;
	getrem_data.res_type = RES_TYPE_INT;

	return lldpmod_getrem_common(request, result, REM_CHASSIS_TYPE_OID, &getrem_data);
}

/* get rem_chassisid */
int	lldpmod_getrem_chassis_id(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	lldpmod_getrem_data_t	getrem_data;

	getrem_data.dump_flag = DUMP_WITH_MAC;
	getrem_data.res_type = RES_TYPE_STR;

	return lldpmod_getrem_common(request, result, REM_CHASSIS_ID_OID, &getrem_data);
}

/******************************************************************************
 *                                                                            *
 * Function: lldpmod_getrem_oid                                               *
 *                                                                            *
 * Purpose: a main entry point for get a lldp remote oid                      *
 *                                                                            *
 * Parameters: request - structure that contains item key and parameters      *
 *              request->key - item key without parameters                    *
 *              request->nparam - number of parameters                        *
 *              request->timeout - processing should not take longer than     *
 *                                 this number of seconds                     *
 *              request->params[N-1] - pointers to item key parameters        *
 *                                                                            *
 *             result - structure that will contain result                    *
 *                                                                            *
 * Return value: SYSINFO_RET_FAIL - function failed, item will be marked      *
 *                                  as not supported by zabbix                *
 *               SYSINFO_RET_OK - success                                     *
 *                                                                            *
 * Comment: get_rparam(request, N-1) can be used to get a pointer to the Nth  *
 *          parameter starting from 0 (first parameter). Make sure it exists  *
 *          by checking value of request->nparam.                             *
 *                                                                            *
 ******************************************************************************/
int	lldpmod_getrem_oid(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	netsnmp_session		*ss;
	char			error[MAX_STRING_LEN];

	int			ret = SYSINFO_RET_OK;
	int			err = SUCCEED;

	char	*param_ip, *param_community, *param_oid, *param_port_num, *param_rem_idx;

	lldpmod_getrem_data_t	getrem_data;

	oid	parsed_param_oid[MAX_OID_LEN];
	size_t	parsed_param_oid_len = MAX_OID_LEN;
	oid	parsed_chk_oid[MAX_OID_LEN];
	size_t	parsed_chk_oid_len = MAX_OID_LEN;


	if (request->nparam != 5)
	{
		SET_MSG_RESULT(result, strdup("Invalid number of parameters"));
		return SYSINFO_RET_FAIL;
	}
	param_ip	= get_rparam(request, 0);
	param_community	= get_rparam(request, 1);
	param_oid	= get_rparam(request, 2);
	param_port_num	= get_rparam(request, 3);
	param_rem_idx	= get_rparam(request, 4);

	if (NULL == param_ip || '\0' == *param_ip)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 1st parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == param_community || '\0' == *param_community)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 2nd parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == param_oid || '\0' == *param_oid)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 3rd parameter"));
		return SYSINFO_RET_FAIL;
	}

	/* parse oid */
	if (NULL == snmp_parse_oid(param_oid, parsed_param_oid, &parsed_param_oid_len))
	{
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "snmp_parse_oid(): cannot parse OID \"%s\".",
				param_oid));
		return SYSINFO_RET_FAIL;
	}


#define CHK_OID(__oid, __res_type, __dump_flag)									\
	if (NULL != snmp_parse_oid(__oid, parsed_chk_oid, &parsed_chk_oid_len))					\
	{													\
		if (parsed_chk_oid_len == parsed_param_oid_len &&						\
			0 == memcmp(parsed_chk_oid, parsed_param_oid, parsed_param_oid_len * sizeof(oid)))	\
		{												\
			getrem_data.dump_flag = __dump_flag;							\
			getrem_data.res_type = __res_type;							\
			goto oidchk_end;									\
		}												\
	}													\
	else													\
	{													\
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "snmp_parse_oid(): cannot parse OID \"%s\".",		\
				__oid));									\
		return SYSINFO_RET_FAIL;									\
	}

	/* LLDP-MIB::lldpRemChassisIdSubtype */
	CHK_OID(REM_CHASSIS_TYPE_OID, RES_TYPE_INT, DUMP_NORMAL);

	/* LLDP-MIB::lldpRemChassisId */
	CHK_OID(REM_CHASSIS_ID_OID, RES_TYPE_STR, DUMP_WITH_MAC);

	/* LLDP-MIB::lldpRemSysCapSupported */
	CHK_OID(REM_CAP_SUPPORTED_OID, RES_TYPE_BSTR, DUMP_NORMAL);

	/* LLDP-MIB::lldpRemSysCapEnabled */
	CHK_OID(REM_CAP_ENABLED_OID, RES_TYPE_BSTR, DUMP_NORMAL);

	/* LLDP-MIB::lldpRemPortIdSubtype */
	CHK_OID(REM_TYPE_OID, RES_TYPE_INT, DUMP_NORMAL);

	/* LLDP-MIB::lldpRemPortId */
	CHK_OID(REM_PORTID_OID, RES_TYPE_STR, DUMP_WITH_MAC);

	/* LLDP-MIB::lldpRemPortDesc */
	CHK_OID(REM_PORTDESC_OID, RES_TYPE_STR, DUMP_NORMAL);

	/* LLDP-MIB::lldpRemSysName */
	CHK_OID(REM_SYSNAME_OID, RES_TYPE_STR, DUMP_NORMAL);

	/* LLDP-MIB::lldpRemSysDesc */
	CHK_OID(REM_SYSDESC_OID, RES_TYPE_STR, DUMP_NORMAL);

	/* LLDP-MIB::lldpRemChassisIdSubtype */
	CHK_OID(REM_CHASSIS_TYPE_OID, RES_TYPE_INT, DUMP_NORMAL);

	/* LLDP-MIB::lldpRemChassisId */
	CHK_OID(REM_CHASSIS_ID_OID, RES_TYPE_STR, DUMP_WITH_MAC);

oidchk_end:

#undef CHK_OID


	if (NULL == param_port_num || '\0' == *param_port_num)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 4th parameter"));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == param_rem_idx || '\0' == *param_rem_idx)
	{
		SET_MSG_RESULT(result, strdup("Invalid format of 5th parameter"));
		return SYSINFO_RET_FAIL;
	}


	if (0 == atol(param_rem_idx))
	{
		/* when remote index is 0, remote device is not connected */
		/* don't perform snmp-walk */
		if (RES_TYPE_INT == getrem_data.res_type)
			SET_TEXT_RESULT(result, strdup(NC_INTVAL));
		else	/* RES_TYPE_STR or RES_TYPE_BSTR */
			SET_TEXT_RESULT(result, strdup(NC_STRVAL));
		goto out;
	}

	if (NULL == (ss = lldpmod_snmp_open_session(param_ip, param_community, error, sizeof(error))))
	{
		err = NETWORK_ERROR;
		goto exit;
	}

	getrem_data.index = (oid)strtoul(param_port_num, NULL, 0);
	getrem_data.rem_index = (oid)strtoul(param_rem_idx, NULL, 0);
	getrem_data.result = result;

	/* call net-snmp library */
	err = lldpmod_snmpwalk(ss, param_oid, error, sizeof(error), lldpmod_snmpwalk_getrem_cb, &getrem_data);

	lldpmod_snmp_close_session(ss);

exit:
	if (SUCCEED_WITH_VALUE != err)
	{
		if (SUCCEED == err)
		{
			/* specified remote index was not found  */
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Object not found. [ %s, %lu, %lu ]",
					param_oid, (unsigned long)getrem_data.index, (unsigned long)getrem_data.rem_index));
		}
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, error));
		}

		char **msg;
		msg = GET_MSG_RESULT(result);
		zabbix_log(LOG_LEVEL_DEBUG, "%s: getting SNMP values failed: %s", MODULE_NAME, NULL != msg && NULL != *msg ? *msg : "(null)");

		ret = SYSINFO_RET_FAIL;
	}

out:
	return ret;

}
