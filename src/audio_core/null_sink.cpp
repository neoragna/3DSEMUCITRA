#include "null_sink.h"

/**
* Feed stereo samples to sink.
* @param samples Samples in interleaved stereo PCM16 format. Size of vector must be multiple of two.
*/

inline void AudioCore::NullSink::EnqueueSamples(const std::vector<s16>& samples) {}
