#if 0

int IRsignal[] = {
// ON, OFF (in 10's of microseconds)
        730, 340,
        50, 40,
        50, 130,
        50, 120,
        50, 130,
        50, 40,
        50, 120,
        50, 130,
        50, 130,
        50, 120,
        50, 130,
        50, 120,
        50, 40,
        50, 40,
        50, 40,
        50, 40,
        50, 120,
        50, 130,
        50, 40,
        50, 120,
        50, 130,
        50, 120,
        50, 40,
        50, 130,
        50, 40,
        50, 40,
        50, 40,
        40, 130,
        50, 40,
        50, 130,
        40, 130,
        50, 40,
        50, 130,
        40, 2860,
        720, 350,
        40, 40,
        50, 130,
        50, 130,
        40, 130,
        50, 40,
        50, 130,
        40, 130,
        50, 130,
        50, 120,
        50, 130,
        50, 120,
        50, 40,
        50, 40,
        50, 40,
        50, 40,
        50, 120,
        50, 40,
        50, 40,
        50, 130,
        50, 40,
        50, 40,
        40, 40,
        50, 40,
        50, 40,
        50, 40,
        50, 40,
        50, 120,
        50, 40,
        50, 130,
        50, 120,
        50, 40,
        50, 130,
        50, 0};


#define RESOLUTION	10
#define FUZZINESS	25
#define MAXPULSE	(65000 / RESOLUTION)
#define NUMPULSES	200

uint16_t pulses[NUMPULSES][2];  // pair is high and low pulse 
uint8_t currentpulse = 0; // index for pulses we're storing

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define DEBUG
int IRcompare(int numpulses, int Signal[], int refsize) {
  int count = MIN(numpulses,refsize);
	int i;

	if(count < 8) {
		return(0);
	}
  printf("count set to: %d\n", count);

for (i=0; i< count-1; i++) {
    int oncode = pulses[i][1] * RESOLUTION;
    int offcode = pulses[i+1][0] * RESOLUTION;
    
#ifdef DEBUG    
	printf("%d - %d", oncode, Signal[i*2 + 0]);
#endif   
    
    // check to make sure the error is less than FUZZINESS percent
    if ( abs(oncode - Signal[i*2 + 0]) <= (Signal[i*2 + 0] * FUZZINESS / 100)) {
#ifdef DEBUG
		printf(" (ok)");
#endif
    } else {
#ifdef DEBUG
		printf(" (x)");
#endif
      // we didn't match perfectly, return a false match
      return 0;
    }
    
    
#ifdef DEBUG
	printf("  \t%d - %d", offcode, Signal[i*2 + 1]);
#endif    
    
    if ( abs(offcode - Signal[i*2 + 1]) <= (Signal[i*2 + 1] * FUZZINESS / 100)) {
#ifdef DEBUG
		printf(" (ok)");
#endif
    } else {
#ifdef DEBUG
		printf(" (x)");
#endif
      // we didn't match perfectly, return a false match
      return 0;
    }
    
#ifdef DEBUG
	printf("\n");
#endif
  }
  // Everything matched!
  return 1;
}

void printpulses(void) {
	uint8_t i;
  printf("\n\r\n\rReceived: \n\rOFF \t\tON\n");
  for (i = 0; i < currentpulse; i++) {
    printf("  %d us,\t%d us\n",pulses[i][0] * RESOLUTION,pulses[i][1] * RESOLUTION);
  }

  // print it in a 'array' format
  printf("int IRsignal[] = {\n");
  printf("// ON, OFF (in 10's of microseconds)\n");
  for (i = 0; i < currentpulse-1; i++) {
    printf("\t"); // tab
    printf("%d",pulses[i][1] * RESOLUTION);
    printf(", ");
    printf("%d",pulses[i+1][0] * RESOLUTION);
    printf(",\n");
  }
  printf("\t"); // tab
  printf("%d",pulses[currentpulse-1][1] * RESOLUTION);
  printf(", 0};\n");
}

int read_ir_pulses(void)
{
	uint16_t highpulse, lowpulse; // temporary storage timing

	currentpulse = 0;
	if(IRDATA) {
		return(0);
	}
	for(;;) {
		highpulse = lowpulse = 0; // start out with no pulse length

		while(IRDATA) {
			highpulse++;
			TIMER_Delay(TIMER2, RESOLUTION);
			if (((highpulse >= MAXPULSE) /*&& (currentpulse != 0)*/) || currentpulse == NUMPULSES) {
				return currentpulse;
			}
		}
		pulses[currentpulse][0] = highpulse;

		while(IRDATA == 0) {
			lowpulse++;
			TIMER_Delay(TIMER2, RESOLUTION);
			if (((lowpulse >= MAXPULSE) /*&& (currentpulse != 0)*/) || currentpulse == NUMPULSES) {
				return currentpulse;
			}
		}
		pulses[currentpulse][1] = lowpulse;

		currentpulse++;
	}
}

#endif
