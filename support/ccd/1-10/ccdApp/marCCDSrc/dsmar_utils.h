struct dserver_vars {
  int x_bkg;
  int y_bkg;
  int x;
  int y;
  int xbin;
  int ybin;
  int frameshift_size;
  double preset;
  int state;
  int prev_state;
};

/*
 * external C function declarations needed for C++
 */

#ifdef _cplusplus
extern "C" {
#endif

void dserver_init_child();
int dserver_check_in_client();
void dserver_close();
int dserver_send(char *cmd, ...);
void dserver_get_state();
void dserver_get_size();
void dserver_set_size();
void dserver_get_bin();
void dserver_set_bin();
void dserver_get_preset();
void dserver_set_preset();
void dserver_start_acq();
void dserver_readout();
void dserver_correct();
void dserver_writefile();
void dserver_abort();
int dserver_header();

#ifdef _cplusplus
}
#endif /* _cplusplus */
