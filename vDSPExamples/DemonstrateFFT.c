/*	This is a sample module to illustrate the use of vDSP's FFT functions.
	This module also times the functions.

	Copyright (C) 2007 Apple Inc.  All rights reserved.
*/


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Accelerate/Accelerate.h>

#include "Demonstrate.h"


#define Iterations	10000	// Number of iterations used in the timing loop.

#define Log2N	10u		// Base-two logarithm of number of elements.
#define	N	(1u<<Log2N)	// Number of elements.


static const float_t TwoPi = 0x3.243f6a8885a308d313198a2e03707344ap1;


/*	Compare two complex vectors and report the relative error between them.
	(The vectors must have unit strides; other strides are not supported.)
*/
static void CompareComplexVectors(
	DSPSplitComplex Expected, DSPSplitComplex Observed, vDSP_Length Length)
{
	double_t Error = 0, Magnitude = 0;

	int i;
	for (i = 0; i < Length; ++i)
	{
		double_t re, im;

		// Accumulate square of magnitude of elements.
		re = Expected.realp[i];
		im = Expected.imagp[i];
		Magnitude += re*re + im*im;

		// Accumulate square of error.
		re = Expected.realp[i] - Observed.realp[i];
		im = Expected.imagp[i] - Observed.imagp[i];
		Error += re*re + im*im;
	}

	printf("\tRelative error in observed result is %g.\n",
		sqrt(Error / Magnitude));
}


/*	Demonstrate the real-to-complex one-dimensional in-place FFT,
	vDSP_fft_zrip.

	The in-place FFT writes results into the same array that contains the
	input data.

	Applications may need to rearrange data before calling the
	real-to-complex FFT.  This is because the vDSP FFT routines currently
	use a separated-data complex format, in which real components and
	imaginary components are stored in different arrays.  For the
	real-to-complex FFT, real data passed using the same arrangements used
	for complex data.  (This is largely due to the nature of the algorithm
	used in performing in the real-to-complex FFT.) The mapping puts
	even-indexed elements of the real data in real components of the
	complex data and odd-indexed elements of the real data in imaginary
	components of the complex data.

	(It is possible to improve this situation by implementing
	interleaved-data complex format.  If you would benefit from such
	routines, please enter an enhancement request at
	http://developer.apple.com/bugreporter.)

	If an application's real data is stored sequentially in an array (as is
	common) and the design cannot be altered to provide data in the
	even-odd split configuration, then the data can be moved using the
	routine vDSP_ctoz.

	The output of the real-to-complex FFT contains only the first N/2
	complex elements, with one exception.  This is because the second N/2
	elements are complex conjugates of the first N/2 elements, so they are
	redundant.

	The exception is that the imaginary parts of elements 0 and N/2 are
	zero, so only the real parts are provided.  The real part of element
	N/2 is stored in the space that would be used for the imaginary part of
	element 0.

	See the vDSP Library manual for illustration and additional
	information.
*/
static void DemonstratevDSP_fft_zrip(FFTSetup Setup)
{
	/*	Define a stride for the array be passed to the FFT.  In many
		applications, the stride is one and is passed to the vDSP
		routine as a constant.
	*/
	const vDSP_Stride Stride = 1;

	// Define a variable for a loop iterator.
	vDSP_Length i;

	// Define some variables used to time the routine.
	ClockData t0, t1;
	double Time;

	printf("\n\tOne-dimensional real FFT of %u elements.\n", (unsigned int) N);

	// Allocate memory for the arrays.
	float *Signal = malloc(N * Stride * sizeof Signal);
	float *ObservedMemory = malloc(N * sizeof *ObservedMemory);

	if (ObservedMemory == NULL || Signal == NULL)
	{
		fprintf(stderr, "Error, failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	// Assign half of ObservedMemory to reals and half to imaginaries.
	DSPSplitComplex Observed = { ObservedMemory, ObservedMemory + N/2 };

	/*	Generate an input signal.  In a real application, data would of
		course be provided from an image file, sensors, or other source.
	*/
	const float Frequency0 = 79, Frequency1 = 296, Frequency2 = 143;
	const float Phase0 = 0, Phase1 = .2f, Phase2 = .6f;
	for (i = 0; i < N; ++i)
		Signal[i*Stride] =
			  cos((i * Frequency0 / N + Phase0) * TwoPi)
			+ cos((i * Frequency1 / N + Phase1) * TwoPi)
			+ cos((i * Frequency2 / N + Phase2) * TwoPi);

	/*	Reinterpret the real signal as an interleaved-data complex
		vector and use vDSP_ctoz to move the data to a separated-data
		complex vector.  This puts the even-indexed elements of Signal
		in Observed.realp and the odd-indexed elements in
		Observed.imagp.

		Note that we pass vDSP_ctoz two times Signal's normal stride,
		because ctoz skips through a complex vector from real to real,
		skipping imaginary elements.  Considering this as a stride of
		two real-sized elements rather than one complex element is a
		legacy use.

		In the destination array, a stride of one is used regardless of
		the source stride.  Since the destination is a buffer allocated
		just for this purpose, there is no point in replicating the
		source stride.
	*/
	vDSP_ctoz((DSPComplex *) Signal, 2*Stride, &Observed, 1, N/2);

	// Perform a real-to-complex FFT.
	vDSP_fft_zrip(Setup, &Observed, 1, Log2N, FFT_FORWARD);

	/*	Prepare expected results based on analytical transformation of
		the input signal.
	*/
	float *ExpectedMemory = malloc(N * sizeof *ExpectedMemory);
	if (ExpectedMemory == NULL)
	{
		fprintf(stderr, "Error, failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	// Assign half of ExpectedMemory to reals and half to imaginaries.
	DSPSplitComplex Expected = { ExpectedMemory, ExpectedMemory + N/2 };

	for (i = 0; i < N/2; ++i)
		Expected.realp[i] = Expected.imagp[i] = 0;

	// Add the frequencies in the signal to the expected results.
	Expected.realp[(int) Frequency0] = N * cos(Phase0 * TwoPi);
	Expected.imagp[(int) Frequency0] = N * sin(Phase0 * TwoPi);

	Expected.realp[(int) Frequency1] = N * cos(Phase1 * TwoPi);
	Expected.imagp[(int) Frequency1] = N * sin(Phase1 * TwoPi);

	Expected.realp[(int) Frequency2] = N * cos(Phase2 * TwoPi);
	Expected.imagp[(int) Frequency2] = N * sin(Phase2 * TwoPi);

	// Compare the observed results to the expected results.
	CompareComplexVectors(Expected, Observed, N/2);

	// Release memory.
	free(ExpectedMemory);

	/*	The above shows how to use the vDSP_fft_zrip routine.  Now we
		will see how fast it is.
	*/

	/*	Zero the signal before timing because repeated FFTs on non-zero
		data can cause abnormalities such as infinities, NaNs, and
		subnormal numbers.
	*/
	for (i = 0; i < N; ++i)
		Signal[i] = 0;

	// Time vDSP_fft_zrip by itself.

	t0 = Clock();

	for (i = 0; i < Iterations; ++i)
		vDSP_fft_zrip(Setup, &Observed, 1, Log2N, FFT_FORWARD);

	t1 = Clock();

	// Average the time over all the loop iterations.
	Time = ClockToSeconds(t1, t0) / Iterations;

	printf("\tvDSP_fft_zrip on %u elements takes %g microseconds.\n",
		(unsigned int) N, Time * 1e6);

	// Time vDSP_fft_zrip with the vDSP_ctoz and vDSP_ztoc transformations.

	t0 = Clock();

	for (i = 0; i < Iterations; ++i)
	{
		vDSP_ctoz((DSPComplex *) Signal, 2*Stride, &Observed, 1, N/2);
		vDSP_fft_zrip(Setup, &Observed, 1, Log2N, FFT_FORWARD);
		vDSP_ztoc(&Observed, 1, (DSPComplex *) Signal, 2*Stride, N/2);
	}

	t1 = Clock();

	// Average the time over all the loop iterations.
	Time = ClockToSeconds(t1, t0) / Iterations;

	printf(
"\tvDSP_fft_zrip with vDSP_ctoz and vDSP_ztoc takes %g microseconds.\n",
		Time * 1e6);

	// Release resources.
	free(ObservedMemory);
	free(Signal);
}


/*	Demonstrate the real-to-complex one-dimensional out-of-place FFT,
	vDSP_fft_zrop.

	The out-of-place FFT writes results into a different array than the
	input.  If you are using vDSP_ctoz to reformat the input, you do not
	need vDSP_fft_zrop because you move the data from an input array to an
	output array when you call vDSP_ctoz.  vDSP_fft_zrop may be useful when
	incoming data arrives in the format used by vDSP_fft_zrop, and you want
	to simultaneously perform an FFT and store the results in another array.
*/
static void DemonstratevDSP_fft_zrop(FFTSetup Setup)
{
	/*	Define a stride for the array be passed to the FFT.  In many
		applications, the stride is one and is passed to the vDSP
		routine as a constant.
	*/
	const vDSP_Stride Stride = 1;

	// Define a variable for a loop iterator.
	vDSP_Length i;

	// Define some variables used to time the routine.
	ClockData t0, t1;
	double Time;

	printf("\n\tOne-dimensional real FFT of %u elements.\n",
		(unsigned int) N);

	// Allocate memory for the arrays.
	float *Signal = malloc(N * Stride * sizeof Signal);
	float *BufferMemory = malloc(N * sizeof *BufferMemory);
	float *ObservedMemory = malloc(N * sizeof *ObservedMemory);

	if (ObservedMemory == NULL || BufferMemory == NULL || Signal == NULL)
	{
		fprintf(stderr, "Error, failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	// Assign half of BufferMemory to reals and half to imaginaries.
	DSPSplitComplex Buffer = { BufferMemory, BufferMemory + N/2 };

	// Assign half of ObservedMemory to reals and half to imaginaries.
	DSPSplitComplex Observed = { ObservedMemory, ObservedMemory + N/2 };

	/*	Generate an input signal.  In a real application, data would of
		course be provided from an image file, sensors, or other source.
	*/
	const float Frequency0 = 48, Frequency1 = 243, Frequency2 = 300;
	const float Phase0 = 1.f/3, Phase1 = .82f, Phase2 = .5f;
	for (i = 0; i < N; ++i)
		Signal[i*Stride] =
			  cos((i * Frequency0 / N + Phase0) * TwoPi)
			+ cos((i * Frequency1 / N + Phase1) * TwoPi)
			+ cos((i * Frequency2 / N + Phase2) * TwoPi);

	/*	Reinterpret the real signal as an interleaved-data complex
		vector and use vDSP_ctoz to move the data to a separated-data
		complex vector.  This puts the even-indexed elements of Signal
		in Observed.realp and the odd-indexed elements in
		Observed.imagp.

		Note that we pass vDSP_ctoz two times Signal's normal stride,
		because ctoz skips through a complex vector from real to real,
		skipping imaginary elements.  Considering this as a stride of
		two real-sized elements rather than one complex element is a
		legacy use.

		In the destination array, a stride of one is used regardless of
		the source stride.  Since the destination is a buffer allocated
		just for this purpose, there is no point in replicating the
		source stride.
	*/
	vDSP_ctoz((DSPComplex *) Signal, 2*Stride, &Buffer, 1, N/2);

	// Perform a real-to-complex FFT.
	vDSP_fft_zrop(Setup, &Buffer, 1, &Observed, 1, Log2N, FFT_FORWARD);

	/*	Prepare expected results based on analytical transformation of
		the input signal.
	*/
	float *ExpectedMemory = malloc(N * sizeof *ExpectedMemory);
	if (ExpectedMemory == NULL)
	{
		fprintf(stderr, "Error, failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	// Assign half of ExpectedMemory to reals and half to imaginaries.
	DSPSplitComplex Expected = { ExpectedMemory, ExpectedMemory + N/2 };

	for (i = 0; i < N/2; ++i)
		Expected.realp[i] = Expected.imagp[i] = 0;

	// Add the frequencies in the signal to the expected results.
	Expected.realp[(int) Frequency0] = N * cos(Phase0 * TwoPi);
	Expected.imagp[(int) Frequency0] = N * sin(Phase0 * TwoPi);

	Expected.realp[(int) Frequency1] = N * cos(Phase1 * TwoPi);
	Expected.imagp[(int) Frequency1] = N * sin(Phase1 * TwoPi);

	Expected.realp[(int) Frequency2] = N * cos(Phase2 * TwoPi);
	Expected.imagp[(int) Frequency2] = N * sin(Phase2 * TwoPi);

	// Compare the observed results to the expected results.
	CompareComplexVectors(Expected, Observed, N/2);

	// Release memory.
	free(ExpectedMemory);

	/*	The above shows how to use the vDSP_fft_zrop routine.  Now we
		will see how fast it is.
	*/

	/*	Zero the signal before timing because repeated FFTs on non-zero
		data can cause abnormalities such as infinities, NaNs, and
		subnormal numbers.
	*/
	for (i = 0; i < N; ++i)
		Signal[i] = 0;

	// Time vDSP_fft_zrop by itself.

	t0 = Clock();

	for (i = 0; i < Iterations; ++i)
		vDSP_fft_zrop(Setup, &Buffer, 1, &Observed, 1, Log2N,
			FFT_FORWARD);

	t1 = Clock();

	// Average the time over all the loop iterations.
	Time = ClockToSeconds(t1, t0) / Iterations;

	printf("\tvDSP_fft_zrop on %u elements takes %g microseconds.\n",
		(unsigned int) N, Time * 1e6);

	/*	Unlike the vDSP_fft_zrip example, we do not time vDSP_fft_zrop
		in conjunction with vDSP_ctoz and vDSP_ztoc.  If your data
		arrangement requires you to use vDSP_ctoz, then you are already
		making a copy of the input data.  So you would just do the FFT
		in-place in that copy; you would call vDSP_fft_zrip and not
		vDSP_fft_zrop.
	*/

	// Release resources.
	free(ObservedMemory);
	free(BufferMemory);
	free(Signal);
}


/*	Demonstrate the complex one-dimensional in-place FFT, vDSP_fft_zip.

	The in-place FFT writes results into the same array that contains the
	input data.  This may be faster than an out-of-place routine because it
	uses less memory (so there is less data to load from memory and a
	greater chance of keeping data in cache).
*/
static void DemonstratevDSP_fft_zip(FFTSetup Setup)
{
	/*	Define a stride for the array be passed to the FFT.  In many
		applications, the stride is one and is passed to the vDSP
		routine as a constant.
	*/
	const vDSP_Stride SignalStride = 1;

	// Define a variable for a loop iterator.
	vDSP_Length i;

	// Define some variables used to time the routine.
	ClockData t0, t1;
	double Time;

	printf("\n\tOne-dimensional complex FFT of %u elements.\n",
		(unsigned int) N);

	// Allocate memory for the arrays.
	DSPSplitComplex Signal;
	Signal.realp = malloc(N * SignalStride * sizeof Signal.realp);
	Signal.imagp = malloc(N * SignalStride * sizeof Signal.imagp);

	if (Signal.realp == NULL || Signal.imagp == NULL)
	{
		fprintf(stderr, "Error, failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	/*	Generate an input signal.  In a real application, data would of
		course be provided from an image file, sensors, or other source.
	*/
	const float Frequency0 = 400, Frequency1 = 623, Frequency2 = 931;
	const float Phase0 = .618, Phase1 = .7f, Phase2 = .125;
	for (i = 0; i < N; ++i)
	{
		Signal.realp[i*SignalStride] =
			  cos((i * Frequency0 / N + Phase0) * TwoPi)
			+ cos((i * Frequency1 / N + Phase1) * TwoPi)
			+ cos((i * Frequency2 / N + Phase2) * TwoPi);
		Signal.imagp[i*SignalStride] =
			  sin((i * Frequency0 / N + Phase0) * TwoPi)
			+ sin((i * Frequency1 / N + Phase1) * TwoPi)
			+ sin((i * Frequency2 / N + Phase2) * TwoPi);
	}

	// Perform an FFT.
	vDSP_fft_zip(Setup, &Signal, SignalStride, Log2N, FFT_FORWARD);

	/*	Prepare expected results based on analytical transformation of
		the input signal.
	*/
	DSPSplitComplex Expected;
	Expected.realp = malloc(N * sizeof Expected.realp);
	Expected.imagp = malloc(N * sizeof Expected.imagp);

	if (Expected.realp == NULL || Expected.imagp == NULL)
	{
		fprintf(stderr, "Error, failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < N/2; ++i)
		Expected.realp[i] = Expected.imagp[i] = 0;

	// Add the frequencies in the signal to the expected results.
	Expected.realp[(int) Frequency0] = N * cos(Phase0 * TwoPi);
	Expected.imagp[(int) Frequency0] = N * sin(Phase0 * TwoPi);

	Expected.realp[(int) Frequency1] = N * cos(Phase1 * TwoPi);
	Expected.imagp[(int) Frequency1] = N * sin(Phase1 * TwoPi);

	Expected.realp[(int) Frequency2] = N * cos(Phase2 * TwoPi);
	Expected.imagp[(int) Frequency2] = N * sin(Phase2 * TwoPi);

	// Compare the observed results to the expected results.
	CompareComplexVectors(Expected, Signal, N);

	// Release memory.
	free(Expected.realp);
	free(Expected.imagp);

	/*	The above shows how to use the vDSP_fft_zip routine.  Now we
		will see how fast it is.
	*/

	/*	Zero the signal before timing because repeated FFTs on non-zero
		data can cause abnormalities such as infinities, NaNs, and
		subnormal numbers.
	*/
	for (i = 0; i < N; ++i)
		Signal.realp[i] = Signal.imagp[i] = 0;

	// Time vDSP_fft_zip by itself.

	t0 = Clock();

	for (i = 0; i < Iterations; ++i)
		vDSP_fft_zip(Setup, &Signal, SignalStride, Log2N, FFT_FORWARD);

	t1 = Clock();

	// Average the time over all the loop iterations.
	Time = ClockToSeconds(t1, t0) / Iterations;

	printf("\tvDSP_fft_zip on %u elements takes %g microseconds.\n",
		(unsigned int) N, Time * 1e6);

	// Release resources.
	free(Signal.realp);
	free(Signal.imagp);
}


/*	Demonstrate the complex one-dimensional out-of-place FFT, vDSP_fft_zop.

	The out-of-place FFT writes results into a different array than the
	input.
*/
static void DemonstratevDSP_fft_zop(FFTSetup Setup)
{
	/*	Define strides for the arrays be passed to the FFT.  In many
		applications, the strides are one and are passed to the vDSP
		routine as constants.
	*/
	const vDSP_Stride SignalStride = 1, ObservedStride = 1;

	// Define a variable for a loop iterator.
	vDSP_Length i;

	// Define some variables used to time the routine.
	ClockData t0, t1;
	double Time;

	printf("\n\tOne-dimensional complex FFT of %u elements.\n",
		(unsigned int) N);

	// Allocate memory for the arrays.
	DSPSplitComplex Signal, Observed;
	Signal.realp = malloc(N * SignalStride * sizeof Signal.realp);
	Signal.imagp = malloc(N * SignalStride * sizeof Signal.imagp);
	Observed.realp = malloc(N * ObservedStride * sizeof Observed.realp);
	Observed.imagp = malloc(N * ObservedStride * sizeof Observed.imagp);

	if (Signal.realp == NULL || Signal.imagp == NULL
		|| Observed.realp == NULL || Observed.imagp == NULL)
	{
		fprintf(stderr, "Error, failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	/*	Generate an input signal.  In a real application, data would of
		course be provided from an image file, sensors, or other source.
	*/
	const float Frequency0 = 300, Frequency1 = 450, Frequency2 = 775;
	const float Phase0 = .3, Phase1 = .45f, Phase2 = .775f;
	for (i = 0; i < N; ++i)
	{
		Signal.realp[i*SignalStride] =
			  cos((i * Frequency0 / N + Phase0) * TwoPi)
			+ cos((i * Frequency1 / N + Phase1) * TwoPi)
			+ cos((i * Frequency2 / N + Phase2) * TwoPi);
		Signal.imagp[i*SignalStride] =
			  sin((i * Frequency0 / N + Phase0) * TwoPi)
			+ sin((i * Frequency1 / N + Phase1) * TwoPi)
			+ sin((i * Frequency2 / N + Phase2) * TwoPi);
	}

	// Perform an FFT.
	vDSP_fft_zop(Setup, &Signal, SignalStride, &Observed, ObservedStride,
		Log2N, FFT_FORWARD);

	/*	Prepare expected results based on analytical transformation of
		the input signal.
	*/
	DSPSplitComplex Expected;
	Expected.realp = malloc(N * sizeof Expected.realp);
	Expected.imagp = malloc(N * sizeof Expected.imagp);

	if (Expected.realp == NULL || Expected.imagp == NULL)
	{
		fprintf(stderr, "Error, failed to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < N/2; ++i)
		Expected.realp[i] = Expected.imagp[i] = 0;

	// Add the frequencies in the signal to the expected results.
	Expected.realp[(int) Frequency0] = N * cos(Phase0 * TwoPi);
	Expected.imagp[(int) Frequency0] = N * sin(Phase0 * TwoPi);

	Expected.realp[(int) Frequency1] = N * cos(Phase1 * TwoPi);
	Expected.imagp[(int) Frequency1] = N * sin(Phase1 * TwoPi);

	Expected.realp[(int) Frequency2] = N * cos(Phase2 * TwoPi);
	Expected.imagp[(int) Frequency2] = N * sin(Phase2 * TwoPi);

	// Compare the observed results to the expected results.
	CompareComplexVectors(Expected, Observed, N);

	// Release memory.
	free(Expected.realp);
	free(Expected.imagp);

	/*	The above shows how to use the vDSP_fft_zop routine.  Now we
		will see how fast it is.
	*/

	// Time vDSP_fft_zop by itself.

	t0 = Clock();

	for (i = 0; i < Iterations; ++i)
		vDSP_fft_zop(Setup, &Signal, SignalStride, &Observed, ObservedStride,
			Log2N, FFT_FORWARD);

	t1 = Clock();

	// Average the time over all the loop iterations.
	Time = ClockToSeconds(t1, t0) / Iterations;

	printf("\tvDSP_fft_zop on %u elements takes %g microseconds.\n",
		(unsigned int) N, Time * 1e6);

	// Release resources.
	free(Signal.realp);
	free(Signal.imagp);
	free(Observed.realp);
	free(Observed.imagp);
}


// Demonstrate vDSP FFT functions.
void DemonstrateFFT(void)
{
	printf("Begin %s.\n", __func__);

	// Initialize data for the FFT routines.
	FFTSetup Setup = vDSP_create_fftsetup(Log2N, FFT_RADIX2);
	if (Setup == NULL)
	{
		fprintf(stderr, "Error, vDSP_create_fftsetup failed.\n");
		exit (EXIT_FAILURE);
	}

	DemonstratevDSP_fft_zrip(Setup);
	DemonstratevDSP_fft_zrop(Setup);
	DemonstratevDSP_fft_zip(Setup);
	DemonstratevDSP_fft_zop(Setup);

	vDSP_destroy_fftsetup(Setup);

	printf("\nEnd %s.\n\n\n", __func__);
}
