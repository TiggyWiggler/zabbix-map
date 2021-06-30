/**********************************************************************
* This file is part of zabbix-map.
* 
* zabbix-map is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* zabbix-map is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with zabbix-map.  If not, see <https://www.gnu.org/licenses/>.
**********************************************************************/

#ifndef ZDATA_HEADER
#define ZDATA_HEADER
/**
 * Zabbix Data.
 * 
 * Common Zabbix data elements that are shared between code modules
 * */

/**
 * \file zdata.h
 * */

#ifndef HOST_INTERFACE_MAX
#define HOST_INTERFACE_MAX 10
#endif

/**
 * Enumerator for the types of device chassis identifiers. 
 * */
enum chassisIdType
{
    chassisIdType_ChassisComponent = 1,
    chassisIdType_InterfaceAlias = 2,
    chassisIdType_PortComponent = 3,
    chassisIdType_MacAddress = 4,
    chassisIdType_NetworkAddress = 5,
    chassisIdType_InterfaceName = 6,
    chassisIdType_Local = 7 // Locally defined on switch. Can be anything.
};

/**
 * Device ports (e.g. switch ports) can be defined numous ways. This enumerator tells us what the portId is pointing to.
 * */
enum portIdType
{
    portIdType_InterfaceAlias = 1,
    portIdType_PortComponent = 2,
    portIdType_MacAddress = 3,
    portIdType_NetworkAddress = 4,
    portIdType_InterfaceName = 5,
    portIdType_agentCircuitId = 6,
    portIdType_Local = 7 // Locally defined on switch. Can be anything.
};

/** Zabbix topology map */
struct zMap
{
    char *name;
    int sysId;
    double width;
    double height;
};

/** A collection of zabbix maps */
struct zMapCol
{
    int count;
    struct zMap *zMaps;
};


/**
 * A remote device detected from the local host.
 * The local host could be an Ethernet switch, so the linked device will be the result of an LLDP query against the local Ethernet switch
 * */
struct linkedDevice
{
    int msap;                            /**< Device Media Service Access Point for the local connection of the remote port. */
    char locPortName[256];               /**< name of the local port that reported this linked device. */
    char remChassisId[256];              /**< Chassis ID for the remotely connection device */
    enum chassisIdType remChassisIdType; /**< The type of data held in remChassisId */
    char remHostName[256];               /**< name of the remote host */
    char remHostDesc[256];               /**< Description of the remote host */
    char remPortId[256];                 /**< Address or identifier of the port on the remote connected Host that is connected to our local interface.*/
    enum portIdType remPortIdType;       /**< The type of data help in remPortID */
    char remPortDesc[256];               /**< Description of the remote port. Useful for labels and diagnostics */
};

struct host
{
    char name[129];
    int id;                                  /**< Id of this data item (can exist only in this connector if creating a new host to be copied to Zabbix) */
    int zabbixId;                            /**< Id of the device in Zabbix (host Id). A host created in this C code but not existing in Zabbix (E.g. Pseudo Host) would have an ID but no Zabbix ID. */
    char interfaces[HOST_INTERFACE_MAX][17]; /**< Array of IP addresses for the host */
    int interfaceCount;
    char chassisId[256];                /**< Chassis ID for the device. E.g. MAC address for the switch or computer */
    enum chassisIdType chassisIdType;   /**< What type of chassis ID is known for this host (see chassisId) */
    struct linkedDevice *linkedDevices; /**< remote devices found typically via LLDP. */
    int devicesCount;
    double xPos; /**< x axis position when placing on the map canvas */
    double yPos; /**< y axis position when placing on the map canvas */
};

/**
 * Collection of hosts
 * */
struct hostCol
{
    int count;
    struct host *hosts;
};

/**
 * One side of a join between two hosts.
 * This approach used instead of directly linking hosts as this allows us to create joins at varying granularity of data based on what information
 * is available to us at any one point of the program. Prevents us having to know everything at the point we make the actual join. */
struct linkElement
{
    int hostId;          /**< host ID. NOT the zabbixId, which could be separate. */
    char chassisId[256]; /**< reference to the chassis of the host. */
    char portRef[256];   /**< reference to the port on the host. */
};

/**
 * a link between two hosts. 
 * This approach used instead of directly linking hosts as this allows us to create joins at varying granularity of data based on what information
 * is available to us at any one point of the program. Prevents us having to know everything at the point we make the actual join. */
struct link
{
    struct linkElement a; /**< one side of the host link */
    struct linkElement b; /**< the other side of the host link */
};

/**
 * Colleciton of host links
 * */
struct linkCol
{
    int count;
    struct link *links;
};

struct hostLink
{
    struct hostCol hosts;
    struct linkCol links;
};
#endif