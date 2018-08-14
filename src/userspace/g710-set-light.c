#include <stdio.h>
	
int main (int argc, char** argv) {
	int target, wasd_or_macro, other;

	if (argc < 3 || argc > 4) {
		printf("You need to call this script with:\n");
		printf("\tg710-set-light target WASD/MACRO_value [OTHER_value]\n");
		return -1;
	}

	target = atoi(argv[1]);
	wasd_or_macro = atoi(argv[2]);
	// set other keys to same value if not specified otherwise
	if (argc == 3) {
		other = wasd_or_macro;
	} else {
	    other = atoi(argv[3]);
	}

	if (target == 0) {
		int wasd = wasd_or_macro;

		if (wasd < 0 || other < 0 || wasd > 4 || other > 4) {
			printf("Only values 0-4 are accepted for WASD and OTHER keys.\n");
			return -2;
		}

		// calculate decimal light value
		int light_value = wasd << 4 | other;

		#ifdef DEBUG
			printf("Setting keyboard LEDs to: %d\n", light_value);
		#endif

		FILE *file;
		file = fopen("/sys/bus/hid/devices/0003:046D:C24D.0002/logitech-g710/led_keys","w");
		fprintf(file,"%d",light_value);
		fclose(file);
	} else {
		int macro = wasd_or_macro;

		if (macro < 0 || macro > 15) {
			printf("Only values 0-15 are accepted for MACRO keys.\n");
			return -2;
		}

		#ifdef DEBUG
			printf("Setting macro LEDs to: %d\n", macro);
		#endif

		FILE *file;
		file = fopen("/sys/bus/hid/devices/0003:046D:C24D.0002/logitech-g710/led_macro","w");
		fprintf(file,"%d",macro);
		fclose(file);		
	}

	return 0;
}
