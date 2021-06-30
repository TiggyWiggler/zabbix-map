# zabbix-map

Zabbix Map is a network topology mapper for Zabbix which uses Zabbix data directly.

Data within Zabbix is used as the source for the map structure and the result are output to a Zabbix Map.

## Prerequisites
 - curl
 - json-c
 - L2 Discovery Module for LLDP [Found here](https://share.zabbix.com/network_devices/l2-discovery-module-for-lldp)

The L2 Discovery Module for LLDP needs to be modified slightly so that it retrieves displays MSAP information. I feel that the modification is too minor to justify a fork of the base project so the instructions for modifying are below. 

- [ ] I will also include a compiled version for 64 bit Linux systems in this project.

Remember to respect the original copyright for the The L2 Discovery Module for LLDP project in addition to this Zabbix Map project.

## L2 Discovery Module for LLDP Modifications
The  L2 Discovery Module for LLDP project requires the following modification before it can be used with the Zabbix Map.

Within the section ‘Module Keys’ starting at line 122, add the following two keys:
`#define MODULE_KEY_REM_CHASSIS_TYPE	"lldp.rem.chassis.type"`
`#define MODULE_KEY_REM_CHASSIS_ID	"lldp.rem.chassis.id"`

within section ‘Function Prototype’ starting at line 170, add the following prototypes:
```
int	lldpmod_getrem_chassis_type(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_chassis_id(AGENT_REQUEST *request, AGENT_RESULT *result);
```

within the ZBX_METRIC keys[] array starting at line 181, add the following keys:
```
{MODULE_KEY_REM_CHASSIS_TYPE,	CF_HAVEPARAMS,	lldpmod_getrem_chassis_type,	"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
{MODULE_KEY_REM_CHASSIS_ID,	CF_HAVEPARAMS,	lldpmod_getrem_chassis_id,		"{HOST.IP},{$SNMP_COMMUNITY},{#PORT_NUM},{#REM_IDX},{$FIXED_VAL}"},
```

At the end of the get remote objects section which ends at line 1663, add the following two functions:

```
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
```

Finally, at the end of the OID CHK section which ends at line 1789, add these two function calls:

```
/* LLDP-MIB::lldpRemChassisIdSubtype */
CHK_OID(REM_CHASSIS_TYPE_OID, RES_TYPE_INT, DUMP_NORMAL);

/* LLDP-MIB::lldpRemChassisId */
CHK_OID(REM_CHASSIS_ID_OID, RES_TYPE_STR, DUMP_WITH_MAC);
```
