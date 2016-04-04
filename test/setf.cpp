#include <stdio.h>

int main()
{
	FILE* fp = fopen("workload-file.txt", "r");
	FILE* fpOut = fopen("workload-file-f.txt", "w");

	char str[256];
	int i = 0;
	while( !feof(fp ) )
	{
		fgets( str, 256, fp );
		fprintf( fpOut, "%s", str);
		i++;
		if( i % 1000 == 0 )
			fprintf(fpOut, "F\n");
	}
	fclose(fp);
	fclose(fpOut);
}

