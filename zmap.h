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

#ifndef ZMAP_HEADER
#define ZMAP_HEADER
/**
 * Zabbix Host Map.
 * 
 * Create Zabbix map (topology maps) from zabbix host collections.
 * */

/**
 * \file zmap.h
 * */

#include "zconn.h"
#include "Forests.h"

struct padding
{
    double top;
    double right;
    double bottom;
    double left;
};

void printHosts(struct hostCol *hosts);
void printLinks(struct linkCol *links);
struct hostLink *mapHosts(struct hostLink *hosts);
void layoutHosts(struct hostLink *hostsLinks, double hostXSpace, double hostYSpace, struct padding treePadding, enum sortMethods *sorts, int sortCount);
#endif