add_library (aubio SHARED)

set_target_properties (aubio PROPERTIES SOVERSION 5.4.8)

find_package(SndFile REQUIRED)

# check for headers
# https://cmake.org/cmake/help/latest/module/CheckIncludeFile.html
include (CheckIncludeFile)
check_include_file ("stdlib.h" HAVE_STDLIB_H)
check_include_file ("stdio.h" HAVE_STDIO_H)
# TODO complex / fftw
#check_include_file ("complex.h" HAVE_COMPLEX_H)
#check_include_file ("fftw3.h" HAVE_FFTW3)
check_include_file ("math.h" HAVE_MATH_H)
check_include_file ("string.h" HAVE_STRING_H)
check_include_file ("errno.h" HAVE_ERRNO_H)
check_include_file ("limits.h" HAVE_LIMITS_H)
check_include_file ("stdarg.h" HAVE_STDARG_H)
# for examples/
check_include_file ("unistd.h" HAVE_UNISTD_H)
check_include_file ("getopt.h" HAVE_GETOPT_H)

# check for optional dependencies
# https://cmake.org/cmake/help/latest/command/find_package.html#command:find_package
# TODO make pkg-config optional
find_package (PkgConfig REQUIRED)

function (add_optional_dependency depname depver)

  pkg_check_modules (${depname} ${depver} ${ARGN})

  if (${depname}_FOUND)
    target_compile_definitions (aubio PUBLIC HAVE_${depname})
    target_compile_options (aubio PRIVATE ${${depname}_CFLAGS})
    target_link_options (aubio PRIVATE ${${depname}_LDFLAGS})
  endif()

endfunction()

# TODO jack

add_optional_dependency (SNDFILE sndfile>=1.0.4)
add_optional_dependency (SAMPLERATE samplerate>=0.0.15)
add_optional_dependency (RUBBERBAND rubberband>=1.3)

add_optional_dependency (LIBAV libavcodec>=54.35.0 libavformat>=52.3.0
                               libavutil>=52.3.0 libswresample>=1.2.0)
add_optional_dependency (VORBISENC vorbisenc vorbis ogg)
add_optional_dependency (FLAC flac)


# generate config.h
target_compile_definitions (aubio PRIVATE HAVE_CONFIG_H)
target_include_directories (aubio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (aubio PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

configure_file (config.h.cmake.in config.h)

target_link_libraries(aubio PRIVATE
	SndFile::sndfile
)
target_include_directories(aubio PUBLIC "aubio/src")

if (WIN32)
    target_link_libraries (aubio PRIVATE ws2_32)
    if (MSVC)
        target_compile_definitions (aubio PRIVATE strncasecmp=_strnicmp)
		target_compile_definitions (aubio PRIVATE AUBIO_DLL_BUILD)
    endif()
endif()

if (SndFile_FOUND)
    target_link_libraries (aubio PRIVATE SndFile::sndfile)
    target_compile_definitions (aubio PRIVATE HAVE_SNDFILE=1)
endif()

set_target_properties(aubio PROPERTIES 
    SOVERSION 5.4.8
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/aubio/bin/
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/aubio/bin/
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)

target_sources (aubio PRIVATE
    "aubio/src/fvec.c"
    "aubio/src/fmat.c"
    "aubio/src/cvec.c"
    "aubio/src/lvec.c"
    "aubio/src/vecutils.c"
    "aubio/src/mathutils.c"
    "aubio/src/musicutils.c"
    # io
    "aubio/src/io/audio_unit.c"
    "aubio/src/io/ioutils.c"
    "aubio/src/io/sink.c"
    "aubio/src/io/sink_apple_audio.c"
    "aubio/src/io/sink_flac.c"
    "aubio/src/io/sink_sndfile.c"
    "aubio/src/io/sink_vorbis.c"
    "aubio/src/io/sink_wavwrite.c"
    "aubio/src/io/source.c"
    "aubio/src/io/source_apple_audio.c"
    "aubio/src/io/source_avcodec.c"
    "aubio/src/io/source_sndfile.c"
    "aubio/src/io/source_wavread.c"
    "aubio/src/io/utils_apple_audio.c"
    # utils
    "aubio/src/utils/log.c"
    "aubio/src/utils/windll.c"
    "aubio/src/utils/hist.c"
    "aubio/src/utils/scale.c"
    "aubio/src/utils/parameter.c"
    "aubio/src/utils/strutils.c"
    # spectral
    "aubio/src/spectral/filterbank_mel.c"
    "aubio/src/spectral/dct_ooura.c"
    "aubio/src/spectral/statistics.c"
    "aubio/src/spectral/tss.c"
    "aubio/src/spectral/dct_fftw.c"
    "aubio/src/spectral/specdesc.c"
    "aubio/src/spectral/awhitening.c"
    "aubio/src/spectral/filterbank.c"
    "aubio/src/spectral/dct_accelerate.c"
    "aubio/src/spectral/fft.c"
    "aubio/src/spectral/dct.c"
    "aubio/src/spectral/mfcc.c"
    "aubio/src/spectral/phasevoc.c"
    "aubio/src/spectral/dct_plain.c"
    "aubio/src/spectral/dct_ipp.c"
    "aubio/src/spectral/ooura_fft8g.c"
    #temporal
    "aubio/src/temporal/biquad.c"
    "aubio/src/temporal/filter.c"
    "aubio/src/temporal/a_weighting.c"
    "aubio/src/temporal/c_weighting.c"
    "aubio/src/temporal/resampler.c"
    # pitch
    "aubio/src/pitch/pitchspecacf.c"
    "aubio/src/pitch/pitchschmitt.c"
    "aubio/src/pitch/pitchmcomb.c"
    "aubio/src/pitch/pitchyinfast.c"
    "aubio/src/pitch/pitchyin.c"
    "aubio/src/pitch/pitchfcomb.c"
    "aubio/src/pitch/pitchyinfft.c"
    "aubio/src/pitch/pitch.c"
    # synth
    "aubio/src/synth/sampler.c"
    "aubio/src/synth/wavetable.c"
    # notes
    "aubio/src/notes/notes.c"
    # tempo
    "aubio/src/tempo/tempo.c"
    "aubio/src/tempo/beattracking.c"
    # effects
    "aubio/src/effects/rubberband_utils.c"
    "aubio/src/effects/pitchshift_dummy.c"
    "aubio/src/effects/pitchshift_rubberband.c"
    "aubio/src/effects/timestretch_dummy.c"
    "aubio/src/effects/timestretch_rubberband.c"
    # onset
    "aubio/src/onset/peakpicker.c"
    "aubio/src/onset/onset.c"
)
