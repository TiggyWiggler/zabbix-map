# zabbix-map

Zabbix Map is a network topology mapper for Zabbix which uses Zabbix data directly.

Data within Zabbix is used as the source for the map structure and the result are output to a Zabbix Map.

##Prerequisites
 - curl
 - json-c
 - L2 Discovery Module for LLDP [Found here](https://share.zabbix.com/network_devices/l2-discovery-module-for-lldp)

The L2 Discovery Module for LLDP needs to be modified slightly so that it retrieves displays MSAP information. I feel that the modification is too minor to justify a fork of the base project so the instructions for modifying are below. I will also include a compiled version for 64 bit Linux systems in this project.

Remember to respect the original copyright for the The L2 Discovery Module for LLDP project in addition to this Zabbix Map project.

##L2 Discovery Module for LLDP Modifications
The  L2 Discovery Module for LLDP project requires the following modification before it can be used with the Zabbix Map.




