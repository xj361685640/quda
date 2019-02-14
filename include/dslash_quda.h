#ifndef _DSLASH_QUDA_H
#define _DSLASH_QUDA_H

#include <quda_internal.h>
#include <tune_quda.h>
#include <color_spinor_field.h>
#include <gauge_field.h>
#include <clover_field.h>
#include <worker.h>

namespace quda {

  /**
    @param pack Sets whether to use a kernel to pack the T dimension
    */
  void setKernelPackT(bool pack);

  /**
    @return Whether the T dimension is kernel packed or not
    */
  bool getKernelPackT();

  void pushKernelPackT(bool pack);
  void popKernelPackT();

  /**
     Sets commDim array used in dslash_pack.cu
   */
  void setPackComms(const int *commDim);

  bool getDslashLaunch();

  void createDslashEvents();
  void destroyDslashEvents();

#ifndef USE_LEGACY_DSLASH

  /**
     @brief Driver for applying the Wilson stencil

     out = D * in

     where D is the gauged Wilson linear operator.

     If kappa is non-zero, the operation is given by out = x + kappa * D in.
     This operator can be applied to both single parity
     (checker-boarded) fields, or to full fields.

     @param[out] out The output result field
     @param[in] in The input field
     @param[in] U The gauge field used for the operator
     @param[in] kappa Scale factor applied
     @param[in] x Vector field we accumulate onto to
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyWilson(ColorSpinorField &out, const ColorSpinorField &in, const GaugeField &U,
                   double kappa, const ColorSpinorField &x, int parity, bool dagger,
                   const int *comm_override, TimeProfile &profile);

  /**
     @brief Driver for applying the Wilson-clover stencil

     out = A * x + kappa * D * in

     where D is the gauged Wilson linear operator.

     This operator can be applied to both single parity
     (checker-boarded) fields, or to full fields.

     @param[out] out The output result field
     @param[in] in Input field that D is applied to
     @param[in] x Input field that A is applied to
     @param[in] U The gauge field used for the operator
     @param[in] A The clover field used for the operator
     @param[in] kappa Scale factor applied
     @param[in] x Vector field we accumulate onto to
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyWilsonClover(ColorSpinorField &out, const ColorSpinorField &in,
                         const GaugeField &U, const CloverField &A,
                         double kappa, const ColorSpinorField &x, int parity, bool dagger,
                         const int *comm_override, TimeProfile &profile);

  /**
     @brief Driver for applying the preconditioned Wilson-clover stencil

     out = A^{-1} * D * in + x

     where D is the gauged Wilson linear operator and A is the clover
     field.  This operator can (at present) be applied to only single
     parity (checker-boarded) fields.  When the dagger operator is
     requested, we do not transpose the order of operations, e.g.

     out = A^{-\dagger} D^\dagger  (no xpay term)

     Although not a conjugate transpose of the regular operator, this
     variant is used to enable kernel fusion between the application
     of D and the subsequent application of A, e.g., in the symmetric
     dagger operator we need to apply

     M = (1 - kappa^2 D^{\dagger} A^{-1} D{^\dagger} A^{-1} )

     and since cannot fuse D{^\dagger} A^{-\dagger}, we instead fused
     A^{-\dagger} D{^\dagger}.

     If kappa is non-zero, the operation is given by out = x + kappa * A^{-1} D in.
     This operator can (at present) be applied to only single parity
     (checker-boarded) fields.

     @param[out] out The output result field
     @param[in] in The input field
     @param[in] U The gauge field used for the operator
     @param[in] A The clover field used for the operator
     @param[in] kappa Scale factor applied
     @param[in] x Vector field we accumulate onto to
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyWilsonCloverPreconditioned(ColorSpinorField &out, const ColorSpinorField &in,
                                       const GaugeField &U, const CloverField &A,
                                       double kappa, const ColorSpinorField &x, int parity, bool dagger,
                                       const int *comm_override, TimeProfile &profile);

  /**
     @brief Driver for applying the twisted-mass stencil

     out = a * D * in + (1 + i*b*gamma_5) * x

     where D is the gauged Wilson linear operator.

     This operator can be applied to both single parity
     (checker-boarded) fields, or to full fields.

     @param[out] out The output result field
     @param[in] in The input field
     @param[in] U The gauge field used for the operator
     @param[in] a Scale factor applied to Wilson term (typically -kappa)
     @param[in] b Twist factor applied (typically 2*mu*kappa)
     @param[in] x Vector field we accumulate onto to
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyTwistedMass(ColorSpinorField &out, const ColorSpinorField &in, const GaugeField &U,
                        double a, double b, const ColorSpinorField &x, int parity, bool dagger,
                        const int *comm_override, TimeProfile &profile);

  /**
     @brief Driver for applying the preconditioned twisted-mass stencil

     out = a*(1 + i*b*gamma_5) * D * in + x

     where D is the gauged Wilson linear operator.  This operator can
     (at present) be applied to only single parity (checker-boarded)
     fields.  For the dagger operator, we generally apply the
     conjugate transpose operator

     out = x + D^\dagger A^{-\dagger}

     with the additional asymmetric special case, where we apply do not
     transpose the order of operations

     out = A^{-\dagger} D^\dagger  (no xpay term)

     This variant is required when have the asymmetric preconditioned
     operator and require the preconditioned twist term to remain in
     between the applications of D.  This would be combined with a
     subsequent non-preconditioned dagger operator, A*x - kappa^2 D, to
     form the full operator.

     @param[out] out The output result field
     @param[in] in The input field
     @param[in] U The gauge field used for the operator
     @param[in] a Scale factor applied to Wilson term ( typically kappa^2 / (1 + b*b) )
     @param[in] b Twist factor applied (typically -2*kappa*mu)
     @param[in] xpay Whether to do xpay or not
     @param[in] x Vector field we accumulate onto to when xpay is true
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] asymmetric Whether this is for the asymmetric preconditioned dagger operator (a*(1 - i*b*gamma_5) * D^dagger * in)
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyTwistedMassPreconditioned(ColorSpinorField &out, const ColorSpinorField &in, const GaugeField &U,
                                      double a, double b, bool xpay, const ColorSpinorField &x, int parity, bool dagger,
                                      bool asymmetric, const int *comm_override, TimeProfile &profile);

  /**
     @brief Driver for applying the non-degenerate twisted-mass
     stencil

     out = a * D * in + (1 + i*b*gamma_5*tau_3 + c*tau_1) * x

     where D is the gauged Wilson linear operator.  The quark fields
     out, in and x are five dimensional, with the fifth dimension
     corresponding to the flavor dimension.  The convention is that
     the first 4-d slice (s=0) corresponds to the positive twist and
     the second slice (s=1) corresponds to the negative twist.

     This operator can be applied to both single parity
     (4d checker-boarded) fields, or to full fields.

     @param[out] out The output result field
     @param[in] in The input field
     @param[in] U The gauge field used for the operator
     @param[in] a Scale factor applied to Wilson term (typically -kappa)
     @param[in] b Chiral twist factor applied (typically 2*mu*kappa)
     @param[in] c Flavor twist factor applied (typically -2*epsilon*kappa)
     @param[in] x Vector field we accumulate onto to
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyNdegTwistedMass(ColorSpinorField &out, const ColorSpinorField &in, const GaugeField &U,
                            double a, double b, double c, const ColorSpinorField &x, int parity, bool dagger,
                            const int *comm_override, TimeProfile &profile);

  /**
     @brief Driver for applying the preconditioned non-degenerate
     twisted-mass stencil

     out = a * (1 + i*b*gamma_5*tau_3 + c*tau_1) * D * in + x

     where D is the gauged Wilson linear operator.  The quark fields
     out, in and x are five dimensional, with the fifth dimension
     corresponding to the flavor dimension.  The convention is that
     the first 4-d slice (s=0) corresponds to the positive twist and
     the second slice (s=1) corresponds to the negative twist.

     This operator can (at present) be applied to only single parity
     (checker-boarded) fields.

     For the dagger operator, we generally apply the
     conjugate transpose operator

     out = x + D^\dagger A^{-\dagger}

     with the additional asymmetric special case, where we apply do not
     transpose the order of operations

     out = A^{-\dagger} D^\dagger  (no xpay term)

     This variant is required when have the asymmetric preconditioned
     operator and require the preconditioned twist term to remain in
     between the applications of D.  This would be combined with a
     subsequent non-preconditioned dagger operator, A*x - kappa^2 D, to
     form the full operator.

     @param[out] out The output result field
     @param[in] in The input field
     @param[in] U The gauge field used for the operator
     @param[in] a Scale factor applied to Wilson term (typically -kappa^2/(1 + b*b -c*c) )
     @param[in] b Chiral twist factor applied (typically -2*mu*kappa)
     @param[in] c Flavor twist factor applied (typically 2*epsilon*kappa)
     @param[in] xpay Whether to do xpay or not
     @param[in] x Vector field we accumulate onto to
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] asymmetric Whether this is for the asymmetric preconditioned dagger operator (a*(1 - i*b*gamma_5) * D^dagger * in)
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyNdegTwistedMassPreconditioned(ColorSpinorField &out, const ColorSpinorField &in, const GaugeField &U,
                                          double a, double b, double c, bool xpay, const ColorSpinorField &x, int parity, bool dagger,
                                          bool asymmetric, const int *comm_override, TimeProfile &profile);

/**
     @brief Driver for applying the twisted-clover stencil

     out = a * D * in + (C + i*b*gamma_5) * x

     where D is the gauged Wilson linear operator, and C is the clover
     field.

     This operator can be applied to both single parity
     (4d checker-boarded) fields, or to full fields.

     @param[out] out The output result field
     @param[in] in The input field
     @param[in] U The gauge field used for the operator
     @param[in] C The clover field used for the operator
     @param[in] a Scale factor applied to Wilson term (typically -kappa)
     @param[in] b Chiral twist factor applied (typically 2*mu*kappa)
     @param[in] x Vector field we accumulate onto to
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyTwistedClover(ColorSpinorField &out, const ColorSpinorField &in, const GaugeField &U, const CloverField &C,
                          double a, double b, const ColorSpinorField &x, int parity, bool dagger,
                          const int *comm_override, TimeProfile &profile);

  /**
     @brief Driver for applying the preconditioned twisted-clover stencil

     out = a * (C + i*b*gamma_5)^{-1} * D * in + x
         = a * C^{-2} (C - i*b*gamma_5) * D * in + x
         = A^{-1} * D * in + x

     where D is the gauged Wilson linear operator and C is the clover
     field.  This operator can (at present) be applied to only single
     parity (checker-boarded) fields.  When the dagger operator is
     requested, we do not transpose the order of operations, e.g.

     out = A^{-\dagger} D^\dagger  (no xpay term)

     Although not a conjugate transpose of the regular operator, this
     variant is used to enable kernel fusion between the application
     of D and the subsequent application of A, e.g., in the symmetric
     dagger operator we need to apply

     M = (1 - kappa^2 D^{\dagger} A^{-\dagger} D{^\dagger} A^{-\dagger} )

     and since cannot fuse D{^\dagger} A^{-\dagger}, we instead fused
     A^{-\dagger} D{^\dagger}.

     @param[out] out The output result field
     @param[in] in The input field
     @param[in] U The gauge field used for the operator
     @param[in] C The clover field used for the operator
     @param[in] a Scale factor applied to Wilson term ( typically 1 / (1 + b*b) or kappa^2 / (1 + b*b) )
     @param[in] b Twist factor applied (typically -2*kappa*mu)
     @param[in] xpay Whether to do xpay or not
     @param[in] x Vector field we accumulate onto to when xpay is true
     @param[in] parity Destination parity
     @param[in] dagger Whether this is for the dagger operator
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyTwistedCloverPreconditioned(ColorSpinorField &out, const ColorSpinorField &in, const GaugeField &U, const CloverField &C,
                                        double a, double b, bool xpay, const ColorSpinorField &x, int parity, bool dagger,
                                        const int *comm_override, TimeProfile &profile);

#else

  // plain Wilson Dslash
  void wilsonDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in,
			const int oddBit, const int daggerBit, const cudaColorSpinorField *x,
			const double &k, const int *commDim, TimeProfile &profile);

  // clover Dslash
  void cloverDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge,
			const FullClover &cloverInv, const cudaColorSpinorField *in,
			const int oddBit, const int daggerBit, const cudaColorSpinorField *x,
			const double &k, const int *commDim, TimeProfile &profile);

  // clover Dslash
  void asymCloverDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge,
			    const FullClover &cloverInv, const cudaColorSpinorField *in,
			    const int oddBit, const int daggerBit, const cudaColorSpinorField *x,
			    const double &k, const int *commDim, TimeProfile &profile);

  // twisted mass Dslash
  void twistedMassDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const   cudaColorSpinorField *in,
			     const int parity, const int dagger, const cudaColorSpinorField *x, const QudaTwistDslashType type,
			     const double &kappa, const double &mu, const double &epsilon, const double &k, const int *commDim, TimeProfile &profile);

  // twisted mass Dslash
  void ndegTwistedMassDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const   cudaColorSpinorField *in,
				 const int parity, const int dagger, const cudaColorSpinorField *x, const QudaTwistDslashType type,
				 const double &kappa, const double &mu, const double &epsilon, const double &k, const int *commDim,
				 TimeProfile &profile);

  // twisted clover Dslash
  void twistedCloverDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge,
			       const FullClover *clover, const FullClover *cloverInv, const   cudaColorSpinorField *in,
			       const int parity, const int dagger, const cudaColorSpinorField *x, const QudaTwistCloverDslashType type,
			       const double &kappa, const double &mu, const double &epsilon, const double &k, const int *commDim,
			       TimeProfile &profile);

#endif

  /**
     @brief Apply clover-matrix field to a color-spinor field
     @param[out] out Result color-spinor field
     @param[in] in Input color-spinor field
     @param[in] clover Clover-matrix field
     @param[in] inverse Whether we are applying the inverse or not
     @param[in] Field parity (if color-spinor field is single parity)
  */
  void ApplyClover(ColorSpinorField &out, const ColorSpinorField &in,
		   const CloverField &clover, bool inverse, int parity);

  enum Dslash5Type {
    DSLASH5_DWF,
    DSLASH5_MOBIUS_PRE,
    DSLASH5_MOBIUS,
    M5_INV_DWF,
    M5_INV_MOBIUS,
    M5_INV_ZMOBIUS
  };

  /**
     @brief Apply either the domain-wall / mobius Dslash5 operator or
     the M5 inverse operator.  In the current implementation, it is
     expected that the color-spinor fields are 4-d preconditioned.
     @param[out] out Result color-spinor field
     @param[in] in Input color-spinor field
     @param[in] x Auxilary input color-spinor field
     @param[in] m_f Fermion mass parameter
     @param[in] m_5 Wilson mass shift
     @param[in] b_5 Mobius coefficient array (length Ls)
     @param[in] c_5 Mobius coefficient array (length Ls)
     @param[in] a Scale factor use in xpay operator
     @param[in] dagger Whether this is for the dagger operator
     @param[in] type Type of dslash we are applying
  */
  void ApplyDslash5(ColorSpinorField &out, const ColorSpinorField &in, const ColorSpinorField &x,
		    double m_f, double m_5, const Complex *b_5, const Complex *c_5,
		    double a, bool dagger, Dslash5Type type);

  /**
     @brief Apply the 5-d domain-wall stencil operator

     out = x + kappa * D_5 * in

     where D_5 is the 5-d Wilson linear operator

     This operator can be applied to both single parity
     (5-d checker-boarded) fields, or to full fields.

     @param[out] out Result color-spinor field
     @param[in] in Input color-spinor field
     @param[in] U Gauge field
     @param[in] x Auxilary input color-spinor field
     @param[in] m_f Fermion mass parameter
     @param[in] kappa Scale factor use in xpay operator
     @param[in] Field parity (if color-spinor field is single parity)
     @param[in] dagger Whether this is for the dagger operator
     @param[in] comm_override Override for which dimensions are partitioned
     @param[in] profile The TimeProfile used for profiling the dslash
  */
  void ApplyDWF(ColorSpinorField &out, const ColorSpinorField &in,
                const GaugeField &U, const ColorSpinorField &x,
                double m_f, double kappa, int parity, bool dagger,
                const int *comm_override, TimeProfile &profile);

  // domain wall Dslash
  void domainWallDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in,
			    const int parity, const int dagger, const cudaColorSpinorField *x,
			    const double &m_f, const double &k, const int *commDim, TimeProfile &profile);

  // Added for 4d EO preconditioning in DWF
  void domainWallDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in,
			    const int parity, const int dagger, const cudaColorSpinorField *x,
			    const double &m_f, const double &a, const double &b,
			    const int *commDim, const int DS_type, TimeProfile &profile);

  // Added for 4d EO preconditioning in Mobius DWF
  void MDWFDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, const cudaColorSpinorField *in,
		      const int parity, const int dagger, const cudaColorSpinorField *x, const double &m_f, const double &k,
		      const double *b5, const double *c_5, const double &m5,
                      const int *commDim, const int DS_type, TimeProfile &profile);

  // staggered Dslash
  void staggeredDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge,
			   const cudaColorSpinorField *in, const int parity, const int dagger,
			   const cudaColorSpinorField *x, const double &k,
			   const int *commDim, TimeProfile &profile);

  void ApplyDslashStaggered(ColorSpinorField& out, const ColorSpinorField& in, const GaugeField& U, const GaugeField& L, double a, const ColorSpinorField& x,
      int parity, bool dagger, bool improved, const int* comm_override, TimeProfile& profile);

  // improved staggered Dslash
  void improvedStaggeredDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &fatGauge, const cudaGaugeField &longGauge,
				   const cudaColorSpinorField *in, const int parity, const int dagger,
				   const cudaColorSpinorField *x, const double &k,
				   const int *commDim, TimeProfile &profile);

  /**
     @brief Apply the twisted-mass gamma operator to a color-spinor field.
     @param[out] out Result color-spinor field
     @param[in] in Input color-spinor field
     @param[in] d Which gamma matrix we are applying (C counting, so gamma_5 has d=4)
     @param[in] kappa kappa parameter
     @param[in] mu mu parameter
     @param[in] epsilon epsilon parameter
     @param[in] dagger Whether we are applying the dagger or not
     @param[in] twist The type of kernel we are doing
  */
  void ApplyTwistGamma(ColorSpinorField &out, const ColorSpinorField &in, int d, double kappa, double mu,
		       double epsilon, int dagger, QudaTwistGamma5Type type);

  /**
     @brief Apply twisted clover-matrix field to a color-spinor field
     @param[out] out Result color-spinor field
     @param[in] in Input color-spinor field
     @param[in] clover Clover-matrix field
     @param[in] kappa kappa parameter
     @param[in] mu mu parameter
     @param[in] epsilon epsilon parameter
     @param[in] Field parity (if color-spinor field is single parity)
     @param[in] dagger Whether we are applying the dagger or not
     @param[in] twist The type of kernel we are doing
       if (twist == QUDA_TWIST_GAMMA5_DIRECT) apply (Clover + i*a*gamma_5) to the input spinor
       else if (twist == QUDA_TWIST_GAMMA5_INVERSE) apply (Clover + i*a*gamma_5)/(Clover^2 + a^2) to the input spinor
  */
  void ApplyTwistClover(ColorSpinorField &out, const ColorSpinorField &in, const CloverField &clover,
			double kappa, double mu, double epsilon, int parity, int dagger, QudaTwistGamma5Type twist);


  /**
     @brief Dslash face packing routine
     @param[out] ghost_buf Array of packed halos, order is [2*dim+dir]
     @param[in] in Input ColorSpinorField to be packed
     @param[in] location Locations where the packed fields are (Device, Host and/or Remote)
     @param[in] nFace Depth of halo
     @param[in] dagger Whether this is for the dagger operator
     @param[in] parity Field parity
     @param[in] dim Which dimensions we are packing
     @param[in] face_num Are we packing backwards (0), forwards (1) or both directions (2)
     @param[in] stream Which stream are we executing in
     @param[in] a Packing coefficient (twisted-mass only)
     @param[in] b Packing coefficient (twisted-mass only)
  */
  void packFace(void *ghost_buf[2*QUDA_MAX_DIM], cudaColorSpinorField &in, MemoryLocation location,
		const int nFace, const int dagger, const int parity, const int dim, const int face_num,
		const cudaStream_t &stream, const double a=0.0, const double b=0.0);

  void packFaceExtended(void *ghost_buf[2*QUDA_MAX_DIM], cudaColorSpinorField &field, MemoryLocation location,
			const int nFace, const int R[], const int dagger, const int parity, const int dim,
			const int face_num, const cudaStream_t &stream, const bool unpack=false);

  /**
     @brief Dslash face packing routine
     @param[out] ghost_buf Array of packed halos, order is [2*dim+dir]
     @param[in] field ColorSpinorField to be packed
     @param[in] location Locations where the packed fields are (Device, Host and/or Remote)
     @param[in] nFace Depth of halo
     @param[in] dagger Whether this is for the dagger operator
     @param[in] parity Field parity
     @param[in] a Twisted mass scale factor (for preconditioned twisted-mass dagger operator)
     @param[in] b Twisted mass chiral twist factor (for preconditioned twisted-mass dagger operator)
     @param[in] c Twisted mass flavor twist factor (for preconditioned non degenerate twisted-mass dagger operator)
     @param[in] stream Which stream are we executing in
  */
  void PackGhost(void *ghost[2*QUDA_MAX_DIM], const ColorSpinorField &field,
                 MemoryLocation location, int nFace, bool dagger, int parity,
                 double a, double b, double c, const cudaStream_t &stream);

  /**
     @brief Applies a gamma5 matrix to a spinor (wrapper to ApplyGamma)
     @param[out] out Output field
     @param[in] in Input field
  */
  void gamma5(ColorSpinorField &out, const ColorSpinorField &in);

}

#endif // _DSLASH_QUDA_H
