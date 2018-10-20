/**CFile***********************************************************************

  FileName    [mtrBasic.c]

  PackageName [mtr]

  Synopsis    [Basic manipulation of multiway branching trees.]

  Description [External procedures included in this module:
	    <ul>
	    <li> Mtr_AllocNode()
	    <li> Mtr_DeallocNode()
	    <li> Mtr_InitTree()
	    <li> Mtr_FreeTree()
	    <li> Mtr_CopyTree()
	    <li> Mtr_MakeFirstChild()
	    <li> Mtr_MakeLastChild()
	    <li> Mtr_CreateFirstChild()
	    <li> Mtr_CreateLastChild()
	    <li> Mtr_MakeNextSibling()
	    <li> Mtr_PrintTree()
	    </ul>
	    ]

  SeeAlso     [cudd package]

  Author      [Fabio Somenzi]

  Copyright   [This file was created at the University of Colorado at
  Boulder.  The University of Colorado at Boulder makes no warranty
  about the suitability of this software for any purpose.  It is
  presented on an AS IS basis.]

******************************************************************************/

#include "util.h"
#include "mtrInt.h"


/*---------------------------------------------------------------------------*/
/* Constant declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Stucture declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Type declarations                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variable declarations                                                     */
/*---------------------------------------------------------------------------*/

#ifndef lint
static char rcsid[] MTR_UNUSED = "$Id: mtrBasic.c,v 1.1.1.1 2003/02/24 22:24:02 wjiang Exp $";
#endif

/*---------------------------------------------------------------------------*/
/* Macro declarations                                                        */
/*---------------------------------------------------------------------------*/

/**AutomaticStart*************************************************************/

/*---------------------------------------------------------------------------*/
/* Static function prototypes                                                */
/*---------------------------------------------------------------------------*/


/**AutomaticEnd***************************************************************/


/*---------------------------------------------------------------------------*/
/* Definition of exported functions                                          */
/*---------------------------------------------------------------------------*/

/**Function********************************************************************

  Synopsis    [Allocates new tree node.]

  Description [Allocates new tree node. Returns pointer to node.]

  SideEffects [None]

  SeeAlso     [Mtr_DeallocNode]

******************************************************************************/
MtrNode *
Mtr_AllocNode(
   )
{
    MtrNode *node;

    node = ALLOC(MtrNode,1);
    return node;

} /* Mtr_AllocNode */


/**Function********************************************************************

  Synopsis    [Deallocates tree node.]

  Description []

  SideEffects [None]

  SeeAlso     [Mtr_AllocNode]

******************************************************************************/
void
Mtr_DeallocNode(
  MtrNode * node /* node to be deallocated */)
{
    FREE(node);
    return;

} /* end of Mtr_DeallocNode */


/**Function********************************************************************

  Synopsis    [Initializes tree with one node.]

  Description [Initializes tree with one node. Returns pointer to node.]

  SideEffects [None]

  SeeAlso     [Mtr_FreeTree Mtr_InitGroupTree]

******************************************************************************/
MtrNode *
Mtr_InitTree(
   )
{
    MtrNode *node;

    node = Mtr_AllocNode();
    if (node == NULL) return(NULL);

    node->parent = node->child = node->elder = node->younger = NULL;
    node->flags = 0;

    return(node);

} /* end of Mtr_InitTree */


/**Function********************************************************************

  Synopsis    [Disposes of tree rooted at node.]

  Description []

  SideEffects [None]

  SeeAlso     [Mtr_InitTree]

******************************************************************************/
void
Mtr_FreeTree(
  MtrNode * node)
{
    if (node == NULL) return;
    if (! MTR_TEST(node,MTR_TERMINAL)) Mtr_FreeTree(node->child);
    Mtr_FreeTree(node->younger);
    Mtr_DeallocNode(node);
    return;

} /* end of Mtr_FreeTree */


/**Function********************************************************************

  Synopsis    [Makes a copy of tree.]

  Description [Makes a copy of tree. If parameter expansion is greater
  than 1, it will expand the tree by that factor. It is an error for
  expansion to be less than 1. Returns a pointer to the copy if
  successful; NULL otherwise.]

  SideEffects [None]

  SeeAlso     [Mtr_InitTree]

******************************************************************************/
MtrNode *
Mtr_CopyTree(
  MtrNode * node,
  int  expansion)
{
    MtrNode *copy;

    if (node == NULL) return(NULL);
    if (expansion < 1) return(NULL);
    copy = Mtr_AllocNode();
    if (copy == NULL) return(NULL);
    copy->parent = copy->elder = copy->child = copy->younger = NULL;
    if (node->child != NULL) {
	copy->child = Mtr_CopyTree(node->child, expansion);
	if (copy->child == NULL) {
	    Mtr_DeallocNode(copy);
	    return(NULL);
	}
    }
    if (node->younger != NULL) {
	copy->younger = Mtr_CopyTree(node->younger, expansion);
	if (copy->younger == NULL) {
	    Mtr_FreeTree(copy);
	    return(NULL);
	}
    }
    copy->flags = node->flags;
    copy->low = node->low * expansion;
    copy->size = node->size * expansion;
    copy->index = node->index * expansion;
    if (copy->younger) copy->younger->elder = copy;
    if (copy->child) {
	MtrNode *auxnode = copy->child;
	while (auxnode != NULL) {
	    auxnode->parent = copy;
	    auxnode = auxnode->younger;
	}
    }
    return(copy);
    
} /* end of Mtr_CopyTree */


/**Function********************************************************************

  Synopsis    [Makes child the first child of parent.]

  Description []

  SideEffects [None]

  SeeAlso     [Mtr_MakeLastChild Mtr_CreateFirstChild]

******************************************************************************/
void
Mtr_MakeFirstChild(
  MtrNode * parent,
  MtrNode * child)
{
    child->parent = parent;
    child->younger = parent->child;
    child->elder = NULL;
    if (parent->child != NULL) {
#ifdef MTR_DEBUG
	assert(parent->child->elder == NULL);
#endif
	parent->child->elder = child;
    }
    parent->child = child;
    return;

} /* end of Mtr_MakeFirstChild */


/**Function********************************************************************

  Synopsis    [Makes child the last child of parent.]

  Description []

  SideEffects [None]

  SeeAlso     [Mtr_MakeFirstChild Mtr_CreateLastChild]

******************************************************************************/
void
Mtr_MakeLastChild(
  MtrNode * parent,
  MtrNode * child)
{
    MtrNode *node;

    child->younger = NULL;

    if (parent->child == NULL) {
	parent->child = child;
	child->elder = NULL;
    } else {
	for (node = parent->child;
	     node->younger != NULL;
	     node = node->younger);
	node->younger = child;
	child->elder = node;
    }
    child->parent = parent;
    return;

} /* end of Mtr_MakeLastChild */


/**Function********************************************************************

  Synopsis    [Creates a new node and makes it the first child of parent.]

  Description [Creates a new node and makes it the first child of
  parent. Returns pointer to new child.]

  SideEffects [None]

  SeeAlso     [Mtr_MakeFirstChild Mtr_CreateLastChild]

******************************************************************************/
MtrNode *
Mtr_CreateFirstChild(
  MtrNode * parent)
{
    MtrNode *child;

    child = Mtr_AllocNode();
    if (child == NULL) return(NULL);

    child->child = child->younger = child-> elder = NULL;
    child->flags = 0;
    Mtr_MakeFirstChild(parent,child);
    return(child);

} /* end of Mtr_CreateFirstChild */


/**Function********************************************************************

  Synopsis    [Creates a new node and makes it the last child of parent.]

  Description [Creates a new node and makes it the last child of parent.
  Returns pointer to new child.]

  SideEffects [None]

  SeeAlso     [Mtr_MakeLastChild Mtr_CreateFirstChild]

******************************************************************************/
MtrNode *
Mtr_CreateLastChild(
  MtrNode * parent)
{
    MtrNode *child;

    child = Mtr_AllocNode();
    if (child == NULL) return(NULL);

    child->child = child->younger = child->elder = NULL;
    child->flags = 0;
    Mtr_MakeLastChild(parent,child);
    return(child);

} /* end of Mtr_CreateLastChild */


/**Function********************************************************************

  Synopsis    [Makes second the next sibling of first.]

  Description [Makes second the next sibling of first. Second becomes a
  child of the parent of first.]

  SideEffects [None]

  SeeAlso     []

******************************************************************************/
void
Mtr_MakeNextSibling(
  MtrNode * first,
  MtrNode * second)
{
    second->younger = first->younger;
    if (first->younger != NULL) {
	first->younger->elder = second;
    }
    second->parent = first->parent;
    first->younger = second;
    second->elder = first;
    return;

} /* end of Mtr_MakeNextSibling */


/**Function********************************************************************

  Synopsis    [Prints a tree, one node per line.]

  Description []

  SideEffects [None]

  SeeAlso     [Mtr_PrintGroups]

******************************************************************************/
void
Mtr_PrintTree(
  MtrNode * node)
{
    if (node == NULL) return;
    (void) fprintf(stdout,
#if SIZEOF_VOID_P == 8
    "N=0x%-8lx C=0x%-8lx Y=0x%-8lx E=0x%-8lx P=0x%-8lx F=%x L=%d S=%d\n",
    (unsigned long) node, (unsigned long) node->child,
    (unsigned long) node->younger, (unsigned long) node->elder,
    (unsigned long) node->parent, node->flags, node->low, node->size);
#else
    "N=0x%-8x C=0x%-8x Y=0x%-8x E=0x%-8x P=0x%-8x F=%x L=%d S=%d\n",
    (unsigned) node, (unsigned) node->child,
    (unsigned) node->younger, (unsigned) node->elder,
    (unsigned) node->parent, node->flags, node->low, node->size);
#endif
    if (!MTR_TEST(node,MTR_TERMINAL)) Mtr_PrintTree(node->child);
    Mtr_PrintTree(node->younger);
    return;

} /* end of Mtr_PrintTree */

/*---------------------------------------------------------------------------*/
/* Definition of internal functions                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Definition of static functions                                            */
/*---------------------------------------------------------------------------*/

