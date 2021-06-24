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