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

#ifndef IP_HEADER
#define IP_HEADER
/**
 * IP Addresses
 * 
 * Functions for working with IP Addresses
 * */
/**
 * \file ip.h
 * */

struct ipRange
{
    unsigned int upper;    // lower part of the range
    unsigned int lower;    // upper part of the range.
};

struct ipRanges
{
    int n;                  // number of IP ranges.
    int size;               // amount of space allocated to upper and lower.
    struct ipRange *ranges;  // dynamic array of ranges.  
};

/**
 * Parse one or more IP ranges.
 * Can be a simple single address or a range or a comma delimited list.
 * Examples:
 * 192.168.4.0
 * 192.168.4.0-192.168.4.255
 * 192.168.4.0-.255
 * 192.168.4.0/24
 * 10.17.24.5/16
 * 10.115.20.19-.101.15
 * 192.168.4.0-.255, 10.17.24.5/16, 10.115.20.19-.101.15
 * @param [in] input    the list of IP address(es)
 * */
struct ipRanges *parseIpRanges(char *input);

#endif