/* Extracted from Chantler data */

#define NUM_SPECIES 22
#define NUM_ENTRIES 274

/* density in g/cm^3 */
extern double matdensity[]; /* NUM_SPECIES */

typedef struct filterMaterial {
	int Z;
	char *name;
	float density;			/* g/cm^3 */
	int numEntries;
	float keV[NUM_ENTRIES];
	float mu[NUM_ENTRIES];	/* cm^2/g */
} filterMaterial;

extern filterMaterial filtermat[]; /* NUM_SPECIES */
