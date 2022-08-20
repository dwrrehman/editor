#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


static inline void editor(const uint8_t* input, size_t size) {
	
}

int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size) {

	// printf("\n[%lu] : { ", size);
	 // 	for (size_t i = 0; i < size; i++) {
		// 	printf("%02hhx(%c) ", input[i], input[i] > 32 ? input[i] : 32);
		// }
	// printf("}\n\n");

	editor(input, size);
	return 0;
}

