#####
#
# SYNOPSIS
#
# AX_CHECK_CUDA
#
# DESCRIPTION
#
# Figures out if CUDA Driver API/nvcc is available, i.e. existence of:
#   nvcc
#   cuda.h
#   libcuda.a
#
# If something isn't found, fails straight away.
#
# The following variables are substituted in the makefile:
# NVCC        : the nvcc compiler command.
# NVCCFLAGS   : nvcc specific flags
# CUDA_CFLAGS : CUDA includes
# CUDA_LDLIBS : CUDA libraries
#
# Defines HAVE_CUDA in config.h
#
# LICENCE
# Public domain
#
#####

AC_DEFUN([AX_CHECK_CUDA], [

# Provide your CUDA path with this
AC_ARG_WITH([cuda],
   [AS_HELP_STRING([--with-cuda=PATH],[prefix where CUDA is installed @<:@default=no@:>@])],
   [],
   [with_cuda=yes])

NVCC=no
CUDA_CFLAGS=
CUDA_LDLIBS=

if test "x$with_cuda" != "xno"
then

   # -----------------------------------------
   # Setup CUDA paths
   # -----------------------------------------
   if test "x$with_cuda" != "xyes"
   then
      AX_NORMALIZE_PATH([with_cuda], ["/"])
      CUDAPATH="$with_cuda"
      CUDA_CFLAGS+=" -I$with_cuda/include"
      CUDA_LDLIBS+=" -L$with_cuda/lib64"
   else
      AC_CHECK_FILE(/usr/local/cuda/,[CUDAPATH="/usr/local/cuda"],[])
      AC_CHECK_FILE(/usr/local/cuda/include,[CUDA_CFLAGS+=" -I/usr/local/cuda/include"],[CUDA_CFLAGS=""])
      AC_CHECK_FILE(/usr/local/cuda/lib64,[CUDA_LDLIBS+=" -L/usr/local/cuda/lib64"],[])
   fi
   CUDA_LDLIBS+=" -lcuda -lcudart -lcublas"


   # -----------------------------------------
   # Checking for nvcc
   # -----------------------------------------
   AC_PATH_PROG([NVCC],[nvcc],[no],[$PATH:$CUDAPATH/bin])
   if test "x$NVCC" = "xno"
   then
      AC_MSG_ERROR([Cannot find nvcc compiler. To enable CUDA, please add path to
                    nvcc in the PATH environment variable and/or specify the path
                    where CUDA is installed using: --with-cuda=PATH])
   fi


   # -----------------------------------------
   # Setup nvcc flags
   # -----------------------------------------

   # Initialize NVCCFLAGS with default values if not already set
   if test -z "$NVCCFLAGS"; then
   NVCCFLAGS="-gencode=arch=compute_60,code=sm_60 \
      -gencode=arch=compute_61,code=sm_61 \
      -gencode=arch=compute_62,code=sm_62 \
      -gencode=arch=compute_70,code=sm_70 \
      -gencode=arch=compute_72,code=sm_72 \
      -gencode=arch=compute_75,code=sm_75 \
      -gencode=arch=compute_80,code=sm_80 \
      -gencode=arch=compute_86,code=sm_86 \
      -gencode=arch=compute_87,code=sm_87 \
      -gencode=arch=compute_89,code=sm_89 \
      -gencode=arch=compute_90,code=sm_90 \
      -gencode=arch=compute_90a,code=sm_90a \
      -gencode=arch=compute_100,code=sm_100 \
      -gencode=arch=compute_100a,code=sm_100a \
      -gencode=arch=compute_101,code=sm_101 \
      -gencode=arch=compute_120,code=sm_120 \
      -w -lineinfo"
   fi

   # Allow users to specify additional nvcc flags
   AC_ARG_VAR(NVCCFLAGS,[Additional nvcc flags (example: NVCCFLAGS="-arch=compute_90 -code=sm_90")])

   # Add -Xcompiler flag with a check for CFLAGS
   if test -n "$CFLAGS"; then
      NVCCFLAGS+= -Xcompiler="$CFLAGS"
   fi

   if test x$DEBUG = xtrue
   then
      NVCCFLAGS+=" -g"
   else
      NVCCFLAGS+=" -O3"
   fi
   AC_ARG_ENABLE([emu],
      AS_HELP_STRING([--enable-emu],[turn on device emulation for CUDA]),
      [case "${enableval}" in
         yes) EMULATION=true;;
         no)  EMULATION=false;;
         *) AC_MSG_ERROR([bad value ${enableval} for --enable-emu]);;
         esac],
      [EMULATION=false])
   if test x$EMULATION = xtrue
   then
      NVCCFLAGS+=" -deviceemu"
   fi


   # -----------------------------------------
   # Check if nvcc works
   # -----------------------------------------
   ac_compile_nvcc=no
   AC_MSG_CHECKING([whether nvcc works])
   cat>conftest.cu<<EOF
   __global__ static void test_cuda() {
      const int tid = threadIdx.x;
      const int bid = blockIdx.x;
      __syncthreads();
   }
EOF

   if $NVCC -c $NVCCFLAGS conftest.cu &> /dev/null
   then
      ac_compile_nvcc=yes
   fi
   rm -f conftest.cu conftest.o
   AC_MSG_RESULT([$ac_compile_nvcc])

   if test "x$ac_compile_nvcc" = "xno"
   then
      AC_MSG_ERROR([CUDA compiler has problems.])
   fi


   # -----------------------------------------
   # Check for headers and libraries
   # -----------------------------------------
   ax_save_CFLAGS="${CFLAGS}"
   ax_save_LIBS="${LIBS}"

   CFLAGS="$CUDA_CFLAGS $CFLAGS"
   LIBS="$CUDA_LDLIBS $LIBS"

   # And the header and the lib
   AC_CHECK_HEADER([cuda.h], [], AC_MSG_FAILURE([Couldn't find cuda.h]), [#include <cuda.h>])
   AC_CHECK_HEADER([cuda_runtime_api.h], [], AC_MSG_FAILURE([Couldn't find cuda_runtime_api.h]), [#include <cuda_runtime_api.h>])
   AC_CHECK_HEADER([cublas.h], [], AC_MSG_FAILURE([Couldn't find cublas.h]), [#include <cublas.h>])
   AC_CHECK_LIB([cuda], [cuInit], [], AC_MSG_FAILURE([Couldn't find libcuda]))
   AC_CHECK_LIB([cudart], [cudaMalloc], [], AC_MSG_FAILURE([Couldn't find libcudart]))
   AC_CHECK_LIB([cublas], [cublasInit], [], AC_MSG_FAILURE([Couldn't find libcublas]))

   # Returning to the original flags
   CFLAGS=${ax_save_CFLAGS}
   LIBS=${ax_save_LIBS}

   AC_DEFINE(HAVE_CUDA,1,[Define if we have CUDA])
fi


# Announcing the new variables
AC_SUBST([NVCC])
AC_SUBST([NVCCFLAGS])
AC_SUBST([CUDA_CFLAGS])
AC_SUBST([CUDA_LDLIBS])
])
