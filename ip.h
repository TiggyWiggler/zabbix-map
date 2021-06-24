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