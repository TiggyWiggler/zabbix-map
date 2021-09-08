/**********************************************************************
 *
 * Copyright (C) 2021 Craig Moore
 * 
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

#ifndef ZCONN_HEADER
#define ZCONN_HEADER
/**
 * Zabbix Connector.
 * 
 * Zabbix Connector communicates with the Zabbix API via JSON RESTFull commands.
 * cURL and JSON-C are both used in this program.
 * */

/**
 * \file zconn.h
 * */

#include "zdata.h"

/**
 * Sets the API end point to be used by all future API calls.
 * @param [in] ep       end point name. e.g. "http://localhost/api_jsonrpc.php" which is also the default value
 * */
void setEndpoint(char *ep);

/**
 * Authenticates a user against given credentials.
 * @param[in]   user    The user name.
 * @param[in]   pw      The password in clear text.
 * @return              0 if fails, 1 if success
 * */
int zconnAuth(char *user ,char *pw);
struct zMapCol zconnGetMaps();

/**
 * Deletes a map according to its name.
 * fails silently if the name is not found.
 * @param [in]  name    name of the map
 * */
void zconnDeleteMapByName(char *name);
struct host zconnNewHost();
void freeHostCol(struct hostCol *hosts);
struct hostCol zconnGetHostsFromFile(char *fileName);
struct hostCol zconnGetHostsFromAPI(char *cacheFile);
int createMap(struct hostLink *hl, char *name, double w, double h, int linkLabels);

#endif


