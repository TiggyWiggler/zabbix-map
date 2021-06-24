#ifndef FORESTS_H
#define FORESTS_H

enum sortMethods
{
    descendants,
    descendantsDesc,
    children,
    childrenDesc,
    generations,
    generationsDesc
};

struct node
{
    /* a single treenode */
    int id;                /* Identifier of the node. Must be unique within a given collection */
    double posX;           /* Either relative or absolute position on the canvas. */
    double posY;           /* Either relative or absolute position on the canvas. */
    int level;             /* Depth from root node */
    int sortPos;           /* Sort position relative to siblings */
    double offsetToParent; /* y axis position relative to parent node used during layout calculations */
    _Bool offsetSet;       /* offsetToParent has been populated. offsetToParent can be anything so no way to know by looking at it. */
    int numDescendants;    /* statistic: number of descendants below this node at all levels (children, grandchildren, etc.) */
    int numChildren;       /* statistic: number of children attached to this node. These are linked nodes at +1 level compared to this node */
    int numGenerations;    /* statistic: number of generations beneath this node. 1 if there are just children, 2 if there are grandchildren, 3... etc. */
};

typedef struct node node;

struct nodeLink
{
    /* Link between two nodes */
    int id1;
    int id2;
};

typedef struct nodeLink nodeLink;

struct nodeCollection
{
    int nodeCount; /* number of nodes in the nodes collection */
    node nodes[];  /* nodes within the tree */
};

typedef struct nodeCollection nodeCollection;


struct nodeLinkCollection
{
    int linkCount;    /* number of node links in the collection */
    nodeLink links[]; /* node links */
};

typedef struct nodeLinkCollection nodeLinkCollection;

struct tree
{
    double width;
    double height;
    /* a tree contains many nodes (tree nodes) plus their interconnections */
    struct nodeCollection *nodes;
    struct nodeLinkCollection *links;
    double posX;           /* origin position on the canvas */
    double posY;           /* origin position on the canvas */
};

typedef struct tree tree;

struct forest
{
    /* a forest is a collection of trees, trees contain tree nodes (node), and those nodes are connected by links (nodeLink) */
    int treeCount;
    tree trees[];
};

typedef struct forest forest;

// Exposed so that external code can create forests for processing.
node createNode(int id);
nodeCollection *addNode(nodeCollection *nodes, node node);
nodeLinkCollection *linkNodes(nodeLinkCollection *links, int id1, int id2);
forest *addTree(forest *f, tree t);
tree createTree();
node *getNodeByIdFromTree(int id, tree *t);
node *getNodeByIdFromForest(int id, forest *f);
int getConnectedNodeCount(int rootId, tree *t);
void layoutForest(forest *f, enum sortMethods methods[], int methodCount, double nodeX, double nodeY, double padding[4]);
void printTree(tree *t);
void freeForest(forest *f);
#endif