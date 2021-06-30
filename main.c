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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zmap.h"
#include "zconn.h"
#include "Forests.h"
#include "ip.h"

struct methodCol
{
    int n;                // number of sort methods
    int s;                // allocated memory in number of sort methods
    enum sortMethods *sm; // Array of sort methods;
};

void showHelp(void);
struct padding parsePadding(char *s);
struct methodCol parseSorts(char *s);
void parseSpacing(char *s, double spaces[2]);

/*
void test()
{
    int authKey = zconnAuth("Admin", "zabbix");
    printf((authKey == 0) ? "Authentication failed.\n" : "Authentication succeeded\n");

    struct hostLink hl;
    struct padding pads = {.bottom = 50, .left = 50, .right = 50, .top = 50};
    enum sortMethods sorts[] = {descendantsDesc};
    char *mapName = "Craigs map";

    hl.hosts = zconnGetHostsFromAPI("hosts.jarray");
    hl.links.count = 0;

    if (hl.hosts.count > 0)
    {
        hl = *(mapHosts(&hl));
        double nodeXSpace = 100.0;
        double nodeYSpace = 100.0;

        layoutHosts(&hl, nodeXSpace, nodeYSpace, pads, sorts, 1);

        // find the overall size of the map. It will be origined at 0,0 so we just need to max x and y coords.
        int i;
        double xMax = 0.0, yMax = 0.0;
        for (i = 0; i < hl.hosts.count; i++)
        {
            if (hl.hosts.hosts[i].xPos > xMax)
                xMax = hl.hosts.hosts[i].xPos;
            if (hl.hosts.hosts[i].yPos > yMax)
                yMax = hl.hosts.hosts[i].yPos;
        }

        xMax += pads.right;
        yMax += pads.bottom;

        // Delete the map if it currently exists.
        zconnDeleteMapByName(mapName);

        // Write the map into Zabbix
        createMap(&hl, mapName, xMax, yMax);

        printHosts(&hl.hosts);
        printLinks(&hl.links);
    }

    freeHostCol(&hl.hosts);
    free(hl.links.links);
}*/

int main(int argc, char *argv[])
{
    char map[80] = "No name map";
    char ip[80] = "0.0.0.0/0";
    char src[10] = "api";
    char cache[25] = "";
    char sortStr[30] = "1"; //descendantsDesc
    char padStr[30] = "50.0, 50.0, 50.0, 50.0";
    char nodeSpace[20] = "100.0,100.0";
    char user[120] = "Admin";
    char pw[120] = "zabbix";
    char debug[6] = "false";
    char *cptr = NULL;
    int h = 0; // show help.
    int i;

    // Extract the input arguments.
    for (i = 1; i < argc; i++)
    {
        if (i % 2 != 0)
        {
            // Parameter names
            if (strcmp(argv[i], "-map") == 0)
                cptr = &map[0];
            else if (strcmp(argv[i], "-ip") == 0)
                cptr = &ip[0];
            else if (strcmp(argv[i], "-src") == 0)
                cptr = &src[0];
            else if (strcmp(argv[i], "-cache") == 0)
                cptr = &cache[0];
            else if (strcmp(argv[i], "-orderby") == 0)
                cptr = &sortStr[0];
            else if (strcmp(argv[i], "-padding") == 0)
                cptr = &padStr[0];
            else if (strcmp(argv[i], "-nodespace") == 0)
                cptr = &nodeSpace[0];
            else if (strcmp(argv[i], "-u") == 0)
                cptr = &user[0];
            else if (strcmp(argv[i], "-pw") == 0)
                cptr = &pw[0];
            else if (strcmp(argv[i], "-debug") == 0)
                cptr = &debug[0];
            else if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "-?") == 0) || (strcmp(argv[i], "--h") == 0) || (strcmp(argv[i], "--?") == 0) || (strcmp(argv[i], "--help") == 0))
                // help required
                h = 1;
        }
        else
        {
            // Parameter values
            if (cptr)
                strcpy(cptr, argv[i]);
        }
    }

    if (h)
        showHelp();
    else
    {
        int authKey = zconnAuth(user, pw);
        if (authKey == 0)
        {
            fprintf(stderr, "Authentication failed for user %s accessing Zabbix", user);
            return 1;
        }

        double spacing[2];
        parseSpacing(nodeSpace, spacing);
        double nodeXSpace = spacing[0], nodeYSpace = spacing[1]; // Spacing between nodes in a tree (objects in a Zabbix map)
        struct padding pads = parsePadding(padStr);
        struct methodCol sorts = parseSorts(sortStr);
        int sortn = 0; // number of sorts

        struct hostLink hl;
        hl.links.count = 0;

        if (strcmp(src, "api") == 0)
        {
            // Use live Zabbix data.
            hl.hosts = zconnGetHostsFromAPI((strcmp("", cache) == 0) ? NULL : cache);
        }
        else
        {
            // Use cached data
            if (strcmp("", cache) == 0)
            {
                fprintf(stderr, "Attempt to use cached data without cache data store location provided\n");
                return 2;
            }
            // else
            hl.hosts = zconnGetHostsFromFile(cache);
        }

        if (hl.hosts.count > 0)
        {
            layoutHosts(&hl, nodeXSpace, nodeYSpace, pads, sorts.sm, sorts.n);
            // find the overall size of the map. It will be origined at 0,0 so we just need to max x and y coords.
            int i;
            double xMax = 0.0, yMax = 0.0;
            for (i = 0; i < hl.hosts.count; i++)
            {
                if (hl.hosts.hosts[i].xPos > xMax)
                    xMax = hl.hosts.hosts[i].xPos;
                if (hl.hosts.hosts[i].yPos > yMax)
                    yMax = hl.hosts.hosts[i].yPos;
            }

            // Add padding on to the overall size.
            xMax += pads.right;
            yMax += pads.bottom;

            // Delete the map if it currently exists.
            zconnDeleteMapByName(map);

            // Write the map into Zabbix
            createMap(&hl, map, xMax, yMax);
        }

        freeHostCol(&hl.hosts);
        if (hl.links.count > 0)
            free(hl.links.links);

        if (strcmp(debug, "true") == 0)
        {
            printf("Map Name: %s\n", map);
            printf("IP: %s\n", ip);
            printf("Source: %s\n", src);
            printf("Cache: %s\n", cache);
            printf("Order By: %s\n", sortStr);
            printf("Padding: %s\n", padStr);
            printf("Node Spacing: %s\n", nodeSpace);
            printf("Username: %s\n", user);
            printf("Password: %s\n", pw);
        }
    }

    return 0;
}

/**
 * Convert string containing padding information into a padding struct.
 * @param [in] s        string containing padding information
 * */
struct padding parsePadding(char *s)
{
    struct padding p = {.bottom = 0, .left = 0, .right = 0, .top = 0}; // Return object with blank data
    char delim[2] = ",";
    char *token;
    int i = 0;
    token = strtok(s, delim);
    while (token != NULL)
    {
        switch (i)
        {
        case 0:
            p.top = atof(token);
            p.bottom = p.top;
            // fall-through intentional to mimick CSS type behaviour when only one dimension provided.
        case 1:
            p.right = atof(token);
            p.left = p.right;
            break;
        case 2:
            p.bottom = atof(token);
            break;
        case 3:
            p.left = atof(token);
            break;
        }
        i++;
        token = strtok(NULL, delim);
    }
    return p;
}

/**
 * Convert string containing sort information into an array of sort methods
 * @param [in] s        string containing sort methods information.
 * */
struct methodCol parseSorts(char *s)
{
    struct methodCol mc;
    mc.n = 0;
    mc.s = 5;
    mc.sm = malloc(mc.s * sizeof mc.sm); // allocate space to the size parameter.
    char delim[2] = ",";
    char *tok = strtok(s, delim);
    while (tok != NULL)
    {
        if (mc.s == mc.n)
        {
            // need more memory to be assigned.
            mc.s += 5;
            enum sortMethods *tmpPtr = realloc(mc.sm, mc.s * sizeof mc.sm);
            if (tmpPtr)
            {
                fprintf(stderr, "Out of memory attempting to add sort methods to memory");
                return mc;
            }
            mc.sm = tmpPtr;
        }

        if (strcmp(tok, "descendants"))
        {
            mc.sm[mc.n++] = descendants;
        }
        else if (strcmp(tok, "descendantsDesc"))
        {
            mc.sm[mc.n++] = descendantsDesc;
        }
        else if (strcmp(tok, "children"))
        {
            mc.sm[mc.n++] = children;
        }
        else if (strcmp(tok, "childrenDesc"))
        {
            mc.sm[mc.n++] = childrenDesc;
        }
        else if (strcmp(tok, "generations"))
        {
            mc.sm[mc.n++] = generations;
        }
        else if (strcmp(tok, "generationsDesc"))
        {
            mc.sm[mc.n++] = generationsDesc;
        }

        tok = strtok(NULL, delim);
    }
    return mc;
}

/**
 * read in a string containing two floats and update the two element float array.
 * Always return the two element array even if parsing is not possible
 * @param [in]      s   The string containing the two floats
 * @param [in,out]  spaces  The two element array   */
void parseSpacing(char *s, double spaces[2])
{
    char delim[2] = ",";
    char *tok;
    int i = 0;
    tok = strtok(s, delim);
    spaces[0] = 0.0;        // Ensure always return something
    spaces[1] = 0.0;
    while (tok != NULL)
    {
        spaces[i++] = atof(tok);
        if (i == 2)
            break;
        tok = strtok(NULL, delim);
    }
}

void showHelp()
{
    // Show help to the user.
    printf("Zabbix Map generator by Craig Moore 2021.\n");
    printf("usage: zabbix-map [OPTION]...\n");
    printf("option should be followed by option value if applicable. Use double quotes if value includes spaces.\n");
    printf("example: zabbix-map -map \"test map\" -ip \"192.168.4.0\\24, 192.168.4.101\" -u admin -p password1\n\n");
    printf(" -map \t\t\tname of the map in Zabbix. Will overwrite if existing.\n");
    printf(" -ip\t\t\tIP address(es) of hosts to be included in the map.\n");
    printf("\t\t\tCan take multiple address or ranges. Must be comma seperated.\n");
    printf("\t\t\tCan have single addresses (E.g. 192.168.4.1)\n");
    printf("\t\t\tor hyphenated ranges (E.g. 192.168.4.0-.128 or 192.168.4.0-5.0)\n");
    printf("\t\t\tor CIDR ranges (E.g. 192.168.4.0/24)\n");
    printf(" -src\t\t\tData source for the map {api,file}.\n");
    printf("\t\t\tif taken from file then cache file is source.\n");
    printf("\t\t\tif api then data comes from live data and cache file is used to store results.\n");
    printf(" -cache\t\t\tname of the cache file.\n");
    printf(" -orderby\t\tOne or more order by values. Used to order host nodes.\n");
    printf("\t\t\tdescendants: order by number of descendants at all levels below subject node.\n");
    printf("\t\t\tchildren: order by number of children at one level below subject node.\n");
    printf("\t\t\tgenerations: order by number levels (generations) below subject node.\n");
    printf("\t\t\tFor any order value suffix 'Desc' to reverse order. \n");
    printf("\t\t\tChain multiple orders with spaces. \n");
    printf("\t\t\texample: -orderby \"descendants childrenDesc\"\n");
    printf(" -padding\t\tpadding of each tree. Applied to sides counter clockwise from north position.\n");
    printf("\t\t\tFour values required if given. example: -padding \"100.0 50.0 100.0 50.0\"\n");
    printf(" -nodespace\t\tSpacing between nodes within the map. Assumes nodes have zero size themselves.\n");
    printf("\t\t\tTwo values required if given. X axis first, Y axis second.\n");
    printf("\t\t\texample: -nodespace \"100.0, 50.0\"\n");
    printf(" -u\t\t\tUsername to be used for the connection to Zabbix server.\n");
    printf(" -pw\t\t\tPassword to be used for the given Zabbix server user.\n");
}