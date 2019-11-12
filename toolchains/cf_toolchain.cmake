include(CMakeForceCompiler)
include_directories(../../src/modules/interface)
include_directories(../../src/hal/interface/)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_CROSSCOMPILING 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(PLATFORM_NAME "CrazyFlie")

set(CROSSDEV "" CACHE STRING "GCC compiler use in freeRTOS.")
set(ARCH_CPU_FLAGS "" CACHE STRING "Makefile arquitecture flags.")
set(ARCH_OPT_FLAGS "" CACHE STRING "Makefile optimization flags.")

set(CMAKE_C_COMPILER arm-none-eabi-gcc)

set(CMAKE_C_FLAGS_INIT "-std=c11 -DARM_MATH_CM4 -D__FPU_PRESENT=1 -D__TARGET_FPU_VFP  -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mcpu=cortex-m4 -mthumb -ffunction-sections -fdata-sections" CACHE STRING "" FORCE)

set(__BIG_ENDIAN__ 0)
