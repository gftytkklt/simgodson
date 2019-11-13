
/* optional technology paremeters are: TSMC25, TSMC18, SMIC18, TSMC13G, TSMC09G, ST09 */
#define ST09

/* optional condition paremeters are: TYPICAL, MAXIMUM, MINIMUM */
#define TYPICAL

#define FREQ 1e9

struct power_stat
{
  int    inst;
  int    reg;
  double timing;
  double area;
  double total;
  double dynamic;
  double leakage;
  double clock;
};

struct power_stat_mux
{
  int    inst;
  int    reg;
  double timing;
  double area;
  double dyn_sel;
  double dyn_data;
  double leakage;
  double clock;
};

struct power_stat_ram
{
  int    inst;
  int    reg;
  double timing;
  double area;
  double dyn_read;
  double dyn_write;
  double leakage;
  double clock;
};

// wire param's
struct wire_param_t {
    // constant
    double topwidth;
    double topthick;
    double topspace;
    double topheight;
    double topepsilon; // dielectric constant
    double rho; // conductance of Cu
    // param's for shielding wire
    double res;
    double cap;
};

// mos param's
struct mos_param_t {
    // constant
    double rd0; // equivalent res of 1um width mos with delay matching
    double rt0; // equivalent res of 1um width mos with transition matching
    double cg0; // gate cap of 1um width mos
    double cd0; // drain cap of 1um width mos
    double alpha; // velocity saturation index of mos
    double vth; // threshold voltage of mos
    double idsat0; // saturated drain current of 1um width mos (uA)
    double isub0; // subthreshold current of 1um width mos (uA)
    double ig0; // gate leakage current of 1um width mos (uA)
};

// all param's
struct param_t {
    struct wire_param_t wire;
    struct mos_param_t nmos;
    struct mos_param_t pmos;
    double vdd; // V
    double freq; // GHz
};

struct timing_t {
    double td; // delay
    double tt; // transition
};

struct power_t {
    double ed; // dynamic energy
    double es; // short-circuit energy
    double pl; // leakage power
};

#define MAX_WIDTH 13
enum {X1,X2,X4,X8,MAX_DRIVE};

extern struct power_stat power_table_cmp[MAX_WIDTH];
extern struct power_stat power_table_decode[MAX_WIDTH];
extern struct power_stat power_table_encode[MAX_WIDTH];
extern struct power_stat power_table_first_one[MAX_WIDTH];
extern struct power_stat power_table_first_one_d[MAX_WIDTH];
extern struct power_stat power_table_second_one_d[MAX_WIDTH];
extern struct power_stat power_table_find_oldest_d[MAX_WIDTH];
extern struct power_stat_mux power_table_mux[MAX_WIDTH][11];
extern struct power_stat power_table_reg[MAX_DRIVE];
extern struct power_stat power_table_reg_en[MAX_DRIVE];
extern struct power_stat power_table_reg_scan[MAX_DRIVE];
extern struct power_stat power_table_reg_scan_en[MAX_DRIVE];
extern struct power_stat power_table_arbiter[9];
extern struct power_stat power_table_crossbar[MAX_WIDTH];         
extern struct power_stat power_table_ram_write[MAX_WIDTH][15];
extern struct power_stat_mux power_table_ram_read[MAX_WIDTH][15];
extern struct power_stat power_table_ialu;
extern struct power_stat power_table_imul;
extern struct power_stat power_table_idiv;
extern struct power_stat power_table_fadd;
extern struct power_stat power_table_fcvt;
extern struct power_stat power_table_fmisc;
extern struct power_stat power_table_fmul;
extern struct power_stat power_table_fdivsqrt;
extern struct power_stat power_table_fmadd;
extern struct power_stat power_table_mdmx;
extern struct power_stat power_table_inst_decode;
extern struct power_stat power_table_pad;
extern struct power_stat power_table_clkbuf;
extern double reg_scan_en_clkcap[MAX_DRIVE];
extern double cap_per_unit;
extern double clkbuf_max_load;
extern double clkbuf_cap;

extern void power_reg_scan_en(struct power_stat * return_power,int instance,int gated_clock,int width,int drive);
extern void power_cmp(struct power_stat * return_power,int instance,int width);
extern void power_first_one(struct power_stat * return_power,int instance,int width);
extern void power_first_one_d(struct power_stat * return_power,int instance,int width);
extern void power_second_one_d(struct power_stat * return_power,int instance,int width);
extern void power_find_oldest_d(struct power_stat * return_power,int instance,int width);
extern void power_decode(struct power_stat * return_power,int instance,int width);
extern void power_encode(struct power_stat * return_power,int instance,int width);
extern void power_mux(struct power_stat_mux * return_power,int instance,int width,int mux);
extern void power_ialu(struct power_stat * return_power,int instance,int gated_clock);
extern void power_imul(struct power_stat * return_power,int instance,int gated_clock);
extern void power_idiv(struct power_stat * return_power,int instance,int gated_clock);
extern void power_fadd(struct power_stat * return_power,int instance,int gated_clock);
extern void power_fmul(struct power_stat * return_power,int instance,int gated_clock);
extern void power_fdivsqrt(struct power_stat * return_power,int instance,int gated_clock);
extern void power_fcvt(struct power_stat * return_power,int instance,int gated_clock);
extern void power_fmisc(struct power_stat * return_power,int instance,int gated_clock);
extern void power_fmadd(struct power_stat * return_power,int instance,int gated_clock);
extern void power_inst_decode(struct power_stat * return_power,int instance,int gated_clock);
extern void power_ram_ff(struct power_stat_ram * return_power,int gated_clock,int Wport,int Rport,int width,int depth);
extern void power_arbiter(struct power_stat * return_power,int instance,int gated_clock,int depth);
extern void power_crossbar(struct power_stat * return_power,int instance,int width);
extern void power_IOpad(struct power_stat * return_power, int instance);

extern void cache_power_init();
extern void clear_access_stats(struct godson2_cpu *st);
extern void initial_power_stats(struct godson2_cpu *st);
extern void update_power_stats(struct godson2_cpu *st);
extern void power_init(struct godson2_cpu *st);
extern void power_reg_stats(struct stat_sdb_t *sdb);    /* stats database */

extern int gated_clock;
  
