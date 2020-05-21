/*************************************************************************************

    Grid physics library, www.github.com/paboyle/Grid 

    Source file: ./lib/AllocationCache.h

    Copyright (C) 2015

Author: Azusa Yamaguchi <ayamaguc@staffmail.ed.ac.uk>
Author: Peter Boyle <paboyle@ph.ed.ac.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    See the full license in the file "LICENSE" in the top level distribution directory
*************************************************************************************/
/*  END LEGAL */
#pragma once

NAMESPACE_BEGIN(Grid);

// Move control to configure.ac and Config.h?

#undef  ALLOCATION_CACHE
#define GRID_ALLOC_ALIGN (2*1024*1024)
#define GRID_ALLOC_SMALL_LIMIT (4096)

/*Pinning pages is costly*/
////////////////////////////////////////////////////////////////////////////
// Advise the LatticeAccelerator class
////////////////////////////////////////////////////////////////////////////
enum ViewAdvise {
 AdviseDefault       = 0x0,    // Reegular data
 AdviseInfrequentUse = 0x1,    // Advise that the data is used infrequently.  This can
                               // significantly influence performance of bulk storage.
 
 AdviseTransient      = 0x2,   // Data will mostly be read.  On some architectures
                               // enables read-only copies of memory to be kept on
                               // host and device.

 AdviseAcceleratorWriteDiscard = 0x4  // Field will be written in entirety on device

};

////////////////////////////////////////////////////////////////////////////
// View Access Mode
////////////////////////////////////////////////////////////////////////////
enum ViewMode {
  AcceleratorRead  = 0x01,
  AcceleratorWrite = 0x02,
  AcceleratorWriteDiscard = 0x04,
  CpuRead  = 0x08,
  CpuWrite = 0x10,
  CpuWriteDiscard = 0x10 // same for now
};

class AllocationCache {
private:

  ////////////////////////////////////////////////////////////
  // For caching recently freed allocations
  ////////////////////////////////////////////////////////////
  typedef struct { 
    void *address;
    size_t bytes;
    int valid;
  } AllocationCacheEntry;

  static const int NallocCacheMax=128; 
  static const int NallocType=4;
  static AllocationCacheEntry Entries[NallocType][NallocCacheMax];
  static int Victim[NallocType];
  static int Ncache[NallocType];

  /////////////////////////////////////////////////
  // Free pool
  /////////////////////////////////////////////////
  static void *Insert(void *ptr,size_t bytes,int type) ;
  static void *Insert(void *ptr,size_t bytes,AllocationCacheEntry *entries,int ncache,int &victim) ;
  static void *Lookup(size_t bytes,int type) ;
  static void *Lookup(size_t bytes,AllocationCacheEntry *entries,int ncache) ;

  /////////////////////////////////////////////////
  // Internal device view
  /////////////////////////////////////////////////
  static void *AcceleratorAllocate(size_t bytes);
  static void  AcceleratorFree    (void *ptr,size_t bytes);
  static int   ViewVictim(void);
  static void  CpuDiscard(int e);
  static void  Discard(int e);
  static void  Evict(int e);
  static void  Flush(int e);
  static void  Clone(int e);
  static int   CpuViewLookup(void *CpuPtr);
  //  static int   AccViewLookup(void *AccPtr);
  static void  AcceleratorViewClose(void* AccPtr);
  static void *AcceleratorViewOpen(void* CpuPtr,size_t bytes,ViewMode mode,ViewAdvise hint);
  static void  CpuViewClose(void* Ptr);
  static void *CpuViewOpen(void* CpuPtr,size_t bytes,ViewMode mode,ViewAdvise hint);

public:
  static void Init(void);

  static void  ViewClose(void* AccPtr,ViewMode mode);
  static void *ViewOpen(void* CpuPtr,size_t bytes,ViewMode mode,ViewAdvise hint);

  static void *CpuAllocate(size_t bytes);
  static void  CpuFree    (void *ptr,size_t bytes);
};

NAMESPACE_END(Grid);


