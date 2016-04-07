/*!
  @file GaugeConfiguration.h

  @brief Declares the GaugeConfiguration class
*/
#ifndef GAUGE_CONFIG_
#define GAUGE_CONFIG_

namespace Grid {
  
  namespace QCD {
    
    /*!
      @brief Smeared configuration container
      
      It will behave like a configuration from the point of view of
      the HMC update and integrators.
      An "advanced configuration" object that can provide not only the 
      data to store the gauge configuration but also operations to manipulate
      it like smearing.
      
      It stores a list of smeared configurations.
    */
    template <class Gimpl>
      class SmearedConfiguration {
    public:
      INHERIT_GIMPL_TYPES(Gimpl) ;
      
    private:
      const unsigned int smearingLevels;
      Smear_Stout<Gimpl> StoutSmearing;
      std::vector<GaugeField> SmearedSet;
      
      // Member functions
      //====================================================================
      void fill_smearedSet(GaugeField& U){
	ThinLinks = &U; //attach the smearing routine to the field U

	//check the pointer is not null
	if (ThinLinks==NULL) 
	  std::cout << GridLogError << "[SmearedConfiguration] Error in ThinLinks pointer\n";
	
	if (smearingLevels > 0){
	  std::cout<< GridLogDebug << "[SmearedConfiguration] Filling SmearedSet\n";
	  GaugeField previous_u(ThinLinks->_grid);
	  
	  previous_u = *ThinLinks;
	  for(int smearLvl = 0; smearLvl < smearingLevels; ++smearLvl){
	    StoutSmearing.smear(SmearedSet[smearLvl],previous_u);
	    previous_u = SmearedSet[smearLvl];
	  }

	}
      }
      //====================================================================
      GaugeField AnalyticSmearedForce(const GaugeField& SigmaKPrime, 
				      const GaugeField& GaugeK) const{
	GridBase *grid = GaugeK._grid;
	GaugeField C(grid), SigmaK(grid), iLambda(grid);
	GaugeLinkField iLambda_mu(grid);
	GaugeLinkField iQ(grid), e_iQ(grid);
	GaugeLinkField SigmaKPrime_mu(grid);
	GaugeLinkField GaugeKmu(grid), Cmu(grid);

	StoutSmearing.BaseSmear(C, GaugeK);

	for (int mu = 0; mu < Nd; mu++){
	  Cmu            = peekLorentz(     C,mu);
	  GaugeKmu       = peekLorentz(GaugeK,mu);
	  SigmaKPrime_mu = peekLorentz(SigmaKPrime,mu);
	  iQ = Ta(Cmu*adj(GaugeKmu));
	  set_iLambda(iLambda_mu, e_iQ, iQ, SigmaKPrime_mu, GaugeKmu);
	  pokeLorentz(SigmaK, SigmaKPrime_mu*e_iQ + adj(Cmu)*iLambda_mu, mu);
	  pokeLorentz(iLambda, iLambda_mu, mu);
	}
	StoutSmearing.derivative(SigmaK, iLambda, GaugeK);
	return SigmaK;
      }
      /*! @brief Returns smeared configuration at level 'Level' */
      const GaugeField& get_smeared_conf(int Level) const{
	return SmearedSet[Level];
      }
      
      void set_iLambda(GaugeLinkField& iLambda, 
		       GaugeLinkField& e_iQ,
		       const GaugeLinkField& iQ, 
		       const GaugeLinkField& Sigmap,
		       const GaugeLinkField& GaugeK)const{
	GridBase *grid = iQ._grid;
	GaugeLinkField iQ2(grid), iQ3(grid), B1(grid), B2(grid), USigmap(grid);
	GaugeLinkField unity(grid);
	unity=1.0;
	
	LatticeReal u(grid), w(grid);
	LatticeComplex f0(grid), f1(grid), f2(grid);
	LatticeReal xi0(grid), xi1(grid), tmp(grid);
	LatticeReal u2(grid), w2(grid), cosw(grid);
	LatticeComplex emiu(grid), e2iu(grid), qt(grid), fden(grid);
	LatticeComplex r01(grid), r11(grid), r21(grid), r02(grid), r12(grid);
	LatticeComplex r22(grid), tr1(grid), tr2(grid);
	LatticeComplex b10(grid), b11(grid), b12(grid), b20(grid), b21(grid), b22(grid);
	LatticeReal unitReal(grid);

	unitReal = 1.0;
	
	// Exponential
	iQ2 = iQ * iQ;
	iQ3 = iQ * iQ2;
	StoutSmearing.set_uw(u,w,iQ2,iQ3);
	StoutSmearing.set_fj(f0,f1,f2,u,w);
	e_iQ = f0*unity + timesMinusI(f1) * iQ - f2 * iQ2;

	// Getting B1, B2, Gamma and Lambda
	xi0 = StoutSmearing.func_xi0(w);
	xi1 = StoutSmearing.func_xi1(w);
	u2 = u * u;
	w2 = w * w;
	cosw = cos(w);
	
	emiu = toComplex(cos(u)) - timesI(toComplex(u));
	e2iu = toComplex(cos(2.0*u)) + timesI(toComplex(2.0*u));

	r01 = (toComplex(2.0*u) + timesI(toComplex(2.0*(u2-w2)))) * e2iu
	  + emiu * (toComplex(16.0*u*cosw + 2.0*u*(3.0*u2+w2)*xi0) +
		    timesI(toComplex(-8.0*u2*cosw + 2.0*(9.0*u2+w2)*xi0)));
	
	r11 = (toComplex(2.0*unitReal) + timesI(toComplex(4.0*u)))* e2iu
	  + emiu * (toComplex(-2.0*cosw + (3.0*u2-w2)*xi0) +
		    timesI(toComplex(2.0*u*cosw + 6.0*u*xi0)));

	r21 = timesI(toComplex(2.0*unitReal)) * e2iu
	  + emiu * (toComplex(-3.0*u*xi0) + timesI(toComplex(cosw - 3.0*xi0)));

	
	r02 = -2.0 * e2iu + emiu * (toComplex(-8.0*u2*xi0) +
				    timesI(toComplex(2.0*u*(cosw + xi0 + 3.0*u2*xi1))));

	r12 = emiu * (toComplex(2.0*u*xi0) + timesI(toComplex(-cosw - xi0 + 3.0*u2*xi1)));

	r22 = emiu * (toComplex(xi0) - timesI(toComplex(3.0*u*xi1)));

	tmp = (2.0*(9.0*u2-w2)*(9.0*u2-w2));
	fden = toComplex(pow(tmp, -1.0));  // 1/tmp

	b10 = toComplex(2.0*u) * r01 + toComplex(3.0*u2 - w2)*r02 - toComplex(30.0*u2 + 2.0*w2)*f0;
	b11 = toComplex(2.0*u) * r11 + toComplex(3.0*u2 - w2)*r12 - toComplex(30.0*u2 + 2.0*w2)*f1;
	b12 = toComplex(2.0*u) * r21 + toComplex(3.0*u2 - w2)*r22 - toComplex(30.0*u2 + 2.0*w2)*f2;

	b20 = r01 - toComplex(3.0*u)*r02 - toComplex(24.0*u)*f0;
	b21 = r11 - toComplex(3.0*u)*r12 - toComplex(24.0*u)*f1;
	b22 = r21 - toComplex(3.0*u)*r22 - toComplex(24.0*u)*f2;

	b10 *= fden;
	b11 *= fden;
	b12 *= fden;
	b20 *= fden;
	b21 *= fden;
	b22 *= fden;
	
	B1 = b10*unity + timesMinusI(b11) * iQ - b12 * iQ2;
	B2 = b20*unity + timesMinusI(b21) * iQ - b22 * iQ2;
	USigmap = GaugeK * Sigmap;

	tr1 = trace(USigmap*B1);
	tr2 = trace(USigmap*B2);

	GaugeLinkField QUS = timesMinusI(iQ) * USigmap;
	GaugeLinkField USQ = USigmap * timesMinusI(iQ);

	GaugeLinkField iGamma = tr1 * timesMinusI(iQ) - tr2 * iQ2 +
	  f1 * USigmap + f2 * QUS + f2 * USQ;

	iLambda = Ta(iGamma);
	
		
      }
      
    public:
      GaugeField* ThinLinks;      /*!< @brief Pointer to the thin 
				    links configuration */
      
      /*! @brief Standard constructor */
    SmearedConfiguration(GridCartesian * UGrid,
			 unsigned int Nsmear, 
			 Smear_Stout<Gimpl>& Stout):
      smearingLevels(Nsmear),
	StoutSmearing(Stout),
	ThinLinks(NULL){
	for (unsigned int i=0; i< smearingLevels; ++i)
	  SmearedSet.push_back(*(new GaugeField(UGrid)));
      }
      
      /*! For just thin links */
    SmearedConfiguration():
      smearingLevels(0),
	StoutSmearing(),
	SmearedSet(),
	ThinLinks(NULL){}
      
      
      // attach the smeared routines to the thin links U and fill the smeared set
      void set_GaugeField(GaugeField& U){ fill_smearedSet(U);}
      
      void smeared_force(GaugeField& SigmaTilde) const{
	if (smearingLevels > 0){
	  GaugeField force = SigmaTilde;//actually = U*SigmaTilde, check this for Grid
	  GaugeLinkField tmp_mu(SigmaTilde._grid);
	  
	  for (int mu = 0; mu < Nd; mu++){
	    tmp_mu = adj(peekLorentz(SmearedSet[smearingLevels-1], mu)) * peekLorentz(force,mu);
	    pokeLorentz(force, tmp_mu, mu);
	  }
	  for(int ismr = smearingLevels - 1; ismr > 0; --ismr)
	    force = AnalyticSmearedForce(force,get_smeared_conf(ismr-1));
	  
	  force = AnalyticSmearedForce(force,*ThinLinks);
	  
	  for (int mu = 0; mu < Nd; mu++){
	    tmp_mu = peekLorentz(*ThinLinks, mu) * peekLorentz(force, mu);
	    pokeLorentz(SigmaTilde, tmp_mu, mu);
	  }
	}// if smearingLevels = 0 do nothing
      }
      
      
      GaugeField& get_SmearedU(){ 
	return SmearedSet[smearingLevels-1];
      }
      
      GaugeField& get_U(bool smeared=false) { 
	// get the config, thin links by default
	if (smeared){
	  if (smearingLevels) return get_SmearedU();
	  else                return *ThinLinks;
	}
	else return *ThinLinks;
      }
      
    };
    
    
  }
  
}






#endif
