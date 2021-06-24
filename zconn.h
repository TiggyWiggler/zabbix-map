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
int createMap(struct hostLink *hl, char *name, double w, double h);

#endif


