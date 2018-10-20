/**CFile****************************************************************

  FileName    [mntkLogic.c]

  PackageName [MVSIS 1.3: Multi-valued logic synthesis system.]

  Synopsis    [Generic mapped network.]

  Author      [MVSIS Group]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - September 8, 2003.]

  Revision    [$Id: mntkLogic.c,v 1.1 2005/02/28 05:34:31 alanmi Exp $]

***********************************************************************/

#include "mntkInt.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static int  Mntk_CollectRelatedLogicInternal( Mntk_Node_t * pNode, float DelayTop, float DelayBot, int fUseTfiOnly, int fCollectUsed, int Limit );
static int  Mntk_CollectTfi_rec( Mntk_NodeVec_t * vVisited, Mntk_Node_t * pNode, float DelayBot );
static void Mntk_CollectTfo( Mntk_NodeVec_t * vQueue, Mntk_Node_t * pPivot, float DelayTop );
static void Mntk_MarkTfo_rec( Mntk_NodeVec_t * vVisited, Mntk_Node_t * pNode, float DelayTop );
static void Mntk_CollectVerifyArrays( Mntk_Man_t * p );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFITIONS                           ///
////////////////////////////////////////////////////////////////////////


/**Function*************************************************************

  Synopsis    [Top-level procedure to compute the candidates for the given node.]

  Description [This procedure returns three arrays of nodes:
  Array p->vWinCands contains the internal nodes of the neighborhood.
  Array p->vWinNodes contains the AND nodes used to set up the SAT problem.
  Array p->vWinTotal contains the all the nodes, including the PI of the neighborhood.]
  
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mntk_CollectRelatedLogic( Mntk_Node_t * pNode )
{
    float DelayUnit;   // the time unit
    float DelayTop;    // the arrival time to start collecting candidates
    float DelayBot;    // the arrival time to stop collecting candidates
    int fUseTfiOnly;   // set to 1 to use only TFI nodes
    int fCollectUsed;  // set to 1 to collect only nodes used in the mapping
    int nLimit;        // the max number of candidates
    int fFinalRun;     // is set to 1 if the window cannot be extended
    int nIter;

    // set the parameters
    DelayUnit = pNode->p->tDelayInv.Worst;
    if ( !pNode->p->fDoingArea )
    {
        DelayTop     = pNode->tArrival.Worst     -  DelayUnit * 1;
        DelayBot     = pNode->tArrival.Worst     -  DelayUnit * pNode->p->ConeDepth;

        fUseTfiOnly  = 0; 
        fCollectUsed = 0;
        nLimit       = pNode->p->ConeSize; 
    }
    else
    {
        DelayTop     = pNode->tRequired[1].Worst -  DelayUnit * 1;
        DelayBot     = pNode->tArrival.Worst     -  DelayUnit * pNode->p->ConeDepth;

        fUseTfiOnly  = 0; 
        fCollectUsed = 1;
        nLimit       = pNode->p->ConeSize; 
    }

    // if the global cones are requested, collect everything
    if ( pNode->p->fGlobalCones )
    {
        DelayBot     = 0;
        nLimit       = 1000000; 
    }

    // increase the window size to include more nodes, if possible
    nIter = 0;
    do
    {
        nIter++;
        fFinalRun = Mntk_CollectRelatedLogicInternal( pNode, DelayTop, DelayBot, fUseTfiOnly, fCollectUsed, nLimit );
        DelayBot -= DelayUnit;
    }
    while ( Mntk_NodeVecReadSize(pNode->p->vWinCands) < nLimit && !fFinalRun );

    // reduce the window size to simplify the problem, if needed
    while ( Mntk_NodeVecReadSize(pNode->p->vWinTotal) > nLimit + 20 )
    {
        DelayBot += DelayUnit;
        Mntk_CollectRelatedLogicInternal( pNode, DelayTop, DelayBot, fUseTfiOnly, fCollectUsed, nLimit );
    }

    // if the number of nodes is more than the limit, move them to the fanin array
    Mntk_CollectVerifyArrays( pNode->p );
}

/**Function*************************************************************

  Synopsis    [Collects the cones for the given delay margins.]

  Description [Returns 1 if it is known that the run is final.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Mntk_CollectRelatedLogicInternal( Mntk_Node_t * pNode, 
    float DelayTop, float DelayBot, int fUseTfiOnly, int fCollectUsed, int Limit )
{
    Mntk_NodeVec_t * vWinTotal = pNode->p->vWinTotal;
    Mntk_NodeVec_t * vWinCands = pNode->p->vWinCands;
    Mntk_NodeVec_t * vWinNodes = pNode->p->vWinNodes;
    Mntk_Node_t * pCand;
    int i, fFinalRun;

    // the node should be references and not a PI
    assert( pNode->nRefs[0] + pNode->nRefs[1] > 0 );
    assert( !Mntk_NodeIsVar(pNode) );

    // clean the arrays
    Mntk_NodeVecClear( vWinTotal  );  // the nodes
    Mntk_NodeVecClear( vWinCands  );  // candidate nodes
    Mntk_NodeVecClear( vWinNodes  );  // AND-nodes used for SAT

    // collect and mark TFI nodes whose arrival is between DelayTop and DelayBot
    // all the nodes are collected in p->vWinTotal
    // the nodes inside the area of interest have delays [DelayTop,DelayBot]
    // the nodes outside the area of interest have delays below DelayBot
    fFinalRun = Mntk_CollectTfi_rec( vWinTotal, pNode, DelayBot );
    assert( vWinTotal->pArray[0] == pNode );

    // sort in the increasing order by the arrival times
    Mntk_NodeVecSortByArrival( vWinTotal, 1 );
    for ( i = 1; i < vWinTotal->nSize; i++ )
        assert( vWinTotal->pArray[i-1]->tArrival.Worst <= vWinTotal->pArray[i]->tArrival.Worst );

    // collect the transitive fanout of these nodes up to DelayTop
    if ( !fUseTfiOnly )
        Mntk_CollectTfo( vWinTotal, pNode, DelayTop );


    // collect the candidates (whose delay is below DelayTop and who are not fanins)
    // also, sort all the collected nodes into two types: internal nodes and fanins
    // mark the node and fanins
    pNode->fUsed = 1;
    for ( i = 0; i < (int)pNode->nLeaves; i++ )
        Mntk_Regular(pNode->ppLeaves[i])->fUsed = 1;
    // process all nodes
    for ( i = 0; i < vWinTotal->nSize; i++ )
    {
        pCand = vWinTotal->pArray[i];
        pCand->fMark0 = 0;
        // the node is a fanin if it is a PI or its delay is below
        if ( !Mntk_NodeIsVar(pCand) && pCand->tArrival.Worst >= DelayBot )
            Mntk_NodeVecPush( vWinNodes, pCand );
        // save the candidate
        if ( !pCand->fUsed && pCand->tArrival.Worst <= DelayTop && 
             (Mntk_NodeReadRefTotal(pCand) > 0 || !fCollectUsed) )
            Mntk_NodeVecPush( vWinCands, pCand );
    }
    // unmark the node and fanins
    pNode->fUsed = 0;
    for ( i = 0; i < (int)pNode->nLeaves; i++ )
        Mntk_Regular(pNode->ppLeaves[i])->fUsed = 0;

    return fFinalRun;
}

/**Function*************************************************************

  Synopsis    [Collects and marks all the relevant nodes in the TFI.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Mntk_CollectTfi_rec( Mntk_NodeVec_t * vVisited, Mntk_Node_t * pNode, float DelayBot )
{
    int i, RetValue = 1;
    // skip the collected node
    assert( !Mntk_IsComplement(pNode) );
    if ( pNode->fMark0 )
        return 1;
    // collect the node
    pNode->fMark0 = 1;
    Mntk_NodeVecPush( vVisited, pNode );
    // do not pursue the node if it is a PI or if it arrives too early
    if ( Mntk_NodeIsVar(pNode) || pNode->tArrival.Worst < DelayBot )
        return Mntk_NodeIsVar(pNode);
    // collect the fanins
    for ( i = 0; i < (int)pNode->nLeaves; i++ )
        RetValue *= Mntk_CollectTfi_rec( vVisited, Mntk_Regular(pNode->ppLeaves[i]), DelayBot );
    return RetValue;
}


/**Function*************************************************************

  Synopsis    [Add the nodes reachable in the TFO up to the given level.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mntk_CollectTfo( Mntk_NodeVec_t * vQueue, Mntk_Node_t * pPivot, float DelayTop )
{
    Mntk_NodeVec_t * vWinVisit = pPivot->p->vWinVisit;
    Mntk_Node_t * pNode, * pFanout;
    int i, k, j;

    // mark the TFO of the node
    Mntk_NodeVecClear( vWinVisit );
    Mntk_MarkTfo_rec( vWinVisit, pPivot, DelayTop );

    // add the involved fanout nodes
    for ( i = 0; i < vQueue->nSize; i++ ) 
    {
        pNode = vQueue->pArray[i];
        // go through the fanouts
        for ( j = 0; j < pNode->vFanouts->nSize; j++ )
        {
            pFanout = pNode->vFanouts->pArray[j];
            // skip the fanout if
            // - it is already collected (fMark0)
            // - it is in the TFO of the node (fMark1)
            // - its arrival time is above the threshold
            if ( pFanout->fMark0 || pFanout->fMark1 || pFanout->tArrival.Worst > DelayTop )
                continue;
            // check whether all the fanins of this fanout belong to the neighborhood
            for ( k = 0; k < (int)pFanout->nLeaves; k++ )
                if ( !Mntk_Regular(pFanout->ppLeaves[k])->fMark0 )
                    break;
            if ( k == (int)pFanout->nLeaves )
            {
                pFanout->fMark0 = 1;
                Mntk_NodeVecPushOrder( vQueue, pFanout, 1 );
            }
        }
    }
    // unmark the TFO
    for ( i = 0; i < vWinVisit->nSize; i++ )
        vWinVisit->pArray[i]->fMark1 = 0;
}

/**Function*************************************************************

  Synopsis    [Mark the TFO of the node.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mntk_MarkTfo_rec( Mntk_NodeVec_t * vVisited, Mntk_Node_t * pNode, float DelayTop )
{
    Mntk_Node_t * pFanout;
    int i;
    // skip the collected node
    assert( !Mntk_IsComplement(pNode) );
    if ( pNode->fMark1 )
        return;
    // collect the node
    pNode->fMark1 = 1;
    Mntk_NodeVecPush( vVisited, pNode );
    // do not pursue the node if it is a PI or if it arrives too early
    if ( pNode->tArrival.Worst >= DelayTop )
        return;
    // visit the fanouts
    if ( pNode->vFanouts )
    for ( i = 0; i < pNode->vFanouts->nSize; i++ )
    {
        pFanout = pNode->vFanouts->pArray[i];
        Mntk_MarkTfo_rec( vVisited, pFanout, DelayTop );
    }
}

/**Function*************************************************************

  Synopsis    [Verifies the arrays.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Mntk_CollectVerifyArrays( Mntk_Man_t * p )
{
    Mntk_Node_t * pNode;
    int i, k;
    // mark all the nodes
    for ( i = 0; i < p->vWinTotal->nSize; i++ )
        p->vWinTotal->pArray[i]->fMark0 = 1;
    // check internal nodes
    for ( i = 0; i < p->vWinNodes->nSize; i++ )
    {
        pNode = p->vWinNodes->pArray[i];
        assert( pNode->fMark0 );
        for ( k = 0; k < (int)pNode->nLeaves; k++ )
            assert( Mntk_Regular(pNode->ppLeaves[k])->fMark0 );
    }
    // unmark all the nodes
    for ( i = 0; i < p->vWinTotal->nSize; i++ )
        p->vWinTotal->pArray[i]->fMark0 = 0;
}


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


