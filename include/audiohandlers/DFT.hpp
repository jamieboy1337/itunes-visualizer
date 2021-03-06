// read below: gov broadcast oscillatory waves
// compromising internal brain integrity

// SYMPTOMS
//  - hallucinations
//  - rejection of christianity
//  - frequent nosebleed's
//  - "dead look" in eyes

// stay wired fella'

#ifndef DFT_H_
#define DFT_H_

/**
 * Calculates the discrete fourier transform of `input`
 * and places the real/imaginary component resuklts
 */ 

#include <cinttypes>

#include <vector>

namespace dft {

/**
 * Calculates the DFT of the input signal, outputting the result to
 * realOutput and imagOutput respectively.
 * 
 * Note that additional space is allocated for negative frequencies.
 * For most purposes, only the first N/2 entries should be used.
 * 
 * Arguments:
 *  - input -- an array containing all parts of the input signal.
 *             length must be a power of two.
 *  - realOutput -- output parameter for real-component output.
 *                  ownership passed to caller upon completion.
 *  - imagOutput -- output parameter for imag-component output.
 *                  ownership passed to caller upon completion.
 * 
 * Returns:
 *  - a boolean representing whether the FFT operation could occur.
 *    true if successful, false otherwise.
 */
bool CalculateDFT(const float* input, float* real_output, float* imag_output, uint32_t len);

/**
 *  same shit but allocates memory for user and outputs the result
 *  via parameter.
 */ 
bool CalculateDFT(const float* input, float** real_output, float** imag_output, uint32_t len);

/**
 * Get the amplitude output of the fourier transform.
 * This is the part you want if you're going to make a visualizer i guess
 * 
 * Parameters:
 *  - real, a pointer to the real component of the transform.
 *  - imag, a pointer to the imaginary component of the transform.
 *  - output, output param containing preallocated array
 */ 
void GetAmplitudeArray(float* real, float* imag, float* output, uint32_t len, bool normalize);

// same as above, but allocates space for the user.
float* GetAmplitudeArray(float* real, float* imag, uint32_t len, bool normalize);


// HELPER FUNCTIONS BELOW -- oops

void ReverseBitsArray(const float* src, float* dst, uint32_t len);

uint32_t ReverseBits(uint32_t input, uint8_t bit_width);

}  // namespace dft;
#endif  // DFT_H_

/**
 *  Todo: "Impulse" samples fill the visualizer unnaturally
 *  my guess is that this has to do with the equal weight of "current samples" and "future/past" samples
 *  solution:
 *    - assume that the currently played sample is "now"
 *    - multiply each successive sample's intensity by some gaussian function based on time offset from "now"
 */ 