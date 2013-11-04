/* This file is part of the Random Ball Cover (RBC) library.
 * (C) Copyright 2010, Lawrence Cayton [lcayton@tuebingen.mpg.de]
 */

#ifndef RBC_CU
#define RBC_CU

#include<sys/time.h>
#include<stdio.h>
//#include<cuda.h>
#include "utils.h"
#include "defs.h"
#include "utilsGPU.h"
#include "rbc.h"
#include "kernels.h"
#include "kernelWrap.h"
#include "sKernelWrap.h"

/* NEVER USED
void queryRBC(const matrix q, const rbcStruct rbcS, unint *NNs, real* NNdists){
  unint m = q.r;
  unint numReps = rbcS.dr.r;
  unint compLength;
  compPlan dcP;
  unint *qMap, *dqMap;
  qMap = (unint*)calloc(PAD(m+(BLOCK_SIZE-1)*PAD(numReps)),sizeof(*qMap));
  matrix dq;
  copyAndMove(&dq, &q);
  
  charMatrix cM;
  cM.r=cM.c=numReps; cM.pr=cM.pc=cM.ld=PAD(numReps);
  cM.mat = (char*)calloc( cM.pr*cM.pc, sizeof(*cM.mat) );
  
  unint *repIDsQ;
  repIDsQ = (unint*)calloc( m, sizeof(*repIDsQ) );
  real *distToRepsQ;
  distToRepsQ = (real*)calloc( m, sizeof(*distToRepsQ) );
  unint *groupCountQ;
  groupCountQ = (unint*)calloc( PAD(numReps), sizeof(*groupCountQ) );
  
  computeReps(dq, rbcS.dr, repIDsQ, distToRepsQ);

  //How many points are assigned to each group?
  computeCounts(repIDsQ, m, groupCountQ);
  
  //Set up the mapping from groups to queries (qMap).
  buildQMap(q, qMap, repIDsQ, numReps, &compLength);
  
  // Setup the computation matrix.  Currently, the computation matrix is 
  // just the identity matrix: each query assigned to a particular 
  // representative is compared only to that representative's points.  
  idIntersection(cM);

  initCompPlan(&dcP, cM, groupCountQ, rbcS.groupCount, numReps);

  checkErr( cudaMalloc( (void**)&dqMap, compLength*sizeof(*dqMap) ) );
  cudaMemcpy( dqMap, qMap, compLength*sizeof(*dqMap), cudaMemcpyHostToDevice );
  
  computeNNs(rbcS.dx, rbcS.dxMap, dq, dqMap, dcP, NNs, NNdists, compLength);
  
  free(qMap);
  cudaFree(dqMap);
  freeCompPlan(&dcP);
  cudaFree(dq.mat);
  free(cM.mat);
  free(repIDsQ);
  free(distToRepsQ);
  free(groupCountQ);
}*/

/** This function is very similar to queryRBC, with a couple of basic changes to handle
  * k-nn.
  * q - query
  * rbcS - rbc structure
  * NNs - output matrix of indexes
  * NNdists - output matrix of distances
  */
void kqueryRBC(const matrix q, const ocl_rbcStruct rbcS, intMatrix NNs, matrix NNdists){
  unint m = q.r;
  unint numReps = rbcS.dr.r;
  unint compLength;
  ocl_compPlan dcP;
  //unint *qMap, *dqMap;
  unint *qMap;
  cl::Buffer dqMap;

  device_matrix_to_file(rbcS.dr, "rbcS_dr.txt");
  device_matrix_to_file(rbcS.dx, "rbcS_dx.txt");
  device_matrix_to_file(rbcS.dxMap, "rbcS_dxMap.txt");
  matrix_to_file(q, "q.txt");

  qMap = (unint*)calloc(PAD(m+(BLOCK_SIZE-1)*PAD(numReps)),sizeof(*qMap));

  ocl_matrix dq;
  copyAndMove(&dq, &q);

  device_matrix_to_file(dq, "dq.txt");
  
  charMatrix cM;
  cM.r=cM.c=numReps; cM.pr=cM.pc=cM.ld=PAD(numReps);
  cM.mat = (char*)calloc( cM.pr*cM.pc, sizeof(*cM.mat) );
  
  unint *repIDsQ;
  repIDsQ = (unint*)calloc( m, sizeof(*repIDsQ) );
  real *distToRepsQ;
  distToRepsQ = (real*)calloc( m, sizeof(*distToRepsQ) );
  unint *groupCountQ;
  groupCountQ = (unint*)calloc( PAD(numReps), sizeof(*groupCountQ) );
  
  /**
   * repIDsQ - indexes of nearest represetatives for consecutive
   *           elements from dq
   * distToRepsQ - distances to nearest representatives
   */
  computeReps(dq, rbcS.dr, repIDsQ, distToRepsQ);

  array_to_file(repIDsQ, m, "repIDsQ.txt");
  array_to_file(distToRepsQ, m, "distToRepsQ.txt");


  /** How many points are assigned to each group?
    * m - numer of query points
    * groupCountQ - representative occurence histogram
    */
  computeCounts(repIDsQ, m, groupCountQ);
  
  //Set up the mapping from groups to queries (qMap).
  buildQMap(q, qMap, repIDsQ, numReps, &compLength);

  printf("comp len: %u\n", compLength);
  
  // Setup the computation matrix.  Currently, the computation matrix is 
  // just the identity matrix: each query assigned to a particular 
  // representative is compared only to that representative's points.  

  // NOTE: currently, idIntersection is the *only* computation matrix 
  // that will work properly with k-nn search (this is not true for 1-nn above).
  idIntersection(cM);

  initCompPlan(&dcP, cM, groupCountQ, rbcS.groupCount, numReps);

  cl::Context& context = OclContextHolder::context;
  cl::CommandQueue& queue = OclContextHolder::queue;

  int byte_size = compLength*sizeof(unint);
  cl_int err;

  dqMap = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);

  err = queue.enqueueWriteBuffer(dqMap, CL_TRUE, 0, byte_size, qMap);
  checkErr(err);

  //checkErr( cudaMalloc( (void**)&dqMap, compLength*sizeof(*dqMap) ) );
  //cudaMemcpy( dqMap, qMap, compLength*sizeof(*dqMap), cudaMemcpyHostToDevice );
  
  computeKNNs(rbcS.dx, rbcS.dxMap, dq, dqMap, dcP, NNs, NNdists, compLength);

//  matrix_to_file(q, "q.txt");
//  matrix_to_file(q, "q.txt");

//  device_matrix_to_file(dcP.numGroups, "rbcS_dx.txt");
//  device_matrix_to_file(rbcS.dxMap, "rbcS_dxMap.txt");

//  cl::Buffer numGroups;
//  cl::Buffer groupCountX;
//  cl::Buffer qToQGroup;
//  cl::Buffer qGroupToXGroup;

  free(qMap);
  freeCompPlan(&dcP);
 // cudaFree(dq.mat);
  free(cM.mat);
  free(repIDsQ);
  free(distToRepsQ);
  free(groupCountQ);
}


void buildRBC(const matrix x, ocl_rbcStruct *rbcS, unint numReps, unint s){
  unint n = x.pr;
  intMatrix xmap;

  setupReps(x, rbcS, numReps);
  copyAndMove(&rbcS->dx, &x);
  
  xmap.r=numReps; xmap.pr=PAD(numReps); xmap.c=s; xmap.pc=xmap.ld=PAD(s);
  xmap.mat = (unint*)calloc( xmap.pr*xmap.pc, sizeof(*xmap.mat) );
  copyAndMoveI(&rbcS->dxMap, &xmap);
  rbcS->groupCount = (unint*)calloc( PAD(numReps), sizeof(*rbcS->groupCount) );
  
  //Figure out how much fits into memory

  // CHECKING AVAILABLE MEMORY OMITED!

  size_t memFree, memTot;
//  cudaMemGetInfo(&memFree, &memTot);
//  memFree = (unint)(((float)memFree)*MEM_USABLE);
//  /* mem needed per rep:
//   *  n*sizeof(real) - dist mat
//   *  n*sizeof(char) - dir
//   *  n*sizeof(int)  - dSums
//   *  sizeof(real)   - dranges
//   *  sizeof(int)    - dCnts
//   *  MEM_USED_IN_SCAN - memory used internally
//   */

 // memFree = 1024 * 1024 * 1024;
 // memTot = 1024 * 1024 * 1024;

  unint ptsAtOnce = 1024 * 64;//DPAD(memFree/((n+1)*sizeof(real) + n*sizeof(char) + (n+1)*sizeof(unint) + 2*MEM_USED_IN_SCAN(n)));
  if(!ptsAtOnce){
    fprintf(stderr,"error: %lu is not enough memory to build the RBC.. exiting\n", (unsigned long)memFree);
    exit(1);
  }

  cl::Context& context = OclContextHolder::context;
  cl::CommandQueue& queue = OclContextHolder::queue;
  cl_int err;

  //Now set everything up for the scans
  ocl_matrix dD;
  dD.pr=dD.r=ptsAtOnce; dD.c=rbcS->dx.r; dD.pc=rbcS->dx.pr; dD.ld=dD.pc;
  //checkErr( cudaMalloc( (void**)&dD.mat, dD.pr*dD.pc*sizeof(*dD.mat) ) );
  int byte_size = dD.pr*dD.pc*sizeof(real);
  dD.mat = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);

  //real *dranges;
  //checkErr( cudaMalloc( (void**)&dranges, ptsAtOnce*sizeof(real) ) );
  byte_size = ptsAtOnce*sizeof(real);
  cl::Buffer dranges(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);

  charMatrix ir;
  ir.r=dD.r; ir.pr=dD.pr; ir.c=dD.c; ir.pc=dD.pc; ir.ld=dD.ld;
  ir.mat = (char*)calloc( ir.pr*ir.pc, sizeof(*ir.mat) );

  ocl_charMatrix dir;
  copyAndMoveC(&dir, &ir);

  ocl_intMatrix dSums; //used to compute memory addresses.
  dSums.r=dir.r; dSums.pr=dir.pr; dSums.c=dir.c; dSums.pc=dir.pc; dSums.ld=dir.ld;
  //checkErr( cudaMalloc( (void**)&dSums.mat, dSums.pc*dSums.pr*sizeof(*dSums.mat) ) );
  byte_size = dSums.pc*dSums.pr*sizeof(int);
  dSums.mat = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);


//  unint *dCnts;
//  checkErr( cudaMalloc( (void**)&dCnts, ptsAtOnce*sizeof(*dCnts) ) );
  byte_size = ptsAtOnce*sizeof(unint);
  cl::Buffer dCnts(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);
  
  //Do the scans to build the dxMap
  unint numLeft = rbcS->dr.r; //points left to process
  unint row = 0; //base row for iteration of while loop
  unint pi, pip; //pi=pts per it, pip=pad(pi)

    while( numLeft > 0 )
    {
        pi = MIN(ptsAtOnce, numLeft);  //points to do this iteration.
        pip = PAD(pi);
        dD.r = pi; dD.pr = pip; dir.r=pi; dir.pr=pip; dSums.r=pi; dSums.pr=pip;

        /** compute the distance matrix
         *  rbcS->dr - matrix of representatives (choosen from input data)
         *  rbcS->dx - matrix of input data
         *  dD - matrix of distances
         */
        distSubMat(rbcS->dr, rbcS->dx, dD, row, pip);

        device_matrix_to_file(dD, "distances.txt");

        device_matrix_to_file(rbcS->dr, "srbS_dr0.txt");

        /** find an appropriate range
         *  dD - matrix of distances
         *  dranges - vector of maximal value for each row
         *  s - desired number of values within a range
         */
        findRangeWrap(dD, dranges, s);

        /** set binary vector for points in range
         *  dD - matrix of disances
         *  dranges - buffer of range upper bounds
         *  dir - matrix (the same size as dD) containing binary indication
         *        if corresponding value from dD belongs to the range
         */
        rangeSearchWrap(dD, dranges, dir);

        device_matrix_to_file(dir, "bin_vec.txt");

        sumWrap(dir, dSums);  //This and the next call perform the parallel compaction.

        device_matrix_to_file(dSums, "dSums.txt");

        buildMapWrap(rbcS->dxMap, dir, dSums, row);

        device_matrix_to_file(rbcS->dxMap, "dxMap.txt");

        getCountsWrap(dCnts,dir,dSums);  //How many points are assigned to each rep?  It is not
                                         //*exactly* s, which is why we need to compute this.

        buffer_to_file<unint>(dCnts, pi, "dCnts.txt");

//    //cudaMemcpy( &rbcS->groupCount[row], dCnts, pi*sizeof(*rbcS->groupCount), cudaMemcpyDeviceToHost );

        err = queue.enqueueReadBuffer(dCnts, CL_TRUE, 0,
                                  pi * sizeof(unint), rbcS->groupCount);

    for(int i = 0; i < pi; ++i)
        printf("[%d]=%d ", i, rbcS->groupCount[i]);
    printf("\n");

    fflush(stdout);

    checkErr(err);
    
    numLeft -= pi;
    row += pi;

    if(numLeft > 0)
    {
        printf("Only one pass is supported!\n");
        exit(1);
    }

  }
  

//  cudaFree(dCnts);
  free(ir.mat);
  free(xmap.mat);
//  cudaFree(dranges);
//  cudaFree(dir.mat);
//  cudaFree(dSums.mat);
//  cudaFree(dD.mat);
}


// Choose representatives and move them to device
void setupReps(matrix x, ocl_rbcStruct *rbcS, unint numReps){
  unint i;
  unint *randInds;
  randInds = (unint*)calloc( PAD(numReps), sizeof(*randInds) );
  subRandPerm(numReps, x.r, randInds);
  
  matrix r;
  r.r=numReps; r.pr=PAD(numReps); r.c=x.c; r.pc=r.ld=PAD(r.c); 
  r.mat = (real*)calloc( r.pr*r.pc, sizeof(*r.mat) );

  for(i=0;i<numReps;i++)
    copyVector(&r.mat[IDX(i,0,r.ld)], &x.mat[IDX(randInds[i],0,x.ld)], x.c);
  
  copyAndMove(&rbcS->dr, &r);

  free(randInds);
  free(r.mat);
}


/** Assign each point in dq to its nearest point in dr.
  *
  */
void computeReps(const ocl_matrix& dq, const ocl_matrix& dr,
                 unint *repIDs, real *distToReps)
{
    //  real *dMins;
    //  unint *dMinIDs;

    cl::Context& context = OclContextHolder::context;
    cl::CommandQueue& queue = OclContextHolder::queue;

    int byte_size = dq.pr*sizeof(real);
    cl_int err;

    cl::Buffer dMins(context, CL_TRUE, byte_size, 0, &err);
    checkErr(err);

    byte_size = dq.pr*sizeof(unint);

    cl::Buffer dMinIDs(context, CL_TRUE, byte_size, 0, &err);
    checkErr(err);

    //checkErr( cudaMalloc((void**)&(dMins), dq.pr*sizeof(*dMins)) );
    //checkErr( cudaMalloc((void**)&(dMinIDs), dq.pr*sizeof(*dMinIDs)) );

    nnWrap(dq,dr,dMins,dMinIDs);

    // POTENTIAL PERFORMANCE BOTTLENECK:

    byte_size = dq.r*sizeof(real);
    err = queue.enqueueReadBuffer(dMins, CL_TRUE, 0, byte_size, distToReps);
    checkErr(err);

    byte_size = dq.r*sizeof(unint);
    err = queue.enqueueReadBuffer(dMinIDs, CL_TRUE, 0, byte_size, repIDs);
    checkErr(err);

    //cudaMemcpy(distToReps,dMins,dq.r*sizeof(*dMins),cudaMemcpyDeviceToHost);
    //cudaMemcpy(repIDs,dMinIDs,dq.r*sizeof(*dMinIDs),cudaMemcpyDeviceToHost);
    //cudaFree(dMins);
    //cudaFree(dMinIDs);
}


//Assumes radii is initialized to 0s
void computeRadii(unint *repIDs, real *distToReps, real *radii, unint n, unint numReps){
  unint i;

  for(i=0;i<n;i++)
    radii[repIDs[i]] = MAX(distToReps[i],radii[repIDs[i]]);
}


//Assumes groupCount is initialized to 0s
void computeCounts(unint *repIDs, unint n, unint *groupCount){
  unint i;
  
  for(i=0;i<n;i++)
    groupCount[repIDs[i]]++;
}


void buildQMap(matrix q, unint *qMap, unint *repIDs,
               unint numReps, unint *compLength)
{
    unint n=q.r;
    unint i;
    unint *gS; //groupSize

    gS = (unint*)calloc(numReps+1,sizeof(*gS));

    /** histogram /
    for(i = 0; i < n; i++)
        gS[repIDs[i]+1]++;

    /** padding */
    for(i = 0; i < numReps + 1; i++)
        gS[i] = PAD(gS[i]);

    /** exclusive prefix sum */
    for(i = 1; i < numReps + 1; i++)
        gS[i] = gS[i - 1] + gS[i];

    /** number of queries after padding */
    *compLength = gS[numReps];

    /** map initialization */
    for(i = 0; i < (*compLength); i++)
        qMap[i] = DUMMY_IDX;



    for(i = 0; i < n; i++)
    {
        qMap[gS[repIDs[i]]] = i;
        gS[repIDs[i]]++;
    }

    free(gS);
}


// Sets the computation matrix to the identity.  
void idIntersection(charMatrix cM){
  unint i;
  for(i=0;i<cM.r;i++){
    if(i<cM.c)
      cM.mat[IDX(i,i,cM.ld)]=1;
  }
}


void fullIntersection(charMatrix cM){
  unint i,j;
  for(i=0;i<cM.r;i++){
    for(j=0;j<cM.c;j++){
      cM.mat[IDX(i,j,cM.ld)]=1;
    }
  }
}

//NEVER USED
/*
void computeNNs(matrix dx, intMatrix dxMap, matrix dq, unint *dqMap, compPlan dcP, unint *NNs, real *NNdists, unint compLength){
  real *dNNdists;
  unint *dMinIDs;
  
  checkErr( cudaMalloc((void**)&dNNdists,compLength*sizeof(*dNNdists)) );
  checkErr( cudaMalloc((void**)&dMinIDs,compLength*sizeof(*dMinIDs)) );

  planNNWrap(dq, dqMap, dx, dxMap, dNNdists, dMinIDs, dcP, compLength );
  cudaMemcpy( NNs, dMinIDs, dq.r*sizeof(*NNs), cudaMemcpyDeviceToHost );
  cudaMemcpy( NNdists, dNNdists, dq.r*sizeof(*dNNdists), cudaMemcpyDeviceToHost );

  cudaFree(dNNdists);
  cudaFree(dMinIDs);
}*/

//void computeKNNs(matrix dx, intMatrix dxMap, matrix dq, unint *dqMap, compPlan dcP, intMatrix NNs, matrix NNdists, unint compLength){
void computeKNNs(const ocl_matrix& dx, const ocl_intMatrix& dxMap, const ocl_matrix& dq, cl::Buffer& dqMap,
                 ocl_compPlan& dcP, intMatrix NNs, matrix NNdists, unint compLength){
  ocl_matrix dNNdists;
  ocl_intMatrix dMinIDs;
  dNNdists.r=compLength; dNNdists.pr=compLength; dNNdists.c=KMAX; dNNdists.pc=KMAX; dNNdists.ld=dNNdists.pc;
  dMinIDs.r=compLength; dMinIDs.pr=compLength; dMinIDs.c=KMAX; dMinIDs.pc=KMAX; dMinIDs.ld=dMinIDs.pc;

  //checkErr( cudaMalloc((void**)&dNNdists.mat,dNNdists.pr*dNNdists.pc*sizeof(*dNNdists.mat)) );
  //checkErr( cudaMalloc((void**)&dMinIDs.mat,dMinIDs.pr*dMinIDs.pc*sizeof(*dMinIDs.mat)) );

  cl::Context& context = OclContextHolder::context;
  cl::CommandQueue& queue = OclContextHolder::queue;

  int byte_size = dNNdists.pr * dNNdists.pc * sizeof(real);
  cl_int err;

  dNNdists.mat = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);

  byte_size = dMinIDs.pr*dMinIDs.pc*sizeof(unint);

  dMinIDs.mat = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);

  planKNNWrap(dq, dqMap, dx, dxMap, dNNdists, dMinIDs, dcP, compLength);


  byte_size = dq.r*KMAX*sizeof(unint);

  err = queue.enqueueReadBuffer(dMinIDs.mat, CL_TRUE, 0, byte_size, NNs.mat);
  checkErr(err);

  byte_size = dq.r*KMAX*sizeof(real);

  err = queue.enqueueReadBuffer(dNNdists.mat, CL_TRUE, 0, byte_size, NNdists.mat);
  checkErr(err);

  //cudaMemcpy( NNs.mat, dMinIDs.mat, dq.r*KMAX*sizeof(*NNs.mat), cudaMemcpyDeviceToHost );
  //cudaMemcpy( NNdists.mat, dNNdists.mat, dq.r*KMAX*sizeof(*NNdists.mat), cudaMemcpyDeviceToHost );
  //cudaFree(dNNdists.mat);
  //cudaFree(dMinIDs.mat);
}


//This calls the dist1Kernel wrapper, but has it compute only 
//a submatrix of the all-pairs distance matrix.  In particular,
//only distances from dr[start,:].. dr[start+length-1] to all of x
//are computed, resulting in a distance matrix of size 
//length by dx.pr.  It is assumed that length is padded.
//void distSubMat(matrix dr, matrix dx, matrix dD, unint start, unint length){
void distSubMat(ocl_matrix& dr, ocl_matrix& dx, ocl_matrix &dD, unint start, unint length){
  dr.r=dr.pr=length;
  //dr.mat = &dr.mat[IDX( start, 0, dr.ld )];
  if(start != 0)
  {
      printf("subbuffers are not supported!\n");
      exit(1);
  }

  dist1Wrap(dr, dx, dD);
}


void destroyRBC(ocl_rbcStruct *rbcS){
//  cudaFree(rbcS->dx.mat);
//  cudaFree(rbcS->dxMap.mat);
//  cudaFree(rbcS->dr.mat);
  free(rbcS->groupCount);
}


/* Danger: this function allocates memory that it does not free.  
 * Use freeCompPlan to clear mem.  
 * See the readme.txt file for a description of why this function is needed.
 */
void initCompPlan(ocl_compPlan *dcP, charMatrix cM, unint *groupCountQ, unint *groupCountX, unint numReps){
  unint i,j,k;
  unint maxNumGroups=0;
  compPlan cP;
  
  unint sNumGroups = numReps;
  cP.numGroups = (unint*)calloc(sNumGroups, sizeof(*cP.numGroups));
  
  for(i=0; i<numReps; i++){
    cP.numGroups[i] = 0;
    for(j=0; j<numReps; j++)
      cP.numGroups[i] += cM.mat[IDX(i,j,cM.ld)];
    maxNumGroups = MAX(cP.numGroups[i], maxNumGroups);
  }
  cP.ld = maxNumGroups;
  
  unint sQToQGroup;
  for(i=0, sQToQGroup=0; i<numReps; i++)
    sQToQGroup += PAD(groupCountQ[i]);
  
  cP.qToQGroup = (unint*)calloc( sQToQGroup, sizeof(*cP.qToQGroup) );

  for(i=0, k=0; i<numReps; i++){
    for(j=0; j<PAD(groupCountQ[i]); j++)
      cP.qToQGroup[k++] = i;
  }
  
  unint sQGroupToXGroup = numReps*maxNumGroups;
  cP.qGroupToXGroup = (unint*)calloc( sQGroupToXGroup, sizeof(*cP.qGroupToXGroup) );
  unint sGroupCountX = maxNumGroups*numReps;
  cP.groupCountX = (unint*)calloc( sGroupCountX, sizeof(*cP.groupCountX) );
  
  for(i=0; i<numReps; i++){
    for(j=0, k=0; j<numReps; j++){
      if( cM.mat[IDX( i, j, cM.ld )] ){
	cP.qGroupToXGroup[IDX( i, k, cP.ld )] = j;
	cP.groupCountX[IDX( i, k++, cP.ld )] = groupCountX[j];
      }
    }
  }

  cl::Context& context = OclContextHolder::context;
  cl::CommandQueue& queue = OclContextHolder::queue;

  cl_int err;

  int byte_size = sNumGroups*sizeof(unint);
  dcP->numGroups = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);
  err = queue.enqueueWriteBuffer(dcP->numGroups, CL_TRUE, 0, byte_size, cP.numGroups);
  checkErr(err);

  byte_size = sGroupCountX*sizeof(unint);
  dcP->groupCountX = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);
  err = queue.enqueueWriteBuffer(dcP->groupCountX, CL_TRUE, 0, byte_size, cP.groupCountX);
  checkErr(err);

  byte_size = sQToQGroup*sizeof(unint);
  dcP->qToQGroup = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);
  err = queue.enqueueWriteBuffer(dcP->qToQGroup, CL_TRUE, 0, byte_size, cP.qToQGroup);
  checkErr(err);

  byte_size = sQGroupToXGroup*sizeof(unint);
  dcP->qGroupToXGroup = cl::Buffer(context, CL_MEM_READ_WRITE, byte_size, 0, &err);
  checkErr(err);
  err = queue.enqueueWriteBuffer(dcP->qGroupToXGroup, CL_TRUE, 0, byte_size, cP.qGroupToXGroup);
  checkErr(err);

  //Move to device
  //checkErr( cudaMalloc( (void**)&dcP->numGroups, sNumGroups*sizeof(*dcP->numGroups) ) );
  //cudaMemcpy( dcP->numGroups, cP.numGroups, sNumGroups*sizeof(*dcP->numGroups), cudaMemcpyHostToDevice );
//  checkErr( cudaMalloc( (void**)&dcP->groupCountX, sGroupCountX*sizeof(*dcP->groupCountX) ) );
//  cudaMemcpy( dcP->groupCountX, cP.groupCountX, sGroupCountX*sizeof(*dcP->groupCountX), cudaMemcpyHostToDevice );
//  checkErr( cudaMalloc( (void**)&dcP->qToQGroup, sQToQGroup*sizeof(*dcP->qToQGroup) ) );
//  cudaMemcpy( dcP->qToQGroup, cP.qToQGroup, sQToQGroup*sizeof(*dcP->qToQGroup), cudaMemcpyHostToDevice );
//  checkErr( cudaMalloc( (void**)&dcP->qGroupToXGroup, sQGroupToXGroup*sizeof(*dcP->qGroupToXGroup) ) );
//  cudaMemcpy( dcP->qGroupToXGroup, cP.qGroupToXGroup, sQGroupToXGroup*sizeof(*dcP->qGroupToXGroup), cudaMemcpyHostToDevice );
  dcP->ld = cP.ld;

  free(cP.numGroups);
  free(cP.groupCountX);
  free(cP.qToQGroup);
  free(cP.qGroupToXGroup);
}


//Frees memory allocated in initCompPlan.
void freeCompPlan(ocl_compPlan *dcP){
//  cudaFree(dcP->numGroups);
//  cudaFree(dcP->groupCountX);
//  cudaFree(dcP->qToQGroup);
//  cudaFree(dcP->qGroupToXGroup);
}

#endif
