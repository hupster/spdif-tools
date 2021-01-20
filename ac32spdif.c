#include <stdio.h>
#include <stdint.h>
//#include <string.h>
#include <memory.h>

short bitrates[] = { 32,  40,  48,  56,  64,  80,  96, 112, 128, 160,
192, 224, 256, 320, 384, 448, 512, 576, 640 };

short fsize44[] = {  70,   88,  105,  122,  140,  175,  209,  244,  279,  349,
418,  488,  558,  697,  836,  976, 1115, 1254, 1394 };

uint32_t DecFS(char sratecode, uint32_t fsizecode)
{
	switch (sratecode)
	{
		case 0:
			return (bitrates[fsizecode / 2] * 2);
		case 1:
			return ((fsizecode % 2) ? fsize44[fsizecode / 2] : fsize44[fsizecode / 2] - 1);
		case 2:
			return (bitrates[fsizecode / 2] * 3);
		default:
			return 0;
	}

	return 0;
}

#pragma pack(1)
struct AC3HEADER {
	uint16_t Syncword;
	uint16_t CRC;
	uint8_t Sampleratecode;
	uint8_t Bitstreamcode;
	// further elements left out
};

struct WAVEHEADER {
    uint8_t  ChunkID[4];
    uint32_t  ChunkSize;
    uint8_t  Format[4];
    uint8_t  SubChunk1ID[4];
    uint32_t  SubChunk1Size;
    uint16_t AudioFormat;
    uint16_t NumChannels;
    uint32_t  SampleRate;
    uint32_t  ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
    uint8_t  SubChunk2ID[4];
    uint32_t  SubChunk2Size;
};

struct AC3PREAMBLE {
	uint16_t sync1;
	uint16_t sync2;
	uint8_t buffer_infoLSB;
	uint8_t buffer_infoMSB;
	uint16_t lengthcode;
};
#pragma pack(4)

char buffer[6144];

FILE *infile, *outfile;

uint32_t bytesread;
uint32_t payloadbytes;
char sampleratecode;
uint32_t framesizecode;

struct AC3PREAMBLE ac3p = {
	.sync1 = 0xF872,
	.sync2 = 0x4E1F,
	.buffer_infoLSB = 1,
	.buffer_infoMSB = 0,
	.lengthcode = 0x3800
};

struct WAVEHEADER wavhdr = {
	.ChunkID = "RIFF",
	.ChunkSize = 0,
	.Format = "WAVE",
	.SubChunk1ID = "fmt ",
	.SubChunk1Size = 16,
	.AudioFormat = 1,
	.NumChannels = 2,
	.SampleRate = 0,
	.ByteRate = 0,
	.BlockAlign = 4,
	.BitsPerSample = 16,
	.SubChunk2ID = "data",
	.SubChunk2Size = 0
};

uint32_t i;
char temp;

int main( int argc, char *argv[ ], char *envp[ ] )
{
	if (argc < 3)
	{
		printf("Wrong syntax. AC3PACK <In.ac3> <Out.wav>.\n");
		return 0;
	}

	infile = fopen(argv[1], "rb");
	outfile = fopen(argv[2], "wb");

	fwrite (&wavhdr, sizeof(struct WAVEHEADER), 1, outfile);

	for(;;)
	{
		memset (buffer, 0, 6144);
		bytesread = fread(buffer, 1, 6, infile);
		if (bytesread < 6)
		{
			printf ("EOF reached (Frame Header reading)!\nCurrent position in INFILE: %li\n", ftell(infile));
			break;
		}
		if ((buffer[0] != 0x0B) || (buffer[1] != 0x77))
		{
			printf("ERROR: INVALID SYNCWORD !\nCurrent position in INFILE: %li\n", ftell(infile));
			break;
		}
		framesizecode = (buffer[4] & 63);
		sampleratecode = ((buffer[4] & 192) / 64);

		if (wavhdr.SampleRate == 0)
		{
				printf ("Framesizecode: %i\n", framesizecode);
				printf ("Sampleratecode: %i\n", sampleratecode);

				switch (sampleratecode)
				{
				case 0:
					wavhdr.SampleRate = 48000;
					wavhdr.ByteRate = 192000;
					break;
				case 1:
					wavhdr.SampleRate = 44100;
					wavhdr.ByteRate = 176000;
					break;
				case 2:
					wavhdr.SampleRate = 32000;
					wavhdr.ByteRate = 128000;
					break;
				default:
					wavhdr.SampleRate = 48000;
					wavhdr.ByteRate = 192000;
				}
		}

		payloadbytes = DecFS (sampleratecode, framesizecode);
		payloadbytes *= 2;

		bytesread = fread (&buffer[6], 1, payloadbytes - 6, infile);
		if ((bytesread + 6) < payloadbytes)
		{
			printf ("EOF reached (Burst Reading)!\nCurrent position in INFILE: %li\n", ftell(infile));
			printf ("Frame size: %i  .. Bytes read: %i\n", payloadbytes, bytesread);
			break;
		}
		ac3p.buffer_infoMSB = (buffer[5] & 7);
		ac3p.lengthcode = (short)(payloadbytes * 8);

		for (i = 0; i < payloadbytes; i += 2)
		{
			temp = buffer[i];
			buffer[i] = buffer[i + 1];
			buffer[i + 1] = temp;
		}

		fwrite (&ac3p, 1, sizeof(struct AC3PREAMBLE), outfile);
		fwrite (buffer, 1, 6136, outfile);
	}

	wavhdr.SubChunk2Size = (ftell(outfile) - 44);
	wavhdr.ChunkSize = wavhdr.SubChunk2Size + 36;

	fseek (outfile, SEEK_SET, 0);
	fwrite (&wavhdr, sizeof(struct WAVEHEADER), 1, outfile);

	fclose (infile);
	fclose (outfile);

	return 0;
}
