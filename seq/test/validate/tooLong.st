/*
 * Test proper rejection of variables whose type is too long
 * to allow faithfull representation in CA, i.e. 64 bit types.
 * On linux-x86_64 this should give error messages for the l and ul
 * variables. On systems where sizeof(int)==8 (64 bit Windows?)
 * even i and ui should be rejected.
 */
program tooLong

int i;
assign i to "";

unsigned int ui;
assign ui to "";

long l;
assign l to "";

unsigned long ul;
assign ul to "";

ss tooLong {
    state xy {
        when () {
        } state xy
    }
}
