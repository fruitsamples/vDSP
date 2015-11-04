/*	This module demonstrates a little of what an FFT does and how to use
	the vDSP routine vDSP_fft_zrip.

	The "Touch Tones" generated when a telephone is dialed are Dual-Tone
	Multi-Frequency (DTMF) tones.  Equipment at the phone company detects
	these tones to know what key is pressed.  This program emulates part of
	that process.

	When you run this program, it prompts you for a key or uses keys passed
	in a command-line argument.  Then, for each key, it generates a signal
	containing the two tones for that key and some noise.  It runs that
	signal through an FFT routine and examines the output to detect the
	tones.

	There is a lot of noise in the signal (scaled to a range four times
	that of the DTMF tones), and the FFT is given just 256 samples, less
	than .08 seconds sampled at 3266 Hz (twice the highest DTMF tone), yet
	the program finds the correct key almost all the time.

	Copyright (C) 2007 Apple Inc.  All rights reserved.
*/


/*	These standard C header files are needed primarily for the DTMF
	demonstration.  They are not generally needed to use vDSP.
*/
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Including the Accelerate headers is of course needed to use vDSP.
#include <Accelerate/Accelerate.h>


// Calculate the number of elements in an array.
#define	NumberOf(a)	(sizeof (a) / sizeof *(a))


static const double_t TwoPi = 0x3.243f6a8885a308d313198a2e03707344ap1;


/*	SampleLength is the number of signal samples to use, and
	Log2SampleLength is the base-two logarithm of the number.
*/
#define	Log2SampleLength	8
#define	SampleLength		(1 << Log2SampleLength)

#define	SamplingFrequency	3266	// Hz at which signal is sampled.


// Define the state for the pseudo-random number generator.
static uint32_t Seed;


// Initialize the pseudo-random number generator.
void InitializeRandom(void)
{
	/*	Set the seed from the time, just to vary the data from run to
		run and demonstrate the program is not specialized for a
		particular case.  Obviously this is not a good seed when
		high-quality pseudo-random numbers are needed.
	*/
	Seed = time(NULL);
}


// Return a pseudo-random number in [0, 1).
float Random(void)
{
	/*	Use a very fast but low quality pseudo-random number generator,
		An Even Quicker Generator from William H. Preuss, Saul A.
		Teukolsky, William T. Vetterling, and Brian P. Flannery,
		_Numerical Recipes in C: The Art of Scientific Computer_ second
		edition (Cambridge University Press: 1992), pages 284-285.
	*/
	Seed = 1664525 * Seed + 1013904223;

	// Convert the high 24 bits to a float in [0, 1).
	return (Seed >> 8) * (1.f/16777216);
}


// Declare a structure to hold two frequencies.
typedef struct { float Frequency[2]; } FrequencyPair;


// Define DTMF frequencies.
static char Keys[] = "123A456B789C*0#D";
static float DTMF0[] = { 1209, 1336, 1477, 1633 };
static float DTMF1[] = {  697,  770,  852,  941 };


// Look up a key in the Keys table and return its DTMF frequencies.
FrequencyPair ConvertKeyToFrequencies(const char Key)
{
	// Find the key.
	char *p = strchr(Keys, Key);
	if (p == 0)
	{
		// Return zeroes if key is not in table.
		FrequencyPair result = { { 0, 0 } };
		return result;
	}
	else
	{
		// Return frequencies assigned to the key.
		int n = p - Keys;
		FrequencyPair result = { { DTMF0[n%4], DTMF1[n/4] } };
		return result;
	}
}


/*	Find a frequency in DFT results.

	Buffer is the output of a real-to-complex DFT.

	Frequencies is an array of N frequencies, of which one is expected to
	appear in the signal.

	This code simply looks for the frequency (of those in the array) that
	has the greatest amplitude in the signal.  A real DTMF detector would
	be more concerned about additional things, such as whether DTMF
	frequencies are present at all.
*/
int FindTone(const DSPSplitComplex Buffer, const float Frequencies[], int N)
{
	// Initialize the value and index for the maximum amplitude seen so far.
	float MaximumValue = -1;
	int MaximumIndex = 0;

	// Iterate through the candidate frequencies.
	for (int i = 0; i < N; ++i)
	{
		/*	Find the index into the DFT results corresponding to
			the frequency.
		*/
		int index = Frequencies[i] / SamplingFrequency
			* SampleLength + .5;

		// Get the real and imaginary parts.
		float re = Buffer.realp[index];
		float im = Buffer.imagp[index];

		// Calculate the square of the amplitude.
		float Value = re*re + im*im;

		// Record the information for the maximum amplitude seen so far.
		if (MaximumValue < Value)
		{
			MaximumValue = Value;
			MaximumIndex = i;
		}
	}

	// Return the best candidate's index (in the Frequencies array).
	return MaximumIndex;
}


/*	Demonstrate using an FFT to detect telephone keys.

	Setup is the result of creating an FFT setup.

	F contains a pair of frequencies to inject into a signal.
*/
void Demonstrate(FFTSetup Setup, FrequencyPair F)
{
	float *Signal = malloc(SampleLength * sizeof *Signal);
	if (Signal == 0)
	{
		fprintf(stderr, "Error, unable to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	printf("\tGenerating signal with noise and DTMF tones...\n");

	// Initialize the signal with noise.
	for (int i = 0; i < SampleLength; ++i)
		Signal[i] = 4 * Random();

	// Add one of the tones to the signal.
	float Phase = Random();	// Start the tone at a pseudo-random time.
	for (int i = 0; i < SampleLength; ++i)
		Signal[i] += sin((i*F.Frequency[0] / SamplingFrequency + Phase)
			* TwoPi);

	// Add the other tone.
	Phase = Random();	// Start the tone at a pseudo-random time.
	for (int i = 0; i < SampleLength; ++i)
		Signal[i] += sin((i*F.Frequency[1]/SamplingFrequency + Phase)
			* TwoPi);

	// Rearrange the signal for vDSP_fft_zrip, using an auxiliary buffer.

	// Get enough memory for two halves.
	float *BufferMemory = malloc(SampleLength * sizeof *BufferMemory);
	if (BufferMemory == 0)
	{
		fprintf(stderr, "Error, unable to allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	// Assign half the memory to reals and half to imaginaries.
	DSPSplitComplex Buffer
		= { BufferMemory, BufferMemory + SampleLength/2 };

	// Copy (and rearrange) the data to the buffer.
	vDSP_ctoz((DSPComplex *) Signal, 2, &Buffer, 1, SampleLength / 2);

	printf("\tAnalyzing signal...\n");

	// Compute the DFT of the signal.
	vDSP_fft_zrip(Setup, &Buffer, 1, Log2SampleLength, FFT_FORWARD);

	// Use the DFT results to identify the tones in the signal.
	int Tone0 = FindTone(Buffer, DTMF0, NumberOf(DTMF0));
	int Tone1 = FindTone(Buffer, DTMF1, NumberOf(DTMF1));

	printf("\tFound frequencies %g and %g for key %c.\n",
		DTMF0[Tone0], DTMF1[Tone1], Keys[Tone1*4 + Tone0]);

	// Release resources.
	free(BufferMemory);
	free(Signal);
}


int main(int argc, char *argv[])
{
	// Initialize the pseudo-random number generator.
	InitializeRandom();

	// Initialize FFT data.
	FFTSetup Setup = vDSP_create_fftsetup(Log2SampleLength, FFT_RADIX2);
	if (Setup == 0)
	{
		fprintf(stderr, "Error, unable to create FFT setup.\n");
		exit(EXIT_FAILURE);
	}

	/*	If there are no command-line arguments, prompt for keys
		interactively.
	*/
	if (argc <= 1)
	{
		// Process keys for the user until they are done.
		int c;
		while (1)
		{
			// Prompt the user.
			printf(
"Please enter a key (one of 0-9, *, #, or A-D):  ");

			// Skip whitespace except newlines.
			do
				c = getchar();
			while (c != '\n' && isspace(c));

			// When there is a blank line or an EOF, quit.
			if (c == '\n' || c == EOF)
				break;

			// Look up the key in the table.
			FrequencyPair F = ConvertKeyToFrequencies(toupper(c));

			// If it is a valid key, demonstrate the FFT.
			if (F.Frequency[0] != 0)
				Demonstrate(Setup, F);

			// Skip anything else on the line.
			do
				c = getchar();
			while (c != EOF && c != '\n');

			// When the input ends, quit.  Otherwise, do more.
			if (c == EOF)
				break;
		}

		// Do not leave the cursor in the middle of a line.
		if (c != '\n')
			printf("\n");
	}

	// If there is one command line argument, process the keys in it.
	else if (argc == 2)
	{
		for (char *p = argv[1]; *p; ++p)
		{
			// Look up the key in the table.
			FrequencyPair F = ConvertKeyToFrequencies(toupper(*p));

			// If it is a valid key, demonstrate the FFT.
			if (F.Frequency[0] != 0)
			{
				printf("Simulating key %c.\n", *p);
				Demonstrate(Setup, F);
			}
			else
				fprintf(stderr,
					"Error, key %c not recognized.\n", *p);
		}
	}

	// If there are too many arguments, print a usage message.
	else
	{
		fprintf(stderr,
			"Usage:  %s [telephone keys 0-9, #, *, or A-D]\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	// Release resources.
	vDSP_destroy_fftsetup(Setup);

	return 0;
}
