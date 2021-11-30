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

#include "zconn.h"
#include "curl/curl.h"
#include "json_tokener.h"
#include "json_object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int g_zDebugMode;

/**
 * Memory structure for JSON responses
 * */
struct MemoryStruct
{
    char *memory;
    size_t size;
};

static char endpoint[256] = "http://localhost/api_jsonrpc.php"; // API End point

/**
 * callback method for CURL to capture response from server
 * */
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr)
    {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Set the end point to be used by the connection module.
void setEndpoint(char *ep)
{
    strcpy(endpoint, ep);
}

int instr(char *tofind, char *findin, uint start)
{
    // Find a string in another string and return its position.
    // return -1 if nothing found.
    int i, j; // Loop itterators

    if (start > strlen(findin) - 1)
        return -1;

    for (i = start, j = 0; findin[i] != '\0' && tofind[j] != '\0'; i++)
    {
        if (findin[i] == tofind[j])
            j++;
        else
            j = 0;
    }
    if (j == 0)
        return -1;
    else if (tofind[j] == '\0')
        return i - j;
    return -1; // ToFind is beyond the limit of FindIn.
}

/**
 * Get a response from the zabbix API
 * @param [in]  method      Zabbix API method name such as user.login or host.get
 * @param [in]  params      parameters specific to a given method. For example user.login requires username and password
 * */
json_object *zconnResp(char *method, json_object *params)
{
    if (g_zDebugMode)
            printf("DEBUG: zconnResp [%s]\n",method);
    /* an authentication string would look like this  "{\"jsonrpc\": \"2.0\",\"method\": \"user.login\",\"params\": {\"user\": \"Admin\",\"password\": \"zabbix\"},\"id\": 1,\"auth\": null}";*/
    static int connId = 0;             // private connection ID. Ensures that API response matches request.*/
    static char authId[40];            // Authorisation ID
    struct json_object *result = NULL; // return object

    if (authId[0] == '\0' && strcmp("user.login", method) != 0)
    {
        fprintf(stderr, "Zabbix Response called for method other than authentication and without valid authentication value set.\n");
        return NULL;
    }

    connId++;

    char connIdString[11];
    sprintf(connIdString, "%i", connId);

    json_object *jobj = json_object_new_object();
    // JSON POST with parameters
    json_object_object_add(jobj, "jsonrpc", json_object_new_string("2.0"));
    json_object_object_add(jobj, "method", json_object_new_string(method));
    if (params)
        json_object_object_add(jobj, "params", params);

    // Authentication ID should be null if attempting to authenticate
    if (strcmp("user.login", method) == 0)
    {
        json_object_object_add(jobj, "auth", json_object_new_null());
    }
    else
    {
        json_object_object_add(jobj, "auth", json_object_new_string(authId));
    }

    json_object_object_add(jobj, "id", json_object_new_string(connIdString));

    //printf("JSON request:\n%s\n", json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY_TAB));

    CURL *curl;
    CURLcode res;

    /* get a curl handle */
    curl = curl_easy_init();
    if (curl)
    {
        // Memory for the response from the API
        struct MemoryStruct chunk;
        chunk.memory = malloc(1); /* will be grown as needed by the realloc above */
        chunk.size = 0;           /* no data at this point */

        const char *data = json_object_to_json_string(jobj);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        /* post binary data */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

        /* pass our list of custom made headers */
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        curl_easy_setopt(curl, CURLOPT_URL, endpoint);

        if (g_zDebugMode)
            printf("DEBUG: zconnResp.curl_easy_perform\n");

        res = curl_easy_perform(curl); /* post away! */

        if (g_zDebugMode)
            printf("DEBUG: zconnResp.curl_easy_perform complete\n");

        json_object_put(jobj); // free the memory so that we can use it again for the response.

        /* Check for errors */
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        }
        else
        {
            if (g_zDebugMode)
            printf("DEBUG: zconnResp.curl_easy_perform CURLE_OK\n");
            /*
            * Now, our chunk.memory points to a memory block that is chunk.size
            * and contains the response from the Zabbix API*/
            int i = 0;

            struct json_tokener *tok = json_tokener_new();
            enum json_tokener_error jerr;
            const char *mystring = chunk.memory;
            int stringlen = chunk.size;
            int val_type;
            char *val_type_str;

            if (g_zDebugMode)
            {
                if (stringlen < 5000)
                {
                    printf("DEBUG: curl response size: %i, curl response: %s\n", stringlen, mystring);
                }
                else
                {
                    printf("DEBUG: curl response size: %i\n", stringlen);
                }
            }
                

            stringlen++; // Include the '\0' if we know we're at the end of input
            do
            {
                jobj = json_tokener_parse_ex(tok, mystring, stringlen);
            } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);
            if (jerr != json_tokener_success)
            {
                fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
                return NULL;
            }

            if (g_zDebugMode)
            printf("DEBUG: parseResult\n");

            // Debug print (uncomment line below to see all API responses).
            //printf("response:\n%s\n", json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY_TAB));

            // We got something from the remote server
            struct json_object *zerr = NULL; // returned error
            if (json_object_object_get_ex(jobj, "error", &zerr))
            {
                /* Error detected */
                // TODO: Need to VALGRIND this bit
                struct json_object *zerrMsg = json_object_object_get(zerr, "message");
                struct json_object *zerrData = json_object_object_get(zerr, "data");
                fprintf(stderr, "Error from Zabbix API. Message: %s, data: %s\n", json_object_get_string(zerrMsg), json_object_get_string(zerrData));
            }
            else
            {
                if (json_object_object_get_ex(jobj, "result", &result))
                {
                    // Success
                    json_object_get(result); // Increment the reference counter so that we maintain the reference after we put the parent
                    if (strcmp("user.login", method) == 0)
                    {
                        // This is the response to the authentication query, so record the authentication response.
                        const char *authStr = json_object_get_string(result);
                        strcpy(authId, authStr);
                    }
                }
            }
            json_object_put(jobj);
            json_tokener_free(tok);
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers); /* free the header list */
        free(chunk.memory);
    }
    return result;
}

int zconnAuth(char *user, char *pw)
{
    /*data = "{\"jsonrpc\": \"2.0\",\"method\": \"user.login\",\"params\": {\"user\": \"Admin\",\"password\": \"zabbix\"},\"id\": 1,\"auth\": null}";*/
    json_object *params = json_object_new_object();

    // Parameter list for the JSON POST
    json_object_object_add(params, "user", json_object_new_string(user));
    json_object_object_add(params, "password", json_object_new_string(pw));

    json_object *result = zconnResp("user.login", params);
    int success = (result) ? 1 : 0;
    json_object_put(result);
    return success;
}

struct zMapCol zconnGetMaps()
{
    // Get all maps from the server
    struct zMapCol ret;
    ret.count = 0;
    ret.zMaps = NULL;

    json_object *params = json_object_new_object();
    json_object_object_add(params, "output", json_object_new_string("extend"));
    json_object *result = zconnResp("map.get", params);
    json_object *jobjTmp;
    if (result)
    {

        int count = json_object_array_length(result); // number of maps returned from API
        int i;                                        // loop itterator
        int strlen;                                   // string length
        json_object *jMap;

        ret.zMaps = malloc(count * sizeof(struct zMap));
        if (!ret.zMaps)
        {
            // Return object with zero count.
            json_object_put(result);
            return ret;
        }
        ret.count = count;

        for (i = 0; i < count; i++)
        {
            jMap = json_object_array_get_idx(result, i);

            json_object_object_get_ex(jMap, "sysmapid", &jobjTmp);
            ret.zMaps[i].sysId = json_object_get_int(jobjTmp);

            // copy map name to new memory as the json_object_put will release the memory.
            json_object_object_get_ex(jMap, "name", &jobjTmp);
            strlen = json_object_get_string_len(jobjTmp);
            ret.zMaps[i].name = malloc(strlen + 1);
            if (ret.zMaps[i].name)
            {
                if (strlen > 0)
                {
                    strcpy(ret.zMaps[i].name, json_object_get_string(jobjTmp));
                }
                else
                {
                    ret.zMaps[i].name = '\0';
                }
            }
            else
            {
                json_object_put(result);
                fprintf(stderr, "Out of memory attempting to create extra space for maps returned from Zabbix.");
                return ret;
            }
        }

        json_object_put(result);
        return ret;
    }
    else
    {
        return ret;
    }
}

void zconnDeleteMapById(int id)
{
    // Delete map by ID
    json_object *idArr = json_object_new_array_ext(1);
    json_object_array_add(idArr, json_object_new_int(id));
    json_object *response = zconnResp("map.delete", idArr);
    json_object_put(response);
}

void zconnDeleteMapByName(char *name)
{
    if (g_zDebugMode)
            printf("DEBUG: zconnDeleteMapByName [%s]\n",name);
    // Delete map by name
    struct zMapCol maps = zconnGetMaps();
    int i; // loop itterator

    for (i = 0; i < maps.count; i++)
    {
        if (strcmp(maps.zMaps[i].name, name) == 0)
        {
            // match found
            zconnDeleteMapById(maps.zMaps[i].sysId);
            break;
        }
    }

    // Free memory in the maps collection
    for (i = 0; i < maps.count; i++)
        free(maps.zMaps[i].name);
    free(maps.zMaps);
}

struct linkedDevice zconnNewLinkedDevice()
{
    // Blank copy of Linked Device structure
    struct linkedDevice ret;
    ret.msap = 0;
    memset(ret.locPortName, '\0', 256);
    memset(ret.remChassisId, '\0', 256);
    ret.remChassisIdType = 0; // invalid enum
    memset(ret.remHostName, '\0', 256);
    memset(ret.remHostDesc, '\0', 256);
    memset(ret.remPortId, '\0', 256);
    ret.remPortIdType = 0; // invalid enum
    memset(ret.remPortDesc, '\0', 256);
    return ret;
}

struct host zconnNewHost()
{
    // Blank copy of Host structure
    struct host ret;
    ret.id = 0;
    ret.zabbixId = 0;
    int i;
    memset(ret.name, '\0', 129);
    for (i = 0; i < HOST_INTERFACE_MAX; i++)
        memset(ret.interfaces[i], '\0', 17); // initialise the interface entry
    ret.interfaceCount = 0;
    ret.devicesCount = 0;
    ret.linkedDevices = NULL;
    memset(ret.chassisId, '\0', 256);
    ret.chassisIdType = 0;
    strcpy(ret.sysDesc,"");
    return ret;
}

struct hostCol zconnParseHosts(json_object *jhosts)
{
    // Convert a JSON object representation of hosts into a struct hostCol (host collection) representation.
    if (g_zDebugMode)
            printf("DEBUG: zconnParseHosts\n");
    int intTmp;
    int i, j, k; // loop itterator.
    json_object *jhost;
    json_object *jobjTmp;
    struct host hostTmp;

    struct hostCol hosts;
    hosts.count = 0;
    hosts.count = json_object_array_length(jhosts);
    hosts.hosts = malloc(hosts.count * sizeof(struct host));

    json_object *interface;
    json_object *interfaceIp;
    int interfaceCount;
    int itemCount;
    json_object *jitem;     // host item, not just any random item :)
    json_object *jitems;    // Host items collection.
    json_object *jitemName; // Name of a given item
    char itemName[256];
    char strTmp[256];        // temp string
    int strStart, strEnd;    // String start and end
    int msap;                // Device Media Service Access Point for the local connection of the remote port.
    struct linkedDevice *ld; // Linked Device
    int ldFound;             // Linked Device found when searching.

    if (!hosts.hosts)
    {
        fprintf(stderr, "Out of memory attempting to create hosts collection");
        exit(1); // failure
    }

    for (i = 0; i < hosts.count; i++)
    {
        jhost = json_object_array_get_idx(jhosts, i);
        if (jhost)
        {
            hosts.hosts[i] = zconnNewHost();
            if (json_object_object_get_ex(jhost, "hostid", &jobjTmp))
                hosts.hosts[i].id = json_object_get_int(jobjTmp);
            hosts.hosts[i].zabbixId = hosts.hosts[i].id; // Copy the device Id and host Id as they are the same for hosts pulled FROM Zabbix

            if (json_object_object_get_ex(jhost, "host", &jobjTmp))
                strcpy(hosts.hosts[i].name, json_object_get_string(jobjTmp));

            if (json_object_object_get_ex(jhost, "interfaces", &jobjTmp))
            {
                // IP addresses
                interfaceCount = json_object_array_length(jobjTmp);
                if (interfaceCount > HOST_INTERFACE_MAX)
                    fprintf(stderr, "Host %i contains more interfaces than the host struct is capable of accepting. Will assimilate as much data as possible.", hosts.hosts[i].id);

                for (j = 0; j < interfaceCount; j++)
                {
                    interface = json_object_array_get_idx(jobjTmp, j);
                    if (json_object_object_get_ex(interface, "ip", &interfaceIp))
                    {
                        // Copy IP address from JSON object to hosts interface entry.
                        strcpy(hosts.hosts[i].interfaces[hosts.hosts[i].interfaceCount], json_object_get_string(interfaceIp));
                        hosts.hosts[i].interfaceCount++;
                    }
                }
            }

            if (json_object_object_get_ex(jhost, "items", &jitems))
            {
                // host items collection
                itemCount = json_object_array_length(jitems);
                for (j = 0; j < itemCount; j++)
                {
                    jitem = json_object_array_get_idx(jitems, j);

                    if (json_object_object_get_ex(jitem, "name", &jitemName))
                    {
                        if (json_object_get_string_len(jitemName) > 0)
                        {
                            // Attempt to extract MSAP number from the item. If successful then this is a reference to a remote connection.
                            memset(itemName, '\0', 256);
                            strcpy(itemName, json_object_get_string(jitemName));

                            msap = 0;

                            // MSAP number will be inside the string in format "[MSAP 201]" so we want the 201 from it.
                            strStart = instr("[MSAP", itemName, 0);
                            strEnd = instr("]", itemName, strStart);

                            if (strStart > -1 && strEnd >= (strStart + 6))
                            {
                                // MSAP string found. 6 to account for "[MSAP "
                                strStart += 6;
                                memset(strTmp, '\0', 256); // Initialise
                                memcpy(strTmp, &itemName[strStart], (strEnd - strStart));
                                msap = atoi(strTmp);
                            }
                            if (msap > 0)
                            {
                                // MSAP value parsed. Check to see if LinkedDevices already contains a reference to the matching remote device.
                                // This is acheived by searching for a matching MSAP in the existing collection.
                                ldFound = 0;
                                for (k = 0; k < hosts.hosts[i].devicesCount; k++)
                                {
                                    if (hosts.hosts[i].linkedDevices[k].msap == msap)
                                    {
                                        // matching linked device found already recorded against host
                                        ldFound = 1;
                                        ld = &(hosts.hosts[i].linkedDevices[k]);
                                        break;
                                    }
                                }
                                if (ldFound == 0)
                                {
                                    // Linked device was not found so we need to create it.
                                    struct linkedDevice *tmpPtr = realloc(hosts.hosts[i].linkedDevices, (hosts.hosts[i].devicesCount + 1) * sizeof(struct linkedDevice));
                                    if (tmpPtr)
                                    {
                                        hosts.hosts[i].linkedDevices = tmpPtr;
                                        hosts.hosts[i].devicesCount++;
                                        hosts.hosts[i].linkedDevices[hosts.hosts[i].devicesCount - 1] = zconnNewLinkedDevice();
                                    }
                                    else
                                    {
                                        fprintf(stderr, "Out of memory while attempting to expand the linked devices buffer");
                                        free(hosts.hosts);
                                        exit(1); // failure
                                    }
                                    // populate new linked Device - MSAP
                                    ld = &(hosts.hosts[i].linkedDevices[hosts.hosts[i].devicesCount - 1]);
                                    ld->msap = msap;

                                    // populate new linked Device - Local Port Details
                                    strStart = instr("[Port - ", itemName, 0);
                                    strEnd = instr("]", itemName, strStart);

                                    if (strStart > -1 && strEnd >= (strStart + 8))
                                    {
                                        // Port string found. 8 to account for "[Port - "
                                        strStart += 8;
                                        memset(strTmp, '\0', 256); // Initialise
                                        memcpy(strTmp, &itemName[strStart], (strEnd - strStart));
                                        strcpy(ld->locPortName, strTmp);
                                    }
                                }
                                // Search the item name for specific key words that indicate what the field is. If found,
                                // copy the value into the appropriate field in the linked devices struct.
                                if (instr("Chassis Info Type", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        ld->remChassisIdType = json_object_get_int(jobjTmp);
                                }
                                else if (instr("Chassis Info", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                    {
                                        strcpy(ld->remChassisId, json_object_get_string(jobjTmp));
                                    }
                                }
                                else if (instr("Host Desc", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        strcpy(ld->remHostDesc, json_object_get_string(jobjTmp));
                                }
                                else if (instr("Host", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        strcpy(ld->remHostName, json_object_get_string(jobjTmp));
                                }
                                else if (instr("Interface Info Type", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        ld->remPortIdType = json_object_get_int(jobjTmp);
                                }
                                else if (instr("Interface Info", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        strcpy(ld->remPortId, json_object_get_string(jobjTmp));
                                }
                                else if (instr("Interface Desc", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        strcpy(ld->remPortDesc, json_object_get_string(jobjTmp));
                                }
                            }
                            else
                            {
                                // MSAP not found or not valid. Data probably relates to the local host device instead.
                                if (instr("Chassis Id Type", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        hosts.hosts[i].chassisIdType = json_object_get_int(jobjTmp);
                                }
                                else if (instr("Chassis Id", itemName, 0) > -1)
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        strcpy(hosts.hosts[i].chassisId, json_object_get_string(jobjTmp));
                                }
                                else if (instr("System description", itemName, 0) > -1) // Not used by Zabbix Mapper directly but useful to the bitmap renderer if used.
                                {
                                    if (json_object_object_get_ex(jitem, "lastvalue", &jobjTmp))
                                        strcpy(hosts.hosts[i].sysDesc, json_object_get_string(jobjTmp));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return hosts;
}

struct hostCol zconnGetHostsFromFile(char *fileName)
{
    if (g_zDebugMode)
            printf("DEBUG: zconnGetHostsFromFile\n");
    /* declare a file pointer */
    FILE *infile;
    char *buffer;
    long numbytes;
    struct hostCol ret; // Default NULL object
    ret.count = 0;
    ret.hosts = NULL;

    /* open an existing file for reading */
    infile = fopen(fileName, "r");

    /* quit if the file does not exist */
    if (infile == NULL)
        return ret;

    /* Get the number of bytes */
    fseek(infile, 0L, SEEK_END);
    numbytes = ftell(infile);

    /* reset the file position indicator to the beginning of the file */
    fseek(infile, 0L, SEEK_SET);

    /* grab sufficient memory for the buffer to hold the text */
    buffer = (char *)calloc(numbytes, sizeof(char));

    /* memory error */
    if (buffer == NULL)
        return ret;

    /* copy all the text into the buffer */
    fread(buffer, sizeof(char), numbytes, infile);
    fclose(infile);

    struct json_tokener *tok = json_tokener_new();
    enum json_tokener_error jerr;
    json_object *jobj;

    do
    {
        jobj = json_tokener_parse_ex(tok, buffer, numbytes);
    } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);
    if (jerr != json_tokener_success)
    {
        fprintf(stderr, "Error: %s\n", json_tokener_error_desc(jerr));
        return ret;
    }

    json_tokener_free(tok);

    /* free the memory we used for the buffer */
    free(buffer);

    if (jobj)
        ret = zconnParseHosts(jobj);

    json_object_put(jobj);
    return ret;
    //return jobj;
}

struct hostCol zconnGetHostsFromAPI(char *cacheFile)
{
    if (g_zDebugMode)
            printf("DEBUG: zconnGetHostsFromAPI\n");
    struct hostCol ret; // Default NULL object
    ret.count = 0;
    ret.hosts = NULL;

    // Get hosts list from the Zabbix API.
    // Build the parameters list
    json_object *outputParam = json_object_new_array_ext(2);
    json_object *interfacesParam = json_object_new_array_ext(2);
    json_object *itemsParam = json_object_new_array_ext(4);

    json_object_array_add(outputParam, json_object_new_string("hostid"));
    json_object_array_add(outputParam, json_object_new_string("host"));

    json_object_array_add(interfacesParam, json_object_new_string("interfaceid"));
    json_object_array_add(interfacesParam, json_object_new_string("ip"));

    json_object_array_add(itemsParam, json_object_new_string("itemid"));
    json_object_array_add(itemsParam, json_object_new_string("name"));
    json_object_array_add(itemsParam, json_object_new_string("lastvalue"));
    json_object_array_add(itemsParam, json_object_new_string("value_type"));

    // Build the request object
    json_object *params = json_object_new_object();
    json_object_object_add(params, "output", outputParam);
    json_object_object_add(params, "selectInterfaces", interfacesParam);
    json_object_object_add(params, "selectItems", itemsParam);

    json_object *result = zconnResp("host.get", params);
    if (cacheFile)
    {
        FILE *fp;
        fp = fopen(cacheFile, "w");
        if (!fp)
        {
            fprintf(stderr, "Could not open cache file for writing");
        }
        else
        {
            fprintf(fp, "%s", json_object_to_json_string_ext(result, JSON_C_TO_STRING_PLAIN));
            fclose(fp);
        }
    }

    if (result)
        ret = zconnParseHosts(result);

    json_object_put(result);
    return ret;
}

/**
 * Create a map in Zabbix from the supplied host link data.
 * @param [in] hl       Host Link data that defines the map
 * @param [in] name     name of the map 
 * @param [in] w        width of the map
 * @param [in] h        height of the map
 * @param [in] linkLabels   if 1 then links will contain labels
 * @return              SysID - ID of the map that is created*/
int createMap(struct hostLink *hl, char *name, double w, double h, int linkLabels)
{
    if (g_zDebugMode)
            printf("DEBUG: createMap [%s]\n",name);
    int i;
    int intMaxLen = 17;     // Maximum length of a string representation of an integer
    char strTmp[intMaxLen]; // String representation of a number
    if (w < 0.0 || h < 0.0)
    {
        fprintf(stderr, "Attempt to create a map of zero dimensions");
        return 0;
    }
    if (hl->hosts.count < 1)
    {
        fprintf(stderr, "Attempt to create a map with no hosts");
        return 0;
    }

    json_object *params = json_object_new_object();
    json_object_object_add(params, "name", json_object_new_string(name));
    json_object_object_add(params, "width", json_object_new_int(w));
    json_object_object_add(params, "height", json_object_new_int(h));
    json_object_object_add(params, "label_type", json_object_new_int(0));

    // Add the hosts as selements
    json_object *selements = json_object_new_array_ext(hl->hosts.count);
    for (i = 0; i < hl->hosts.count; i++)
    {
        struct host *h = &hl->hosts.hosts[i];
        json_object *selement = json_object_new_object();

        // selement ID
        snprintf(strTmp, intMaxLen, "%i", h->id); // ID of the host, used for the selement ID
        json_object_object_add(selement, "selementid", json_object_new_string(strTmp));

        // host id inside an elements array
        json_object *elements = json_object_new_array_ext(1);
        json_object *hostId = json_object_new_object();
        json_object_object_add(hostId, "hostid", json_object_new_string(strTmp));
        json_object_array_add(elements, hostId);
        json_object_object_add(selement, "elements", elements);

        // element type
        json_object_object_add(selement, "elementtype", json_object_new_int((h->zabbixId == 0) ? 4 : 0));

        // icon to be used for the host.
        snprintf(strTmp, intMaxLen, "%i", 2);
        json_object_object_add(selement, "iconid_off", json_object_new_string(strTmp));

        json_object_object_add(selement, "label", json_object_new_string(h->name));

        // position of the host on the canvas
        json_object_object_add(selement, "x", json_object_new_int(h->xPos));
        json_object_object_add(selement, "y", json_object_new_int(h->yPos));

        json_object_array_add(selements, selement); // Add this object to the selements array.
    }
    json_object_object_add(params, "selements", selements);

    // Add the host links as links
    json_object *links = json_object_new_array_ext(hl->links.count);
    for (i = 0; i < hl->links.count; i++)
    {
        struct link *l = &hl->links.links[i];
        
        // Create the link
        json_object *link = json_object_new_object();
        snprintf(strTmp, intMaxLen, "%i", l->a.hostId);
        json_object_object_add(link, "selementid1", json_object_new_string(strTmp));
        snprintf(strTmp, intMaxLen, "%i", l->b.hostId);
        json_object_object_add(link, "selementid2", json_object_new_string(strTmp));

        // Add the label to the link
        if(linkLabels==1)
        {
            int labelMaxLen = 256;
            int j;
            char label[labelMaxLen + 1];
            memset(label, '\0',257);
            struct host *ha, *hb;   // Hosts a and b

            // Get a pointer to the two hosts referenced in this link
            for (j=0;j<hl->hosts.count;j++)
            if (hl->hosts.hosts[j].id==l->a.hostId)
                {
                    ha = &hl->hosts.hosts[j];
                    break;
                }

            for (j=0;j<hl->hosts.count;j++)
            if (hl->hosts.hosts[j].id==l->b.hostId)
                {
                    hb = &hl->hosts.hosts[j];
                    break;
                }



            /*int i;
            for (i=0;i<hl->hosts.count;i++)
            {
                if(hl->hosts.hosts[i].id == l->a.hostId)
                {
                    // Add the host name from side a
                    strncat(label,hl->hosts.hosts[i].name,labelMaxLen-strlen(label));
                    break;
                }
            }*/

            //strncat(label,l->a.chassisId,labelMaxLen-strlen(label));
            strncat(label," [",labelMaxLen-strlen(label));

            // Place the ports in the correct order in the labels negating the need to put the host names on the links too (takes up too much space).
            if(ha->yPos<hb->yPos)
                strncat(label,l->a.portRef,labelMaxLen-strlen(label));
            else if (ha->yPos>hb->yPos)
                strncat(label,l->b.portRef,labelMaxLen-strlen(label));
            else if (ha->xPos<hb->yPos)
                strncat(label,l->a.portRef,labelMaxLen-strlen(label));
            else 
                strncat(label,l->b.portRef,labelMaxLen-strlen(label));

            strncat(label,"]\n<->\n",labelMaxLen-strlen(label));
            //strncat(label,l->b.chassisId,labelMaxLen-strlen(label));

            /*for (i=0;i<hl->hosts.count;i++)
            {
                if(hl->hosts.hosts[i].id == l->b.hostId)
                {
                    // Add the host name from side b
                    strncat(label,hl->hosts.hosts[i].name,labelMaxLen-strlen(label));
                    break;
                }
            }*/

            strncat(label," [",labelMaxLen-strlen(label));
            // Place the ports in the correct order in the labels negating the need to put the host names on the links too (takes up too much space).
            if(ha->yPos<hb->yPos)
                strncat(label,l->b.portRef,labelMaxLen-strlen(label));
            else if (ha->yPos>hb->yPos)
                strncat(label,l->a.portRef,labelMaxLen-strlen(label));
            else if (ha->xPos<hb->yPos)
                strncat(label,l->b.portRef,labelMaxLen-strlen(label));
            else 
                strncat(label,l->a.portRef,labelMaxLen-strlen(label));

            strncat(label,"]",labelMaxLen-strlen(label));
            json_object_object_add(link, "label", json_object_new_string(label));
        }

        json_object_array_add(links, link); // Add this link to the links array.
    }
    json_object_object_add(params, "links", links);

    json_object *result = zconnResp("map.create", params);
    int mapId = 0;
    if (result)
    {
        json_object *jobjSysMapIds = json_object_object_get(result, "sysmapids");
        json_object *jobjMapId = json_object_array_get_idx(jobjSysMapIds, 0);
        mapId = json_object_get_int(jobjMapId);
    }
    json_object_put(result);
    return mapId;
}

/**
 * Free all memory related to a hosts collection.
 * Does not free the host collection object itself
 * */
void freeHostCol(struct hostCol *hosts)
{
    if (g_zDebugMode)
            printf("DEBUG: freeHostCol\n");
    int i; // loop itterator.
    if (hosts->count > 0)
    {
        for (i = 0; i < hosts->count; i++)
        {
            free(hosts->hosts[i].linkedDevices);
        }
        free(hosts->hosts);
    }
}
