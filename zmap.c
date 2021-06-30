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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "zmap.h"


char *stripSeparators(char *input)
{
    // Remove seperators from a string.
    // used for comparing strings.
    int n = strlen(input);
    char *ret = calloc(n + 2, 1);
    char check;
    int i = 0; // loop itterator.
    int o = 0; // forward looking offset;
    while (i + o < n)
    {
        check = input[i + o];
        if (check == ' ' || check == ':' || check == '-')
        {
            o++;
            continue;
        }
        ret[i] = input[i + o];
        i++;
    }
    ret[i + 1] = '\0'; // ensure terminator.
    return ret;
}

struct linkCol findAllLinks(struct hostCol *hosts)
{
    // Find all links between hosts.
    // this is actually clearer than creating object links due to removing the need to relink objects as the connections are discovered.
    int i, j, k;
    int size = 10;
    struct linkCol ret;
    ret.count = 0;
    ret.links = malloc(size * sizeof(struct link));
    struct link *linkColTmp;
    struct linkElement *a;
    struct linkElement *b;
    struct linkedDevice *ld;

    for (i = 0; i < hosts->count; i++)
    {
        for (j = 0; j < hosts->hosts[i].devicesCount; j++)
        {
            // Check for space
            if (size == ret.count)
            {
                // Increase size of return variable
                size += 10;
                linkColTmp = realloc(ret.links, size * sizeof(struct link));
                if (linkColTmp)
                    ret.links = linkColTmp;
                else
                {
                    fprintf(stderr, "Out of memory attempting to create space for host link");
                    return ret;
                }
            }
            // Build the link data.
            a = &(ret.links[ret.count].a);
            b = &(ret.links[ret.count].b);
            ld = &(hosts->hosts[i].linkedDevices[j]);
            a->hostId = hosts->hosts[i].id;
            b->hostId = 0;
            strcpy(a->chassisId, hosts->hosts[i].chassisId);
            strcpy(a->portRef, ld->locPortName);

            strcpy(b->chassisId, ld->remChassisId);
            strcpy(b->portRef, ld->remPortId);

            char *leftStrTmp = {0};
            char *rightStrTmp = stripSeparators(b->chassisId);

            // Try to find a host that matches the other side of the equation.
            for (k = 0; k < hosts->count; k++)
            {
                leftStrTmp = stripSeparators(hosts->hosts[k].chassisId);

                if (strcmp(leftStrTmp, rightStrTmp) == 0)
                {
                    // match found
                    b->hostId = hosts->hosts[k].id;
                    free(leftStrTmp);
                    break;
                }

                free(leftStrTmp);
            }
            free(rightStrTmp);
            ret.count++;
        }
    }

    /* Remove connections in two directions (where a==>b and b==>a).
    For example, if host 123 is connected to host 456, we will have two entries in the table as so:
    host a      host b
    123         456 
    456         123
    so we need to remove one of those options.
    It is quicker to do this after the fact than to do this while building the list as it equates to a single pass 
    here instead of multiple table reads while building. */
    int z = 0;
    if (ret.count > 1)
    {
        for (i = 0; i < ret.count; i++)
        {
            for (j = ret.count - 1; j > 0; j--)
            {
                if (i != j)
                {
                    if (ret.links[i].a.hostId == ret.links[j].b.hostId && ret.links[i].b.hostId == ret.links[j].a.hostId)
                    {
                        // Match. remove j.
                        for (k = j; k < ret.count - 1; k++)
                        {
                            ret.links[k] = ret.links[k + 1];
                        }
                        ret.count--;
                    }
                }
            }
        }
    }
    return ret;
}

int zconnFreeId(struct hostCol *hosts)
{
    // Gets the first free ID from a collection of hosts.
    int i, j = 0;
    int found = 0;
    do
    {
        found = 0;
        j++;
        for (i = 0; i < hosts->count; i++)
        {
            if (hosts->hosts[i].id == j)
            {
                found = 1;
                break;
            }
        }
    } while (found == 1);
    return j;
}

struct hostCol *addPseudoHosts(struct hostCol *hosts, struct linkCol *links)
{
    // Add hosts where we know a host should exist even if it is not present in the original data.
    int i, j;                // loop itterator
    int size = hosts->count; // we assume that the hosts is sized to its current count and realloc from that.
    struct host *h;
    for (i = 0; i < links->count; i++)
    {
        if (links->links[i].a.hostId == 0 && links->links[i].b.hostId == 0)
        {
            // impossible
            fprintf(stderr, "both sides of link are zero");
            return hosts;
        }
        if (links->links[i].a.hostId == 0 || links->links[i].b.hostId == 0)
        {
            // One side of the link does not have an assigned host, so a pseudo host is required.
            for (j = 0; j < links->count; j++)
            {
                // attempt to find an existing match.
                // only hosts that are pseudo hosts will not have a zabbixID set
                if (i != j && hosts->hosts[j].zabbixId == 0)
                {
                    if (hosts->hosts[j].chassisId == (links->links[i].a.hostId == 0 ? links->links[i].a.chassisId : links->links[i].b.chassisId))
                    {
                        // We have found an existing match
                        if (links->links[i].a.hostId == 0)
                        {

                            links->links[i].a.hostId = hosts->hosts[j].id;
                            break;
                        }
                        else
                        {
                            links->links[i].b.hostId = hosts->hosts[j].id;
                            break;
                        }
                    }
                }
            }
            if (links->links[i].a.hostId == 0 || links->links[i].b.hostId == 0)
            {
                // same condition that brought us into this branch is still true, this means that
                // an existing pseudo host was not found in the bit above this code.
                // we need to add a new host.
                // check if there is sufficient size and resize if not.
                if (size == hosts->count)
                {
                    size += 5; // prevent calling this too often.
                    struct host *hostsTmp = realloc(hosts->hosts, size * sizeof *(hosts->hosts));
                    if (hostsTmp)
                        hosts->hosts = hostsTmp;
                    else
                        return hosts;
                }
                h = &(hosts->hosts[hosts->count]); // pointer to new host
                *h = zconnNewHost();
                strcpy(h->chassisId, (links->links[i].a.hostId == 0 ? links->links[i].a.chassisId : links->links[i].b.chassisId));
                h->id = zconnFreeId(hosts);

                // Link this host
                if (links->links[i].a.hostId == 0)
                    links->links[i].a.hostId = h->id;
                else
                    links->links[i].b.hostId = h->id;

                // Name
                strcpy(h->name, h->chassisId);
                
                hosts->count++;
            }
        }
    }
    return hosts;
}

void setPseudoPortString(char str[256], int portId)
{
    // Generate string for a pseudo hub port.
    // pp = pseudo port.
    char strTmp[15];
    memset(str, '\0', 256);
    strcat(str, "pp");
    sprintf(strTmp, "%i", portId);
    strcat(str, strTmp);
}

void addPseudoHubs(struct hostLink *hostsLinks)
{
    // If multiple devices are connected to the same port of the same host, then add a pseudo hub in between those devices.
    /*
    EXAMPLE. Assume we have four links setup like this:
    Link    a side of link      b side of link
    [0]         SW1.P1      --      SW2.P2              
    [1]         SW1.P2      --      SW3.P4
    [2]         SW4.P5      --      SW5.P2
    [3]         SW4.P5      --      SW6.P4

    In this example we can see in links 2 and 3 that there should be a hub betwee SW4.P5 (as it is linked to twice), SW5.P2, and SW6.P4.
    We will use these terms throughout the documentation of this module for clarity.
    */
    int i = 0, j = 0;
    struct linkElement *iLe, *jLe;           // Link Elements being traversed by itterators i and j.
    struct linkElement commonLink;           // This is the connection device that everything was connected to.
    struct host *pseudoHub;                  // pointer to new pseudo hub (just a new host but used as a hub)
    int pseudoPort;                          // Ensure that all connections to the pseduo hub are to unique ports.
    struct host *hostTmpPtr;                 // Temp pointer used during realloc.
    struct link *linkTmpPtr;                 // Temp pointer used during realloc.
    struct link *newLink;                    // pointer to new link that is created at the end of creating a hub for the final (root) connection.
    int hostsSize = hostsLinks->hosts.count; // Assume hosts collection is allocated to the current count.
    int linksSize = hostsLinks->links.count; // assume links colletion is allocated to the current count.

    if (hostsLinks->links.count < 2)
        return;

    // Loop for hosts connected to the same device. We need to check both sides of all links against both sides
    // of all other links, so if there are two links we have to check left of 1, right of 1, left of 2, right of 2.
    while (i < ((hostsLinks->links.count * 2) - 2))
    {
        pseudoHub = NULL;
        pseudoPort = 1;
        if (i < hostsLinks->links.count)
        {
            // i is traversing the a side of the links
            iLe = &(hostsLinks->links.links[i].a);
        }
        else
        {
            // i is traversing the b side of the links
            iLe = &(hostsLinks->links.links[i - hostsLinks->links.count].b);
        }

        for (j = i + 1; j < ((hostsLinks->links.count * 2) - 1); j++)
        {
            // Compare either the left of right hand side of the link depending on where we are in the loop.
            if (j < hostsLinks->links.count)
            {
                // i is traversing the a side of the links
                jLe = &(hostsLinks->links.links[j].a);
            }
            else
            {
                // i is traversing the b side of the links
                jLe = &(hostsLinks->links.links[j - hostsLinks->links.count].b);
            }

            if (iLe->hostId == jLe->hostId && strcmp(iLe->portRef, jLe->portRef) == 0 && strcmp(iLe->portRef, "") != 0)
            {
                // Two devices are connected to the same host on the same port. We need to insert a hub here.
                if (pseudoHub == NULL)
                {
                    //we need to add a new pseudo hub
                    if (hostsLinks->hosts.count = hostsSize)
                    {
                        // We need to resize the hosts collection to accept more host objects.
                        hostsSize += 5;
                        hostTmpPtr = realloc(hostsLinks->hosts.hosts, hostsSize * sizeof *(hostsLinks->hosts.hosts));
                        if (!hostTmpPtr)
                        {
                            fprintf(stderr, "Out of memory trying to allocate space for more hosts");
                            return;
                        }
                        hostsLinks->hosts.hosts = hostTmpPtr;
                    }
                    pseudoHub = &hostsLinks->hosts.hosts[hostsLinks->hosts.count]; // New pseudo hub
                    hostsLinks->hosts.count++;
                    *pseudoHub = zconnNewHost();
                    pseudoHub->id = zconnFreeId(&hostsLinks->hosts);
                    strcpy(pseudoHub->name, "pseudo hub");
                }

                // Now that we have a new pseudo hub (new host we will use as a hub). We now need to point all devices
                // to this hub that we want to be connected to this hub. So this includes everything that is currently
                // pointing to the same shared host and port, and then that actual host and port (the last item there
                // will require an additional link too).
                // From our example, iLe would be line 2 and jLe would be line 3. Sticking with the example, we are about to change
                // SW4.P5 reference on the a side of line 3 to point to the pseudo hub
                jLe->hostId = pseudoHub->id;
                strcpy(jLe->chassisId, pseudoHub->name);

                // Ensure that all connections to the pseudo hub are to unique ports (prevents this hub being picked up as
                // requiring a new pseudo hub in subsequent scans).
                setPseudoPortString(jLe->portRef, pseudoPort);

                pseudoPort++;
            }
        }
        if (pseudoHub != NULL)
        {
            // We have had to create a pseudo hub for jLe's that matched our iLe, so now we need to point our iLe at the same pseudo hub
            // First remember the device that iLe is connected to.
            // From our example, iLe would be line 2 Sticking with the example, we are about to copy the a side
            // reference (SW4.P5) before we change it
            strcpy(commonLink.chassisId, iLe->chassisId);
            commonLink.hostId = iLe->hostId;
            strcpy(commonLink.portRef, iLe->portRef);

            // now point iLe to the pseudo host
            // From our example, iLe would be line 2 Sticking with the example, we are about to change
            // SW4.P5 reference on the a side of line 2 to point to the pseudo hub
            iLe->hostId = pseudoHub->id;
            strcpy(iLe->chassisId, pseudoHub->name);
            // Ensure that all connections to the pseudo hub are to unique ports (prevents this hub being picked up as
            // requiring a new pseudo hub in subsequent scans).
            setPseudoPortString(iLe->portRef, pseudoPort);
            pseudoPort++;

            // Now create a new link which creates a connection between our new pseudo host and the common hosts that iLe and jLe were pointing to (common device would be SW4.P5 from our example).
            if (hostsLinks->links.count = linksSize)
            {
                // We need to resize the links collection to accept more host objects.
                linksSize += 5;
                linkTmpPtr = realloc(hostsLinks->links.links, linksSize * sizeof *(hostsLinks->links.links));
                if (!linkTmpPtr)
                {
                    fprintf(stderr, "Out of memory trying to allocate space for more links");
                    return;
                }
                hostsLinks->links.links = linkTmpPtr;
            }
            newLink = &hostsLinks->links.links[hostsLinks->links.count];
            hostsLinks->links.count++;

            // first connect the new link to the pseudo hub
            newLink->a.hostId = pseudoHub->id;
            strcpy(newLink->a.chassisId, pseudoHub->name);

            setPseudoPortString(newLink->a.portRef, pseudoPort);

            // Now connect the new other side of the link to the common device (SW4.P5 from our example)
            newLink->b.hostId = commonLink.hostId;
            strcpy(newLink->b.chassisId, commonLink.chassisId);
            strcpy(newLink->b.portRef, commonLink.portRef);

            // If I was traversing the right hand side (b side) of the links when we added this new link in then
            // we need to adjust its current position as the addition of the new link will have invalidated the position.
            if (i >= hostsLinks->links.count)
                i++;
        }
        i++;
    }
}

void printHosts(struct hostCol *hosts)
{
    // debug tool. print out the contents of hosts so that we can check for correct parsing.
    int i, j; // loop itterators.
    printf("***** Hosts *****\n");
    for (i = 0; i < hosts->count; i++)
    {
        // list Host data
        printf("host %i [%i] with name '%s'\n", hosts->hosts[i].id, hosts->hosts[i].zabbixId, hosts->hosts[i].name);
        printf("\tChassis ID: '%s' ", hosts->hosts[i].chassisId);
        printf("ID Type: %i\n", hosts->hosts[i].chassisIdType);
        printf("x position: %.1f\ty position: %.1f\n", hosts->hosts[i].xPos, hosts->hosts[i].yPos);
        printf("\tHas %i interface%s:\n", hosts->hosts[i].interfaceCount, (hosts->hosts[i].interfaceCount == 1) ? "" : "s");
        for (j = 0; j < hosts->hosts[i].interfaceCount; j++)
        {
            // List interfaces
            printf("\t\t%i: %s\n", j, hosts->hosts[i].interfaces[j]);
        }
        printf("\tHas %i linked device%s:\n", hosts->hosts[i].devicesCount, (hosts->hosts[i].devicesCount == 1) ? "" : "s");
        if (hosts->hosts[i].devicesCount > 0)
        {
            // header
            printf("\t\tId\tMSAP\tPort Connections\n");
        }
        for (j = 0; j < hosts->hosts[i].devicesCount; j++)
        {
            // List linked devices

            printf("\t\t%i\t%i\t[%s]-->[%s]\n", j, hosts->hosts[i].linkedDevices[j].msap, hosts->hosts[i].linkedDevices[j].locPortName, hosts->hosts[i].linkedDevices[j].remPortId);
        }
        printf("\n");
    }
}

void printLinks(struct linkCol *links)
{
    int i;
    printf("***** Links *****\n");
    for (i = 0; i < links->count; i++)
    {
        printf("a:HostId: %i[%s][%s], b:HostId: %i[%s][%s]\n", links->links[i].a.hostId, links->links[i].a.chassisId, links->links[i].a.portRef, links->links[i].b.hostId, links->links[i].b.chassisId, links->links[i].b.portRef);
    }
}

struct hostLink *mapHosts(struct hostLink *hl)
{
    hl->links = findAllLinks(&(hl->hosts));
    hl->hosts = *addPseudoHosts(&(hl->hosts), &(hl->links));
    addPseudoHubs(hl);
    return hl;
}

/** 
 * Create a forest from a hostLink object
 * @param [in] hl       hostlink object containing all hosts and their associated links that will be converted into a forest of trees.
 * */
struct forest *hostLinkToForest(struct forest *f, struct hostLink *hl)
{

    // TODO: Test and Valgrind this as I have not even tested that it runs yet.

    if (hl->hosts.count == 0)
        return f;

    // The memory is externally created but we need to ensure that it is in a 'reset' state
    int fTreesSize = 5; // maximum amount of possible trees in the memory allocated in forest.
    struct forest *fTmpPtr = realloc(f, (fTreesSize * sizeof(struct tree)) + sizeof *fTmpPtr);
    if (!fTmpPtr)
    {
        fprintf(stderr, "Out of memory allocating space to forest");
        return f;
    }
    f = fTmpPtr;
    f->treeCount = 0;

    /* Itterate the hosts. For each host:
    - Check to see if it has already been processed (Added to a tree)
    - if not, create a new tree and add the host to the tree.
    - recursively add everything that is linked to this host into the same tree
    - link these items within the tree
    - add tree to forest */
    int i, j, k;
    for (i = 0; i < hl->hosts.count; i++)
    {
        struct host *h = &hl->hosts.hosts[i];
        if (!getNodeByIdFromForest(h->id, f))
        {
            // node not found in forest, this means it has not been processed yet (Added to a tree).
            // Create a new tree, create a node from the host, then add the host to the tree
            tree t = createTree();
            node n = createNode(h->id);
            t.nodes = addNode(t.nodes, n);

            // find all hosts linked to this host, create nodes from them and then add them to the same tree.
            // Hosts to be added in the next cycle
            int hostsToBeAddedCount = 0;
            int hostsToBeAddedSize = 5;
            int *hostsToBeAddedTmp;
            int *hostsToBeAdded = malloc(hostsToBeAddedSize * sizeof *hostsToBeAdded);
            // hosts added in the previous cycle.
            int hostsAddedCount = 1;
            int hostsAddedSize = 1;
            int *hostsAddedTmp;
            int *hostsAdded = malloc(hostsAddedSize * sizeof *hostsAdded);
            hostsAdded[0] = h->id;

            while (hostsAddedCount > 0)
            {

                for (j = 0; j < hl->links.count; j++)
                {
                    // Itterate each link

                    for (k = 0; k < hostsAddedCount; k++)
                    {

                        // Itterate each host that has been added to the tree in the previous cycle.
                        if (hl->links.links[j].a.hostId == hostsAdded[k] && !getNodeByIdFromTree(hl->links.links[j].b.hostId, &t))
                        {
                            // We have found a link where the a side matches a host we added in the previous pass.
                            // This means that the links b side is a host that we need to add to the tree if it has
                            // not already been added.
                            if (hostsToBeAddedSize == hostsToBeAddedCount)
                            {
                                // We need to increase the amount of allocated space.
                                hostsToBeAddedSize += 5;
                                hostsToBeAddedTmp = realloc(hostsToBeAdded, hostsToBeAddedSize * sizeof *hostsToBeAddedTmp);
                                if (!hostsToBeAddedTmp)
                                {
                                    fprintf(stderr, "Out of memory trying to increase hostsToBeAdded to %i", hostsToBeAddedSize);
                                    return f;
                                }
                                hostsToBeAdded = hostsToBeAddedTmp;
                            }
                            hostsToBeAdded[hostsToBeAddedCount] = hl->links.links[j].b.hostId;
                            hostsToBeAddedCount++;
                        }
                        else if (hl->links.links[j].b.hostId == hostsAdded[k] && !getNodeByIdFromTree(hl->links.links[j].a.hostId, &t))
                        {
                            // We have found a link where the a side matches a host we added in the previous pass.
                            // This means that the links b side is a host that we need to add to the tree if it has
                            // not already been added.
                            if (hostsToBeAddedSize == hostsToBeAddedCount)
                            {
                                // We need to increase the amount of allocated space.
                                hostsToBeAddedSize += 5;
                                hostsToBeAddedTmp = realloc(hostsToBeAdded, hostsToBeAddedSize * sizeof *hostsToBeAddedTmp);
                                if (!hostsToBeAddedTmp)
                                {
                                    fprintf(stderr, "Out of memory trying to increase hostsToBeAdded to %i", hostsToBeAddedSize);
                                    return f;
                                }
                                hostsToBeAdded = hostsToBeAddedTmp;
                            }
                            hostsToBeAdded[hostsToBeAddedCount] = hl->links.links[j].a.hostId;
                            hostsToBeAddedCount++;
                        }
                    }
                }

                // Add the new hosts.
                hostsAddedCount = 0;
                for (j = 0; j < hostsToBeAddedCount; j++)
                {
                    if (!getNodeByIdFromTree(hostsToBeAdded[j], &t))
                    {
                        // Host has not been added to the new tree already.
                        // should be impossible for the host to be in any other tree of the forest so do not test for it.
                        // If I should find that later on hosts are appearing in other trees, don't just add the check, fix it!
                        n = createNode(hostsToBeAdded[j]);
                        t.nodes = addNode(t.nodes, n);

                        // record that we have added this host for the next pass.
                        if (hostsAddedCount == hostsAddedSize)
                        {
                            // Need to increase storage space for hosts added
                            hostsAddedSize += 2;
                            hostsAddedTmp = realloc(hostsAdded, hostsAddedSize * sizeof *hostsAddedTmp);
                            if (!hostsAddedTmp)
                            {
                                fprintf(stderr, "Out of memory while trying to increase size of hostsAdded to %i", hostsAddedSize);
                                return f;
                            }
                            hostsAdded = hostsAddedTmp;
                        }
                        hostsAdded[hostsAddedCount] = hostsToBeAdded[j];
                        hostsAddedCount++;
                    }
                }
                hostsToBeAddedCount = 0; // reset each loop.
            }

            if (t.nodes->nodeCount > 1)
            {
                // tree has multiple nodes so now we must add in the links.
                for (j = 0; j < t.nodes->nodeCount; j++)
                {
                    // Itterate the nodes that are in the tree, then for each node pull all of the HostLink links, then check to see
                    // if that link already exists and if not, add it.
                    for (k = 0; k < hl->links.count; k++)
                    {
                        // Itterate the HostLink links.
                        int l;
                        int exists = 0;
                        for (l = 0; l < t.links->linkCount; l++)
                        {
                            // Itterate the existing tree links to see if the link has already been created.
                            if ((t.links->links[l].id1 == hl->links.links[k].a.hostId && t.links->links[l].id2 == hl->links.links[k].b.hostId) ||
                                (t.links->links[l].id1 == hl->links.links[k].b.hostId && t.links->links[l].id2 == hl->links.links[k].a.hostId))
                                exists = 1;
                        }
                        if (exists == 0)
                        {
                            // Link does not already exist, so create it.
                            t.links = linkNodes(t.links, hl->links.links[k].a.hostId, hl->links.links[k].b.hostId);
                        }
                    }
                }
            }

            if (f->treeCount == fTreesSize)
            {
                // we need to increase the allocated space for the trees.
                fTreesSize += 3;
                struct forest *fTmpPtr = realloc(f, (fTreesSize * sizeof(struct tree)) + sizeof *fTmpPtr);
                if (!fTmpPtr)
                {
                    fprintf(stderr, "Out of memory allocating space to forest");
                    return f;
                }
                f = fTmpPtr;
            }

            f->trees[f->treeCount] = t;
            f->treeCount++;

            free(hostsAdded);
            free(hostsToBeAdded);
        }
    }
    
    return f;
}

/**
 * Calculate the target placement of the hosts on the zabbix map canvas.
 * @param [in,out]  hostsLinks      The hosts that need to be laid out on the canvas along with their associated links. No need to create links or hosts here, so no need to worry about returning a new pointer.
 * @param [in]  hostXSpace      spacing between hosts on the x axis
 * @param [in]  hostYSpace      spacing between hosts on the y axis
 * @param [in]  treePadding     spacing within the individual trees
 * @param [in]  sorts           sorting methods to be used with the trees
 * @param [in]  sortCount       number of sorting methods that have been supplied. */
void layoutHosts(struct hostLink *hostsLinks, double hostXSpace, double hostYSpace, struct padding treePadding, enum sortMethods *sorts, int sortCount)
{
    int i, j, k;         // loop itterator
    int rootOrd;      // Ordinal position of the root node.
    int linkCount;    // number of links for the root node.
    int linkCountTmp;   // number of links for a given node scratch memory.
    struct forest *f = malloc(sizeof *f);
    struct host *h;
    struct node *n;
    struct tree *t;
    f->treeCount = 0;

    // Convert the hosts and links taken from Zabbix into a generalised forest (collection of tree structures)
    f = hostLinkToForest(f, hostsLinks);

    if (f)
    {
        // set root for each tree.
        for (i = 0; i < f->treeCount; i++)
        {
            if (f->trees[i].nodes->nodeCount == 1)
            {
                // There is only one node
                f->trees[i].nodes->nodes[0].level = 0; // set root.
            }
            else
            {
                // Multiple nodes (or zero, but that should not happen);
                rootOrd = -1;
                linkCount=0;
                for (j=0;j<f->trees[i].nodes->nodeCount;j++)
                {
                    // which node has the highest number of other nodes connected to it? We will take the first highest result as the root.
                    linkCountTmp = getConnectedNodeCount(f->trees[i].nodes->nodes[j].id, &f->trees[i]);
                    if(linkCountTmp > linkCount)
                    {
                        linkCount = linkCountTmp;
                        rootOrd = j;
                    }
                }
                f->trees[i].nodes->nodes[rootOrd].level = 0;        // Set the node with the highest number of connected nodes as the root.
            }
        }

        // Layout the forest, positioning all of the tree nodes correctly within their respective trees and also positioning the trees respective to each other.
        double padding[4] = {treePadding.top, treePadding.right, treePadding.bottom, treePadding.left};
        layoutForest(f, sorts, sortCount, hostXSpace, hostYSpace, padding);

        // update the hostslinks with the positioning data in the forest.
        for (i=0; i<hostsLinks->hosts.count;i++)
        {
            for(j=0;j<f->treeCount;j++)
            {
                for(k=0; k<f->trees[j].nodes->nodeCount;k++)
                {
                    // for each host in hostsLinks, find the corresponding node in the corresponding tree and then 
                    // position the host by applying the x and y position of the tree plus the node which gives the
                    // absolute position of the tree node (and hence the host) on the canvas.
                    if(hostsLinks->hosts.hosts[i].id == f->trees[j].nodes->nodes[k].id)
                    {
                        // Found the tree and treenode that corresponds to the host.
                        // set the hosts position.
                        h = &hostsLinks->hosts.hosts[i];
                        t = &f->trees[j];
                        n = &t->nodes->nodes[k];
                        h->xPos = t->posX + n->posX;        // TODO: posX and xPos: be consistent.
                        h->yPos = t->posY + n->posY;
                    }
                }
            }
        }
        // DEBUG print trees.
        //for (i = 0; i < f->treeCount; i++)
        //    printTree(&f->trees[i]);
    }
    freeForest(f);
}
