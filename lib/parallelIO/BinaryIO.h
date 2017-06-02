    /*************************************************************************************

    Grid physics library, www.github.com/paboyle/Grid 

    Source file: ./lib/parallelIO/BinaryIO.h

    Copyright (C) 2015

Author: Peter Boyle <paboyle@ph.ed.ac.uk>
Author: paboyle <paboyle@ph.ed.ac.uk>

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
#ifndef GRID_BINARY_IO_H
#define GRID_BINARY_IO_H


#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif
#include <arpa/inet.h>
#include <algorithm>
// 64bit endian swap is a portability pain
#ifndef __has_builtin         // Optional of course.
#define __has_builtin(x) 0  // Compatibility with non-clang compilers.
#endif

#if HAVE_DECL_BE64TOH 
#undef Grid_ntohll
#define Grid_ntohll be64toh
#endif

#if HAVE_DECL_NTOHLL
#undef  Grid_ntohll
#define Grid_ntohll ntohll
#endif

#ifndef Grid_ntohll

#if BYTE_ORDER == BIG_ENDIAN 

#define Grid_ntohll(A) (A)

#else 

#if __has_builtin(__builtin_bswap64)
#define Grid_ntohll(A) __builtin_bswap64(A)
#else
#error
#endif

#endif

#endif

namespace Grid { 

  // A little helper
  inline void removeWhitespace(std::string &key)
  {
    key.erase(std::remove_if(key.begin(), key.end(), ::isspace),key.end());
  }

class BinaryIO {

 public:


  // Network is big endian
  static inline void htobe32_v(void *file_object,uint32_t bytes){ be32toh_v(file_object,bytes);} 
  static inline void htobe64_v(void *file_object,uint32_t bytes){ be64toh_v(file_object,bytes);} 
  static inline void htole32_v(void *file_object,uint32_t bytes){ le32toh_v(file_object,bytes);} 
  static inline void htole64_v(void *file_object,uint32_t bytes){ le64toh_v(file_object,bytes);} 

  static inline void be32toh_v(void *file_object,uint32_t bytes)
  {
    uint32_t * f = (uint32_t *)file_object;
    for(int i=0;i*sizeof(uint32_t)<bytes;i++){  
      f[i] = ntohl(f[i]);
    }
  }

  // LE must Swap and switch to host
  static inline void le32toh_v(void *file_object,uint32_t bytes)
  {
    uint32_t *fp = (uint32_t *)file_object;
    uint32_t f;

    for(int i=0;i*sizeof(uint32_t)<bytes;i++){  
      f = fp[i];
      // got network order and the network to host
      f = ((f&0xFF)<<24) | ((f&0xFF00)<<8) | ((f&0xFF0000)>>8) | ((f&0xFF000000UL)>>24) ; 
      fp[i] = ntohl(f);
    }
  }

  // BE is same as network
  static inline void be64toh_v(void *file_object,uint32_t bytes)
  {
    uint64_t * f = (uint64_t *)file_object;
    for(int i=0;i*sizeof(uint64_t)<bytes;i++){  
      f[i] = Grid_ntohll(f[i]);
    }
  }
  
  // LE must swap and switch;
  static inline void le64toh_v(void *file_object,uint32_t bytes)
  {
    uint64_t *fp = (uint64_t *)file_object;
    uint64_t f,g;
    
    for(int i=0;i*sizeof(uint64_t)<bytes;i++){  
      f = fp[i];
      // got network order and the network to host
      g = ((f&0xFF)<<24) | ((f&0xFF00)<<8) | ((f&0xFF0000)>>8) | ((f&0xFF000000UL)>>24) ; 
      g = g << 32;
      f = f >> 32;
      g|= ((f&0xFF)<<24) | ((f&0xFF00)<<8) | ((f&0xFF0000)>>8) | ((f&0xFF000000UL)>>24) ; 
      fp[i] = Grid_ntohll(g);
    }
  }

  template<class vobj,class fobj,class munger> static inline void Uint32Checksum(Lattice<vobj> &lat,munger munge,uint32_t &csum)
  {
    typedef typename vobj::scalar_object sobj;
    GridBase *grid = lat._grid ;
    std::cout <<GridLogMessage<< "Uint32Checksum "<<norm2(lat)<<std::endl;
    sobj siteObj;
    fobj fileObj;

    csum = 0;
    std::vector<int> lcoor;
    for(int l=0;l<grid->lSites();l++){
      Lexicographic::CoorFromIndex(lcoor,l,grid->_ldimensions);
      peekLocalSite(siteObj,lat,lcoor);
      munge(siteObj,fileObj,csum);
    }
    grid->GlobalSum(csum);
  }
    
  static inline void Uint32Checksum(uint32_t *buf,uint32_t buf_size_bytes,uint32_t &csum)
  {
    for(int i=0;i*sizeof(uint32_t)<buf_size_bytes;i++){
      csum=csum+buf[i];
    }
  }
  
  template<class vobj,class fobj,class munger>
  static inline uint32_t readObjectSerial(Lattice<vobj> &Umu,std::string file,munger munge,int offset,const std::string &format)
  {
    typedef typename vobj::scalar_object sobj;

    GridBase *grid = Umu._grid;

    std::cout<< GridLogMessage<< "Serial read I/O "<< file<< std::endl;
    GridStopWatch timer; timer.Start();

    int ieee32big = (format == std::string("IEEE32BIG"));
    int ieee32    = (format == std::string("IEEE32"));
    int ieee64big = (format == std::string("IEEE64BIG"));
    int ieee64    = (format == std::string("IEEE64"));

    // Find the location of each site and send to primary node
    // Take loop order from Chroma; defines loop order now that NERSC doc no longer
    // available (how short sighted is that?)
    std::ifstream fin(file,std::ios::binary|std::ios::in);
    fin.seekg(offset);

    Umu = zero;
    uint32_t csum=0;
    uint64_t bytes=0;
    fobj file_object;
    sobj munged;
    
    for(int t=0;t<grid->_fdimensions[3];t++){
    for(int z=0;z<grid->_fdimensions[2];z++){
    for(int y=0;y<grid->_fdimensions[1];y++){
    for(int x=0;x<grid->_fdimensions[0];x++){

      std::vector<int> site({x,y,z,t});

      if (grid->IsBoss()) {
        fin.read((char *)&file_object, sizeof(file_object));
        bytes += sizeof(file_object);
        if (ieee32big) be32toh_v((void *)&file_object, sizeof(file_object));
        if (ieee32) le32toh_v((void *)&file_object, sizeof(file_object));
        if (ieee64big) be64toh_v((void *)&file_object, sizeof(file_object));
        if (ieee64) le64toh_v((void *)&file_object, sizeof(file_object));

        munge(file_object, munged, csum);
      }
      // The boss who read the file has their value poked
      pokeSite(munged,Umu,site);
    }}}}
    timer.Stop();
    std::cout<<GridLogPerformance<<"readObjectSerial: read "<< bytes <<" bytes in "<<timer.Elapsed() <<" "
       << (double)bytes/ (double)timer.useconds() <<" MB/s "  <<std::endl;

    return csum;
  }

  template<class vobj,class fobj,class munger> 
  static inline uint32_t writeObjectSerial(Lattice<vobj> &Umu,std::string file,munger munge,int offset,const std::string & format)
  {
    typedef typename vobj::scalar_object sobj;

    GridBase *grid = Umu._grid;

    int ieee32big = (format == std::string("IEEE32BIG"));
    int ieee32    = (format == std::string("IEEE32"));
    int ieee64big = (format == std::string("IEEE64BIG"));
    int ieee64    = (format == std::string("IEEE64"));

    //////////////////////////////////////////////////
    // Serialise through node zero
    //////////////////////////////////////////////////
    std::cout<< GridLogMessage<< "Serial write I/O "<< file<<std::endl;
    GridStopWatch timer; timer.Start();

    std::ofstream fout;
    if ( grid->IsBoss() ) {
      fout.open(file,std::ios::binary|std::ios::out|std::ios::in);
      fout.seekp(offset);
    }
    uint64_t bytes=0;
    uint32_t csum=0;
    fobj file_object;
    sobj unmunged;
    for(int t=0;t<grid->_fdimensions[3];t++){
    for(int z=0;z<grid->_fdimensions[2];z++){
    for(int y=0;y<grid->_fdimensions[1];y++){
    for(int x=0;x<grid->_fdimensions[0];x++){

      std::vector<int> site({x,y,z,t});
      // peek & write
      peekSite(unmunged,Umu,site);

      munge(unmunged,file_object,csum);

      
      if ( grid->IsBoss() ) {
  
  if(ieee32big) htobe32_v((void *)&file_object,sizeof(file_object));
  if(ieee32)    htole32_v((void *)&file_object,sizeof(file_object));
  if(ieee64big) htobe64_v((void *)&file_object,sizeof(file_object));
  if(ieee64)    htole64_v((void *)&file_object,sizeof(file_object));

  // NB could gather an xstrip as an optimisation.
  fout.write((char *)&file_object,sizeof(file_object));
  bytes+=sizeof(file_object);
      }
    }}}}
    timer.Stop();
    std::cout<<GridLogPerformance<<"writeObjectSerial: wrote "<< bytes <<" bytes in "<<timer.Elapsed() <<" "
       << (double)bytes/timer.useconds() <<" MB/s "  <<std::endl;

    return csum;
  }

  static inline uint32_t writeRNGSerial(GridSerialRNG &serial,GridParallelRNG &parallel,std::string file,int offset)
  {
    typedef typename GridSerialRNG::RngStateType RngStateType;
    const int RngStateCount = GridSerialRNG::RngStateCount;


    GridBase *grid = parallel._grid;
    int gsites = grid->_gsites;

    //////////////////////////////////////////////////
    // Serialise through node zero
    //////////////////////////////////////////////////
    std::cout<< GridLogMessage<< "Serial RNG write I/O "<< file<<std::endl;

    std::ofstream fout;
    if ( grid->IsBoss() ) {
      fout.open(file,std::ios::binary|std::ios::out|std::ios::in);
      fout.seekp(offset);
    }
    
    uint32_t csum=0;
    std::vector<RngStateType> saved(RngStateCount);
    int bytes = sizeof(RngStateType)*saved.size();
    std::vector<int> gcoor;

    for(int gidx=0;gidx<gsites;gidx++){

      int rank,o_idx,i_idx;
      grid->GlobalIndexToGlobalCoor(gidx,gcoor);
      grid->GlobalCoorToRankIndex(rank,o_idx,i_idx,gcoor);
      int l_idx=parallel.generator_idx(o_idx,i_idx);

      if( rank == grid->ThisRank() ){
  //  std::cout << "rank" << rank<<" Getting state for index "<<l_idx<<std::endl;
  parallel.GetState(saved,l_idx);
      }

      grid->Broadcast(rank,(void *)&saved[0],bytes);

      if ( grid->IsBoss() ) {
  Uint32Checksum((uint32_t *)&saved[0],bytes,csum);
  fout.write((char *)&saved[0],bytes);
      }

    }

    if ( grid->IsBoss() ) {
      serial.GetState(saved,0);
      Uint32Checksum((uint32_t *)&saved[0],bytes,csum);
      fout.write((char *)&saved[0],bytes);
    }
    grid->Broadcast(0,(void *)&csum,sizeof(csum));
    return csum;
  }
  static inline uint32_t readRNGSerial(GridSerialRNG &serial,GridParallelRNG &parallel,std::string file,int offset)
  {
    typedef typename GridSerialRNG::RngStateType RngStateType;
    const int RngStateCount = GridSerialRNG::RngStateCount;

    GridBase *grid = parallel._grid;
    int gsites = grid->_gsites;

    //////////////////////////////////////////////////
    // Serialise through node zero
    //////////////////////////////////////////////////
    std::cout<< GridLogMessage<< "Serial RNG read I/O "<< file<<std::endl;

    std::ifstream fin(file,std::ios::binary|std::ios::in);
    fin.seekg(offset);
    
    uint32_t csum=0;
    std::vector<RngStateType> saved(RngStateCount);
    int bytes = sizeof(RngStateType)*saved.size();
    std::vector<int> gcoor;

    for(int gidx=0;gidx<gsites;gidx++){

      int rank,o_idx,i_idx;
      grid->GlobalIndexToGlobalCoor(gidx,gcoor);
      grid->GlobalCoorToRankIndex(rank,o_idx,i_idx,gcoor);
      int l_idx=parallel.generator_idx(o_idx,i_idx);

      if ( grid->IsBoss() ) {
  fin.read((char *)&saved[0],bytes);
  Uint32Checksum((uint32_t *)&saved[0],bytes,csum);
      }

      grid->Broadcast(0,(void *)&saved[0],bytes);

      if( rank == grid->ThisRank() ){
  parallel.SetState(saved,l_idx);
      }

    }

    if ( grid->IsBoss() ) {
      fin.read((char *)&saved[0],bytes);
      serial.SetState(saved,0);
      Uint32Checksum((uint32_t *)&saved[0],bytes,csum);
    }

    grid->Broadcast(0,(void *)&csum,sizeof(csum));

    return csum;
  }


  template<class vobj,class fobj,class munger>
  static inline uint32_t readObjectParallel(Lattice<vobj> &Umu,std::string file,munger munge,int offset,const std::string &format)
  {
    typedef typename vobj::scalar_object sobj;

    GridBase *grid = Umu._grid;

    int ieee32big = (format == std::string("IEEE32BIG"));
    int ieee32    = (format == std::string("IEEE32"));
    int ieee64big = (format == std::string("IEEE64BIG"));
    int ieee64    = (format == std::string("IEEE64"));

    // original code was very slow, 40MB/s reading of a file that should be read at 3GB/s on BNL-KNL
    // the code below achieves 4GB/s for reading the file and overall 1GB/s for reading+munging
    std::vector<fobj> fbuf;

    std::ifstream fin;

    int nd = grid->_ndimension;
    for(int d=0;d<nd;d++){
      assert(grid->CheckerBoarded(d) == 0);
    }

    GridStopWatch timer; timer.Start();
    uint64_t bytes=0;

    int myrank = grid->ThisRank();

    // only read relevant slice in last dimension that is not 1
    int slice_dim;
    for (slice_dim = nd-1; slice_dim >= 0; slice_dim--)
      if (grid->_ldimensions[slice_dim]!=1)
	break;
    auto slice_vol = grid->_gsites / grid->_processors[slice_dim];
    auto slice_offset = grid->_processor_coor[slice_dim]*slice_vol;

    fin.open(file,std::ios::binary|std::ios::in);
    fin.seekg(0,std::ios_base::end);
    fbuf.resize(slice_vol);
    GridStopWatch gsw; gsw.Start();
    fin.seekg(sizeof(fobj)*slice_offset+offset);
    fin.read((char *)&fbuf[0],sizeof(fobj)*slice_vol);assert( fin.fail()==0);
    gsw.Stop();
    RealD gbs = slice_vol*sizeof(fobj) / 1073.741824 / gsw.useconds();
    std::cout << "Node " << myrank << " reads " << slice_vol << " objects from " << file << 
      " at " << gbs << " GB/s " << std::endl;
    fflush(stdout);

    // could also select one node for reading and then broadcast, does not seem to
    // matter much in a filesystem that is mounted over the fabric

    Umu = zero;
    uint32_t csum = 0;

#pragma omp parallel
    {
      uint32_t t_csum = 0; 
      int64_t t_bytes = 0;
      fobj fileObj;
      sobj siteObj;
      std::vector<int> gsite(nd);
      std::vector<int> lsite(nd);

#pragma omp for
      for(int tlex=0;tlex<grid->lSites();tlex++){
	
	Lexicographic::CoorFromIndex(lsite,tlex,grid->_ldimensions);
	for(int d=0;d<nd;d++){
	  gsite[d] = lsite[d]+grid->_processor_coor[d]*grid->_ldimensions[d];
	}
	
	int g_idx;
	grid->GlobalCoorToGlobalIndex(gsite,g_idx);

	fileObj = fbuf[g_idx - slice_offset];
	t_bytes+=sizeof(fileObj);
	
	if(ieee32big) be32toh_v((void *)&fileObj,sizeof(fileObj));
	if(ieee32)    le32toh_v((void *)&fileObj,sizeof(fileObj));
	if(ieee64big) be64toh_v((void *)&fileObj,sizeof(fileObj));
	if(ieee64)    le64toh_v((void *)&fileObj,sizeof(fileObj));
	
	munge(fileObj,siteObj,t_csum);
	
	pokeLocalSite(siteObj,Umu,lsite);

      }

#pragma omp critical
      {
	csum += t_csum; // checksum is linear
	bytes += t_bytes;
      }
    }

    grid->GlobalSum(csum);
    grid->GlobalSum(bytes);
    grid->Barrier();

    timer.Stop();
    std::cout<<GridLogPerformance<<"readObjectParallel: read "<< bytes <<" bytes in "<<timer.Elapsed() <<" "
	     << (double)bytes/timer.useconds() <<" MB/s "  <<std::endl;
    
    return csum;
  }

  //////////////////////////////////////////////////////////
  // Parallel writer
  //////////////////////////////////////////////////////////
  template<class vobj,class fobj,class munger>
  static inline uint32_t writeObjectParallel(Lattice<vobj> &Umu,std::string file,munger munge,int offset,const std::string & format)
  {
    typedef typename vobj::scalar_object sobj;
    GridBase *grid = Umu._grid;

    int ieee32big = (format == std::string("IEEE32BIG"));
    int ieee32    = (format == std::string("IEEE32"));
    int ieee64big = (format == std::string("IEEE64BIG"));
    int ieee64    = (format == std::string("IEEE64"));

    int nd = grid->_ndimension;
    for(int d=0;d<nd;d++){
      assert(grid->CheckerBoarded(d) == 0);
    }

    std::vector<int> parallel(nd,1);
    std::vector<int> ioproc  (nd);
    std::vector<int> start(nd);
    std::vector<int> range(nd);

    uint64_t slice_vol = 1;

    int IOnode = 1;

    for(int d=0;d<grid->_ndimension;d++) {

      if ( d!= grid->_ndimension-1 ) parallel[d] = 0;

      if (parallel[d]) {
  range[d] = grid->_ldimensions[d];
  start[d] = grid->_processor_coor[d]*range[d];
  ioproc[d]= grid->_processor_coor[d];
      } else {
  range[d] = grid->_gdimensions[d];
  start[d] = 0;
  ioproc[d]= 0;

  if ( grid->_processor_coor[d] != 0 ) IOnode = 0;
      }

      slice_vol = slice_vol * range[d];
    }
    
    {
      uint32_t tmp = IOnode;
      grid->GlobalSum(tmp);
      std::cout<< GridLogMessage<< "Parallel write I/O from "<< file << " with " <<tmp<< " IOnodes for subslice ";
      for(int d=0;d<grid->_ndimension;d++){
  std::cout<< range[d];
  if( d< grid->_ndimension-1 ) 
    std::cout<< " x ";
      }
      std::cout << std::endl;
    }

    GridStopWatch timer; timer.Start();
    uint64_t bytes=0;

    int myrank = grid->ThisRank();
    int iorank = grid->RankFromProcessorCoor(ioproc);

    // Take into account block size of parallel file systems want about
    // 4-16MB chunks.
    // Ideally one reader/writer per xy plane and read these contiguously
    // with comms from nominated I/O nodes.
    std::ofstream fout;
    if ( IOnode ) fout.open(file,std::ios::binary|std::ios::in|std::ios::out);

    //////////////////////////////////////////////////////////
    // Find the location of each site and send to primary node
    // Take loop order from Chroma; defines loop order now that NERSC doc no longer
    // available (how short sighted is that?)
    //////////////////////////////////////////////////////////

    uint32_t csum=0;
    fobj fileObj;
    static sobj siteObj; // static for SHMEM target; otherwise dynamic allocate with AlignedAllocator

    // should aggregate a whole chunk and then write.
    // need to implement these loops in Nd independent way with a lexico conversion
    for(int tlex=0;tlex<slice_vol;tlex++){
  
      std::vector<int> tsite(nd); // temporary mixed up site
      std::vector<int> gsite(nd);
      std::vector<int> lsite(nd);
      std::vector<int> iosite(nd);

      Lexicographic::CoorFromIndex(tsite,tlex,range);

      for(int d=0;d<nd;d++){
  lsite[d] = tsite[d]%grid->_ldimensions[d];  // local site
  gsite[d] = tsite[d]+start[d];               // global site
      }


      /////////////////////////
      // Get the rank of owner of data
      /////////////////////////
      int rank, o_idx,i_idx, g_idx;
      grid->GlobalCoorToRankIndex(rank,o_idx,i_idx,gsite);
      grid->GlobalCoorToGlobalIndex(gsite,g_idx);

      ////////////////////////////////
      // iorank writes from the seek
      ////////////////////////////////
      
      // Owner of data peeks it
      peekLocalSite(siteObj,Umu,lsite);

      // Pair of nodes may need to do pt2pt send
      if ( rank != iorank ) { // comms is necessary
  if ( (myrank == rank) || (myrank==iorank) ) { // and we have to do it
    // Send to IOrank 
    grid->SendRecvPacket((void *)&siteObj,(void *)&siteObj,rank,iorank,sizeof(siteObj));
  }
      }

      grid->Barrier(); // necessary?

      if (myrank == iorank) {
  
  munge(siteObj,fileObj,csum);

  if(ieee32big) htobe32_v((void *)&fileObj,sizeof(fileObj));
  if(ieee32)    htole32_v((void *)&fileObj,sizeof(fileObj));
  if(ieee64big) htobe64_v((void *)&fileObj,sizeof(fileObj));
  if(ieee64)    htole64_v((void *)&fileObj,sizeof(fileObj));
  
  fout.seekp(offset+g_idx*sizeof(fileObj));
  fout.write((char *)&fileObj,sizeof(fileObj));
  bytes+=sizeof(fileObj);
      }
    }

    grid->GlobalSum(csum);
    grid->GlobalSum(bytes);

    timer.Stop();
    std::cout<<GridLogPerformance<<"writeObjectParallel: wrote "<< bytes <<" bytes in "<<timer.Elapsed() <<" "
       << (double)bytes/timer.useconds() <<" MB/s "  <<std::endl;

    return csum;
  }

};

}

#endif
