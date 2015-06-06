#include <string.h>
#include <stdio.h>
#include <ctype.h>


int main() {
	char foo[] = "loff.";
	char *fooptr;
	fooptr = (char *)&foo;
	int checksum = 0;

	while (isspace(*fooptr))
		fooptr++;

	while (*fooptr != '\0') {
		checksum = checksum ^ *fooptr; 
		fooptr++;
	}
	printf("checksum is: %i\n", checksum);
}

