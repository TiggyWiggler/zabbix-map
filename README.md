# zabbix-map

Zabbix Map is a network topology mapper for Zabbix which uses Zabbix data directly.

Data within Zabbix is used as the source for the map structure and the result are output to a Zabbix Map.

Note: This is my first ever C project. I am NOT an expert and I make no claim that the code contained in this project is optimised or expertly written. Assume this code was built by an idiot barely capable of bashing two rocks together and we should get on fine.

## Prerequisites
 - curl
 - json-c
 - L2 Discovery Module for LLDP [Found here](https://share.zabbix.com/network_devices/l2-discovery-module-for-lldp)

The L2 Discovery Module for LLDP needs to be modified slightly so that it retrieves displays MSAP information. I feel that the modification is too minor to justify a fork of the base project so the instructions for modifying are below. 

- [ ] I will also include a compiled version for 64 bit Linux systems in this project.

Remember to respect the original copyright for the The L2 Discovery Module for LLDP project in addition to this Zabbix Map project.

## L2 Discovery Module for LLDP Modifications
The  L2 Discovery Module for LLDP project requires the following modification before it can be used with the Zabbix Map.

### lldp_get.c

I have included a modified copy of the lldp_get.c source code within the source code of this here zabbix-map project. You will need to get a copy of the L2 Discovery Module for LLDP [Found here](https://share.zabbix.com/network_devices/l2-discovery-module-for-lldp) if you wish to copy in the pre-modified code that I have included.

See ~/L2DM-LLDP/source/modules/lldp_get.c

In case you do not want to use this approach (or cannot) and instead wish to directly modify the lldp_get.c code directly from the  L2 Discovery Module for LLDP project, the required modifications and changes are shown herein.

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

### LLDP Templates
Exported copies of The templates listed below are included in the source files of this project. It should not be necessary to manually modify the templates as you can use the existing exports provided, but directions on how to modify templates as required is included for completeness and in case direct import is not an option for you or your organisation.

See ~/L2DM-LLDP/data/templates/zbx_export_templates.xml

Both templates “LLDP - Local Common” and “Template LLDP – General” need to be applied to any host that you wish to map as all of the data pulled by these templates is used by the mapping application to ascertain the relationship between the hosts. Personally I have a discovery action that adds these two templates to any host that is discovered.

“Template LLDP – IndexNum” is provided in the “L2 Discovery Module for LLDP” project but is ignored for our benefit.

#### Template LLDP – General
within Template LLDP – General there are two discovery rules:
1. 	if_discovery
2.	lldp_discovery_remote

within lldp_discovery_remote there are seven item prototypes. In each of the seven item prototypes update the name to include “[MSAP {#REM_IDX}]”. For example:
[Port - {#PORT_NAME}] - [MSAP {#REM_IDX}] - [ Connect to ] Chassis Info

#### Template LLDP – Local Common
This is a new template not already existing within the “L2 Discovery Module for LLDP” project. It has two items in it that are configured as follows:
First item.
Name: “chassis Id”
Key:	“SNMP-Chassis-Id”
Type:	“SNMP agent”
SNMP OID:	“LLDP-MIB::lldpLocChassisId.0”
Type:	“Text”
Applications: 	“LLDP-Local-Common” <- Create this application if not already existing.

Second item:
Name:	“chassis Id type”
Key:	“SNMP-Chassis-Id-Type”
Type:	“SNMP agent”
SNMP OID:	“LLDP-MIB::lldpLocChassisIdSubtype.0”
Type:	“Numeric (unsigned)”
Applications:	“LLDP-Local-Common” <- Create this application if not already existing.


## Building
Update the references inside the makefile to your local copy of curl and json-c and then `make all` from within the root of the project. 

## Usage

usage: zabbix-map [OPTION]…

option should be followed by option value if applicable. Use double quotes if value includes spaces.

example: `zabbix-map -map "test map" -ip "192.168.4.0\24, 192.168.4.101" -u admin -p password1`

<table>
	<thead>
		<tr>
			<th>Option</th>
			<th>Description</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>-map</td>
			<td>name of the map in Zabbix. Will overwrite if existing.</td>
		</tr>
		<tr>	
			<td>-ip</td>
			<td>Can take multiple address or ranges. Must be comma seperated.<br/>
			Can have single addresses (E.g. 192.168.4.1)<br/>
			or hyphenated ranges (E.g. 192.168.4.0-.128 or 192.168.4.0-5.0)<br/>
			or CIDR ranges (E.g. 192.168.4.0/24)</td>
		</tr>
		<tr>	
			<td>-src</td>
			<td>Data source for the map {api,file}.</br>
			if taken from file then cache file is source.<br/>
			if api then data comes from live data and cache file is used to store results.</td>
		</tr>
		<tr>	
			<td>-cache</td>
			<td>name of the cache file.</td>
		</tr>
		<tr>	
			<td>-orderby</td>
			<td>One or more order by values. Used to order host nodes.<br/>
			descendants: order by number of descendants at all levels below subject node.<br/>
			children: order by number of children at one level below subject node.<br/>
			generations: order by number levels (generations) below subject node.<br/>
			For any order value suffix 'Desc' to reverse order. <br/>
			Chain multiple orders with spaces. <br/>
			example: <code>-orderby "descendants childrenDesc"</code></td>
		</tr>
		<tr>	
			<td>-padding</td>
			<td>padding of each tree. Applied to sides counter clockwise from north position. Four values required if given.<br/> 
example: <code>-padding "100.0 50.0 100.0 50.0"</code></td>
		</tr>
		<tr>	
			<td>-nodespace</td>
			<td>Spacing between nodes within the map. Assumes nodes have zero size themselves. Two values required if given. X axis first, Y axis second.<br/>example: <code>-nodespace "100.0, 50.0"</code></td>
		</tr>
		<tr>	
			<td>-u</td>
			<td>Username to be used for the connection to Zabbix server. Plaintext.</td>
		</tr>
		<tr>	
			<td>-pw</td>
			<td>Password to be used for the given Zabbix server user. Plaintext.</td>
		</tr>
	</tbody>
</table>


