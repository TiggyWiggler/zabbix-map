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
```
#define MODULE_KEY_REM_CHASSIS_TYPE	"lldp.rem.chassis.type"
#define MODULE_KEY_REM_CHASSIS_ID	"lldp.rem.chassis.id"
```

within section ‘Function Prototype’ starting at line 170, add the following prototypes:
```
int	lldpmod_getrem_chassis_type(AGENT_REQUEST *request, AGENT_RESULT *result);
int	lldpmod_getrem_chassis_id(AGENT_REQUEST *request, AGENT_RESULT *result);
```

within the ZBX_METRIC keys[] array starting at line 181, add the following keys. Note that there is a comma at the end of the last key because you have to remember to keep the NULL key at the end (so add this lot before the existing NULL).:
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

## Usage
usage: zabbix-map [OPTION]...
option should be followed by option value if applicable. Use double quotes if value includes spaces.
example: zabbix-map -map "test map" -ip "192.168.4.0\24, 192.168.4.101" -u admin -p password1

 -map 			name of the map in Zabbix. Will overwrite if existing.
 -ip			IP address(es) of hosts to be included in the map.
			Can take multiple address or ranges. Must be comma seperated.
			Can have single addresses (E.g. 192.168.4.1)
			or hyphenated ranges (E.g. 192.168.4.0-.128 or 192.168.4.0-5.0)
			or CIDR ranges (E.g. 192.168.4.0/24)
 -src			Data source for the map {api,file}.
			if taken from file then cache file is source.
			if api then data comes from live data and cache file is used to store results.
 -cache			name of the cache file.
 -orderby		One or more order by values. Used to order host nodes.
			descendants: order by number of descendants at all levels below subject node.
			children: order by number of children at one level below subject node.
			generations: order by number levels (generations) below subject node.
			For any order value suffix 'Desc' to reverse order. 
			Chain multiple orders with spaces. 
			example: -orderby "descendants childrenDesc"
 -padding		padding of each tree. Applied to sides counter clockwise from north position.
			Four values required if given. example: -padding "100.0 50.0 100.0 50.0"
 -nodespace		Spacing between nodes within the map. Assumes nodes have zero size themselves.
			Two values required if given. X axis first, Y axis second.
			example: -nodespace "100.0, 50.0"
 -u			Username to be used for the connection to Zabbix server.
 -pw			Password to be used for the given Zabbix server user.
craig@ubuntu:~/Documents/Projects/c/zabbixMapGit/zabbix-map$ 
