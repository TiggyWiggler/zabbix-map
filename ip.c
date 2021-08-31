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

#include "ip.h"
#include "strcommon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct ipRanges *parseIpRanges(char *input);
char *ui2ip(unsigned int ipAsUInt);

unsigned int ip2ui(char *ip)
{
    /* An IP consists of four ranges. */
    long ipAsUInt = 0;
    /* Deal with first range. */
    char *cPtr = strtok(ip, ".");
    if (cPtr)
        ipAsUInt += atoi(cPtr) * pow(256, 3);

    /* Proceed with the remaining ones. */
    int exponent = 2;
    while (cPtr && exponent >= 0)
    {
        cPtr = strtok(NULL, ".\0");
        if (cPtr)
            ipAsUInt += atoi(cPtr) * pow(256, exponent--);
    }

    return ipAsUInt;
}

char *ui2ip(unsigned int ipAsUInt)
{
    char *ip = malloc(16 * sizeof(char));
    int exponent;
    for (exponent = 3; exponent >= 0; --exponent)
    {
        int r = ipAsUInt / pow(256, exponent);
        char buf[4];
        sprintf(buf, "%d", r);
        strcat(ip, buf);
        strcat(ip, ".");
        ipAsUInt -= r * pow(256, exponent);
    }
    /* Replace last dot with '\0'. */
    ip[strlen(ip) - 1] = 0;
    return ip;
}

unsigned int createBitmask(const char *bitmask)
{
    unsigned int times = (unsigned int)atol(bitmask) , i, bitmaskAsUInt = 0;
    /* Fill in set bits (1) from the right. */
    for (i = 0; i < times; ++i)
    {
        bitmaskAsUInt <<= 1;
        bitmaskAsUInt |= 1;
    }
    /* Shift in unset bits from the right. */
    for (i = 0; i < 32 - times; ++i)
        bitmaskAsUInt <<= 1;
    return bitmaskAsUInt;
}

/**
 * separate an IP string into four separate octets.
 * @param [in] ip   the IP address string. E.g. "192.168.4.1"
 * @param [in, out] octets   memory location for the octets that we will produce.
 * @return          1 if success, zero otherwise.*/
int ip2octets(const char *ip, char octets[4][4])
{

    char *token;
    const char delim[1] = ".";
    char ipTmp[strlen(ip) + 1]; // Temp string required to remove const restriction for strtok.
    strcpy(ipTmp, ip);
    token = strtok(ipTmp, delim);
    int i;
    for (i = 0; i < 4; i++)
    {
        // Get the octets from the IP string
        if (token != NULL)
        {
            if (strlen(token) > 3)
            {
                fprintf(stderr, "IP address octet more than three characters. Not possible to calculate spread range.");
                return 0;
            }
            if (strlen(token) < 1)
            {
                fprintf(stderr, "IP address octet is zero characters characters. Not possible to calculate spread range.");
                return 0;
            }
            strcpy(octets[i], token);
            token = strtok(NULL, delim);
        }
        else
            memset(octets[i], '\0', 4);
    }
    return 1;
}

/**
 * Calculate an IP range from a from address and a to address accepting that the to address could be partial.
 * for example. From 192.168.4.0-.255 would go from 192.168.4.0 to 192.168.4.255.
 * @param [in] from     the start of the IP range
 * @param [in] to       the end of the IP range. Can be partial address. */
struct ipRange spreadToIpRange(const char *from, const char *to)
{
    struct ipRange ret;
    ret.lower = 0;
    ret.upper = 0;

    // Copy from and to so that they remain immutable
    char f[strlen(from)];
    char t[strlen(to)];
    strcpy(f, from);
    strcpy(t, to);

    // As to address may be partial, we shall reverse it and build up the full IP address backwards.
    reverse(f);
    reverse(t);

    char fromTerms[4][4]; // from address as terms
    char toTerms[4][4];   // to address as terms
    char *token;
    const char delim[1] = ".";
    int i = 0;

    // Get the tokens 'octets' of the 'From' address
    ip2octets(f, fromTerms);
    //printf("FROM: [%s].[%s].[%s].[%s]\n", fromTerms[0], fromTerms[1], fromTerms[2], fromTerms[3]);

    // Get the tokens (octets) of the 'To' address.
    ip2octets(t, toTerms);

    for (i = 0; i < 4; i++)
    {
        // with the 'to' address, we copy the terms from the supplied 'to' address until we run out of terms, then we
        // use the terms from the 'from' address to fill in the blanks.
        if (toTerms[i][0] == '\0')
        {
            strcpy(toTerms[i], fromTerms[i]);
        }
    }
    //printf("TO: [%s].[%s].[%s].[%s]\n", toTerms[0], toTerms[1], toTerms[2], toTerms[3]);

    // Build the complete 'to' IP address
    char tNew[17];
    memset(tNew, '\0', 17);
    for (i = 0; i < 4; i++)
    {
        strcat(tNew, toTerms[i]);
        if (i < 3)
            strcat(tNew, ".");
    }
    reverse(tNew);

    strcpy(f, from); // Replace the 'from' string as it will have been affected by the strtok function.
    ret.lower = ip2ui(f);
    ret.upper = ip2ui(tNew);
    return ret;
}

/**
 * Calculate an IP range from a network address and CIDR notation addition
 * @param [in]  net     the IP network e.g. "192.168.0.1"
 * @param [in]  cidr    the CIDR range (number of mask bits)*/
struct ipRange cidrToIpRange(char *net, char *cidr)
{
    struct ipRange ret;
    ret.lower = 0;
    ret.upper = 0;

    int cidrInt = atoi(cidr);
    if (cidrInt < 0 || cidrInt > 32)
    {
        fprintf(stderr, "CIDR out of range");
        return ret;
    }

    unsigned int netint = ip2ui(net);
    unsigned int mask = createBitmask(cidr);

    ret.lower = netint & mask;       // Start of range
    ret.upper = ret.lower | (~mask); // end of range
    return ret;
}

/**
 * Input string containing zero or more IP ranges. parse the string and return all of the ranges in the string. 
 * Example value input: "192.68.4.0/24, 10.17.12.112, 10.17.43.0-.128, 10.17.43.200-10.17.43.220"
 * This example would have four terms*/
struct ipRanges *parseIpRanges(char *input)
{
    int step = 5;     // How many extra IP ranges shall we add to the return parameter with each increase to the storage.
    int spread, cidr; //  spread range or cidr range (if neither then a single address)
    struct ipRanges *ret = malloc(sizeof *ret);
    ret->ranges = malloc(0);
    char term1[18]; // The first part of an IP range. e.g. the first part of "192.68.4.0/24" is "192.68.4.0"
    char term2[18]; // The second part of an IP range. e.g. the second part of "192.68.4.0/24" is "24"
    int term;       // Which term are we trying to parse.
    int i;
    ret->n = 0;
    ret->size = 0;
    char delim[2] = ",";                          // If there are multiple ranges then they will be comma separated.
    char *r_ptr = input;                          // reentrant memory
    char *token = strtok_r(input, delim, &r_ptr); // reentrant version required due to ip2ui behaviour.

    while (token != NULL)
    {

        if (ret->n == ret->size)
        {
            // Increase space in the return variable.
            struct ipRange *retTmp = realloc(ret->ranges, ((ret->size + step) * sizeof ret->ranges));
            if (!retTmp)
            {
                fprintf(stderr, "Out of memory trying to allocate space for ipRanges");
                return ret;
            }
            ret->ranges = retTmp;
            ret->size += step;
        }

        // parse the IP range and identify the type of range
        spread = 0;
        cidr = 0;
        term = 1;
        memset(term1, '\0', 18);
        memset(term2, '\0', 18);
        i = 0;

        while (token[i] != '\0')
        {
            switch (token[i])
            {
            case ' ':
                i++;
                continue;
            case '-':
                spread = 1;
                term = 2;
                break;
            case '/':
                cidr = 1;
                term = 2;
                break;
            default:
                if (term == 1)
                {
                    term1[strlen(term1)] = token[i];
                }
                else
                {
                    term2[strlen(term2)] = token[i];
                }
            }

            i++;
        }

        if (spread)
        {
            ret->ranges[ret->n] = spreadToIpRange(term1, term2);
        }
        else if (cidr)
        {
            ret->ranges[ret->n] = cidrToIpRange(term1, term2);
        }
        else
        {

            ret->ranges[ret->n].lower = ip2ui(term1);
            ret->ranges[ret->n].upper = ret->ranges[ret->n].lower;
        }

        // Get next string
        token = strtok_r(NULL, delim, &r_ptr);
        ret->n++;
    }
    return ret;
}