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
#include <math.h>
#include "Forests.h"

enum side
{
    left,
    right
};

struct rect
{
    double w; // width
    double h; // height
    double x; // x axis position
    double y; // y axis position
    int id;   // identifier
};

typedef struct rect rect;

struct nodePtrCollection /* Node Pointer Collection */
{
    int nodeCount; /* number of nodes in the nodes collection */
    node *nodes[]; /* nodes within the tree */
};

typedef struct nodePtrCollection nodePtrCollection;

node createNode(int id)
{
    /* create a new node with the given id */
    node ret;
    ret.id = id;
    ret.level = -1; /* default value if not defined */
    ret.offsetToParent = 0.0;
    ret.offsetSet = 0; // false
    ret.relativeSet = 0; // false
    ret.posX = 0.0;
    ret.posY = 0.0;
    ret.w = 100.0; // assumed width of a node. TODO: make this 1.0 (unit space) and allow it to be set by parameter.
    ret.h = 100.0; // assumed height of a node. TODO: make this 1.0 (unit space) and allow it to be set by parameter.
    ret.sortPos = -0;
    ret.numChildren = 0;
    ret.numDescendants = 0;
    ret.numGenerations = 0;
    return ret;
}

nodeCollection *addNode(nodeCollection *nodes, node node)
{
    /* Expand the node collection to fit the new node */
    int nodeCount = nodes->nodeCount;
    nodeCount++;
    int targetSize = sizeof nodes + (nodeCount * sizeof(struct node));
    nodes = realloc(nodes, targetSize);
    if (!nodes)
        return NULL;

    /* Add the new node to the collection */
    nodes->nodeCount++;
    nodes->nodes[nodeCount - 1] = node;
    return nodes;
}

nodePtrCollection *addPtrNode(nodePtrCollection *nodes, node *node)
{
    /* Expand the node collection to fit the new node pointer */
    int nodeCount = nodes->nodeCount;
    nodeCount++;
    int targetSize = sizeof *nodes + (nodeCount * sizeof(struct node *));
    void *node_check = realloc(nodes, targetSize);
    if (!node_check)
    {
        free(nodes);
        return NULL;
    }

    nodes = node_check;

    /* Add the new node pointer to the collection */
    nodes->nodeCount++;
    nodes->nodes[nodeCount - 1] = node;
    return nodes;
}

nodeLinkCollection *linkNodes(nodeLinkCollection *links, int id1, int id2)
{
    nodeLink link = {.id1 = id1, .id2 = id2};

    /* Link two nodes by their identifiers */
    int count = links->linkCount;
    count++;
    int targetSize = sizeof links + (count * sizeof(struct nodeLink));
    links = realloc(links, targetSize);

    links->linkCount = count;
    links->links[count - 1] = link;
    return links;
}

forest *addTree(forest *f, tree t)
{
    /* increase allocated memory ready for new tree to be inserted */
    f = realloc(f, sizeof(struct forest) + (f->treeCount + 1 * (sizeof t)));
    if (!f)
        return NULL;
    f->trees[f->treeCount] = t;
    f->treeCount++;
    return f;
}

tree createTree()
{
    /* create a new empty tree */
    tree ret;

    ret.width = 0.0;
    ret.height = 0.0;
    ret.posX = 0.0;
    ret.posY = 0.0;

    /* Create space in the tree for the nodes */
    ret.nodes = malloc(sizeof(nodeCollection));
    if (ret.nodes)
        ret.nodes->nodeCount = 0;

    ret.links = malloc(sizeof(nodeLinkCollection));
    if (ret.links)
        ret.links->linkCount = 0;
    return ret;
}

void freeForest(forest *f)
{
    /* free all memory in a forest. Assumes nothing has been freed already. */
    int i;
    int x;
    tree t;
    for (i = 0; i <= f->treeCount - 1; i++)
    {
        t = f->trees[i];
        /* free the tree */
        free(t.links);
        free(t.nodes);
    }
    /* free the forest itself */
    free(f);
}

int rootId(tree *t)
{
    /* return the Identifier of the root node. */
    int i; // Loop itterator
    if (t->nodes->nodeCount < 1)
        return -1;

    for (i = 0; i < t->nodes->nodeCount; i++)
    {
        if (t->nodes->nodes[i].level == 0)
            return t->nodes->nodes[i].id; // This is the root node
    }

    // if you arrive here then no root node was found.
    return -1;
}

node *getNodeById(int id, tree *t)
{
    /* Returns a node that matches the requested identifier. */
    int i;
    for (i = 0; i <= t->nodes->nodeCount - 1; i++)
    {
        /* itterate through the nodes collection. */
        if (t->nodes->nodes[i].id == id)
        {
            /* this is a match */
            return &(t->nodes->nodes[i]);
        }
    }
    // if we go this far then nothing found
    return NULL;
}

node *getNodeByIdFromTree(int id, tree *t)
{
    /* Returns a node that matches the requested identifier. */
    int i;
    for (i = 0; i <= t->nodes->nodeCount - 1; i++)
    {
        /* itterate through the nodes collection. */
        if (t->nodes->nodes[i].id == id)
        {
            /* this is a match */
            return &(t->nodes->nodes[i]);
        }
    }
    // if we go this far then nothing found
    return NULL;
}

/**
 * Gets a node that matches the requested identifier */
node *getNodeByIdFromForest(int id, forest *f)
{
    node *ret = NULL;
    int i;
    for (i = 0; i < f->treeCount; i++)
    {
        ret = getNodeByIdFromTree(id, &f->trees[i]);
        if (ret)
            return ret; // found
    }
    return ret;
}

nodePtrCollection *getConnectedNodes(int rootId, tree *t)
{
    /* Get an array of all nodes that are linked to a given node. 
        we will return a pointer to the nodeCollection */
    int i = 0;  // Loop counter
    int id = 0; // Identifier of linked node.
    struct node *tempNode;

    if (!t->links)
        return NULL;

    if (t->links->linkCount == 0)
        return NULL;

    nodePtrCollection *ret = malloc(sizeof *ret);
    ret->nodeCount = 0;

    for (i = 0; i < t->links->linkCount; i++)
    {
        if (t->links->links[i].id1 == rootId || t->links->links[i].id2 == rootId)
        {
            // Root node is ID 1. Get the node connected to it
            tempNode = getNodeById((t->links->links[i].id1 == rootId) ? t->links->links[i].id2 : t->links->links[i].id1, t);
            if (tempNode)
                ret = addPtrNode(ret, tempNode);
        }
    }
    return ret;
}

/**
 * Get the number of nodes connected to a given tree node */
int getConnectedNodeCount(int rootId, tree *t)
{
    nodePtrCollection *nodes = getConnectedNodes(rootId, t);
    if (!nodes)
        return 0;
    int count = nodes->nodeCount;
    free(nodes);
    return count;
}

void setLevels(tree *t)
{
    /* Set the level of every node in the tree.
        requires that at least one node has level = 0 as this will be the root node. */
    int i, x;       // Loop itterators.
    int targetSize; // debug
    node *rootNode = NULL;
    targetSize = sizeof(struct node *);
    struct node **loopNodes = malloc(targetSize);     // Nodes to be itterated in a single loop
    struct node **nextLoopNodes = malloc(targetSize); // Nodes to be itterated in next single loop
    int count = 0;                                    // number of node pointers in loopNodes;
    int nextCount = 0;                                // number of node pointers in nextLoopNodes;
    nodePtrCollection *tempNodesPtrCollection;        // scratch memory

    if (!t->nodes)
        goto freeExit;

    if (t->nodes->nodeCount == 0)
        goto freeExit;

    for (i = 0; i < t->nodes->nodeCount; i++)
    {
        if (t->nodes->nodes[i].level == 0)
        {
            // found the root node
            rootNode = &(t->nodes->nodes[i]);
            break;
        }
    }

    if (!rootNode)
        goto freeExit;

    // start with the root node
    *loopNodes = rootNode;
    count = 1;

    do
    {
        for (i = 0; i < count; i++)
        {
            /* Loop through all of the nodes captured in the last pass through */
            // get all nodes connected to the node
            tempNodesPtrCollection = getConnectedNodes(loopNodes[i]->id, t);
            if (tempNodesPtrCollection)
                for (x = 0; x < tempNodesPtrCollection->nodeCount; x++)
                {
                    if (tempNodesPtrCollection->nodes[x]->level == -1)
                    {
                        /* Add this node to the collection of nodes to itterate on the next loop */
                        nextLoopNodes = realloc(nextLoopNodes, (nextCount + 1) * (sizeof(*nextLoopNodes)));
                        nextLoopNodes[nextCount] = tempNodesPtrCollection->nodes[x];
                        nextLoopNodes[nextCount]->level = loopNodes[i]->level + 1; // Set level of the nodes as one more than current;
                        nextCount++;
                    }
                }
            free(tempNodesPtrCollection);
        }

        // Copy over objects into the next loop.
        count = nextCount;
        loopNodes = realloc(loopNodes, count * sizeof(*loopNodes));

        for (i = 0; i < count; i++)
        {
            loopNodes[i] = nextLoopNodes[i];
        }

        // Reset next loop memory objects
        nextLoopNodes = realloc(nextLoopNodes, sizeof(*nextLoopNodes));
        nextCount = 0;

    } while (count > 0);

freeExit:
    free(loopNodes);
    free(nextLoopNodes);
    return;
}

int minSort(struct node *nodes[], int count)
{
    /* Return the minimum value of sortPosition for all nodes in the array */
    int minSort = 0; //minimum sort value;
    int i;           //loop itterator;

    if (count == 0)
        return minSort;

    for (i = 0; i < count; i++)
    {
        if (nodes[i]->sortPos < minSort)
            minSort = nodes[i]->sortPos;
    }

    return minSort;
}

int maxSort(struct node *nodes[], int count)
{
    /* Return the maximum value of sortPosition for all nodes in the array */
    int maxSort = 0; //maximum sort value;
    int i;           //loop itterator;

    if (count == 0)
        return maxSort;

    for (i = 0; i < count; i++)
    {
        if (nodes[i]->sortPos > maxSort)
            maxSort = nodes[i]->sortPos;
    }

    return maxSort;
}

nodePtrCollection *getNodesAtDepth(int depth, tree *t)
{
    /* returns all nodes that are at a given depth (level) */
    int i;           // loop itterator.
    int maxPtrs = 1; // maximum number of node pointers. This is the size of allocated memory in return variable. You end up with ret being larger than needed but reduce the repeated  reallocs.
    nodePtrCollection *ret = malloc((sizeof *ret) + (maxPtrs * sizeof(struct node *)));
    ret->nodeCount = 0;

    for (i = 0; i < t->nodes->nodeCount; i++)
    {
        // Itterate the nodes.
        if (t->nodes->nodes[i].level == depth)
        {
            // add this node to the return value;
            // do we need to adjust the size of the return variable.
            if (ret->nodeCount == maxPtrs)
            {
                // We are at capacity.
                maxPtrs = (int)ceil(maxPtrs * 1.5);
                void *retCheck = realloc(ret, (sizeof *ret) + (maxPtrs * sizeof(struct node *)));
                if (!retCheck)
                    return NULL;
                ret = retCheck;
            }
            // Add new desired node to the return value and increment the count of nodes in the return value;
            ret->nodes[ret->nodeCount++] = &(t->nodes->nodes[i]);
        }
    }
    return ret;
}

int comparSortPos(const void *p1, const void *p2)
{
    /* qsort comparitor to sort by sort position. Required when obtaining filtered lists. Filtered lists are generally
    nodePtrCollections so this function takes a pointer to a pointer of type struct node.
    /* Return value meaning
    <0 The element pointed by p1 goes before the element pointed by p2
    0  The element pointed by p1 is equivalent to the element pointed by p2
    >0 The element pointed by p1 goes after the element pointed by p2*/
    const node *node1 = *(const node **)p1;
    const node *node2 = *(const node **)p2;

    if (node1->sortPos > node2->sortPos)
    {
        return 1;
    }
    else
    {
        if (node1->sortPos < node2->sortPos)
        {
            return -1;
        }
        return 0; // if all other checks fail.
    }
}

nodePtrCollection *getChildren(int id, tree *t)
{
    /* returns all children of a given node. */
    int i = 0;      // Loop counter
    node *tempNode; // scratch memory
    node *parent = getNodeById(id, t);
    if (!parent)
        return NULL; // could not get the parent node

    if (!t->links)
        return NULL;

    if (t->links->linkCount == 0)
        return NULL;

    int maxPtrs = 1; // maximum number of node pointers. This is the size of allocated memory in return variable. You end up with ret being larger than needed but reduce the repeated  reallocs.
    nodePtrCollection *ret = malloc((sizeof *ret) + (maxPtrs * sizeof(struct node *)));
    ret->nodeCount = 0;

    for (i = 0; i < t->links->linkCount; i++)
    {
        if (t->links->links[i].id1 == id || t->links->links[i].id2 == id)
        {
            // Root node is ID 1. Get the node connected to it
            tempNode = getNodeById((t->links->links[i].id1 == id) ? t->links->links[i].id2 : t->links->links[i].id1, t);
            if (tempNode)
            {
                if (tempNode->level > parent->level)
                {
                    // this is a child node
                    if (ret->nodeCount == maxPtrs)
                    {
                        // We are at capacity.
                        maxPtrs = (int)ceil(maxPtrs * 1.5);
                        void *retCheck = realloc(ret, (sizeof *ret) + (maxPtrs * sizeof(struct node *)));
                        if (!retCheck)
                            return NULL;
                        ret = retCheck;
                    }
                    ret = addPtrNode(ret, tempNode);
                }
            }
        }
    }
    qsort(&(ret->nodes), ret->nodeCount, sizeof(struct node **), comparSortPos);
    return ret;
}

nodePtrCollection *getDescendants(int id, tree *t)
{
    /* get all children, grandchildren, etc. down to the leaf node. */
    int i, x, temp; // loop itterators
    int childCount; // number of children
    int targetSize = 0;
    nodePtrCollection *ret = getChildren(id, t);
    if (!ret)
        return NULL;
    childCount = ret->nodeCount;
    nodePtrCollection *childRet;

    for (i = 0; i < childCount; i++)
    {
        // recursive loop will get the children of the children that were obtained earlier in the call of this function.
        // we will then add those children to our children thus building up a list of all descendants.
        childRet = getDescendants(ret->nodes[i]->id, t);
        if (childRet->nodeCount > 0)
        {
            // memory required to merge arrays
            temp = ret->nodes[i]->id;
            targetSize = childRet->nodeCount + ret->nodeCount;
            void *retCheck = realloc(ret, (sizeof *ret) + (targetSize * sizeof(struct node *)));
            if (!retCheck)
                return NULL;
            ret = retCheck;
            // merge arrays
            for (x = 0; x < childRet->nodeCount; x++)
            {
                // copy from child array to main return array
                ret->nodes[ret->nodeCount++] = childRet->nodes[x];
            }
        }
        free(childRet);
    }
    qsort(&(ret->nodes), ret->nodeCount, sizeof(struct node **), comparSortPos);
    return ret;
}

int generationCount(int id, tree *t)
{
    /* Get the count of generations BELOW a node. 
    If there are no descendants then the generation count is zero */
    /* This value is calculated on demand here and is not taken from the statistics set */
    int minLevel = 0, maxLevel = 0;
    int i; // loop itterator.
    nodePtrCollection *descendants = getDescendants(id, t);
    if (!descendants || descendants->nodeCount == 0)
    {
        free(descendants);
        return 0;
    }

    for (i = 0; i < descendants->nodeCount; i++)
    {
        if (i == 0)
        {
            /* initialise the levels */
            minLevel = descendants->nodes[i]->level;
            maxLevel = descendants->nodes[i]->level;
        }
        else
        {
            if (descendants->nodes[i]->level < minLevel)
                minLevel = descendants->nodes[i]->level;
            if (descendants->nodes[i]->level > maxLevel)
                maxLevel = descendants->nodes[i]->level;
        }
    }

    free(descendants);
    return maxLevel - minLevel + 1;
}

void calcStats(int id, tree *t)
{
    /* Calculates the statistics for all nodes in a tree, working from a given node (id) */
    node *root = getNodeById(id, t);
    int descendants = 0, generations = 0;
    int i; // loop itterator
    nodePtrCollection *children;
    if (!root)
        return;

    children = getChildren(id, t);
    if (!children)
    {
        return;
    }

    if (children->nodeCount == 0)
    {
        /* no children below this node, therefore this is a leaf node */
        root->numChildren = 0;
        root->numDescendants = 0;
        root->numGenerations = 0;
    }
    else
    {
        /* calculate values from the children collection */
        root->numChildren = children->nodeCount;
        for (i = 0; i < children->nodeCount; i++)
        {
            // Calculate the child nodes statistics first.
            calcStats(children->nodes[i]->id, t);

            descendants += children->nodes[i]->numDescendants; // aggregate number of descendants
            if (children->nodes[i]->numGenerations > generations)
            {
                // take the maximum number of generations
                generations = children->nodes[i]->numGenerations;
            }
        }
        // Generations needs to be incremented as the current node is a parent of all child generations
        generations++;

        // Record the values
        root->numGenerations = generations;
        root->numDescendants = descendants + children->nodeCount;
    }

tidyup:
    /* Tidy up */
    free(children);
}

int comparDescendantCount(const void *p1, const void *p2)
{
    /* qsort comparitor for descendant count */
    /* uses the node statistics. */
    /* Return value meaning
    <0 The element pointed by p1 goes before the element pointed by p2
    0  The element pointed by p1 is equivalent to the element pointed by p2
    >0 The element pointed by p1 goes after the element pointed by p2*/
    const node *node1 = p1;
    const node *node2 = p2;
    if (node1->numDescendants > node2->numDescendants)
    {
        return 1;
    }
    else
    {
        if (node1->numDescendants < node2->numDescendants)
        {
            return -1;
        }
        return 0; // if all other checks fail.
    }
}

int comparDescendantCountDESC(const void *p1, const void *p2)
{
    /* as comparDescendantCount but with descending count */
    return comparDescendantCount(p1, p2) * -1;
}

int comparChildrenCount(const void *p1, const void *p2)
{
    /* qsort comparitor for children count */
    /* uses the node statistics. */
    /* Return value meaning
    <0 The element pointed by p1 goes before the element pointed by p2
    0  The element pointed by p1 is equivalent to the element pointed by p2
    >0 The element pointed by p1 goes after the element pointed by p2*/
    const node *node1 = p1;
    const node *node2 = p2;
    if (node1->numChildren > node2->numChildren)
    {
        return 1;
    }
    else
    {
        if (node1->numChildren < node2->numChildren)
        {
            return -1;
        }
        return 0; // if all other checks fail.
    }
}

int comparRectID(const void *p1, const void *p2)
{
    /* qsort comparitor for rectangle identifiers */
    /* Return value meaning
    <0 The element pointed by p1 goes before the element pointed by p2
    0  The element pointed by p1 is equivalent to the element pointed by p2
    >0 The element pointed by p1 goes after the element pointed by p2*/
    const struct rect *r1 = p1;
    const struct rect *r2 = p2;
    if (r1->id > r2->id)
    {
        return 1;
    }
    else
    {
        if (r1->id < r2->id)
        {
            return -1;
        }
        return 0; // if all other checks fail then they are equal.
    }
}

int comparRectHeight(const void *p1, const void *p2)
{
    /* qsort comparitor for rectangle height */
    /* Return value meaning
    <0 The element pointed by p1 goes before the element pointed by p2
    0  The element pointed by p1 is equivalent to the element pointed by p2
    >0 The element pointed by p1 goes after the element pointed by p2*/
    const struct rect *r1 = p1;
    const struct rect *r2 = p2;
    if (r1->h > r2->h)
    {
        return 1;
    }
    else
    {
        if (r1->h < r2->h)
        {
            return -1;
        }
        return 0; // if all other checks fail then they are equal.
    }
}

int comparRectHeightDesc(const void *p1, const void *p2)
{
    /* qsort comparitor by descending rectangle height */
    return comparRectHeight(p1, p2) * -1; // invert the ascending result.
}

void FFDH(rect *rects, int n, double width)
{
    /* First fit descending height.
    For a basic explanation of the algorithm see E.G. Coffman Jr. and M.R. Garey and D.S. Johnson and R.E. Tarjan. 
    Performance bounds for level-oriented two-dimensional packing algorithms. SIAM Journal on Computing, 9:808--826, 1980.
    Note: This is not an optimum implementation, there are other implementations on GitHub that can be used.
    I just needed to knock something together without reading someone else's documentation and so this rough-and-ready solution
    will do for now. Recommend replacing during a future refrator.
    rects[] = pointer to array of rectangles 
    n   =   number of rectangles pointer to.
    width   =   strip width depending on your terminology.*/
    double maxWidth = 0;                              // maximum width of all rectangles
    int i;                                            // loop itterator
    _Bool *processed = malloc(n * sizeof *processed); // Flag which rectangles have been processed.
    int processedCount = 0;                           // how many rectangles have been processed
    int x = 0;                                        // x axis position in the current strip
    int y = 0;                                        // y axis position.
    double currentStripHeight;                        // current strip height
    if (n == 0)
        return;

    for (i = 0; i < n; i++)
    {
        if ((rects + i)->w > maxWidth)
        {
            // Record the maximum of all rectangles in the array
            maxWidth = (rects + i)->w;
        }
    }

    if (maxWidth > width)
    {
        printf("maxWidth cannot be larger that strip width;");
        free(processed);
        exit(EXIT_FAILURE);
    }

    qsort(rects, n, sizeof(rect), comparRectHeightDesc); // order rectangles by descending height.

    // Initiate the processed array
    for (i = 0; i < n; i++)
    {
        processed[i] = 0;
    }

    while (processedCount < n)
    {
        /*Loop until all rectangles are assigned.
        current strip is the height of the tallest rectange. Rectangles are ordered by height, so the first
        unprocessed rectangle fits this bill. all other unprocessed rectangles must fit by neccessity*/
        currentStripHeight = 0;
        for (i = 0; i < n; i++)
        {
            if (processed[i] == 0)
            {
                // related rectangle has not been processed.
                if (currentStripHeight == 0)
                    currentStripHeight = (rects + i)->h;

                if (width - x >= (rects + i)->w)
                {
                    // rectangle can fit in the available space in the current strip.
                    (rects + i)->x = x; // assign position to this rectangle
                    (rects + i)->y = y;
                    x += (rects + i)->w; // consume the x axis space in this strip
                    processed[i] = 1;    // mark the rectangle as processed.
                    processedCount++;
                }
            }
        }
        // Move up to the next strip.
        y += currentStripHeight;
        x = 0;
    }
    free(processed);
}

int comparChildrenCountDESC(const void *p1, const void *p2)
{
    /* as comparChildrenCount but with descending count */
    return comparChildrenCount(p1, p2) * -1;
}

int comparGenerationCount(const void *p1, const void *p2)
{
    /* qsort comparitor for generation count */
    /* uses the node statistics. */
    /* Return value meaning
    <0 The element pointed by p1 goes before the element pointed by p2
    0  The element pointed by p1 is equivalent to the element pointed by p2
    >0 The element pointed by p1 goes after the element pointed by p2*/
    const node *node1 = p1;
    const node *node2 = p2;
    if (node1->numGenerations > node2->numGenerations)
    {
        return 1;
    }
    else
    {
        if (node1->numGenerations < node2->numGenerations)
        {
            return -1;
        }
        return 0; // if all other checks fail.
    }
}

int comparGenerationCountDESC(const void *p1, const void *p2)
{
    /* as comparGenerationCount but with descending count */
    return comparGenerationCount(p1, p2) * -1;
}

nodePtrCollection *getNodesBySortPos(int sortPos, tree *t)
{
    /* Get all nodes with a given sort position */
    int i; // loop itterator.
    struct node *tempNode;
    int maxPtrs = 1; // maximum number of node pointers. This is the size of allocated memory in return variable. You end up with ret being larger than needed but reduce the repeated  reallocs.
    nodePtrCollection *ret = malloc((sizeof *ret) + (maxPtrs * sizeof(struct node *)));
    ret->nodeCount = 0;

    for (i = 0; i < t->nodes->nodeCount; i++)
    {
        if (t->nodes->nodes[i].sortPos == sortPos)
        {
            // we have a match
            tempNode = &t->nodes->nodes[i];

            // Check to see if there is room to save this node.
            if (ret->nodeCount == maxPtrs)
            {
                // We are at capacity.
                maxPtrs = (int)ceil(maxPtrs * 1.5);
                void *retCheck = realloc(ret, (sizeof *ret) + (maxPtrs * sizeof(struct node *)));
                if (!retCheck)
                    return NULL;
                ret = retCheck;
            }
            ret = addPtrNode(ret, tempNode);
        }
    }
    return ret;
}

void arrangeTrees(forest *f)
{
    /* position all of the trees within the forest. */
    /* Trees must be dimensioned before calling this function */
    // Create a list of generalised rectangles representing the individual trees which can be passed to a rectangle packing algorithm.
    int i, j;               // loop itterator;
    long double area = 0.0; // Total area of all rectangles
    double maxWidth = 0.0;  // maxmium width of rectangles
    double width = 0.0;     // target width for the result rectangle.
    if (f->treeCount == 0)
        return;

    rect *rects = malloc(f->treeCount * (sizeof *rects));

    for (i = 0; i < f->treeCount; i++)
    {
        (rects + i)->id = i; // Id of rectangle related to ordinal position of tree in the forest.
        (rects + i)->w = f->trees[i].width;
        (rects + i)->h = f->trees[i].height;
        //debug
        //printf("tree with width %f and height %f has area %f\n", f->trees[i].width, f->trees[i].height, f->trees[i].width * f->trees[i].height);
        area += (f->trees[i].width * f->trees[i].height); // calculate total area.
        if (f->trees[i].width > maxWidth)
            maxWidth = f->trees[i].width;
    }

    width = sqrt(area);
    if (width < maxWidth)
        width = maxWidth; // Cannot have a target rectangle narrower than the maximum width.

    if (width == 0)
    {
        fprintf(stderr, "Cannot arrange trees with total of zero width.");
        free(rects);
        exit(EXIT_FAILURE);
    }

    // Perform the rectangle packing
    FFDH(rects, f->treeCount, width);

    // Copy the results back into the trees
    for (i = 0; i < f->treeCount; i++)
    {
        for (j = 0; j < f->treeCount; j++)
        {
            // The order of the rects is altered in FFDH, however we still have the ordinal position of each tree
            // set as the ID of the rect, so we just need to find the correct rect for each tree now.
            if ((rects + j)->id == i)
            {
                // Correct rect found, write the data back to the tree.
                f->trees[i].posX = (rects + j)->x;
                f->trees[i].posY = (rects + j)->y;
            }
        }
    }

    free(rects);
}

void sortTree(enum sortMethods methods[], int count, tree *t)
{
    /* Sorts a tree according to one or more sorting methods 
    methods[] is an array of SortMethods (enum);
    count is the number of methods in the array
    *t is a pointer to the tree that contains the tree nodes.*/

    /* When each node is sorted it will be given a value of sortPos. If one node is sort position '2' and another node is sort position '3' then the node with sort 
    position '2' is sorted higher (further to the left on a simple tree).
    IMPORTANT: When multiple nodes are at the same sort position, the next position after that skips the number of nodes at the previous level that were at
    the same sort position. This allows successive passes of sorting to sort nodes that were at the same sortPos without having to reposition
    the nodes that were higher or lower. for example, assume these are all nodes:
    ElementId     |   SortPosition
    1             |   0
    2             |   1
    3             |   2
    4             |   2
    5             |   2
    6             |   5
    7             |   6
    Here 3, 4, and 5 were all given position 2 but the next node (6) was given position 5. 
    This means that in the next sorting pass nodes 3, 4, and 5 can be sorted against each other without affecting nodes 1, 2, 6, or 7.*/
    int i, x;         // loop counter;
    int grpStart = 0; // start position of the group of nodes at a given sort value.
    int grpLen = 0;   // count of nodes that are in the given sort group.
    int nextSortPos;
    int (*compar)(const void *, const void *); // qsort function pointer

    if (count < 1)
        return; // no sorts to complete.

    if (t->nodes->nodeCount < 2)
        return; // nothing to do.

    // Itterate through the sorting methods, applying them only to nodes of equal sort position each time.
    for (i = 0; i < count; i++)
    {
        // assign pointer to the correct compare functio
        switch (methods[i])
        {
        case descendants:
            compar = comparDescendantCount;
            break;
        case descendantsDesc:
            compar = comparDescendantCountDESC;
            break;
        case children:
            compar = comparChildrenCount;
            break;
        case childrenDesc:
            compar = comparChildrenCountDESC;
            break;
        case generations:
            compar = comparGenerationCount;
            break;
        case generationsDesc:
            compar = comparGenerationCountDESC;
            break;
        }

        // Table scan through the nodes and identify groups of nodes of the same sort position.
        grpStart = 0; // reset counter
        do
        {
            grpLen = 1; // number of nodes in group
            do
            {
                // Build the list of nodes with the same sort position
                if (t->nodes->nodes[grpStart].sortPos == t->nodes->nodes[grpStart + grpLen].sortPos)
                {
                    grpLen++;
                }
                else
                    break;
            } while (grpStart + grpLen < t->nodes->nodeCount);

            // List built
            if (grpLen > 1)
            {
                // We actually have a group. Sort by the desired method
                qsort(&(t->nodes->nodes[grpStart]), grpLen, sizeof(struct node), compar);
                nextSortPos = t->nodes->nodes[grpStart].sortPos + 1;
                for (x = grpStart + 1; x < grpStart + grpLen; x++)
                {
                    // Group is sorted, so now assigned correct sortPos value;
                    if (compar(&(t->nodes->nodes[x - 1]), &(t->nodes->nodes[x])) < 0)
                    {
                        // nodes are ascending
                        t->nodes->nodes[x].sortPos = nextSortPos;
                    }
                    else
                    {
                        // nodes are at the same level as each other
                        t->nodes->nodes[x].sortPos = t->nodes->nodes[x - 1].sortPos;
                    }
                    nextSortPos++;
                }
            }
            // move start for next itteration
            grpStart += grpLen;
        } while (grpStart < t->nodes->nodeCount - 1);
    }
}

double getCumulativeOffset(int id, tree *t, int depth, enum side side, double minOffset)
{
    /* Get the cumulative offsets running down either the left or right contour of a nodes children */
    /* Must be initialised before calling this function */
    // depth = how far down the respective tree we should drill.
    int i;      // loop itterator
    double ret; // return value
    struct node *thisNode = getNodeById(id, t);
    if (!thisNode)
        return 0.0;

    if (depth < 1)
        return thisNode->offsetToParent;

    nodePtrCollection *children = getChildren(id, t); // my children

    if (side == left)
    {
        // follow the left contour of the subtree
        // find the furthest left child that was deep enough
        for (i = 0; i < children->nodeCount; i++)
        {
            if (children->nodes[i]->numGenerations >= depth - 1)
            {
                // This tree is deep enough. Itteratively get the total offset.
                ret = getCumulativeOffset(children->nodes[i]->id, t, depth - 1, side, minOffset) + thisNode->offsetToParent;
                free(children);
                return ret;
            }
        }
    }
    else
    {
        // follow the right contour of the subtree
        // find the furthest right child that was deep enough
        for (i = children->nodeCount - 1; i >= 0; i--)
        {
            if (children->nodes[i]->numGenerations >= depth - 1)
            {
                // This tree is deep enough. Itteratively get the total offset.
                ret = getCumulativeOffset(children->nodes[i]->id, t, depth - 1, side, minOffset) + thisNode->offsetToParent;
                free(children);
                return ret;
            }
        }
    }
}

void calcOffsets(int id, tree *t, double offsetMin)
{
    /* used for creating tidy trees (Edward M. Reingold and John S. Tilford).
    Calculate the offset of each child respective to the parent (node represented by parameter id) .
    Nodes should be sorted before calling this function. Do not sort after this process is complete or you will need to
    reset the offsetSet flags and rerun it.     */
    int i = 0, x = 0, d = 0; // loop itterators
    int clumpDepthMax;       // Maximum depth of all children that are in the left 'clump'. see 'Generalisation to m-ary Trees and Forests in Tidier Drawings of Trees by Edward M. Reingold and John S. Tilford'
    double offsetMax;        // The maxmimum required offset at root computed as we itterate down two neighbouring subtrees
    int depthMax;            // maximum depth that will compare. We only need to compare the shortest of either the left (clump) or right child.
    int clumpChildId;        // id of the child from the clump being compared against the right child.
    double offsetInterval;   // Aggregated offsets between the selected clump child and all other children to the right of it within the clump. Explained better below under "Interval offsets".
    double offset;           // offset calculated while itterating.
    double offsetsTotal;     // Total of all child offsets.
    node *thisNode = getNodeById(id, t);
    if (thisNode->offsetSet == 1)
        return; // already initialised.

    nodePtrCollection *children = getChildren(id, t); // my children

    for (i = 0; i < children->nodeCount; i++)
    {
        if (children->nodes[i]->offsetSet == 1)
        {
            /*  This child has already had its offset set. 
                This means that this child is probably the child of a different node 
                which can happen if there is a loop (see test case 7).
                Within this function we should ignore children that have already been positioned
                as children of other nodes */
            if (i < children->nodeCount - 1)
                children->nodes[i] = children->nodes[i + 1];        // Conditionally shuffle the results back
            children->nodeCount--;      // unconditionally reduce the count of children.
        }
    }

    if (!children || children->nodeCount == 0)
    {
        free(children);
        return;
    }

    if (children->nodeCount == 1)
    {
        // Only one child so will not be offset in any way, but we need to show that it has been initialised.
        // Ensure that sub-children have their position set.
        calcOffsets(children->nodes[0]->id, t, offsetMin);
        children->nodes[0]->offsetToParent = 0;
        children->nodes[0]->offsetSet = 1;

        free(children);
        return;
    }

    double *offsets = malloc(sizeof *offsets * (children->nodeCount - 1));

    for (i = 0; i < children->nodeCount - 1; i++)
    {
        /*As we move from the first child through to the last child, all children that have been processed become the left tree 'clump' 
        (see 'Generalisation to m-ary Trees and Forests in Tidier Drawings of Trees by Edward M. Reingold and John S. Tilford')
        This clump then has to be compared to the next child (right tree) so that we get the lower depths of any child in the clump. 
        For example, if we have three children, first has depth 3, second has depth 2, and third has depth 3, then
        the first and second children get compared as far as depth 2 (max depth of second child) whereas when the third child
        is assessed, it is assessed against the 'clump' of the first child and second child and therefore has to be 
        assessed to depth 3 as both the first and third child have this depth. This is how 'threads' (see threaded binary trees) work with m-ary trees. */

        // variable 'i' represents the left hand child that we are comparing against i+1 which is the right hand child.

        clumpDepthMax = 0;
        offsetMax = 0;
        for (x = 0; x <= i; x++)
        {
            // Get maximum generation count for all children already processed.
            if (children->nodes[x]->numGenerations > clumpDepthMax)
            {
                clumpDepthMax = children->nodes[x]->numGenerations;
            }
        }
        depthMax = clumpDepthMax > children->nodes[i + 1]->numGenerations ? children->nodes[i + 1]->numGenerations : clumpDepthMax;

        // Ensure that the two child subtrees I am about to traverse are initialised.
        if (children->nodes[i]->offsetSet == 0)
        {
            // only hit on first pass when i == 0
            calcOffsets(children->nodes[i]->id, t, offsetMin);
        }

        if (children->nodes[i + 1]->offsetSet == 0)
        {
            // only hit on first pass when i == 0
            calcOffsets(children->nodes[i + 1]->id, t, offsetMin);
        }

        for (d = 0; d <= depthMax; d++)
        {
            /* d = depth.
            starting at the root of the two sub trees, drill down itteratively to the required depth.
            find the correct child from the 'clump' for the current depth.
            This is done by finding the furthest right branch of the left subtree that is deep enough (long enough) for the required depth.*/
            clumpChildId = 0;
            offsetInterval = 0.0;

            for (x = i; x >= 0; x--)
            {
                if (children->nodes[x]->numGenerations >= d)
                {
                    /* Get the first child from the left clump working right to left that is deep enough (has enough generations) 
                    to be compared to the current node of the right tree.
                    This is my implementation of the 'threading' solution required for tidier trees */
                    clumpChildId = children->nodes[x]->id;
                    break;
                }

                /*Interval offsets
                we are about to calculate the offset between children(I + 1) and the chosen ClumpChild, ClumpChild having an index less that I + 1.
                When we calcualte the offset we will arrive at the offset between children(I + 1) and the chosen ClumpChild but we actually need to record
                in children(I + 1) the offset between children(I + 1) and children(I). Therefore we need to subtract the aggregated offsets between
                children(I + 1) and the chosen ClumpChild so that the offsets are not 'reapplied'.*/
                offsetInterval += offsets[x];
            }

            if (clumpChildId == 0)
            {
                free(children);
                free(offsets);
                exit(EXIT_FAILURE);
            }
            /*Itterate through (walk) the right contour of the left tree (MyChildren(i)) and the left contour of the right tree (MyChildren(i+1)).
            Calculation below reads as CurrOffset = RightTree.LeftContour - LeftTree.RightContour*/
            offset = getCumulativeOffset(children->nodes[i + 1]->id, t, d, left, offsetMin) - getCumulativeOffset(clumpChildId, t, d, right, offsetMin);

            if (offsetMax < offsetMin - offset - offsetInterval)
            {
                offsetMax = offsetMin - offset - offsetInterval;
            }
        }
        offsets[i] = offsetMax;
    }

    // Update offsetToParent on all child nodes to accurate reflect their offset positioning relative to the parent. i.e. calculate their final values.

    /* Set all of the child offsets relative to this node. 
    We know that the first child will be offset (negatively) half of the total required offsets. For example, if the total required
    offsets was 1, then the first child would be offset -(1/2) = -0.5.*/
    offsetsTotal = 0;
    for (i = 0; i < children->nodeCount - 1; i++)
    {
        offsetsTotal += offsets[i];
    }

    for (i = 0; i < children->nodeCount; i++)
    {
        if (i == 0)
        {
            children->nodes[i]->offsetToParent = (offsetsTotal / 2.0) * -1;
        }
        else
        {
            children->nodes[i]->offsetToParent = children->nodes[i - 1]->offsetToParent + offsets[i - 1];
        }
    }
    thisNode->offsetSet = 1;
    free(offsets);
    free(children);
}

void setRelativePositions(int id, tree *t, double parentRelativeX)
{
    // set the position of the node relative to its parent.
    // part of a recursive call from offsetToRelative() used to set the relative position of all nodes in the tree.
    int i;                                  // loop itterator
    struct node *node = getNodeById(id, t); // current node
    nodePtrCollection *children;            // children of current node
    if (node)
    {
        node->posX = parentRelativeX + node->offsetToParent;
        node->posY = node->level;
        node->relativeSet=1;
        children = getChildren(id, t);

        if (children)
            for (i = 0; i < children->nodeCount; i++)
            {
                if (children->nodes[i]->relativeSet == 1)
                {
                /*  This child has already had its relative position set. 
                This means that this child is probably the child of a different node 
                which can happen if there is a loop (see test case 7).
                Within this function we should ignore children that have already been positioned
                as children of other nodes */
                    if (i < children->nodeCount - 1)
                        children->nodes[i] = children->nodes[i + 1]; // Conditionally shuffle the results back
                    children->nodeCount--;                           // unconditionally reduce the count of children.
                }
            }

        if (children && children->nodeCount > 0)
        {
            for (i = 0; i < children->nodeCount; i++)
            {
                setRelativePositions(children->nodes[i]->id, t, node->posX);
            }
        }
    }
    free(children);
}

void offsetToRelative(tree *t)
{
    /* The offsetToParent property of the nodes has been set. 
    now aggregate those offsets to get a position relative to the tree root. */
    int root = rootId(t);
    int i; // loop itterator
    double minOffset = 0.0;
    setRelativePositions(root, t, 0.0); // Set positions of all tree nodes relative to the root. Some nodes will have negative position.
    // Correct position of all nodes to remove any negative position.
    if (t->nodes->nodeCount > 0)
    {
        for (i = 0; i < t->nodes->nodeCount; i++)
        {
            if (t->nodes->nodes[i].posX < minOffset)
                minOffset = t->nodes->nodes[i].posX; // record minimum value of posX
        }

        if (minOffset != 0)
        {
            // adjust left position of all nodes to remove negative offset.
            for (i = 0; i < t->nodes->nodeCount; i++)
            {
                t->nodes->nodes[i].posX -= minOffset;
            }
        }
    }
}

void sizeTree(tree *t, double padding[4])
{
    // calculate the size of the tree from the position of the nodes, and apply padding.
    // Padding array is defined with padding[0] at the 12 o'clock position, rotating clockwise with padding[3] at the 9 o'clock position.
    double minX, minY, maxX, maxY; // Extents of nodes in the tree;
    int i;                         // loop itterator.
    if (t->nodes->nodeCount > 0)
    {
        for (i = 0; i < t->nodes->nodeCount; i++)
        {
            t->nodes->nodes[i].posY += padding[0]; // apply top pad.
            t->nodes->nodes[i].posX += padding[3]; // apply left pad.
            if (i == 0)
            {
                // first node, use this to initialise values
                minX = t->nodes->nodes[0].posX;
                maxX = minX + t->nodes->nodes[0].w;
                minY = t->nodes->nodes[0].posY;
                maxY = minY + t->nodes->nodes[0].h;
            }
            else
            {
                if (t->nodes->nodes[i].posX < minX)
                    minX = t->nodes->nodes[i].posX;
                if (t->nodes->nodes[i].posX + t->nodes->nodes[i].w > maxX)
                    maxX = t->nodes->nodes[i].posX + t->nodes->nodes[i].w;
                if (t->nodes->nodes[i].posY < minY)
                    minY = t->nodes->nodes[i].posY;
                if (t->nodes->nodes[i].posY + t->nodes->nodes[i].h > maxY)
                    maxY = t->nodes->nodes[i].posY + t->nodes->nodes[i].h;
            }
        }
        t->height = maxY - minY;
        t->height += padding[2] + padding[0];
        t->width = maxX - minX;
        t->width += padding[1] + padding[3];
    }
}

/**
 * position the nodes within the tree and calculate dimension limits.
 * @param [in]  t       The tree
 * @param [in] methods  the sort methods for the tree
 * @param [in] methodCount  the number of sort methods passed in
 * @param [in] nodeX        space between nodes on the x axis.
 * @param [in] nodeY        space between nodes on the y axis.
 * @param [in] padding      four element array declaring internal padding of the tree. array elements give padding north, east, south, west.
 * */
void layoutTree(tree *t, enum sortMethods methods[], int methodCount, double nodeX, double nodeY, double padding[4])
{
    // Wrapper to execute all layout functions for a given tree.
    int root; // root node id
    int i;    // loop itterator
    setLevels(t);
    root = rootId(t);
    if (root > -1)
    {
        calcStats(root, t);
        sortTree(methods, methodCount, t);
        calcOffsets(root, t, 1.0);
        offsetToRelative(t);
        for (i = 0; i < t->nodes->nodeCount; i++)
        {
            /* Apply the desired positioning to each node in the tree. Nodes have unit size (1.0) at this point.
                The strategy i am using here works, but is actually wrong. Each node should be positioned relative
                to the left of it and above it. So each node should not take its own height and width, but the
                height of the node above it and the width of the node to the left of it.
                That is a problem for another day and as all nodes are currently the same width and height I 
                don't need to worry about it. 
                TODO: Try to implement the proper approach as explained above.
            */
            t->nodes->nodes[i].posX *= (nodeX + t->nodes->nodes[i].w);
            t->nodes->nodes[i].posY *= (nodeY + t->nodes->nodes[i].h);
        }
        sizeTree(t, padding);
    }
}

void layoutForest(forest *f, enum sortMethods methods[], int methodCount, double nodeX, double nodeY, double padding[4])
{
    // Layout all trees in the forest.
    // methods[] is an array of sort methods
    // methodCount is the number of elements in the array
    // nodeX is the internode spacing on the X axis
    // nodeY is the internode spacing on the Y axis
    // padding is the padding assigned to each tree. Array elements start at 12 o'clock position and move in a clockwise direction such that padding[3] is the 9 o'clock position.
    int i; // loop itterator
    if (f->treeCount == 0)
        return;

    for (i = 0; i < f->treeCount; i++)
    {
        // layout each tree
        layoutTree(&(f->trees[i]), methods, methodCount, nodeX, nodeY, padding);
    }
    arrangeTrees(f);
}

void printTree(tree *t)
{
    /* print test data for a tree */
    int i; /* loop itterator */
    int levelMin, levelMax;
    int generations;
    int errCount; // error counter
    if (!t)
    {
        printf("Tree pointer invalid");
        return;
    }

    if (!t->nodes)
    {
        printf("Node collection invalid");
        return;
    }

    if (t->nodes->nodeCount == 0)
    {
        printf("No nodes found in tree");
        return;
    }

    printf("Node ID   Level     SortPos     offset  posX    posY     Children   Descendants  Generations   Children   Descendants  Generations     Check\n");
    printf("                                                         (demand)   (demand)     (demand)      (stats)    (stats)      (stats)\n");
    for (i = 0; i < t->nodes->nodeCount; i++)
    {
        errCount = 0; // reset
        nodePtrCollection *children = getChildren(t->nodes->nodes[i].id, t);
        nodePtrCollection *descendants = getDescendants(t->nodes->nodes[i].id, t);
        generations = generationCount(t->nodes->nodes[i].id, t);
        printf("%-10i", t->nodes->nodes[i].id);
        printf("%-10i", t->nodes->nodes[i].level);
        printf("%-12i", t->nodes->nodes[i].sortPos);
        printf("%-8.2f", t->nodes->nodes[i].offsetToParent);
        printf("%-8.2f", t->nodes->nodes[i].posX);
        printf("%-9.2f", t->nodes->nodes[i].posY);
        printf("%-11i", (children) ? children->nodeCount : 0);
        printf("%-13i", (descendants) ? descendants->nodeCount : 0);
        printf("%-14i", generations);
        printf("%-11i", t->nodes->nodes[i].numChildren);
        printf("%-13i", t->nodes->nodes[i].numDescendants);
        printf("%-16i", t->nodes->nodes[i].numGenerations);

        // check for errors
        if (children && children->nodeCount != t->nodes->nodes[i].numChildren)
            errCount++;

        if (descendants && descendants->nodeCount != t->nodes->nodes[i].numDescendants)
            errCount++;

        if (generations != t->nodes->nodes[i].numGenerations)
            errCount++;

        if (t->nodes->nodes[i].id < 0)
            errCount++;

        if (errCount == 0)
        {
            printf("OK");
        }
        else
        {
            printf("ERROR!");
        }

        printf("\n");

        if (i == 0)
        {
            // Initialisation
            levelMin = t->nodes->nodes[i].level;
            levelMax = levelMin;
        }
        else
        {
            if (t->nodes->nodes[i].level < levelMin)
                levelMin = t->nodes->nodes[i].level;

            if (t->nodes->nodes[i].level > levelMax)
                levelMax = t->nodes->nodes[i].level;
        }

        free(children);
        free(descendants);
    }

    printf("\n");

    for (i = levelMin; i <= levelMax; i++)
    {
        nodePtrCollection *nodes = getNodesAtDepth(i, t);
        printf("Nodes at depth %i: %i\n", i, nodes->nodeCount);
        free(nodes);
    }

    printf("tree width: %.2f and height: %.2f\n", t->width, t->height);

    printf("\n");
}