#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		printf("Usage: %s [input_file_name] [output_file_name]\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], argv[2]) == 0)
	{
		printf("Error: The input and output file names cant be the same.\n");
		return 1;
	}

	FILE* inputFd = fopen(argv[1], "rb");
	if (!inputFd)
	{
		printf("Error: Failed to open %s.\n", argv[1]);
		return 1;
	}

	FILE* outputFd = fopen(argv[2], "wb");
	if (!outputFd)
	{
		printf("Error: Failed to open %s.\n", argv[2]);
		fclose(inputFd);
		return 1;
	}

	char line[1024];
	char objTag[16];
	memset(line, 0, sizeof(line));
	memset(objTag, 0, sizeof(objTag));
	float u = 0.0f;
	float v = 0.0f;

	unsigned int numFlippedUvs = 0;
	while (!feof(inputFd))
	{
		long initialFilePos = ftell(inputFd);
		int scanResult = fscanf(inputFd, "%s %f %f", objTag, &u, &v);
		if (scanResult != EOF && objTag[0] == 'v' && objTag[1] == 't')
		{
			fprintf(outputFd, "%s %f %f\n", objTag, u, 1.0f - v);
			numFlippedUvs++;
			continue;
		}

		fseek(inputFd, initialFilePos, SEEK_SET);
		fgets(line, sizeof(line), inputFd);
		fprintf(outputFd, "%s", line);
		memset(line, 0, sizeof(line));
	}

	fclose(inputFd);
	fclose(outputFd);

	if (numFlippedUvs == 0)
	{
		printf("Failed to flip UVs of %s, no UVs were found.\n", argv[1]);
		printf("The file %s is a mirror of the input file, please delete it.\n", argv[2]);
		return 1;
	} else
	{
		printf("Successfully flipped the UVs of %s! (%u UVs were flipped)\n", argv[1], numFlippedUvs);
	}

	return 0;
}
