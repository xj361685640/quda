#include <cstdlib>
#include <cstdio>
#include <string>
#include <iostream>

#include <color_spinor_field.h>
#include <clover_field.h>

//these are access control for staggered action
#ifdef GPU_STAGGERED_DIRAC
#if (__COMPUTE_CAPABILITY__ >= 300) // Kepler works best with texture loads only
//#define DIRECT_ACCESS_FAT_LINK
//#define DIRECT_ACCESS_LONG_LINK
//#define DIRECT_ACCESS_SPINOR
//#define DIRECT_ACCESS_ACCUM
//#define DIRECT_ACCESS_INTER
//#define DIRECT_ACCESS_PACK
#else // Fermi
//#define DIRECT_ACCESS_FAT_LINK
//#define DIRECT_ACCESS_LONG_LINK
//#define DIRECT_ACCESS_SPINOR
//#define DIRECT_ACCESS_ACCUM
//#define DIRECT_ACCESS_INTER
//#define DIRECT_ACCESS_PACK
#endif

#endif // GPU_STAGGERED_DIRAC

#include <quda_internal.h>
#include <dslash_quda.h>
#include <sys/time.h>
#include <blas_quda.h>

#include <inline_ptx.h>

namespace quda {

  namespace staggered {
#include <dslash_constants.h>
#include <dslash_textures.h>
#include <dslash_index.cuh>

#undef GPU_CLOVER_DIRAC
#undef GPU_DOMAIN_WALL_DIRAC
#define DD_IMPROVED 0
#include <staggered_dslash_def.h> // staggered Dslash kernels
#undef DD_IMPROVED

#include <dslash_quda.cuh>
  } // end namespace staggered

  // declare the dslash events
#include <dslash_events.cuh>

  using namespace staggered;

#ifdef GPU_STAGGERED_DIRAC
  template <typename sFloat, typename gFloat>
  class StaggeredDslashCuda : public DslashCuda {

  private:
    const unsigned int nSrc;
    const unsigned int max_register_block = 4;

  protected:
    bool tuneAuxDim() const { return true; } // Do tune the aux dimensions.
    unsigned int sharedBytesPerThread() const
    {
#ifdef PARALLEL_DIR
      int reg_size = (typeid(sFloat)==typeid(double2) ? sizeof(double) : sizeof(float));
      return 6 * reg_size;
#else
      return 0;
#endif
    }

  public:
    StaggeredDslashCuda(cudaColorSpinorField *out, const gFloat *gauge0, const gFloat *gauge1,
			const QudaReconstructType reconstruct, const cudaColorSpinorField *in,
			const cudaColorSpinorField *x, const double a, const int dagger)
      : DslashCuda(out, in, x, reconstruct, dagger), nSrc(in->X(4))
    { 
      dslashParam.gauge0 = (void*)gauge0;
      dslashParam.gauge1 = (void*)gauge1;
      dslashParam.a = a;
      dslashParam.a_f = a;
    }

    virtual ~StaggeredDslashCuda() { unbindSpinorTex<sFloat>(in, out, x); }

    void apply(const cudaStream_t &stream)
    {
#ifdef USE_TEXTURE_OBJECTS
      dslashParam.ghostTex = in->GhostTex();
      dslashParam.ghostTexNorm = in->GhostTexNorm();
#else
      bindSpinorTex<sFloat>(in, out, x);
#endif // USE_TEXTURE_OBJECTS

      TuneParam tp = tuneLaunch(*this, getTuning(), getVerbosity());
      dslashParam.swizzle = tp.aux.x;
      switch(tp.aux.y) {
      case 1:
	{
	  constexpr int register_block_size = 1;
	  STAGGERED_DSLASH(tp.grid, tp.block, tp.shared_bytes, stream, dslashParam);
	  break;
	}
#ifdef BLOCKSOLVER
      case 2:
	{
	  constexpr int register_block_size = 2;
	  STAGGERED_DSLASH(tp.grid, tp.block, tp.shared_bytes, stream, dslashParam);
	  break;
	}
      case 3:
	{
	  constexpr int register_block_size = 3;
	  STAGGERED_DSLASH(tp.grid, tp.block, tp.shared_bytes, stream, dslashParam);
	  break;
	}
      case 4:
	{
	  constexpr int register_block_size = 4;
	  STAGGERED_DSLASH(tp.grid, tp.block, tp.shared_bytes, stream, dslashParam);
	  break;
	}
#if 0 // disable these for now for compile time
      case 5:
	{
	  constexpr int register_block_size = 5;
	  STAGGERED_DSLASH(tp.grid, tp.block, tp.shared_bytes, stream, dslashParam);
	  break;
	}
      case 6:
	{
	  constexpr int register_block_size = 6;
	  STAGGERED_DSLASH(tp.grid, tp.block, tp.shared_bytes, stream, dslashParam);
	  break;
	}
#endif // 0
#endif // BLOCKSOLVER
      default:
	errorQuda("Register blocking factor %d not supported", tp.aux.y);
      }
    }

    bool advanceBlockDim(TuneParam &param) const
    {
      // note here we assume that the register block size is constant
      // (auxiliary dimension is advanced slower than block/grid dimensions)
      const unsigned int max_shared = deviceProp.sharedMemPerBlock;
      const unsigned int y_length = nSrc / param.aux.y; // y-thread dimension is nSrc / register block size
      // first try to advance block.y
      if (param.block.y < y_length && param.block.y < (unsigned int)deviceProp.maxThreadsDim[1] &&
	  sharedBytesPerThread()*param.block.x*param.block.y < max_shared &&
	  (param.block.x*(param.block.y+1u)) <= (unsigned int)deviceProp.maxThreadsPerBlock) {
	param.block.y++;
	param.grid.y = (y_length + param.block.y - 1) / param.block.y;
	return true;
      } else { // if can't advance, reset and advance block.x
	bool rtn = DslashCuda::advanceBlockDim(param);
	param.block.y = 1;
	param.grid.y = y_length;
	return rtn;
      }
    }

    bool advanceAux(TuneParam &param) const
    {
#ifdef SWIZZLE
      if (dslashParam.kernel_type != INTERIOR_KERNEL) { // only swizzle the halo kernels
	if (param.aux.x < 2*deviceProp.multiProcessorCount) {
	  param.aux.x++;
	  return true;
	}
      }
#endif // SWIZZLE
      param.aux.x = 1;

#ifdef BLOCKSOLVER // register tiling of multi-src dslash only enabled if compiling the block solver
      for (unsigned int register_block=param.aux.y+1; register_block <= max_register_block && register_block <= nSrc; register_block++) {
	if (nSrc % register_block == 0) { // register block size must be a divisor of 5-th dimention
	  param.aux.y = register_block;
	  return true;
	}
      }
#endif // BLOCKSOLVER
      param.aux.y = 1;
      return false;
    }

    void initTuneParam(TuneParam &param) const
    {
      DslashCuda::initTuneParam(param);
      param.block.y = 1;
      param.grid.y = nSrc;
      param.aux.x = 1;
      param.aux.y = 1;
    }

    void defaultTuneParam(TuneParam &param) const { initTuneParam(param); }

    int Nface() const { return 2; }
  };
#endif // GPU_STAGGERED_DIRAC

#include <dslash_policy.cuh>

  void staggeredDslashCuda(cudaColorSpinorField *out, const cudaGaugeField &gauge, 
			   const cudaColorSpinorField *in, const int parity, 
			   const int dagger, const cudaColorSpinorField *x,
			   const double &k, const int *commOverride, TimeProfile &profile)
  {
    inSpinor = (cudaColorSpinorField*)in; // EVIL
    inSpinor->createComms(1);

#ifdef GPU_STAGGERED_DIRAC

    dslashParam.Ls = out->X(4);

    dslashParam.parity = parity;
    dslashParam.gauge_stride = gauge.Stride();
    dslashParam.fat_link_max = gauge.LinkMax(); // May need to use this in the preconditioning step 
    dslashParam.is_composite = out->IsComposite();//TODO: also one needs to check somewhere that both 'in' and 'out' are composite fields
    dslashParam.composite_Vh = out->IsComposite() ? out->Component(0).ComponentVolumeCB() : 0 ;

    // in the solver for the improved staggered action

    for(int i=0;i<4;i++){
      dslashParam.ghostDim[i] = comm_dim_partitioned(i); // determines whether to use regular or ghost indexing at boundary
      dslashParam.ghostOffset[i][0] = in->GhostOffset(i,0)/in->FieldOrder();
      dslashParam.ghostOffset[i][1] = in->GhostOffset(i,1)/in->FieldOrder();
      dslashParam.ghostNormOffset[i][0] = in->GhostNormOffset(i,0);
      dslashParam.ghostNormOffset[i][1] = in->GhostNormOffset(i,1);
      dslashParam.commDim[i] = (!commOverride[i]) ? 0 : comm_dim_partitioned(i); // switch off comms if override = 0
    }
    void *gauge0, *gauge1;
    bindGaugeTex(gauge, parity, &gauge0, &gauge1);

    if (in->Precision() != gauge.Precision()) {
      errorQuda("Mixing precisions gauge=%d and spinor=%d not supported",
		gauge.Precision(), in->Precision());
    }

    if (gauge.Reconstruct() == QUDA_RECONSTRUCT_9 || gauge.Reconstruct() == QUDA_RECONSTRUCT_13) {
      errorQuda("Reconstruct %d not supported", gauge.Reconstruct());
    }

    DslashCuda *dslash = 0;
    size_t regSize = sizeof(float);

    if (in->Precision() == QUDA_DOUBLE_PRECISION) {
      dslash = new StaggeredDslashCuda<double2, double2>
	(out, (double2*)gauge0, (double2*)gauge1, gauge.Reconstruct(), in, x, k, dagger);
      regSize = sizeof(double);
    } else if (in->Precision() == QUDA_SINGLE_PRECISION) {
      dslash = new StaggeredDslashCuda<float2, float2>
	(out, (float2*)gauge0, (float2*)gauge1, gauge.Reconstruct(), in, x, k, dagger);
    } else if (in->Precision() == QUDA_HALF_PRECISION) {	
      dslash = new StaggeredDslashCuda<short2, short2>
	(out, (short2*)gauge0, (short2*)gauge1, gauge.Reconstruct(), in, x, k, dagger);
    }

    // the parameters passed to dslashCuda must be 4-d volume and 3-d
    // faces because Ls is added as the y-dimension in thread space
    int ghostFace[QUDA_MAX_DIM];
    for (int i=0; i<4; i++) ghostFace[i] = in->GhostFace()[i] / in->X(4);

    DslashPolicyTune dslash_policy(*dslash, const_cast<cudaColorSpinorField*>(in), regSize, parity, dagger,  in->Volume()/in->X(4), ghostFace, profile);
    dslash_policy.apply(0);

    delete dslash;
    unbindGaugeTex(gauge);

    checkCudaError();

#else
    errorQuda("Staggered dslash has not been built");
#endif  // GPU_STAGGERED_DIRAC
  }

}
