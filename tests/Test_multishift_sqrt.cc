#include <Grid.h>

using namespace std;
using namespace Grid;
using namespace Grid::QCD;

template<class Field> class DumbOperator  : public LinearOperatorBase<Field> {
public:
  LatticeComplex scale;
  LatticeComplex sqrtscale;
  DumbOperator(GridBase *grid)
    : scale(grid),
      sqrtscale(grid)
  {
    GridParallelRNG  pRNG(grid);  
    std::vector<int> seeds({5,6,7,8});
    pRNG.SeedFixedIntegers(seeds);

    random(pRNG,sqrtscale);
    sqrtscale = sqrtscale * adj(sqrtscale);// force real pos def
    scale = sqrtscale * sqrtscale; 
  }
  // Support for coarsening to a multigrid
  void OpDiag (const Field &in, Field &out) {};
  void OpDir  (const Field &in, Field &out,int dir,int disp){};

  void Op     (const Field &in, Field &out){
    out = scale * in;
  }
  void AdjOp  (const Field &in, Field &out){
    out = scale * in;
  }
  void HermOp(const Field &in, Field &out){
    double n1, n2;
    HermOpAndNorm(in,out,n1,n2);
  }
  void HermOpAndNorm(const Field &in, Field &out,double &n1,double &n2){
    ComplexD dot;

    out = scale * in;

    dot= innerProduct(in,out);
    n1=real(dot);

    dot = innerProduct(out,out);
    n2=real(dot);
  }
  void ApplySqrt(const Field &in, Field &out){
    out = sqrtscale * in;
  }
};


int main (int argc, char ** argv)
{
  Grid_init(&argc,&argv);

  GridCartesian *grid = SpaceTimeGrid::makeFourDimGrid(GridDefaultLatt(), 
						       GridDefaultSimd(Nd,vComplexF::Nsimd()),
						       GridDefaultMpi());

  double     lo=0.001;
  double     hi=1.0;
  int precision=64;
  int    degree=10;
  AlgRemez remez(lo,hi,precision);

  ////////////////////////////////////////
  // sqrt and inverse sqrt
  ////////////////////////////////////////
  std::cout << "Generating degree "<<degree<<" for x^(1/2)"<<std::endl;
  remez.generateApprox(degree,1,2);

  MultiShiftFunction Sqrt(remez,1.0e-6,false);

  GridParallelRNG  pRNG(grid);  
  std::vector<int> seeds({1,2,3,4});
  pRNG.SeedFixedIntegers(seeds);

  LatticeFermion    src(grid); random(pRNG,src);
  LatticeFermion    combined(grid);
  LatticeFermion    reference(grid);
  LatticeFermion    error(grid);
  std::vector<LatticeFermion> result(degree,grid);
  LatticeFermion    summed(grid);

  ConjugateGradientMultiShift<LatticeFermion> MSCG(10000,Sqrt);

  DumbOperator<LatticeFermion> Diagonal(grid);

  MSCG(Diagonal,src,result);


  //  double a = norm;
  //  for(int n=0;n<poles.size();n++){
  //    a = a + residues[n]/(x+poles[n]);
  //  }
  assert(Sqrt.order==degree);

  combined = Sqrt.norm*src;
  for(int i=0;i<degree;i++){
    combined = combined + Sqrt.residues[i]*result[i];
  }
  
  Diagonal.ApplySqrt(src,reference);

  error = reference - combined;

  std::cout << " Reference "<<norm2(reference)<<std::endl;
  std::cout << " combined  "<<norm2(combined) <<std::endl;
  std::cout << " error     "<<norm2(error)    <<std::endl;

  MSCG(Diagonal,src,summed);
  error = summed - combined;
  std::cout << " summed-combined "<<norm2(error)    <<std::endl;

  Grid_finalize();
}
