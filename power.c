#include <stdarg.h>
#include <math.h>
#include "godson2_cpu.h"
#include "noc.h"
#include "sim.h"
#include "power.h"
#include "cacti/cacti_interface.h"

#ifdef POWER_STAT

int gated_clock = 0;
int godson2_ram = 1;
int initial_power = 0 ;

void power_stat_sum(struct power_stat * sum, struct power_stat * a, struct power_stat * b, ...)
{
  struct power_stat * arg = b;

  sum->total = sum->dynamic = sum->leakage = sum->clock = 0.0;
  sum->total = sum->total + a->total;
  sum->dynamic = sum->dynamic + a->dynamic;
  sum->leakage = sum->leakage + a->leakage;
  sum->clock = sum->clock + a->clock;
 
  va_list arg_ptr;
  arg = va_arg(arg_ptr, struct power_stat *);
  va_start(arg_ptr, b);

  while(arg != 0xffffffff){
    sum->total = sum->total + arg->total;
    sum->dynamic = sum->dynamic + arg->dynamic;
    sum->leakage = sum->leakage + arg->leakage;
    sum->clock = sum->clock + arg->clock;
    arg = va_arg(arg_ptr, struct power_stat *);
  };
  va_end(arg_ptr);
}

void power_stat_accu(struct power_stat *power, struct power_stat *a)
{
  power->dynamic = power->dynamic + a->dynamic;
  if(gated_clock) power->clock = power->clock + a->clock;
}

/* interconnect power calculate */
 //param st090
 struct param_t param = {
  { 0.42, //topwidth;
    0.9,  //topthick;
    0.42, //topspace;
    0.7, //topheight;
    4.2, //topepsilon
    0.022, //rho
    57.5198, //res
    306.5 //cap
  },
  { 343.2382 * 3.06, //rd0 
    197.5825 * 3.06, //rt0 
    1.1802, //cg0 
    0.7952, //cd0 
    1.20, //alpha
    0.2328, //vth
    2.572/3.06, //idsat0
    0.0000418/(3.06+5.28), //isub0
    0.0000846/(3.06+5.28), //ig0
  },
  { 457.1645*5.28, //rd0 
    343.2382*5.28, //rt0 
    1.1802, //cg0 
    0.7952, //cd0 
    1.67, //alpha
    0.1533, //vth
    2.635/5.28, //idsat0
    0.0000418/(3.06+5.28), //isub0
    0.0000846/(3.06+5.28), //ig0
  },
  1.2, //double vdd
  0.5 //double GHz freq
};

void intcnt_timing_power (double width_nmos, double length_wire, double stage,
                          struct timing_t * timing, struct power_t * power)
{
    double wn, wp, rdp, rdn, rtp, rtn, cd, cg, tdsr, tdsf, ttsr, ttsf, lw, rw, cw,
        vdd, vthn, vthp, alphan, alphap, gamman, gammap, tdr, tdf, ttr, ttf,
        ed, tevr, tevf, sigmar, sigmaf, ceffr, cefff, fr, ff, gr, gf, hr, hf,
        idn, idp, esr, esf, isub, ig, pl;;

    wn = width_nmos;
    wp = wn * 1.7255;
    // equivalent resistance with delay matching
    rdp = param.pmos.rd0 / wp;
    rdn = param.nmos.rd0 / wn;
    // equivalent resistance with transition matching
    rtp = param.pmos.rt0 / wp;
    rtn = param.nmos.rt0 / wn;
    // capacitance of mos
    cd = param.nmos.cd0 * wn + param.pmos.cd0 * wp; // drain cap
    cg = param.nmos.cg0 * wn + param.pmos.cg0 * wp; // gate cap

    // step input response of single stage
    lw = length_wire / stage;
    rw = param.wire.res * lw / 1000;
    cw = param.wire.cap * lw / 1000;
    // delay
    tdsr = ( 0.377 * rw * cw + 0.693 * ( rdn * ( cd + cg ) + rdn * cw + rw * cg ) ) * 1e-3; // ps
    tdsf = ( 0.377 * rw * cw + 0.693 * ( rdp * ( cd + cg ) + rdp * cw + rw * cg ) ) * 1e-3; // ps
    // transition
    ttsr = ( 1.1 * rw * cw + 2.75 * ( rtn * ( cd + cg ) + rtn * cw + rw * cg ) ) * 1e-3;
    ttsf = ( 1.1 * rw * cw + 2.75 * ( rtp * ( cd + cg ) + rtp * cw + rw * cg ) ) * 1e-3;

    // contribution of input slope to the delay
    vdd = param.vdd;
    vthn = fabs(param.nmos.vth / vdd);
    vthp = fabs(param.pmos.vth / vdd);
    alphan = param.nmos.alpha;
    alphap = param.pmos.alpha;
    gamman = 0.5 - ( 1 - vthn ) / ( 1 + alphan );
    gammap = 0.5 - ( 1 - vthp ) / ( 1 + alphap );

    // final delay
    tdr = tdsr + ttsr * gamman;
    tdf = tdsf + ttsf * gammap;
    // final transition
    ttr = ttsr;
    ttf = ttsf;
    // return
    timing->td = ( tdr + tdf ) * stage / 2;
    timing->tt = ( ttr + ttf ) * stage / 2;

    // dynamic energy
    ed = 1e-3 * 0.5 * cw * pow(vdd, 2.0); // pJ
    // short-circuit energy
    //// effective capacitance: ceffr, cefff
    tevr = 0.5 * ( 1 - vthn - vthp ) * ttr; // time when the effective capacitance is evaluated
    tevf = 0.5 * ( 1 - vthn - vthp ) * ttf;
    sigmar = rw * 0.5 * cw / tevr;
    sigmaf = rw * 0.5 * cw / tevf;
    ceffr = 0.5 * cw + 0.5 * cw *
        ( 1 - 2 * sigmar + 2 * pow(sigmar, 2) * ( 1 - exp( -1 / sigmar ) ) ); // fF
    cefff = 0.5 * cw + 0.5 * cw *
        ( 1 - 2 * sigmaf + 2 * pow(sigmaf, 2) * ( 1 - exp( -1 / sigmaf ) ) ); // fF
    //// coefficients associated with rise and fall inputs: gr, hr, fr, gf, hf, ff
    fr = 1 / ( alphan + 2 ) - alphap / ( 2 * alphan + 6 ) + alphap * ( alphap * 0.5 - 1 )
        / ( alphan + 4 );
    ff = 1 / ( alphap + 2 ) - alphan / ( 2 * alphap + 6 ) + alphan * ( alphan * 0.5 - 1 )
        / ( alphap + 4 );
    gr = ( alphan + 1 ) * pow(( 1 - vthn ), alphan) * pow(( 1 - vthp ), (0.5 * alphap))
        / ( fr * pow(( 1 - vthn - vthp ), ( 0.5 * alphap + alphan + 2 ) ));
    gf = ( alphap + 1 ) * pow(( 1 - vthp ), alphap) * pow(( 1 - vthn ), (0.5 * alphan))
        / ( ff * pow(( 1 - vthn - vthp ), ( 0.5 * alphan + alphap + 2 ) ));
    hr = pow(2, alphap) * ( alphap + 1 ) * pow(( 1 - vthp ), alphap)
        / pow(( 1 - vthn - vthp ), ( alphap + 1 ));
    hf = pow(2, alphan) * ( alphan + 1 ) * pow(( 1 - vthn ), alphan)
        / pow(( 1 - vthn - vthp ), ( alphan + 1 ));
    //// short-circuit power
    idn = param.nmos.idsat0 * wn;
    idp = param.pmos.idsat0 * wp;
    esr = 1e-3 * 2 * pow(idn, 2) * pow(ttr, 2) * vdd
        / ( vdd * gr * ceffr + 2 * hr * idn * ttr * 1e-3 ); // pJ
    esf = 1e-3 * 2 * pow(idp, 2) * pow(ttf, 2) * vdd
        / ( vdd * gf * cefff + 2 * hf * idp * ttf * 1e-3); // pJ
    // leakage power
    isub = ( param.nmos.isub0 * wn + param.pmos.isub0 * wp ) / 2;
    ig = param.nmos.ig0 * wn + param.pmos.ig0 * wp;
    pl = vdd * ( isub + ig ); // mA * V = mW
    // return
    power->ed = ed * stage;
    power->es = ( esf + esr ) * stage / 2;
    power->pl = pl * stage;
}

void intcnt_opt (double length_wire, struct timing_t * timingopt, struct power_t * poweropt) {
    double wn, wnopt;
    double wnmin = 2; // minimum width of nmos (um)
    double wnmax = 25; // maximum width of nmos (um)
    double wnstep = 0.1;
    double stg, stgopt;
    double stgmin = 2;
    double stgmax = 20;
    double stgstep = 1;
    double lw;
    double pwr, pwropt;
    struct timing_t timing;
    struct power_t power;

    timingopt->td = 999999;
    timingopt->tt = 999999;
    poweropt->ed = 999999;
    poweropt->es = 999999;
    poweropt->pl = 999999;
    wnopt = 0;
    stgopt = 0;

    for (stg=stgmin; stg<=stgmax; stg=stg+stgstep) {
        for (wn=wnmin; wn<=wnmax; wn=wn+wnstep) {
            lw = length_wire / stg;
            intcnt_timing_power (wn, lw, stg, &timing, &power);

            // evaluation
            if ( (timing.td <= 500) && (timing.tt <= 200) ) {
                //pwr = power.ed + power.es + power.pl;
                //pwropt = ( poweropt->ed + poweropt->es ) * param.freq + poweropt->pl; // mW at 1GHz
                //if (pwr < pwropt) {
                if (timing.td < timingopt->td) {
                    wnopt = wn;
                    stgopt = stg;
                    timingopt->td = timing.td;
                    timingopt->tt = timing.tt;
                    poweropt->ed = power.ed;
                    poweropt->es = power.es;
                    poweropt->pl = power.pl;
                }
            }
        }
    }
    printf("stgopt=%lf    wnopt=%lf (um)\n", stgopt, wnopt);
    printf("timingopt.td=%lf (ps)    timingopt.tt=%lf (ps)\n", timingopt->td, timingopt->tt);
    printf("poweropt.ed=%lf (pJ)    poweropt.es=%lf (pJ)    poweropt.pl=%lf (mW)\n", poweropt->ed, poweropt->es,
        poweropt->pl);
}

total_result_type regfile_power_cacti;
total_result_type itlb_ram_power_cacti;
total_result_type dtlb_ram_power_cacti;
total_result_type icache_power_cacti;
total_result_type dcache_power_cacti;
total_result_type cache_dl2_power_cacti;

void cache_power_init()
{
  regfile_power_cacti = cacti_interface(
	64 * 8 * 8,// int cache_size
	8 * 8,   // int line_size
	1,   // int associativity
	0,   // int rw_ports
	4,   // int excl_read_ports
	4,   // int excl_write_ports
	0,   // int single_ended_read_ports
	1,   // int banks
	0.10,// double tech_node
	data_width, // int output_width
	0,   // int specific_tag
	0,   // int tag_width
	0,   // int access_mode
	1,   // int pure_sram
    0.0, // double Nspd_predef
    0,   // int Ndwl_predef
    0,   // int Ndbl_predef
    1);  // int optimize)

  itlb_ram_power_cacti = cacti_interface(
        28 * (cpus[0].itlb->nsets * cpus[0].itlb->assoc),
		28,1,
		0,1,1,0,1,0.10,data_width,1,28,0,1,0.0,0,0,1);

  dtlb_ram_power_cacti = cacti_interface(
        4*pa_size * (cpus[0].dtlb->nsets*cpus[0].dtlb->assoc),
		4*pa_size,1,
		0,1,1,0,1,0.10,data_width,1,4*pa_size,0,1,0.0,0,0,1);

  icache_power_cacti = cacti_interface(
		cpus[0].icache->nsets * cpus[0].icache->bsize * cpus[0].icache->assoc * 8,
		cpus[0].icache->bsize*8,cpus[0].icache->assoc,
		1,0,0,0,1,0.10,data_width*cpus[0].icache->assoc,1,va_size,0,0,0.0,0,0,1);

  dcache_power_cacti = cacti_interface(
	    cpus[0].dcache->nsets * cpus[0].dcache->bsize * cpus[0].dcache->assoc * 8,
		cpus[0].dcache->bsize*8,cpus[0].dcache->assoc,
		1,0,0,0,1,0.10,data_width*cpus[0].dcache->assoc,1,va_size,0,0,0.0,0,0,1);

  cache_dl2_power_cacti = cacti_interface(
	    cpus[0].cache_dl2->nsets * cpus[0].cache_dl2->bsize * cpus[0].cache_dl2->assoc * 8,
		cpus[0].cache_dl2->bsize*8,cpus[0].cache_dl2->assoc,
		1,0,0,0,1,0.10,data_width*cpus[0].cache_dl2->assoc,1,pa_size,0,0,0.0,0,0,1);

  output_data(&regfile_power_cacti.result,&regfile_power_cacti.area,&regfile_power_cacti.params);
  output_data(&itlb_ram_power_cacti.result,&itlb_ram_power_cacti.area,&itlb_ram_power_cacti.params);
  output_data(&dtlb_ram_power_cacti.result,&dtlb_ram_power_cacti.area,&dtlb_ram_power_cacti.params);
  output_data(&icache_power_cacti.result,&icache_power_cacti.area,&icache_power_cacti.params);
  output_data(&cache_dl2_power_cacti.result,&cache_dl2_power_cacti.area,&cache_dl2_power_cacti.params);
  output_data(&cache_dl2_power_cacti.result,&cache_dl2_power_cacti.area,&cache_dl2_power_cacti.params);

}

void calculate_decode_power(struct godson2_cpu *st,struct power_stat *decode_power)
{
  static struct power_stat pipe_in, inst_decode;
  
  if(initial_power) { 
    //pipeline in 
    power_reg_scan_en(&pipe_in, decode_width, gated_clock, decode_pipein_bits, X4);
    //instruction decode
    power_inst_decode(&inst_decode, decode_width, gated_clock);
    //sum up
    decode_power->inst    = pipe_in.inst  + inst_decode.inst;
    decode_power->area    = pipe_in.area  + inst_decode.area;
    decode_power->reg     = pipe_in.reg   + inst_decode.reg;
    decode_power->clock   = pipe_in.clock + inst_decode.clock;
    decode_power->leakage = pipe_in.leakage + inst_decode.leakage;
    decode_power->dynamic = decode_power->total   = 0.0; 
  }

  if(gated_clock) {
    decode_power->clock = pipe_in.clock * st->decode_access
                        + inst_decode.clock * st->decode_access;
  }

  decode_power->dynamic = pipe_in.dynamic * st->decode_access
                        + inst_decode.dynamic * st->decode_access;
}

void calculate_rename_power(struct godson2_cpu *st,struct power_stat *rename_power)
{
  static struct power_stat pipe_in, fix_alloc, fix_cmp, fix_encode, fp_alloc, fp_cmp, fp_encode, pipe_out;
  static struct power_stat_ram fix_ctrl, fix_lastvalid, fix_name, fp_ctrl, fp_lastvalid, fp_name;
  static struct power_stat_mux mux_out;

  if(initial_power){
    //pipeline in 
    power_reg_scan_en(&pipe_in, map_width, gated_clock, map_pipein_bits, X4);
    //prmt: physical register mapping table, include grmt and frmt
    //grmt:
    //allocate physical regsiter
    power_first_one_d(&fix_alloc, map_width, fix_phy_num);
    //a ram used to hold state&valid in each entry
    power_ram_ff(&fix_ctrl, gated_clock, fix_wb_width+commit_width, map_width*3, fix_state_bits, fix_phy_num);
    //a ram used to hold last_valid in each entry
    power_ram_ff(&fix_lastvalid, gated_clock, map_width, 1, fix_phy_num, brq_ifq_size);
    //a ram used to hold logic register in each entry  
    power_ram_ff(&fix_name, gated_clock, map_width, 0, fix_phy_lognum, fix_phy_num);
    //cmpare logic used to transfrom logic register to physical register
    power_cmp(&fix_cmp, fix_phy_num*map_width*3, fix_phy_lognum);
    //encode compared results to form physical regiser
    power_encode(&fix_encode, map_width*3, fix_phy_num);
    //frmt:
    //allocate physical regsiter
    power_first_one_d(&fp_alloc, map_width, fp_phy_num);
    //a ram used to hold state&valid in each entry
    power_ram_ff(&fp_ctrl, gated_clock, fp_wb_width+commit_width, map_width*3, fp_state_bits, fp_phy_num);
    //a ram used to hold last_valid in each entry
    power_ram_ff(&fp_lastvalid, gated_clock, map_width, 1, fp_phy_num, brq_ifq_size);
    //a ram used to hold logic register in each entry  
    power_ram_ff(&fp_name, gated_clock, map_width, 0, fp_phy_lognum, fp_phy_num);
    //cmpare logic used to transfrom logic register to physical register
    power_cmp(&fp_cmp, fp_phy_num*map_width*3, fp_phy_lognum);
    //encode compared results to form physical regiser
    power_encode(&fp_encode, map_width*3, fp_phy_num);
    //pipeline_out 
    power_mux(&mux_out, map_width, map_pipeout_bits, 2);
    power_reg_scan_en(&pipe_out, map_width, gated_clock, map_pipeout_bits, X4);
    //sum up
    rename_power->inst = pipe_in.inst + fix_alloc.inst + fix_cmp.inst + fix_encode.inst + fp_alloc.inst + fp_cmp.inst + fp_encode.inst + pipe_out.inst
                       + fix_ctrl.inst + fix_lastvalid.inst + fix_name.inst + fp_ctrl.inst + fp_lastvalid.inst + fp_name.inst + mux_out.inst;
    rename_power->reg  = pipe_in.reg + fix_alloc.reg + fix_cmp.reg + fix_encode.reg + fp_alloc.reg + fp_cmp.reg + fp_encode.reg + pipe_out.reg
                       + fix_ctrl.reg + fix_lastvalid.reg + fix_name.reg + fp_ctrl.reg + fp_lastvalid.reg + fp_name.reg + mux_out.reg;
    rename_power->area = pipe_in.area + fix_alloc.area + fix_cmp.area + fix_encode.area + fp_alloc.area + fp_cmp.area + fp_encode.area + pipe_out.area
                       + fix_ctrl.area + fix_lastvalid.area + fix_name.area + fp_ctrl.area + fp_lastvalid.area + fp_name.area + mux_out.area;
    rename_power->clock = pipe_in.clock + fix_alloc.clock + fix_cmp.clock + fix_encode.clock + fp_alloc.clock + fp_cmp.clock + fp_encode.clock + pipe_out.clock
                       + fix_ctrl.clock + fix_lastvalid.clock + fix_name.clock + fp_ctrl.clock + fp_lastvalid.clock + fp_name.clock + mux_out.clock;
    rename_power->leakage = pipe_in.leakage + fix_alloc.leakage + fix_cmp.leakage + fix_encode.leakage + fp_alloc.leakage + fp_cmp.leakage + fp_encode.leakage + pipe_out.leakage
                           + fix_ctrl.leakage + fix_lastvalid.leakage + fix_name.leakage + fp_ctrl.leakage + fp_lastvalid.leakage + fp_name.leakage + mux_out.leakage;
    rename_power->dynamic = rename_power->total = 0.0; 
  }

  if(gated_clock) {
    rename_power->clock = pipe_in.clock * st->decode_access
                        + fix_ctrl.clock * (st->fix_wb_access+st->fix_commit_access)
                        + fix_lastvalid.clock * st->fix_br_access
                        + fix_name.clock * st->fix_map_access
                        + fp_ctrl.clock * (st->fp_wb_access+st->fp_commit_access)
                        + fp_lastvalid.clock * st->fp_br_access
                        + fp_name.clock * st->fp_map_access
                        + pipe_out.clock * (st->fix_map_access+st->fp_map_access);
  }
  
  rename_power->dynamic = pipe_in.dynamic * st->decode_access
                        + fix_alloc.dynamic * st->fix_map_access
                        + fix_ctrl.dyn_write * (st->fix_wb_access+st->fix_commit_access)
                        + fix_ctrl.dyn_read  * (st->fix_map_access*3)
                        + fix_lastvalid.dyn_write * st->fix_br_access
                        + fix_lastvalid.dyn_read  * st->bht_wb_access
                        + fix_name.dyn_write * st->fix_map_access
                        + fix_cmp.dynamic * (fix_phy_num*st->fix_map_access*3+st->fix_map_access*map_width*3)
                        + fix_encode.dynamic * st->fix_map_access*3
                        + fp_alloc.dynamic * st->fp_map_access
                        + fp_ctrl.dyn_write * (st->fp_wb_access+st->fp_commit_access)
                        + fp_ctrl.dyn_read  * st->fp_map_access*3
                        + fp_lastvalid.dyn_write * st->fp_br_access
                        + fp_lastvalid.dyn_read  * st->bht_wb_access
                        + fp_name.dyn_write * st->fp_map_access
                        + fp_cmp.dynamic * (fp_phy_num*st->fp_map_access*3+st->fp_map_access*map_width*3)
                        + fp_encode.dynamic * st->fp_map_access*3
                        + mux_out.dyn_sel  * (st->fix_map_access+st->fp_map_access)
                        + mux_out.dyn_data * (st->fix_map_access+st->fp_map_access)
                        + pipe_out.dynamic * (st->fix_map_access+st->fp_map_access);
}

void calculate_window_power(struct godson2_cpu *st,struct power_stat *window_power)
{
  static struct power_stat fixq_alloc,fixq_wb,fixq_age,fixq_select,fpq_alloc,fpq_wb,fpq_age,fpq_select;
  static struct power_stat_ram fixq_misc,fixq_psrc,fpq_misc,fpq_psrc;

  //issuequeue: include fixq and fpq
  if(initial_power) { 
    //fixq:
    //allocate fixqueue entries
    power_first_one_d(&fixq_alloc, map_width, int_issue_ifq_size);
    //a ram queue used to hold state&valid in each entry
    power_ram_ff(&fixq_misc, gated_clock, map_width, fix_issue_width, fixq_misc_bits, int_issue_ifq_size);
    //a ram queue used to hold psrc0,psrc1,psrc2 info in each entry  
    power_ram_ff(&fixq_psrc, gated_clock, map_width, fix_issue_width, fix_phy_lognum*2+8, int_issue_ifq_size);
    //compare logic used to change inst ready state when writeback
    power_cmp(&fixq_wb, int_issue_ifq_size*fix_wb_width*3*2, fix_phy_lognum);
    //selects the oldest inst to issue
    power_reg_scan_en(&fixq_age, int_issue_ifq_size, gated_clock, 3, X4);
    power_find_oldest_d(&fixq_select,fix_issue_width,int_issue_ifq_size);
    
    //fpq:
    //allocate floatpoint queue entries
    power_first_one_d(&fpq_alloc, map_width, fp_issue_ifq_size);
    //a ram used to hold state&valid in each entry
    power_ram_ff(&fpq_misc, gated_clock, map_width, fix_issue_width, fpq_misc_bits, fp_issue_ifq_size);
    //a ram used to hold float point queue info in each entry  
    power_ram_ff(&fpq_psrc, gated_clock, map_width, fp_issue_width, fp_phy_lognum*2+8, fp_issue_ifq_size);
    //compare logic used to change inst ready state when writeback
    power_cmp(&fpq_wb, fp_issue_ifq_size*fp_wb_width*3*2, fp_phy_lognum);
    //selects the oldest inst to issue
    power_reg_scan_en(&fpq_age, fp_issue_ifq_size, gated_clock, 3, X4);
    power_find_oldest_d(&fpq_select,fp_issue_width,fp_issue_ifq_size);
    //sum up
    window_power->inst = fixq_alloc.inst + fixq_wb.inst + fixq_psrc.inst + fixq_misc.inst + fixq_age.inst + fixq_select.inst
                       + fpq_alloc.inst + fpq_wb.inst + fpq_psrc.inst + fpq_misc.inst + fpq_age.inst + fpq_select.inst;
    window_power->reg = fixq_alloc.reg + fixq_wb.reg + fixq_psrc.reg + fixq_misc.reg + fixq_age.reg + fixq_select.reg
                       + fpq_alloc.reg + fpq_wb.reg + fpq_psrc.reg + fpq_misc.reg + fpq_age.reg + fpq_select.reg;
    window_power->area = fixq_alloc.area + fixq_wb.area + fixq_psrc.area + fixq_misc.area + fixq_age.area + fixq_select.area
                       + fpq_alloc.area + fpq_wb.area + fpq_psrc.area + fpq_misc.area + fpq_age.area + fpq_select.area;
    window_power->clock = fixq_alloc.clock + fixq_wb.clock + fixq_psrc.clock + fixq_misc.clock + fixq_age.clock + fixq_select.clock
                       + fpq_alloc.clock + fpq_wb.clock + fpq_psrc.clock + fpq_misc.clock + fpq_age.clock + fpq_select.clock;
    window_power->leakage = fixq_alloc.leakage + fixq_wb.leakage + fixq_psrc.leakage + fixq_misc.leakage + fixq_age.leakage + fixq_select.leakage
                       + fpq_alloc.leakage + fpq_wb.leakage + fpq_psrc.leakage + fpq_misc.leakage + fpq_age.leakage + fpq_select.leakage;
    window_power->dynamic = window_power->total = 0.0;
  }     

  if(gated_clock) {
    window_power->clock = fixq_misc.clock * st->fix_map_access
                        + fixq_psrc.clock * st->fix_map_access
                        + fpq_misc.clock * st->fp_map_access
                        + fpq_psrc.clock * st->fp_map_access
                        + fixq_age.clock * int_issue_ifq_size
						+ fpq_age.clock * fp_issue_ifq_size;
  }

  window_power->dynamic = fixq_alloc.dynamic * st->fix_map_access
                        + fixq_misc.dyn_write * st->fix_map_access
                        + fixq_misc.dyn_read  * fix_issue_width //st->fix_issue_access
                        + fixq_psrc.dyn_write * st->fix_map_access
                        + fixq_psrc.dyn_read  * fix_issue_width //st->fix_issue_access
                        + fixq_wb.dynamic * (int_issue_ifq_size*st->fix_wb_access*3*2 + st->fix_map_access*fix_wb_width*3*2)
                        + fpq_alloc.dynamic * st->fp_map_access
                        + fpq_misc.dyn_write * st->fp_map_access
                        + fpq_misc.dyn_read  * fp_issue_width //st->fp_issue_access
                        + fpq_psrc.dyn_write * st->fp_map_access
                        + fpq_psrc.dyn_read  * fp_issue_width //st->fp_issue_access
                        + fpq_wb.dynamic * (fp_issue_ifq_size*st->fp_wb_access*3*2 + st->fp_map_access*fp_wb_width*3*2)
                        + fixq_age.dynamic * int_issue_ifq_size 
                        + fixq_select.dynamic * fix_issue_width
						+ fpq_age.dynamic * fp_issue_ifq_size
						+ fpq_select.dynamic * fp_issue_width;
}

void calculate_bpred_power(struct godson2_cpu *st,struct power_stat *bpred_power)
{
  static struct power_stat pc_reg,btb_cam_low,btb_cam_high;
  static struct power_stat_mux pc_mux;
  static struct power_stat_ram bht_ram,btb_ram;

  if(initial_power) {
   //bpred:
   //pc power, one read and four write port ( bht, btb, icache, itlb )
    power_reg_scan_en(&pc_reg, 1, gated_clock, va_size, X4);
    power_mux(&pc_mux,1,va_size,4);
    //bht power
    power_ram_ff(&bht_ram, gated_clock, 1, 1, 2*8, bht_nsets/2);
    //btb ram
    power_ram_ff(&btb_ram, gated_clock, 1, 1, va_size, btb_size);
    //btb cam
    power_cmp(&btb_cam_low, 5*btb_size, btb_pc_low); //pc_lo0~3,brbus_lo
    power_cmp(&btb_cam_high, 2*btb_size, btb_pc_high); //pc_high,brbus_lo
    //sum up
    bpred_power->inst = pc_reg.inst + pc_mux.inst + bht_ram.inst + btb_ram.inst + btb_cam_low.inst + btb_cam_high.inst;
    bpred_power->reg  = pc_reg.reg + pc_mux.reg + bht_ram.reg + btb_ram.reg;
    bpred_power->area = pc_reg.area + pc_mux.area + bht_ram.area + btb_ram.area + btb_cam_low.area + btb_cam_high.area;
    bpred_power->clock = pc_reg.clock + pc_mux.clock + bht_ram.clock + btb_ram.clock;
    bpred_power->leakage = pc_reg.leakage + pc_mux.leakage + bht_ram.leakage + btb_ram.leakage + btb_cam_low.leakage + btb_cam_high.leakage;
    bpred_power->dynamic = bpred_power->total = 0.0;
  }

  if(gated_clock) {
     bpred_power->clock = pc_reg.clock * 0.5
                        + bht_ram.clock * st->bht_wb_access
                        + btb_ram.clock * st->btb_wb_access;
  }

  bpred_power->dynamic = pc_reg.dynamic * 0.5
                       + pc_mux.dyn_sel * 0.1
                       + pc_mux.dyn_data * 0.5
                       + bht_ram.dyn_write * st->bht_wb_access
                       + bht_ram.dyn_read * st->bht_fetch_access
                       + btb_ram.dyn_write * st->btb_wb_access
                       + btb_ram.dyn_read * st->btb_fetch_access
                       + btb_cam_low.dynamic * (4*st->btb_fetch_access + st->fix_br_access + st->fp_br_access) * btb_size
                       + btb_cam_high.dynamic * (4*st->btb_fetch_access + st->fix_br_access + st->fp_br_access) * btb_size;
}


void calculate_ialu_power(struct godson2_cpu *st,struct power_stat *ialu_power)
{
  static struct power_stat  ialu_pipein,ialu_alu1,ialu_mult,ialu_div;

  if(initial_power) {
    //ialu: include memaddr
    //pipeline in 
    power_reg_scan_en(&ialu_pipein, fix_issue_width, gated_clock, ialu_pipein_bits,X4);
    power_ialu(&ialu_alu1, ialu_num, gated_clock);
    power_imul(&ialu_mult, 1, gated_clock);
    power_idiv(&ialu_div, 1, gated_clock);
    //pipeline_out 
    //power_reg_scan_en(&ialu_pipeout, fix_wb_width, gated_clock, ialu_pipeout_bits, X4);
    //sum up
    ialu_power->inst = ialu_pipein.inst + ialu_alu1.inst + ialu_mult.inst + ialu_div.inst;
    ialu_power->reg  = ialu_pipein.reg + ialu_alu1.reg + ialu_mult.reg + ialu_div.reg;
    ialu_power->area = ialu_pipein.area + ialu_alu1.area + ialu_mult.area + ialu_div.area;
    ialu_power->clock = ialu_pipein.clock + ialu_alu1.clock + ialu_mult.clock + ialu_div.clock;
    ialu_power->leakage = ialu_pipein.leakage + ialu_alu1.leakage + ialu_mult.leakage + ialu_div.leakage;
    ialu_power->dynamic = ialu_power->total = 0.0;
  }

  if(gated_clock) {
    ialu_power->clock = ialu_pipein.clock * st->fix_issue_access
                     + ialu_alu1.clock * (st->fix_add_access+st->fix_br_access+st->fix_mem_access)
                     + ialu_mult.clock * st->fix_mult_access
                     + ialu_div.clock * st->fix_div_access;
  }

  ialu_power->dynamic = ialu_pipein.dynamic * st->fix_issue_access
                     + ialu_alu1.dynamic * (st->fix_add_access+st->fix_br_access+st->fix_mem_access)
                     + ialu_mult.dynamic * st->fix_mult_access
                     + ialu_div.dynamic * st->fix_div_access;
}


void calculate_falu_power(struct godson2_cpu *st,struct power_stat *falu_power)
{
  static struct power_stat falu_pipein,falu_add,falu_misc,falu_cvt,falu_mult,falu_div,falu_mdmx,falu_pipeout;

  if(initial_power) {
    //pipeline in 
    power_reg_scan_en(&falu_pipein, fp_issue_width, gated_clock, falu_pipein_bits, X4);
    power_fmadd(&falu_add, 1, gated_clock);
    power_fmadd(&falu_mult, 1, gated_clock);
    //power_fadd(&falu_add, falu_num, gated_clock);
    //power_fmul(&falu_mult, 1, gated_clock);
    power_fmisc(&falu_misc, 1, gated_clock);
    power_fcvt(&falu_cvt, 1, gated_clock);
    power_fdivsqrt(&falu_div, 1, gated_clock);
    power_mdmx(&falu_mdmx, 1, gated_clock);
    //pipeline_out 
    power_reg_scan_en(&falu_pipeout, fp_wb_width, gated_clock, falu_pipeout_bits, X4);
    //sum up
    falu_power->inst = falu_pipein.inst + falu_add.inst + falu_misc.inst + falu_cvt.inst + falu_mult.inst + falu_div.inst  + falu_mdmx.inst + falu_pipeout.inst;
    falu_power->reg  = falu_pipein.reg + falu_add.reg + falu_misc.reg + falu_cvt.reg + falu_mult.reg + falu_div.reg + falu_mdmx.reg + falu_pipeout.reg;
    falu_power->area = falu_pipein.area + falu_add.area + falu_misc.area + falu_cvt.area + falu_mult.area + falu_div.area + falu_mdmx.area + falu_pipeout.area;
    falu_power->clock = falu_pipein.clock + falu_add.clock + falu_misc.clock + falu_cvt.clock + falu_mult.clock + falu_div.clock + falu_mdmx.clock + falu_pipeout.clock;
    falu_power->leakage = falu_pipein.leakage + falu_add.leakage + falu_misc.leakage + falu_cvt.leakage + falu_mult.leakage + falu_div.leakage + falu_mdmx.leakage + falu_pipeout.leakage;
    falu_power->dynamic = falu_power->total = 0.0;
  }

  if(gated_clock) {
    falu_power->clock = falu_pipein.clock * st->fp_issue_access
                     + falu_add.clock * st->fp_add_access
                     + falu_misc.clock * (st->fp_br_access+st->fp_cmp_access)
                     + falu_cvt.clock * st->fp_cvt_access
                     + falu_mult.clock * st->fp_mult_access
                     + falu_div.clock * st->fp_div_access
                     + falu_mdmx.clock * st->fp_mdmx_access
                     + falu_pipeout.clock * st->fp_wb_access;
  }

  falu_power->dynamic = falu_pipein.dynamic * st->fp_issue_access
                     + falu_add.dynamic * st->fp_add_access
                     + falu_misc.dynamic * (st->fp_br_access+st->fp_cmp_access)
                     + falu_cvt.dynamic * st->fp_cvt_access
                     + falu_mult.dynamic * st->fp_mult_access
                     + falu_div.dynamic * st->fp_div_access
                     + falu_mdmx.dynamic * st->fp_mdmx_access
                     + falu_pipeout.dynamic * st->fp_wb_access;
}

void calculate_roq_power(struct godson2_cpu *st,struct power_stat *roq_power)
{
  static struct power_stat_ram roq_ctrl,roq_ram,brq_ctrl,brq_ram;

  if(initial_power) {
    //roq:
    //a ram queue used to hold roq states&valid in each entry
    power_ram_ff(&roq_ctrl, gated_clock, map_width+fix_wb_width+fp_wb_width+commit_width, commit_width, roq_state_bits, roq_ifq_size);
    //a ram queue used to hold roq data in each entry  
    power_ram_ff(&roq_ram, gated_clock, map_width, commit_width, roq_bits, roq_ifq_size);
    //brq:
    //a ram queue used to hold brq states&valid in each entry
    power_ram_ff(&brq_ctrl, gated_clock, map_width+fix_wb_width+fp_wb_width+commit_width, commit_width, brq_state_bits, brq_ifq_size);
    //a ram queue used to hold brq data in each entry  
    power_ram_ff(&brq_ram, gated_clock, map_width, commit_width, brq_bits, brq_ifq_size);
    //sum up
    roq_power->inst = roq_ctrl.inst + roq_ram.inst + brq_ctrl.inst + brq_ram.inst;
    roq_power->reg  = roq_ctrl.reg + roq_ram.reg + brq_ctrl.reg + brq_ram.reg;
    roq_power->area = roq_ctrl.area + roq_ram.area + brq_ctrl.area + brq_ram.area;
    roq_power->clock = roq_ctrl.clock + roq_ram.clock + brq_ctrl.clock + brq_ram.clock;
    roq_power->leakage = roq_ctrl.leakage + roq_ram.leakage + brq_ctrl.leakage + brq_ram.leakage;
    roq_power->dynamic = roq_power->total = 0.0;
  }

  if(gated_clock) {
    roq_power->clock = roq_ctrl.clock * (st->fix_wb_access+st->fp_wb_access+st->fix_map_access+st->fp_map_access+st->commit_access)
                     + roq_ram.clock * (st->fix_map_access+st->fp_map_access)
                     + brq_ctrl.clock * (st->fix_wb_access+st->fp_wb_access+st->fix_map_access+st->fp_map_access+st->commit_access)
                     + brq_ram.clock * (st->fix_map_access+st->fp_map_access);
  }
  
  roq_power->dynamic = roq_ctrl.dyn_write * (st->fix_wb_access+st->fp_wb_access+st->fix_map_access+st->fp_map_access+st->commit_access)
                     + roq_ctrl.dyn_read  * st->commit_access
                     + roq_ram.dyn_write * 0.8 * (st->fix_map_access+st->fp_map_access)
                     + roq_ram.dyn_read  * st->commit_access
                     + brq_ctrl.dyn_write * 0.1 * (st->fix_wb_access+st->fix_map_access+st->commit_access)
                     + brq_ctrl.dyn_read  * 0.1 * st->commit_access
                     + brq_ram.dyn_write * 0.1 * st->fix_map_access
                     + brq_ram.dyn_read  * 0.1 * st->commit_access;
}

void calculate_lsq_power(struct godson2_cpu *st,struct power_stat *lsq_power)
{
  static struct power_stat_ram lsq_ctrl,lsq_ram;
  static struct power_stat lsq_compare;

  if(initial_power) {
    //lsq:
    //a ram queue used to hold state&valid in each entry
    power_ram_ff(&lsq_ctrl, gated_clock, lsq_wb_width+map_width+refill_width+commit_width, commit_width, lsq_state_bits, lsq_ifq_size);
    //a ram queue used to hold loadstore queue info in each entry  
    power_ram_ff(&lsq_ram, gated_clock, lsq_wb_width+map_width+refill_width, commit_width, lsq_bits, lsq_ifq_size);
    //cmpare logic used to change lsq info when refill or dcacheread
    //lsq_wb_width and lsq_wb_access refer to dcacheread
    power_cmp(&lsq_compare, lsq_ifq_size*lsq_wb_width, pa_size);
    //sum up
    lsq_power->inst = lsq_ctrl.inst + lsq_ram.inst + lsq_compare.inst;
    lsq_power->reg  = lsq_ctrl.reg + lsq_ram.reg + lsq_compare.reg;
    lsq_power->area = lsq_ctrl.area + lsq_ram.area + lsq_compare.area;
    lsq_power->clock = lsq_ctrl.clock + lsq_ram.clock + lsq_compare.clock;
    lsq_power->leakage = lsq_ctrl.leakage + lsq_ram.leakage + lsq_compare.leakage;
    lsq_power->dynamic = lsq_power->total = 0.0;
  }

  if(gated_clock) {
    lsq_power->clock = lsq_ctrl.clock * (st->lsq_wb_access+st->lsq_map_access+st->dcache_refill_access)
                     + lsq_ram.clock * (st->lsq_wb_access+st->lsq_map_access+st->dcache_refill_access);
  }
  
  lsq_power->dynamic = lsq_ctrl.dyn_write * (st->lsq_wb_access+st->lsq_map_access+st->dcache_refill_access)
                     + lsq_ctrl.dyn_read  * st->lsq_commit_access
                     + lsq_ram.dyn_write * (st->lsq_wb_access+st->lsq_map_access+st->dcache_refill_access)
                     + lsq_ram.dyn_read  * st->lsq_commit_access
                     + lsq_compare.dynamic * lsq_ifq_size*(st->lsq_wb_access+st->dcache_refill_access); 
}

void calculate_cache2mem_power(struct godson2_cpu *st,struct power_stat *cache2mem_power)
{
  static struct power_stat missq_alloc,wtbkq_alloc;
  static struct power_stat_ram missq_ram,wtbkq_ram;

  if(initial_power) { 
    //missq allocate one entry
    power_first_one_d(&missq_alloc, missq_ifq_size, missq_bits);
    //missq ram power
    power_ram_ff(&missq_ram, gated_clock, 2, 2, missq_bits, missq_ifq_size);
    //wtbkq allocate one entry
    power_first_one_d(&wtbkq_alloc, wtbkq_ifq_size, wtbkq_bits);
    //wtbkq ram power
    power_ram_ff(&wtbkq_ram, gated_clock, 1, 1, wtbkq_bits, wtbkq_ifq_size);
    //sum up
    cache2mem_power->inst = missq_alloc.inst + wtbkq_alloc.inst + missq_ram.inst + wtbkq_ram.inst;  
    cache2mem_power->reg  = missq_alloc.reg + wtbkq_alloc.reg + missq_ram.reg + wtbkq_ram.reg;  
    cache2mem_power->area = missq_alloc.area + wtbkq_alloc.area + missq_ram.area + wtbkq_ram.area;  
    cache2mem_power->clock = missq_alloc.clock + wtbkq_alloc.clock + missq_ram.clock + wtbkq_ram.clock;  
    cache2mem_power->leakage = missq_alloc.leakage + wtbkq_alloc.leakage + missq_ram.leakage + wtbkq_ram.leakage;  
    cache2mem_power->dynamic = cache2mem_power->total = 0.0;  
  }

   if(gated_clock) {
     cache2mem_power->clock = wtbkq_ram.clock * st->wtbkq_access
                            + missq_ram.clock * st->missq_access;
   }
   
   cache2mem_power->dynamic = missq_alloc.dynamic * st->missq_access
                            + missq_ram.dyn_write * st->missq_access
                            + missq_ram.dyn_read  * (st->dcache_refill_access+st->icache_refill_access)
                            + wtbkq_alloc.dynamic * st->wtbkq_access
                            + wtbkq_ram.dyn_write * st->wtbkq_access
                            + wtbkq_ram.dyn_read  * st->replace_access;
}

void calculate_regfile_power(struct godson2_cpu *st,struct power_stat *regfile_power)
{
  static struct power_stat fix_regfile_pipein, fp_regfile_pipein, fcr_reg;
  static struct power_stat_ram fix_regfile,fp_regfile;

  if(initial_power) {
    //fix regfile pipeline in 
    power_reg_scan_en(&fix_regfile_pipein, fix_issue_width, gated_clock, fix_regfile_pipein_bits, X4);
    //fp regfile pipeline in 
    power_reg_scan_en(&fp_regfile_pipein, fp_issue_width, gated_clock, fp_regfile_pipein_bits, X4);
    //physical register file ram part
    if(godson2_ram) { 
      fix_regfile.inst = 2;
      fix_regfile.reg  = 2;
      fix_regfile.timing = 0.0;
      fix_regfile.area = 246682.0;
      fix_regfile.leakage = 0.029;
      fix_regfile.clock = 0.0;
      fix_regfile.dyn_write = 0.24;
      fix_regfile.dyn_read  = 0.20;
      fp_regfile.inst = 2;
      fp_regfile.reg  = 2;
      fp_regfile.timing = 0.0;
      fp_regfile.area = 246682.0;                   
      fp_regfile.leakage = 0.029;
      fp_regfile.clock = 0.0;
      fp_regfile.dyn_write = 0.24;
      fp_regfile.dyn_read  = 0.20;}
    else { 
      fix_regfile.inst = 1;
      fix_regfile.reg  = 1;
      fix_regfile.timing = regfile_power_cacti.result.access_time;
      fix_regfile.area = regfile_power_cacti.area.totalarea;
      fix_regfile.leakage =  regfile_power_cacti.result.total_power.readOp.leakage + regfile_power_cacti.result.total_power.writeOp.leakage;
      fix_regfile.clock = 0.0;
      fix_regfile.dyn_write = regfile_power_cacti.result.total_power.writeOp.dynamic * 0.001;
      fix_regfile.dyn_read  = regfile_power_cacti.result.total_power.readOp.dynamic  * 0.001;
      fp_regfile.inst = 1;
      fp_regfile.reg  = 1;
      fp_regfile.timing = 0.0;
      fp_regfile.area = regfile_power_cacti.area.totalarea;                   
      fp_regfile.leakage = regfile_power_cacti.result.total_power.readOp.leakage + regfile_power_cacti.result.total_power.writeOp.leakage;
      fp_regfile.clock = 0.0;
      fp_regfile.dyn_write = regfile_power_cacti.result.total_power.writeOp.dynamic * 0.001;
      fp_regfile.dyn_read  = regfile_power_cacti.result.total_power.readOp.dynamic  * 0.001;}
    //fp fcr_assign 
    power_reg_scan_en(&fcr_reg, fp_fcr_reg_bits, gated_clock, 1, X4);
    //sum up
    regfile_power->inst = fix_regfile_pipein.inst + fix_regfile.inst + fp_regfile_pipein.inst + fp_regfile.inst + fcr_reg.inst;
    regfile_power->reg  = fix_regfile_pipein.reg + fix_regfile.reg + fp_regfile_pipein.reg + fp_regfile.reg + fcr_reg.reg;
    regfile_power->area = fix_regfile_pipein.area + fix_regfile.area + fp_regfile_pipein.area + fp_regfile.area + fcr_reg.area;
    regfile_power->clock = fix_regfile_pipein.clock + fix_regfile.clock + fp_regfile_pipein.clock + fp_regfile.clock + fcr_reg.clock;
    regfile_power->leakage = fix_regfile_pipein.leakage + fix_regfile.leakage + fp_regfile_pipein.leakage + fp_regfile.leakage + fcr_reg.leakage;
    regfile_power->dynamic = regfile_power->total = 0.0;
  }  

  if(gated_clock) {
    regfile_power->clock = fix_regfile_pipein.clock * st->fix_issue_access
                         + fp_regfile_pipein.clock *  st->fp_issue_access
                         + fcr_reg.clock * st->fp_commit_access;
  }

  regfile_power->dynamic = fix_regfile_pipein.dynamic * st->fix_issue_access
                         + fix_regfile.dyn_write * (st->fix_wb_access / 4.0)
                         + fix_regfile.dyn_read * (st->fix_issue_access / 4.0) 
                         + fp_regfile_pipein.dynamic * st->fp_issue_access
                         + fp_regfile.dyn_write * (st->fp_wb_access / 4.0)
                         + fp_regfile.dyn_read * (st->fp_issue_access / 4.0) 
                         + fcr_reg.dynamic * st->fp_commit_access; 
}

void calculate_icache_power(struct godson2_cpu *st,struct power_stat *icache_power)
{
  static struct power_stat  icache_pipein;
  static struct power_stat_ram icache_ram;

  if(initial_power) {
    //pipeline in 
    power_reg_scan_en(&icache_pipein, 2, gated_clock, icache_pipein_bits, X4);
    //icache power
    if(godson2_ram) { 
      icache_ram.inst = 20;
      icache_ram.reg  = 20;
      icache_ram.timing = 0.0;
      icache_ram.area = 1116902.40625;
      icache_ram.leakage = 0.019684072;
      icache_ram.clock = 0.0;  
      icache_ram.dyn_write = 0.64;
      icache_ram.dyn_read  = 0.52;}
    else { 
      icache_ram.inst = 1;
      icache_ram.reg  = 1;
      icache_ram.timing = icache_power_cacti.result.access_time;
      icache_ram.area = icache_power_cacti.area.totalarea;
      icache_ram.leakage = icache_power_cacti.result.total_power.readOp.leakage + icache_power_cacti.result.total_power.writeOp.leakage;
      icache_ram.clock = 0.0;
      icache_ram.dyn_write = icache_power_cacti.result.total_power.writeOp.dynamic * 0.001;
      icache_ram.dyn_read  = icache_power_cacti.result.total_power.readOp.dynamic  * 0.001; }
    //sum up
    icache_power->inst = icache_pipein.inst + icache_ram.inst;
    icache_power->reg  = icache_pipein.reg + icache_ram.reg;
    icache_power->area = icache_pipein.area + icache_ram.area;
    icache_power->clock = icache_pipein.clock + icache_ram.clock;
    icache_power->leakage = icache_pipein.leakage + icache_ram.leakage;
    icache_power->dynamic = icache_power->total = 0.0;
  }

  if(gated_clock) {
    icache_power->clock = icache_pipein.clock * (st->icache_fetch_access+st->icache_refill_access)
                        + icache_ram.clock;
  }
  
  icache_power->dynamic = icache_pipein.dynamic * (st->icache_fetch_access+st->icache_refill_access)
                        + icache_ram.dyn_write * st->icache_refill_access
                        + icache_ram.dyn_read  * st->icache_fetch_access;
}

//itlb_pipein_bits = 95;
void calculate_itlb_power(struct godson2_cpu *st,struct power_stat *itlb_power)
{
  static struct power_stat itlb_pipein,itlb_cam_cmp;
  static struct power_stat_ram itlb_cam_reg,itlb_ram;

  if(initial_power) {
    //pipeline in 
    power_reg_scan_en(&itlb_pipein, 1, gated_clock, itlb_pipein_bits, X4);
    //itlb cam power
    power_ram_ff(&itlb_cam_reg, gated_clock, 1, 0, 40, st->itlb->nsets*st->itlb->assoc);
    //power_reg_scan_en(&itlb_cam_reg, st->itlb->nsets*st->itlb->assoc, gated_clock, 40, X4);
    power_cmp(&itlb_cam_cmp, st->itlb->nsets*st->itlb->assoc, 38);
    //itlb ram power
    if(godson2_ram) { 
      power_ram_ff(&itlb_ram, gated_clock, 1, 1, 28, st->itlb->nsets*st->itlb->assoc);}
    else {
      itlb_ram.inst = 1;
      itlb_ram.reg  = 1;
      itlb_ram.timing = itlb_ram_power_cacti.result.access_time;
      itlb_ram.area = itlb_ram_power_cacti.area.totalarea;	
      itlb_ram.clock = 0;
      itlb_ram.leakage = itlb_ram_power_cacti.result.total_power.readOp.leakage + itlb_ram_power_cacti.result.total_power.writeOp.leakage;
      itlb_ram.dyn_write = itlb_ram_power_cacti.result.total_power.writeOp.dynamic * 0.001;
      itlb_ram.dyn_read  = itlb_ram_power_cacti.result.total_power.readOp.dynamic * 0.001;}
    //sum up
    itlb_power->inst = itlb_pipein.inst + itlb_cam_reg.inst + itlb_cam_cmp.inst + itlb_ram.inst;
    itlb_power->reg  = itlb_pipein.reg + itlb_cam_reg.reg + itlb_cam_cmp.reg + itlb_ram.reg;
    itlb_power->area = itlb_pipein.area + itlb_cam_reg.area + itlb_cam_cmp.area + itlb_ram.area;
    itlb_power->clock = itlb_pipein.clock + itlb_cam_reg.clock + itlb_cam_cmp.clock + itlb_ram.clock;
    itlb_power->leakage = itlb_pipein.leakage + itlb_cam_reg.leakage + itlb_cam_cmp.leakage + itlb_ram.leakage;
    itlb_power->dynamic = itlb_power->total = 0.0;
  }

  if(gated_clock) {
    itlb_power->clock = itlb_pipein.clock * st->icache_fetch_access
                      + itlb_cam_reg.clock * 0.1;
  }

  itlb_power->dynamic = itlb_pipein.dynamic * st->icache_fetch_access
                      + itlb_cam_reg.dyn_write * 0.1  //tlb miss(write) occurs occasionally  
                      + itlb_cam_cmp.dynamic * st->icache_fetch_access*(st->itlb->nsets*st->itlb->assoc)
                      + itlb_ram.dyn_write * 0.1
                      + itlb_ram.dyn_read  * st->icache_fetch_access;
}

void calculate_dcache_power(struct godson2_cpu *st,struct power_stat *dcache_power)
{
  static struct power_stat dcache_pipein;
  static struct power_stat_ram dcache_ram,dcache_w_ram;

  if(initial_power) {
    //pipeline in 
    power_reg_scan_en(&dcache_pipein, 3, gated_clock, dcache_pipein_bits, X4);
    power_ram_ff(&dcache_w_ram, gated_clock, 1, 1, cpus[0].dcache->assoc, cpus[0].dcache->nsets); 
	//dcache power
    if(godson2_ram) { 
      dcache_ram.inst = 20;
      dcache_ram.reg  = 20;
      dcache_ram.timing = 0.0;
      dcache_ram.area = 1116902.40625;
      dcache_ram.leakage = 0.019684072;
      dcache_ram.clock = 0.0;
      dcache_ram.dyn_write = 0.93;
      dcache_ram.dyn_read  = 0.83;}
    else { 
      dcache_ram.inst = 1;
      dcache_ram.reg  = 1;
      dcache_ram.timing = dcache_power_cacti.result.access_time;
      dcache_ram.area = dcache_power_cacti.area.totalarea;
      dcache_ram.leakage = dcache_power_cacti.result.total_power.readOp.leakage + dcache_power_cacti.result.total_power.writeOp.leakage;
      dcache_ram.clock = 0.0;
      dcache_ram.dyn_write = dcache_power_cacti.result.total_power.writeOp.dynamic * 0.001;
      dcache_ram.dyn_read  = dcache_power_cacti.result.total_power.readOp.dynamic  * 0.001; }
    //sum up
	dcache_power->inst = dcache_pipein.inst + dcache_ram.inst + dcache_w_ram.inst;
    dcache_power->reg  = dcache_pipein.reg + dcache_ram.reg + dcache_w_ram.reg;
    dcache_power->area = dcache_pipein.area + dcache_ram.area + dcache_w_ram.area;
    dcache_power->clock = dcache_pipein.clock + dcache_ram.clock + dcache_w_ram.clock;
    dcache_power->leakage = dcache_pipein.leakage + dcache_ram.leakage + dcache_w_ram.leakage;
    dcache_power->dynamic = dcache_power->total = 0.0;
  }

  if(gated_clock) {
    dcache_power->clock = dcache_pipein.clock * (st->lsq_wb_access+st->lsq_store_commit_access+st->dcache_refill_access)
                        + (dcache_ram.clock + dcache_w_ram.clock) * (st->lsq_store_commit_access || st->dcache_refill_access);
  }
  
  dcache_power->dynamic = dcache_pipein.dynamic * (st->lsq_wb_access+st->lsq_store_commit_access+st->dcache_refill_access)
                        + (dcache_ram.dyn_write + dcache_w_ram.dyn_write) * (st->lsq_store_commit_access || st->dcache_refill_access)
                        + (dcache_ram.dyn_read + dcache_w_ram.dyn_read)  * (st->lsq_wb_access || st->dcache_refill_access);
}

void calculate_dtlb_power(struct godson2_cpu *st,struct power_stat *dtlb_power)
{
  static struct power_stat dtlb_pipein,dtlb_cr_reg,dtlb_cam_cmp;
  static struct power_stat_ram dtlb_cam_reg,dtlb_ram;

  if(initial_power) {
    //pipeline in 
    power_reg_scan_en(&dtlb_pipein, 1, gated_clock, dtlb_pipein_bits, X4);
    //cr regisers 
    power_reg_scan_en(&dtlb_cr_reg, 1, gated_clock, 894, X4);
    //dtlb cam power
    power_ram_ff(&dtlb_cam_reg, gated_clock, 1, 0, 38, st->dtlb->nsets*st->dtlb->assoc);
    //power_reg_scan_en(&dtlb_cam_reg, st->dtlb->nsets*st->dtlb->assoc, gated_clock, 38, X4);
    power_cmp(&dtlb_cam_cmp, st->dtlb->nsets*st->dtlb->assoc, 37);
    //dtlb ram power
    if(godson2_ram) { 
      dtlb_ram.inst = 4;
      dtlb_ram.reg  = 4;
      dtlb_ram.timing = 0.0;
      dtlb_ram.area = 58988.08 * 4;	
      dtlb_ram.clock = 0;
      dtlb_ram.leakage = 0.011 * 4;
      dtlb_ram.dyn_write = 0.06 * 4;
      dtlb_ram.dyn_read  = 0.052 * 4;}
    else {
      dtlb_ram.inst = 1;
      dtlb_ram.reg  = 1;
      dtlb_ram.timing = dtlb_ram_power_cacti.result.access_time;
      dtlb_ram.area = dtlb_ram_power_cacti.area.totalarea;	
      dtlb_ram.clock = 0;
      dtlb_ram.leakage =  dtlb_ram_power_cacti.result.total_power.readOp.leakage + dtlb_ram_power_cacti.result.total_power.writeOp.leakage;
      dtlb_ram.dyn_write = dtlb_ram_power_cacti.result.total_power.writeOp.dynamic * 0.001;
      dtlb_ram.dyn_read  = dtlb_ram_power_cacti.result.total_power.readOp.dynamic * 0.001;}
    //sum up
    dtlb_power->inst = dtlb_pipein.inst + dtlb_cr_reg.inst + dtlb_cam_reg.inst + dtlb_cam_cmp.inst + dtlb_ram.inst;
    dtlb_power->reg  = dtlb_pipein.reg + dtlb_cr_reg.reg + dtlb_cam_reg.reg + dtlb_cam_cmp.reg + dtlb_ram.reg;
    dtlb_power->area = dtlb_pipein.area + dtlb_cr_reg.area + dtlb_cam_reg.area + dtlb_cam_cmp.area + dtlb_ram.area;
    dtlb_power->clock = dtlb_pipein.clock + dtlb_cr_reg.clock + dtlb_cam_reg.clock + dtlb_cam_cmp.clock + dtlb_ram.clock;
    dtlb_power->leakage = dtlb_pipein.leakage + dtlb_cr_reg.leakage + dtlb_cam_reg.leakage + dtlb_cam_cmp.leakage + dtlb_ram.leakage;
    dtlb_power->dynamic = dtlb_power->total = 0.0;
  }

  if(gated_clock) {
    dtlb_power->clock = dtlb_pipein.clock * st->lsq_wb_access
                      + dtlb_cr_reg.clock * st->lsq_wb_access
                      + dtlb_cam_reg.clock * 0.1;
  }

  dtlb_power->dynamic = dtlb_pipein.dynamic * st->lsq_wb_access * 0.05
                      + dtlb_cr_reg.dynamic * st->lsq_wb_access * 0.05
                      + dtlb_cam_reg.dyn_write * 0.02 //tlb miss(write) occurs occasionally  
                      + dtlb_cam_cmp.dynamic * st->lsq_wb_access*(st->dtlb->nsets*st->dtlb->assoc)
                      + dtlb_ram.dyn_write * 0.02
                      + dtlb_ram.dyn_read  * st->lsq_wb_access;
}

void calculate_cache_dl2_power(struct godson2_cpu *st,struct power_stat *cache_dl2_power)
{
  static struct power_stat cache_dl2_pipein,cache_dl2_sramin,cache_dl2_sramout;
  static struct power_stat_ram cache_dl2_ram;
  
  if(initial_power) {
    //pipeline in 
    power_reg_scan_en(&cache_dl2_pipein, 3, gated_clock, cache_dl2_pipein_bits, X4);
    power_reg_scan_en(&cache_dl2_sramin, 1, gated_clock, cache_dl2_sramin_bits, X4);
    power_reg_scan_en(&cache_dl2_sramout, 1, gated_clock, cache_dl2_sramout_bits, X4);
    //cache2 power
    if(godson2_ram) { 
      cache_dl2_ram.inst = 80;
      cache_dl2_ram.reg  = 80;
      cache_dl2_ram.timing = 0.0;
      cache_dl2_ram.area = 7078145.62496;
      cache_dl2_ram.leakage = 0.203;
      cache_dl2_ram.clock = 0.1672;
      cache_dl2_ram.dyn_write = 0.264;
      cache_dl2_ram.dyn_read  = 0.240;}
    else { 
      cache_dl2_ram.inst = 1;
      cache_dl2_ram.reg  = 1;
      cache_dl2_ram.timing = cache_dl2_power_cacti.result.access_time;
      cache_dl2_ram.area = cache_dl2_power_cacti.area.totalarea;
      cache_dl2_ram.leakage =  cache_dl2_power_cacti.result.total_power.readOp.leakage + cache_dl2_power_cacti.result.total_power.writeOp.leakage;
      cache_dl2_ram.clock = 0.0;
      cache_dl2_ram.dyn_write = cache_dl2_power_cacti.result.total_power.writeOp.dynamic * 0.001;
      cache_dl2_ram.dyn_read  = cache_dl2_power_cacti.result.total_power.readOp.dynamic  * 0.001;}
    //sum up
    cache_dl2_power->dynamic = cache_dl2_power->total = 0.0;
    cache_dl2_power->inst = cache_dl2_pipein.inst + cache_dl2_sramin.inst + cache_dl2_sramout.inst + cache_dl2_ram.inst;
    cache_dl2_power->reg  = cache_dl2_pipein.reg + cache_dl2_sramin.reg + cache_dl2_sramout.reg + cache_dl2_ram.reg;
    cache_dl2_power->area = cache_dl2_pipein.area + cache_dl2_sramin.area + cache_dl2_sramout.area +  cache_dl2_ram.area;
    cache_dl2_power->clock = cache_dl2_pipein.clock + cache_dl2_sramin.clock + cache_dl2_sramout.clock + cache_dl2_ram.clock;
    cache_dl2_power->leakage = cache_dl2_pipein.leakage + cache_dl2_sramin.leakage + cache_dl2_sramout.leakage + cache_dl2_ram.leakage;
  }
  
  if(gated_clock) {
    cache_dl2_power->clock = cache_dl2_pipein.clock * (st->cache_dl2_fetch_access+st->cache_dl2_wtbk_access+st->cache_dl2_refill_access)
                           + cache_dl2_sramin.clock * st->cache_dl2_refill_access
						   + cache_dl2_sramout.clock * st->cache_dl2_fetch_access
						   + cache_dl2_ram.clock;
  }
  
  cache_dl2_power->dynamic = cache_dl2_pipein.dynamic * (st->cache_dl2_fetch_access+st->cache_dl2_refill_access)
                           + (cache_dl2_sramin.dynamic + cache_dl2_ram.dyn_write) * (st->cache_dl2_refill_access / 1.0)
                           + (cache_dl2_ram.dyn_read + cache_dl2_sramout.dynamic) * (st->cache_dl2_fetch_access / 1.0);
}

void calculate_input_buffer_power(struct godson2_cpu *st,struct power_stat *input_buffer_power,int type)
{
  static struct power_stat_ram inputb_per_type[NUM_DIRECTIONS];
  int direction;
  
  if(initial_power) {
    input_buffer_power->inst = input_buffer_power->reg = input_buffer_power->area = input_buffer_power->clock = 0.0;
    input_buffer_power->leakage = input_buffer_power->dynamic =  input_buffer_power->total = 0.0;
    for(direction=0 ; direction<NUM_DIRECTIONS ; direction++){
      power_ram_ff(&inputb_per_type[direction],gated_clock,1,1,input_buffer_bits[type],router_ifq_size);
      input_buffer_power->inst += inputb_per_type[direction].inst;
      input_buffer_power->reg  += inputb_per_type[direction].reg;
      input_buffer_power->area += inputb_per_type[direction].area;
      input_buffer_power->leakage += inputb_per_type[direction].leakage;
      input_buffer_power->clock += inputb_per_type[direction].clock;}
  }

  input_buffer_power->dynamic = 0.0;
  if(gated_clock) input_buffer_power->clock = 0.0;
  for(direction=0 ; direction<NUM_DIRECTIONS ; direction++){
    if(gated_clock) {
      input_buffer_power->clock += (inputb_per_type[direction].clock * st->router->input_buffer_write_access[type][direction]);
    } 
    input_buffer_power->dynamic = input_buffer_power->dynamic
                                + inputb_per_type[direction].dyn_write * st->router->input_buffer_write_access[type][direction]
                                + inputb_per_type[direction].dyn_read  * st->router->input_buffer_read_access[type][direction];
  }
}

void calculate_arbiter_power(struct godson2_cpu *st,struct power_stat *arbiter_power,int type)
{
  static struct power_stat arbiter; 	
  if(initial_power) {
    power_arbiter(&arbiter,1,gated_clock,router_ifq_size);
    arbiter_power->inst = arbiter.inst;
    arbiter_power->reg  = arbiter.reg;
    arbiter_power->area = arbiter.area;
    arbiter_power->clock = arbiter.clock;
    arbiter_power->leakage = arbiter.leakage;    
    arbiter_power->dynamic = arbiter_power->total = 0.0;
  }
  
  if(gated_clock) arbiter_power->clock = arbiter.clock * (st->router->crossbar_access[type]>=1);
  arbiter_power->dynamic = arbiter.dynamic * st->router->crossbar_access[type];
}

void calculate_crossbar_power(struct godson2_cpu *st,struct power_stat *crossbar_power,int type)
{
  static struct power_stat crossbar;
  if(initial_power) {
    ower_crossbar(&crossbar,1,input_buffer_bits[type]);
    crossbar_power->inst = crossbar.inst;
    crossbar_power->reg  = crossbar.reg;
    crossbar_power->area = crossbar.area;
    crossbar_power->clock = crossbar.clock;
    crossbar_power->leakage = crossbar.leakage;
    crossbar_power->dynamic = crossbar_power->total = 0.0;
  }
  crossbar_power->clock = 0.0;
  crossbar_power->dynamic = crossbar.dynamic * st->router->crossbar_access[type];
}

void calculate_link_power(struct godson2_cpu *st,struct power_stat *link_power,int type)
{
  static struct timing_t timing;
  static struct power_t power;
  if(initial_power) {
    intcnt_opt(3000, &timing, &power);
    link_power->inst = 0;
    link_power->reg  = 0;
    link_power->area = 0;
    link_power->timing = timing.td;
    link_power->leakage = power.pl * 0.001 * input_buffer_bits[type] * NUM_DIRECTIONS;
    link_power->dynamic = link_power->clock = 0.0;
  }
  link_power->clock = 0.0;
  link_power->dynamic = (power.ed + power.es) * 0.001 * input_buffer_bits[type] * 0.5 * st->router->crossbar_access[type];
}  

void calculate_clocktree_power(struct godson2_cpu *st,struct power_stat *clocktree_power)
{
  static int cpu_reg_access;
  if(initial_power) {
    int clock_load = st->cpu_power->reg * reg_scan_en_clkcap[X4]
                   + st->cpu_power->area * 0.02 * cap_per_unit;
    clocktree_power->inst = clock_load/(clkbuf_max_load - clkbuf_cap);
    clocktree_power->reg = 0;
    clocktree_power->area = clocktree_power->inst * power_table_clkbuf.area;
    clocktree_power->leakage = clocktree_power->inst * power_table_clkbuf.leakage;
    clocktree_power->clock = clocktree_power->inst * power_table_clkbuf.clock;
    clocktree_power->total = 0.0;
  }
  clocktree_power->dynamic = 0.0;
  if(gated_clock) {
    cpu_reg_access = decode_pipein_bits * st->decode_access
        /*rename*/ + map_pipein_bits * st->decode_access
                   + fix_state_bits * (st->fix_wb_access+st->fix_commit_access)
                   + fix_phy_num * st->fix_br_access
                   + fix_phy_lognum * st->fix_map_access
                   + fp_state_bits * (st->fp_wb_access+st->fp_commit_access)
                   + fp_phy_num * st->fp_br_access
                   + fp_phy_lognum * st->fp_map_access
                   + map_pipeout_bits * (st->fix_map_access+st->fp_map_access)
        /*issue*/  + fixq_misc_bits * st->fix_map_access
                   + (fix_phy_lognum*2+8) * st->fix_map_access
                   + fpq_misc_bits * st->fp_map_access
                   + (fp_phy_lognum*2+8) * st->fp_map_access
         /*bpred*/ + va_size * 0.5
                   + 2*4 * st->bht_wb_access
                   + va_size * st->btb_wb_access
           /*alu*/ + ialu_pipein_bits * st->fix_issue_access
                   + ialu_num * power_table_ialu.reg * (st->fix_add_access+st->fix_br_access+st->fix_mem_access)
                   + power_table_imul.reg * st->fix_mult_access
                   + power_table_idiv.reg * st->fix_div_access
                   + ialu_pipeout_bits * st->fix_wb_access
                   + falu_pipein_bits * st->fp_issue_access
                   + falu_num * power_table_fadd.reg * st->fp_add_access
                   + power_table_fmisc.reg * (st->fp_br_access+st->fp_cmp_access)
                   + power_table_fcvt.reg * st->fp_cvt_access
                   + power_table_fmul.reg * st->fp_mult_access
                   + power_table_fdivsqrt.reg * st->fp_div_access
                   + power_table_mdmx.reg * st->fp_mdmx_access
                   + falu_pipeout_bits * st->fp_wb_access
           /*roq*/ + roq_state_bits * (st->fix_wb_access+st->fp_wb_access+st->fix_map_access+st->fp_map_access+st->commit_access)
                   + roq_bits * st->fix_map_access+st->fp_map_access
                   + brq_state_bits * (st->fix_wb_access+st->fp_wb_access+st->fix_map_access+st->fp_map_access+st->commit_access)
                   + brq_bits * (st->fix_map_access+st->fp_map_access)
           /*lsq*/ + lsq_state_bits * (st->lsq_wb_access+st->lsq_map_access+st->dcache_refill_access)
                   + lsq_bits * (st->lsq_wb_access+st->lsq_map_access+st->dcache_refill_access)
     /*cache2mem*/ + wtbkq_bits * st->wtbkq_access
                   + missq_bits * st->missq_access
       /*regfile*/ + fix_regfile_pipein_bits * st->fix_issue_access
                   + fp_regfile_pipein_bits * st->fp_issue_access
        /*icache*/ + icache_pipein_bits * (st->icache_fetch_access+st->icache_refill_access)
          /*itlb*/ + itlb_pipein_bits * st->icache_fetch_access  
        /*dcache*/ + dcache_pipein_bits * (st->lsq_wb_access+st->lsq_store_commit_access+st->dcache_refill_access)
          /*dtlb*/ + dtlb_pipein_bits * st->lsq_wb_access
                   + 894 * st->lsq_wb_access
     /*cache_dl2*/ + cache_dl2_pipein_bits * (st->cache_dl2_fetch_access+st->cache_dl2_wtbk_access+st->cache_dl2_refill_access)
  /*input_buffer*/ + input_buffer_bits[REQ] * st->router->input_buffer_write_access[REQ][HOME]
                   + input_buffer_bits[REQ] * st->router->input_buffer_write_access[REQ][UP]
                   + input_buffer_bits[REQ] * st->router->input_buffer_write_access[REQ][DOWN]
                   + input_buffer_bits[REQ] * st->router->input_buffer_write_access[REQ][LEFT]
                   + input_buffer_bits[REQ] * st->router->input_buffer_write_access[REQ][RIGHT]
                   + input_buffer_bits[INVN] * st->router->input_buffer_write_access[INVN][HOME]
                   + input_buffer_bits[INVN] * st->router->input_buffer_write_access[INVN][UP]
                   + input_buffer_bits[INVN] * st->router->input_buffer_write_access[INVN][DOWN]
                   + input_buffer_bits[INVN] * st->router->input_buffer_write_access[INVN][LEFT]
                   + input_buffer_bits[INVN] * st->router->input_buffer_write_access[INVN][RIGHT]
                   + input_buffer_bits[WTBK] * st->router->input_buffer_write_access[WTBK][HOME]
                   + input_buffer_bits[WTBK] * st->router->input_buffer_write_access[WTBK][UP]
                   + input_buffer_bits[WTBK] * st->router->input_buffer_write_access[WTBK][DOWN]
                   + input_buffer_bits[WTBK] * st->router->input_buffer_write_access[WTBK][LEFT]
                   + input_buffer_bits[WTBK] * st->router->input_buffer_write_access[WTBK][RIGHT]
                   + input_buffer_bits[RESP] * st->router->input_buffer_write_access[RESP][HOME]
                   + input_buffer_bits[RESP] * st->router->input_buffer_write_access[RESP][UP]
                   + input_buffer_bits[RESP] * st->router->input_buffer_write_access[RESP][DOWN]
                   + input_buffer_bits[RESP] * st->router->input_buffer_write_access[RESP][LEFT]
                   + input_buffer_bits[RESP] * st->router->input_buffer_write_access[RESP][RIGHT]
       /*arbiter*/ + power_table_arbiter[router_ifq_size].reg * (st->router->crossbar_access[HOME]>=1)
                   + power_table_arbiter[router_ifq_size].reg * (st->router->crossbar_access[UP]>=1)
                   + power_table_arbiter[router_ifq_size].reg * (st->router->crossbar_access[DOWN]>=1)
                   + power_table_arbiter[router_ifq_size].reg * (st->router->crossbar_access[LEFT]>=1)
                   + power_table_arbiter[router_ifq_size].reg * (st->router->crossbar_access[RIGHT]>=1);

    clocktree_power->clock = st->clocktree_power->inst * power_table_clkbuf.clock * ((double)cpu_reg_access/st->cpu_power->reg);
  }
}

void calculate_IOpad_power(struct godson2_cpu *st,struct power_stat *IOpad_power)
{
 static struct power_stat io_power; 
 if(initial_power) {
  power_IOpad(&io_power,io_bits);
  IOpad_power->inst = io_power.inst; 
  IOpad_power->reg  = io_power.reg; 
  IOpad_power->area = io_power.area; 
  IOpad_power->clock = io_power.clock; 
  IOpad_power->leakage = io_power.leakage; 
  IOpad_power->dynamic = IOpad_power->total = 0.0; 
 }
  IOpad_power->clock = 0.0;
  IOpad_power->dynamic = io_power.dynamic * st->memq_access;
}

void clear_access_stats(struct godson2_cpu *st)
{
  st->avg_bht_fetch_access        += st->bht_fetch_access ;
  st->avg_bht_wb_access           += st->bht_wb_access ;
  st->avg_btb_fetch_access        += st->btb_fetch_access ;
  st->avg_btb_wb_access           += st->btb_wb_access ;
  st->avg_decode_access           += st->decode_access ;
  st->avg_fix_map_access          += st->fix_map_access ;
  st->avg_fp_map_access           += st->fp_map_access ;
  st->avg_fix_wb_access           += st->fix_wb_access ;
  st->avg_fp_wb_access            += st->fp_wb_access ;
  st->avg_fix_issue_access        += st->fix_issue_access ;
  st->avg_fp_issue_access         += st->fp_issue_access ;
  st->avg_fix_commit_access       += st->fix_commit_access ;
  st->avg_fp_commit_access        += st->fp_commit_access ;
  st->avg_fix_add_access          += st->fix_add_access ;
  st->avg_fix_br_access           += st->fix_br_access ;
  st->avg_fix_mem_access          += st->fix_mem_access ;
  st->avg_fix_mult_access         += st->fix_mult_access ;
  st->avg_fix_div_access          += st->fix_div_access ;
  st->avg_fp_add_access           += st->fp_add_access ;
  st->avg_fp_br_access            += st->fp_br_access ;
  st->avg_fp_cmp_access           += st->fp_cmp_access ;
  st->avg_fp_cvt_access           += st->fp_cvt_access ;
  st->avg_fp_mult_access          += st->fp_mult_access ;
  st->avg_fp_div_access           += st->fp_div_access ;
  st->avg_fp_mdmx_access          += st->fp_mdmx_access ;
  st->avg_commit_access           += st->commit_access ;
  st->avg_lsq_map_access          += st->lsq_map_access ;
  st->avg_lsq_wb_access           += st->lsq_wb_access ;
  st->avg_lsq_commit_access       += st->lsq_commit_access ;
  st->avg_lsq_store_commit_access += st->lsq_store_commit_access ;
  st->avg_icache_fetch_access     += st->icache_fetch_access ;
  st->avg_icache_refill_access    += st->icache_refill_access ;
  st->avg_dcache_refill_access    += st->dcache_refill_access ;
  st->avg_replace_access          += st->replace_access ;
  st->avg_missq_access            += st->missq_access ;
  st->avg_wtbkq_access            += st->wtbkq_access ;
  st->avg_cache_dl2_fetch_access  += st->cache_dl2_fetch_access ;
  st->avg_cache_dl2_refill_access += st->cache_dl2_refill_access ;
  st->avg_cache_dl2_wtbk_access   += st->cache_dl2_wtbk_access ;
  st->avg_memq_access             += st->memq_access ;


  st->bht_fetch_access = 0;
  st->bht_wb_access = 0;
  st->btb_fetch_access = 0;
  st->btb_wb_access = 0;
  st->decode_access = 0;
  
  st->fix_map_access = 0;
  st->fp_map_access = 0;
  st->fix_wb_access = 0;
  st->fp_wb_access = 0;
  st->fix_issue_access = 0;
  st->fp_issue_access = 0;
  st->fix_commit_access = 0;
  st->fp_commit_access = 0;
  
  st->fix_add_access = 0;
  st->fix_br_access = 0;
  st->fix_mem_access = 0;
  st->fix_mult_access = 0;
  st->fix_div_access = 0;
  st->fp_add_access = 0;
  st->fp_br_access = 0;
  st->fp_cmp_access = 0;
  st->fp_cvt_access = 0;
  st->fp_mult_access = 0;
  st->fp_div_access = 0;
  st->fp_mdmx_access = 0;
  
  st->commit_access = 0;
  st->lsq_map_access = 0;
  st->lsq_wb_access = 0;
  st->lsq_commit_access = 0;
  st->lsq_store_commit_access = 0;
  st->icache_fetch_access = 0;
  st->icache_refill_access = 0;
  st->dcache_refill_access = 0;
  st->replace_access = 0;
  st->missq_access = 0;
  st->wtbkq_access = 0;
  st->cache_dl2_fetch_access = 0;
  st->cache_dl2_refill_access = 0;
  st->cache_dl2_wtbk_access = 0;
  st->memq_access = 0;

  int i,j;
  for(i=0 ; i<NUM_TYPES ; i++){
	for(j=0 ; j<NUM_DIRECTIONS ; j++){
	  st->router->input_buffer_read_access[i][j] = 0;
	  st->router->input_buffer_write_access[i][j] = 0;
	}
	st->router->crossbar_access[i] = 0;
  }
	
}

/* compute power statistics on each cycle, for each conditional clocking style.  Obviously
most of the speed penalty comes here, so if you don't want per-cycle power estimates
you could post-process 

*/
void update_power_stats(struct godson2_cpu *st)
{
  struct power_stat rename_power,window_power,bpred_power,decode_power,ialu_power,falu_power,roq_power,lsq_power,cache2mem_power,regfile_power,icache_power,itlb_power,dcache_power,dtlb_power,cache_dl2_power,router_power,input_buffer_power[NUM_TYPES],arbiter_power[NUM_TYPES],crossbar_power[NUM_TYPES],link_power[NUM_TYPES],clocktree_power,IOpad_power;

  int i;

  calculate_bpred_power(st,&bpred_power);
  calculate_decode_power(st,&decode_power);
  calculate_rename_power(st,&rename_power);
  calculate_window_power(st,&window_power);
  calculate_regfile_power(st,&regfile_power);
  calculate_ialu_power(st,&ialu_power);
  calculate_falu_power(st,&falu_power);
  calculate_lsq_power(st,&lsq_power);
  calculate_roq_power(st,&roq_power);
  calculate_cache2mem_power(st,&cache2mem_power);
  calculate_icache_power(st,&icache_power);
  calculate_itlb_power(st,&itlb_power);
  calculate_dcache_power(st,&dcache_power);
  calculate_dtlb_power(st,&dtlb_power);
  calculate_cache_dl2_power(st,&cache_dl2_power);
  for(i=0 ; i<NUM_TYPES ; i++){
    calculate_input_buffer_power(st,&input_buffer_power[i],i);
    calculate_arbiter_power(st,&arbiter_power[i],i);
    calculate_crossbar_power(st,&crossbar_power[i],i);
    calculate_link_power(st,&link_power[i],i);
  }
  calculate_IOpad_power(st,&IOpad_power);

  calculate_clocktree_power(st,&clocktree_power);
 
  power_stat_accu(st->bpred_power,&bpred_power);
  power_stat_accu(st->decode_power,&decode_power);
  power_stat_accu(st->rename_power,&rename_power);
  power_stat_accu(st->window_power,&window_power);
  power_stat_accu(st->regfile_power,&regfile_power);
  power_stat_accu(st->ialu_power,&ialu_power);
  power_stat_accu(st->falu_power,&falu_power);
  power_stat_accu(st->lsq_power,&lsq_power);
  power_stat_accu(st->roq_power,&roq_power);
  power_stat_accu(st->cache2mem_power,&cache2mem_power);
  power_stat_accu(st->icache_power,&icache_power);
  power_stat_accu(st->itlb_power,&itlb_power);
  power_stat_accu(st->dcache_power,&dcache_power);
  power_stat_accu(st->dtlb_power,&dtlb_power);
  power_stat_accu(st->cache_dl2_power,&cache_dl2_power);
  for(i=0 ; i<NUM_TYPES ; i++){
    power_stat_accu(st->input_buffer_power[i],&input_buffer_power[i]);
    power_stat_accu(st->arbiter_power[i],&arbiter_power[i]);
    power_stat_accu(st->crossbar_power[i],&crossbar_power[i]);
    power_stat_accu(st->link_power[i],&link_power[i]);
  }
  for(i=0 ; i<NUM_TYPES ; i++){
	router_power.dynamic = input_buffer_power[i].dynamic + arbiter_power[i].dynamic + crossbar_power[i].dynamic + link_power[i].dynamic;
    if(gated_clock)	router_power.clock = input_buffer_power[i].clock + arbiter_power[i].clock + crossbar_power[i].clock + link_power[i].clock;
  }
  power_stat_accu(st->router_power,&router_power);
  power_stat_accu(st->clocktree_power,&clocktree_power);
  power_stat_accu(st->IOpad_power,&IOpad_power);
}

void power_init(struct godson2_cpu *st)
{
  st->cpu_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->rename_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->window_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->bpred_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->decode_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->regfile_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->ialu_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->falu_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->lsq_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->roq_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->cache2mem_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->icache_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->itlb_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->dcache_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->dtlb_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->cache_dl2_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->router_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  int i;
  for(i=0 ; i<NUM_TYPES ; i++){
	st->input_buffer_power[i] = (struct power_stat *)malloc(sizeof(struct power_stat));
	st->arbiter_power[i] = (struct power_stat *)malloc(sizeof(struct power_stat));
	st->crossbar_power[i] = (struct power_stat *)malloc(sizeof(struct power_stat));
	st->link_power[i] = (struct power_stat *)malloc(sizeof(struct power_stat));
  }

  st->clocktree_power = (struct power_stat *)malloc(sizeof(struct power_stat));
  st->IOpad_power = (struct power_stat *)malloc(sizeof(struct power_stat));
}

void initial_power_stats(struct godson2_cpu *st)
{
  initial_power = 1;
  int i;
  clear_access_stats(st);
  calculate_bpred_power(st,st->bpred_power);
  calculate_decode_power(st,st->decode_power);
  calculate_rename_power(st,st->rename_power);
  calculate_window_power(st,st->window_power);
  calculate_regfile_power(st,st->regfile_power);
  calculate_ialu_power(st,st->ialu_power);
  calculate_falu_power(st,st->falu_power);
  calculate_lsq_power(st,st->lsq_power);
  calculate_roq_power(st,st->roq_power);
  calculate_cache2mem_power(st,st->cache2mem_power);
  calculate_icache_power(st,st->icache_power);
  calculate_itlb_power(st,st->itlb_power);
  calculate_dcache_power(st,st->dcache_power);
  calculate_dtlb_power(st,st->dtlb_power);
  calculate_cache_dl2_power(st,st->cache_dl2_power);
  for(i=0 ; i<NUM_TYPES ; i++){
    calculate_input_buffer_power(st,st->input_buffer_power[i],i);
    calculate_arbiter_power(st,st->arbiter_power[i],i);
    calculate_crossbar_power(st,st->crossbar_power[i],i);
    calculate_link_power(st,st->link_power[i],i);
  }
  st->router_power->inst = st->router_power->reg = st->router_power->area = st->router_power->leakage = 0.0;
  st->router_power->clock = st->router_power->dynamic = st->router_power->total = 0.0;
  for(i=0 ; i<NUM_TYPES ; i++){
	st->router_power->inst += st->input_buffer_power[i]->inst + st->arbiter_power[i]->inst + st->crossbar_power[i]->inst + st->link_power[i]->inst;
	st->router_power->reg += st->input_buffer_power[i]->reg + st->arbiter_power[i]->reg + st->crossbar_power[i]->reg + st->link_power[i]->reg;
	st->router_power->area += st->input_buffer_power[i]->area + st->arbiter_power[i]->area + st->crossbar_power[i]->area + st->link_power[i]->area;
	st->router_power->leakage += st->input_buffer_power[i]->leakage + st->arbiter_power[i]->leakage + st->crossbar_power[i]->leakage + st->link_power[i]->leakage;
	st->router_power->clock += st->input_buffer_power[i]->clock + st->arbiter_power[i]->clock + st->crossbar_power[i]->clock + st->link_power[i]->clock;
  }

  calculate_IOpad_power(st,st->IOpad_power);
  
  st->cpu_power->reg = st->rename_power->reg + st->window_power->reg
                     + st->bpred_power->reg + st->decode_power->reg 
                     + st->regfile_power->reg + st->ialu_power->reg 
                     + st->falu_power->reg 
                     + st->lsq_power->reg + st->roq_power->reg 
                     + st->cache2mem_power->reg + st->icache_power->reg 
                     + st->itlb_power->reg + st->dcache_power->reg 
                     + st->dtlb_power->reg + st->cache_dl2_power->reg 
                     + st->router_power->reg + st->IOpad_power->reg;    

  st->cpu_power->area = st->rename_power->area + st->window_power->area
                      + st->bpred_power->area + st->decode_power->area 
                      + st->regfile_power->area + st->ialu_power->area 
                      + st->falu_power->area 
                      + st->lsq_power->area + st->roq_power->area 
                      + st->cache2mem_power->area + st->icache_power->area 
                      + st->itlb_power->area + st->dcache_power->area 
                      + st->dtlb_power->area + st->cache_dl2_power->area 
                      + st->router_power->area + st->clocktree_power->area
                      + st->IOpad_power->area;    

  calculate_clocktree_power(st,st->clocktree_power);
  
  initial_power = 0;

  st->cpu_power->inst = st->rename_power->inst + st->window_power->inst
                      + st->bpred_power->inst + st->decode_power->inst 
                      + st->regfile_power->inst + st->ialu_power->inst 
                      + st->falu_power->inst 
                      + st->lsq_power->inst + st->roq_power->inst 
                      + st->cache2mem_power->inst + st->icache_power->inst 
                      + st->itlb_power->inst + st->dcache_power->inst 
                      + st->dtlb_power->inst + st->cache_dl2_power->inst 
                      + st->router_power->inst + st->IOpad_power->inst
                      + st->clocktree_power->inst;    

  st->cpu_power->leakage = st->rename_power->leakage + st->window_power->leakage
                      + st->bpred_power->leakage + st->decode_power->leakage 
                      + st->regfile_power->leakage + st->ialu_power->leakage 
                      + st->falu_power->leakage 
                      + st->lsq_power->leakage + st->roq_power->leakage 
                      + st->cache2mem_power->leakage + st->icache_power->leakage 
                      + st->itlb_power->leakage + st->dcache_power->leakage 
                      + st->dtlb_power->leakage + st->cache_dl2_power->leakage 
                      + st->router_power->leakage + st->IOpad_power->leakage
                      + st->clocktree_power->leakage;    
}

void power_reg_stats(struct stat_sdb_t *sdb)	/* stats database */
{
  int i;
  for(i=0 ; i<total_cpus ; i++){
    stat_reg_counter(cpus[i].sdb, "sim_cycle", "total number of cycle passed", &sim_cycle, 0, NULL);
    
	stat_reg_counter(cpus[i].sdb, "avg_bht_fetch_access       ", "average access", &cpus[i].avg_bht_fetch_access       , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_bht_wb_access          ", "average access", &cpus[i].avg_bht_wb_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_btb_fetch_access       ", "average access", &cpus[i].avg_btb_fetch_access       , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_btb_wb_access          ", "average access", &cpus[i].avg_btb_wb_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_decode_access          ", "average access", &cpus[i].avg_decode_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_map_access         ", "average access", &cpus[i].avg_fix_map_access         , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_map_access          ", "average access", &cpus[i].avg_fp_map_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_wb_access          ", "average access", &cpus[i].avg_fix_wb_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_wb_access           ", "average access", &cpus[i].avg_fp_wb_access           , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_issue_access       ", "average access", &cpus[i].avg_fix_issue_access       , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_issue_access        ", "average access", &cpus[i].avg_fp_issue_access        , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_commit_access      ", "average access", &cpus[i].avg_fix_commit_access      , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_commit_access       ", "average access", &cpus[i].avg_fp_commit_access       , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_add_access         ", "average access", &cpus[i].avg_fix_add_access         , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_br_access          ", "average access", &cpus[i].avg_fix_br_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_mem_access         ", "average access", &cpus[i].avg_fix_mem_access         , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_mult_access        ", "average access", &cpus[i].avg_fix_mult_access        , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fix_div_access         ", "average access", &cpus[i].avg_fix_div_access         , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_add_access          ", "average access", &cpus[i].avg_fp_add_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_br_access           ", "average access", &cpus[i].avg_fp_br_access           , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_cmp_access          ", "average access", &cpus[i].avg_fp_cmp_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_cvt_access          ", "average access", &cpus[i].avg_fp_cvt_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_mult_access         ", "average access", &cpus[i].avg_fp_mult_access         , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_fp_div_access          ", "average access", &cpus[i].avg_fp_div_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_commit_access          ", "average access", &cpus[i].avg_commit_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_lsq_map_access         ", "average access", &cpus[i].avg_lsq_map_access         , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_lsq_wb_access          ", "average access", &cpus[i].avg_lsq_wb_access          , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_lsq_commit_access      ", "average access", &cpus[i].avg_lsq_commit_access      , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_lsq_store_commit_access", "average access", &cpus[i].avg_lsq_store_commit_access, 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_icache_fetch_access    ", "average access", &cpus[i].avg_icache_fetch_access    , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_icache_refill_access   ", "average access", &cpus[i].avg_icache_refill_access   , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_dcache_refill_access   ", "average access", &cpus[i].avg_dcache_refill_access   , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_replace_access         ", "average access", &cpus[i].avg_replace_access         , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_missq_access           ", "average access", &cpus[i].avg_missq_access           , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_wtbkq_access           ", "average access", &cpus[i].avg_wtbkq_access           , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_cache_dl2_fetch_access ", "average access", &cpus[i].avg_cache_dl2_fetch_access , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_cache_dl2_refill_access", "average access", &cpus[i].avg_cache_dl2_refill_access, 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_cache_dl2_wtbk_access      ", "average access", &cpus[i].avg_cache_dl2_wtbk_access      , 0, NULL);
	stat_reg_counter(cpus[i].sdb, "avg_memq_access                ", "average access", &cpus[i].avg_memq_access                , 0, NULL);

    stat_reg_int(cpus[i].sdb, "cpu_inst", "avg inst usage of the whole cpu", &cpus[i].cpu_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "bpred_inst", "avg inst usage of bpred unit", &cpus[i].bpred_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "decode_inst", "avg inst usage of decode", &cpus[i].decode_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "rename_inst", "avg inst usage of rename unit", &cpus[i].rename_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "window_inst", "avg inst usage of instruction window", &cpus[i].window_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "regfile_inst", "avg inst usage of arch. regfile", &cpus[i].regfile_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "ialu_inst", "avg inst usage of ialu", &cpus[i].ialu_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "falu_inst", "avg inst usage of falu", &cpus[i].falu_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "lsq_inst", "avg inst usage of load/store queue", &cpus[i].lsq_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "roq_inst", "avg inst usage of roq", &cpus[i].roq_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "icache_inst", "avg inst usage of icache", &cpus[i].icache_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "dcache_inst", "avg inst usage of dcache", &cpus[i].dcache_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "itlb_inst", "avg inst usage of itlb", &cpus[i].itlb_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "dtlb_inst", "avg inst usage of dtlb", &cpus[i].dtlb_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "cache_dl2_inst", "avg inst usage of cache_dl2", &cpus[i].cache_dl2_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "cache2mem_inst", "avg inst usage of cache2mem", &cpus[i].cache2mem_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "router_inst", "avg inst usage of router", &cpus[i].router_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "input_buffer_inst", "avg inst usage of router input buffer of type REQ", &cpus[i].input_buffer_power[REQ]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "arbiter_inst", "avg inst usage of router arbiter of type REQ", &cpus[i].arbiter_power[REQ]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "crossbar_inst", "avg inst usage of router crossbar of type REQ", &cpus[i].crossbar_power[REQ]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "link_inst", "avg inst usage of router link of type REQ", &cpus[i].link_power[REQ]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "input_buffer_inst", "avg inst usage of router input buffer of type INVN", &cpus[i].input_buffer_power[INVN]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "arbiter_inst", "avg inst usage of router arbiter of type INVN", &cpus[i].arbiter_power[INVN]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "crossbar_inst", "avg inst usage of router crossbar of type INVN", &cpus[i].crossbar_power[INVN]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "link_inst", "avg inst usage of router link of type INVN", &cpus[i].link_power[INVN]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "input_buffer_inst", "avg inst usage of router input buffer of type WTBK", &cpus[i].input_buffer_power[WTBK]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "arbiter_inst", "avg inst usage of router arbiter of type WTBK", &cpus[i].arbiter_power[WTBK]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "crossbar_inst", "avg inst usage of router crossbar of type WTBK", &cpus[i].crossbar_power[WTBK]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "link_inst", "avg inst usage of router link of type WTBK", &cpus[i].link_power[WTBK]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "input_buffer_inst", "avg inst usage of router input buffer of type RESP", &cpus[i].input_buffer_power[RESP]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "arbiter_inst", "avg inst usage of router arbiter of type RESP", &cpus[i].arbiter_power[RESP]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "crossbar_inst", "avg inst usage of router crossbar of type RESP", &cpus[i].crossbar_power[RESP]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "link_inst", "avg inst usage of router link of type RESP", &cpus[i].link_power[RESP]->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "IOpad_inst", "avg inst usage of IOpad", &cpus[i].IOpad_power->inst, 0, NULL);
    stat_reg_int(cpus[i].sdb, "clocktree_inst", "avg inst usage of clock", &cpus[i].clocktree_power->inst, 0, NULL);

    stat_reg_int(cpus[i].sdb, "cpu_reg", "avg reg usage of the whold cpu", &cpus[i].cpu_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "bpred_reg", "avg reg usage of bpred unit", &cpus[i].bpred_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "decode_reg", "avg reg usage of decode", &cpus[i].decode_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "rename_reg", "avg reg usage of rename unit", &cpus[i].rename_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "window_reg", "avg reg usage of regruction window", &cpus[i].window_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "regfile_reg", "avg reg usage of arch. regfile", &cpus[i].regfile_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "ialu_reg", "avg reg usage of ialu", &cpus[i].ialu_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "falu_reg", "avg reg usage of falu", &cpus[i].falu_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "lsq_reg", "avg reg usage of load/store queue", &cpus[i].lsq_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "roq_reg", "avg reg usage of roq", &cpus[i].roq_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "icache_reg", "avg reg usage of icache", &cpus[i].icache_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "dcache_reg", "avg reg usage of dcache", &cpus[i].dcache_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "itlb_reg", "avg reg usage of itlb", &cpus[i].itlb_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "dtlb_reg", "avg reg usage of dtlb", &cpus[i].dtlb_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "cache_dl2_reg", "avg reg usage of cache_dl2", &cpus[i].cache_dl2_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "cache2mem_reg", "avg reg usage of cache2mem", &cpus[i].cache2mem_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "router_reg", "avg reg usage of router", &cpus[i].router_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "input_buffer_reg", "avg reg usage of router input buffer of type REQ", &cpus[i].input_buffer_power[REQ]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "arbiter_reg", "avg reg usage of router arbiter of type REQ", &cpus[i].arbiter_power[REQ]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "crossbar_reg", "avg reg usage of router crossbar of type REQ", &cpus[i].crossbar_power[REQ]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "link_reg", "avg reg usage of router link of type REQ", &cpus[i].link_power[REQ]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "input_buffer_reg", "avg reg usage of router input buffer of type INVN", &cpus[i].input_buffer_power[INVN]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "arbiter_reg", "avg reg usage of router arbiter of type INVN", &cpus[i].arbiter_power[INVN]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "crossbar_reg", "avg reg usage of router crossbar of type INVN", &cpus[i].crossbar_power[INVN]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "link_reg", "avg reg usage of router link of type INVN", &cpus[i].link_power[INVN]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "input_buffer_reg", "avg reg usage of router input buffer of type WTBK", &cpus[i].input_buffer_power[WTBK]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "arbiter_reg", "avg reg usage of router arbiter of type WTBK", &cpus[i].arbiter_power[WTBK]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "crossbar_reg", "avg reg usage of router crossbar of type WTBK", &cpus[i].crossbar_power[WTBK]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "link_reg", "avg reg usage of router link of type WTBK", &cpus[i].link_power[WTBK]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "input_buffer_reg", "avg reg usage of router input buffer of type RESP", &cpus[i].input_buffer_power[RESP]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "arbiter_reg", "avg reg usage of router arbiter of type RESP", &cpus[i].arbiter_power[RESP]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "crossbar_reg", "avg reg usage of router crossbar of type RESP", &cpus[i].crossbar_power[RESP]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "link_reg", "avg reg usage of router link of type RESP", &cpus[i].link_power[RESP]->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "IOpad_reg", "avg reg usage of IOpad", &cpus[i].IOpad_power->reg, 0, NULL);
    stat_reg_int(cpus[i].sdb, "clocktree_reg", "avg reg usage of clock", &cpus[i].clocktree_power->reg, 0, NULL);

    stat_reg_double(cpus[i].sdb, "cpu_area", "avg area usage of the whold cpu", &cpus[i].cpu_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "bpred_area", "avg area usage of bpred unit", &cpus[i].bpred_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "decode_area", "avg area usage of decode", &cpus[i].decode_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "rename_area", "avg area usage of rename unit", &cpus[i].rename_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "window_area", "avg area usage of arearuction window", &cpus[i].window_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "regfile_area", "avg area usage of arch. areafile", &cpus[i].regfile_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "ialu_area", "avg area usage of ialu", &cpus[i].ialu_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "falu_area", "avg area usage of falu", &cpus[i].falu_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "lsq_area", "avg area usage of load/store queue", &cpus[i].lsq_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "roq_area", "avg area usage of roq", &cpus[i].roq_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "icache_area", "avg area usage of icache", &cpus[i].icache_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "dcache_area", "avg area usage of dcache", &cpus[i].dcache_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "itlb_area", "avg area usage of itlb", &cpus[i].itlb_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "dtlb_area", "avg area usage of dtlb", &cpus[i].dtlb_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "cache_dl2_area", "avg area usage of cache_dl2", &cpus[i].cache_dl2_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "cache2mem_area", "avg area usage of cache2mem", &cpus[i].cache2mem_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "router_area", "avg area usage of router", &cpus[i].router_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "input_buffer_area", "avg area usage of router input buffer of type REQ", &cpus[i].input_buffer_power[REQ]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "arbiter_area", "avg area usage of router arbiter of type REQ", &cpus[i].arbiter_power[REQ]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "crossbar_area", "avg area usage of router crossbar of type REQ", &cpus[i].crossbar_power[REQ]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "link_area", "avg area usage of router link of type REQ", &cpus[i].link_power[REQ]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "input_buffer_area", "avg area usage of router input buffer of type INVN", &cpus[i].input_buffer_power[INVN]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "arbiter_area", "avg area usage of router arbiter of type INVN", &cpus[i].arbiter_power[INVN]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "crossbar_area", "avg area usage of router crossbar of type INVN", &cpus[i].crossbar_power[INVN]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "link_area", "avg area usage of router link of type INVN", &cpus[i].link_power[INVN]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "input_buffer_area", "avg area usage of router input buffer of type WTBK", &cpus[i].input_buffer_power[WTBK]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "arbiter_area", "avg area usage of router arbiter of type WTBK", &cpus[i].arbiter_power[WTBK]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "crossbar_area", "avg area usage of router crossbar of type WTBK", &cpus[i].crossbar_power[WTBK]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "link_area", "avg area usage of router link of type WTBK", &cpus[i].link_power[WTBK]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "input_buffer_area", "avg area usage of router input buffer of type RESP", &cpus[i].input_buffer_power[RESP]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "arbiter_area", "avg area usage of router arbiter of type RESP", &cpus[i].arbiter_power[RESP]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "crossbar_area", "avg area usage of router crossbar of type RESP", &cpus[i].crossbar_power[RESP]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "link_area", "avg area usage of router link of type RESP", &cpus[i].link_power[RESP]->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "IOpad_area", "avg area usage of IOpad", &cpus[i].IOpad_power->area, 0, NULL);
    stat_reg_double(cpus[i].sdb, "clocktree_area", "avg area usage of clock", &cpus[i].clocktree_power->area, 0, NULL);

    stat_reg_double(cpus[i].sdb, "cpu_power_leakage", "avg leakage power usage of the whold cpu", &cpus[i].cpu_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "bpred_power_leakage", "avg leakage power usage of bpred unit", &cpus[i].bpred_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "decode_power_leakage", "avg leakage power usage of decode", &cpus[i].decode_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "rename_power_leakage", "avg leakage power usage of rename unit", &cpus[i].rename_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "window_power_leakage", "avg leakage power usage of instruction window", &cpus[i].window_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "regfile_power_leakage", "avg leakage power usage of arch. regfile", &cpus[i].regfile_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "ialu_power_leakage", "avg leakage power usage of ialu", &cpus[i].ialu_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "falu_power_leakage", "avg leakage power usage of falu", &cpus[i].falu_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "lsq_power_leakage", "avg leakage power usage of load/store queue", &cpus[i].lsq_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "roq_power_leakage", "avg leakage power usage of roq", &cpus[i].roq_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "icache_power_leakage", "avg leakage power usage of icache", &cpus[i].icache_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "dcache_power_leakage", "avg leakage power usage of dcache", &cpus[i].dcache_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "itlb_power_leakage", "avg leakage power usage of itlb", &cpus[i].itlb_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "dtlb_power_leakage", "avg leakage power usage of dtlb", &cpus[i].dtlb_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "cache_dl2_power_leakage", "avg leakage power usage of cache_dl2", &cpus[i].cache_dl2_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "cache2mem_power_leakage", "avg leakage power usage of cache2mem", &cpus[i].cache2mem_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "router_power_leakage", "avg leakage power usage of router", &cpus[i].router_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "input_buffer_power_leakage", "avg leakage power usage of router input buffer of type REQ", &cpus[i].input_buffer_power[REQ]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "arbiter_power_leakage", "avg leakage power usage of router arbiter of type REQ", &cpus[i].arbiter_power[REQ]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "crossbar_power_leakage", "avg leakage power usage of router crossbar of type REQ", &cpus[i].crossbar_power[REQ]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "link_power_leakage", "avg leakage power usage of router link of type REQ", &cpus[i].link_power[REQ]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "input_buffer_power_leakage", "avg leakage power usage of router input buffer of type INVN", &cpus[i].input_buffer_power[INVN]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "arbiter_power_leakage", "avg leakage power usage of router arbiter of type INVN", &cpus[i].arbiter_power[INVN]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "crossbar_power_leakage", "avg leakage power usage of router crossbar of type INVN", &cpus[i].crossbar_power[INVN]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "link_power_leakage", "avg leakage power usage of router link of type INVN", &cpus[i].link_power[INVN]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "input_buffer_power_leakage", "avg leakage power usage of router input buffer of type WTBK", &cpus[i].input_buffer_power[WTBK]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "arbiter_power_leakage", "avg leakage power usage of router arbiter of type WTBK", &cpus[i].arbiter_power[WTBK]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "crossbar_power_leakage", "avg leakage power usage of router crossbar of type WTBK", &cpus[i].crossbar_power[WTBK]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "link_power_leakage", "avg leakage power usage of router link of type WTBK", &cpus[i].link_power[WTBK]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "input_buffer_power_leakage", "avg leakage power usage of router input buffer of type RESP", &cpus[i].input_buffer_power[RESP]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "arbiter_power_leakage", "avg leakage power usage of router arbiter of type RESP", &cpus[i].arbiter_power[RESP]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "crossbar_power_leakage", "avg leakage power usage of router crossbar of type RESP", &cpus[i].crossbar_power[RESP]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "link_power_leakage", "avg leakage power usage of router link of type RESP", &cpus[i].link_power[RESP]->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "IOpad_power_leakage", "avg leakage power usage of IOpad", &cpus[i].IOpad_power->leakage, 0, NULL);
    stat_reg_double(cpus[i].sdb, "clocktree_power_leakage", "avg leakage power usage of clock", &cpus[i].clocktree_power->leakage, 0, NULL);

    stat_reg_double(cpus[i].sdb, "avg_cpu_power_total", "avg total power usage of the whole cpu", &cpus[i].avg_cpu_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_bpred_power_total", "avg total power usage of bpred unit", &cpus[i].avg_bpred_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_decode_power_total", "avg total power usage of decode", &cpus[i].avg_decode_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_rename_power_total", "avg total power usage of rename unit", &cpus[i].avg_rename_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_window_power_total", "avg total power usage of instruction window", &cpus[i].avg_window_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_regfile_power_total", "avg total power usage of arch. regfile", &cpus[i].avg_regfile_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_ialu_power_total", "avg total power usage of ialu", &cpus[i].avg_ialu_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_falu_power_total", "avg total power usage of falu", &cpus[i].avg_falu_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_lsq_power_total", "avg total power usage of load/store queue", &cpus[i].avg_lsq_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_roq_power_total", "avg total power usage of roq", &cpus[i].avg_roq_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_icache_power_total", "avg total power usage of icache", &cpus[i].avg_icache_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_dcache_power_total", "avg total power usage of dcache", &cpus[i].avg_dcache_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_itlb_power_total", "avg total power usage of itlb", &cpus[i].avg_itlb_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_dtlb_power_total", "avg total power usage of dtlb", &cpus[i].avg_dtlb_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_cache_dl2_power_total", "avg total power usage of cache_dl2", &cpus[i].avg_cache_dl2_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_cache2mem_power_total", "avg total power usage of cache2mem", &cpus[i].avg_cache2mem_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_router_power_total", "avg total power usage of router", &cpus[i].avg_router_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_total", "avg total power usage of router input buffer of type REQ", &cpus[i].avg_input_buffer_power_total[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_total", "avg total power usage of router arbiter of type REQ", &cpus[i].avg_arbiter_power_total[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_total", "avg total power usage of router crossbar of type REQ", &cpus[i].avg_crossbar_power_total[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_total", "avg total power usage of router link of type REQ", &cpus[i].avg_link_power_total[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_total", "avg total power usage of router input buffer of type INVN", &cpus[i].avg_input_buffer_power_total[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_total", "avg total power usage of router arbiter of type INVN", &cpus[i].avg_arbiter_power_total[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_total", "avg total power usage of router crossbar of type INVN", &cpus[i].avg_crossbar_power_total[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_total", "avg total power usage of router link of type INVN", &cpus[i].avg_link_power_total[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_total", "avg total power usage of router input buffer of type WTBK", &cpus[i].avg_input_buffer_power_total[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_total", "avg total power usage of router arbiter of type WTBK", &cpus[i].avg_arbiter_power_total[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_total", "avg total power usage of router crossbar of type WTBK", &cpus[i].avg_crossbar_power_total[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_total", "avg total power usage of router link of type WTBK", &cpus[i].avg_link_power_total[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_total", "avg total power usage of router input buffer of type RESP", &cpus[i].avg_input_buffer_power_total[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_total", "avg total power usage of router arbiter of type RESP", &cpus[i].avg_arbiter_power_total[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_total", "avg total power usage of router crossbar of type RESP", &cpus[i].avg_crossbar_power_total[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_total", "avg total power usage of router link of type RESP", &cpus[i].avg_link_power_total[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_IOpad_power_total", "avg total power usage of IOpad", &cpus[i].avg_IOpad_power_total, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_clocktree_power_total", "avg total power usage of clock", &cpus[i].avg_clocktree_power_total, 0, NULL);

    stat_reg_double(cpus[i].sdb, "avg_cpu_power_dynamic", "avg dynamic power usage of the whole cpu unit", &cpus[i].avg_cpu_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_bpred_power_dynamic", "avg dynamic power usage of bpred unit", &cpus[i].avg_bpred_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_decode_power_dynamic", "avg dynamic power usage of decode", &cpus[i].avg_decode_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_rename_power_dynamic", "avg dynamic power usage of rename unit", &cpus[i].avg_rename_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_window_power_dynamic", "avg dynamic power usage of instruction window", &cpus[i].avg_window_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_regfile_power_dynamic", "avg dynamic power usage of arch. regfile", &cpus[i].avg_regfile_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_ialu_power_dynamic", "avg dynamic power usage of ialu", &cpus[i].avg_ialu_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_falu_power_dynamic", "avg dynamic power usage of falu", &cpus[i].avg_falu_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_lsq_power_dynamic", "avg dynamic power usage of load/store queue", &cpus[i].avg_lsq_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_roq_power_dynamic", "avg dynamic power usage of roq", &cpus[i].avg_roq_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_icache_power_dynamic", "avg dynamic power usage of icache", &cpus[i].avg_icache_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_dcache_power_dynamic", "avg dynamic power usage of dcache", &cpus[i].avg_dcache_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_itlb_power_dynamic", "avg dynamic power usage of itlb", &cpus[i].avg_itlb_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_dtlb_power_dynamic", "avg dynamic power usage of dtlb", &cpus[i].avg_dtlb_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_cache_dl2_power_dynamic", "avg dynamic power usage of cache_dl2", &cpus[i].avg_cache_dl2_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_cache2mem_power_dynamic", "avg dynamic power usage of cache2mem", &cpus[i].avg_cache2mem_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_router_power_dynamic", "avg dynamic power usage of router", &cpus[i].avg_router_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_dynamic", "avg dynamic power usage of router input buffer of type REQ", &cpus[i].avg_input_buffer_power_dynamic[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_dynamic", "avg dynamic power usage of router arbiter of type REQ", &cpus[i].avg_arbiter_power_dynamic[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_dynamic", "avg dynamic power usage of router crossbar of type REQ", &cpus[i].avg_crossbar_power_dynamic[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_dynamic", "avg dynamic power usage of router link of type REQ", &cpus[i].avg_link_power_dynamic[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_dynamic", "avg dynamic power usage of router input buffer of type INVN", &cpus[i].avg_input_buffer_power_dynamic[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_dynamic", "avg dynamic power usage of router arbiter of type INVN", &cpus[i].avg_arbiter_power_dynamic[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_dynamic", "avg dynamic power usage of router crossbar of type INVN", &cpus[i].avg_crossbar_power_dynamic[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_dynamic", "avg dynamic power usage of router link of type INVN", &cpus[i].avg_link_power_dynamic[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_dynamic", "avg dynamic power usage of router input buffer of type WTBK", &cpus[i].avg_input_buffer_power_dynamic[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_dynamic", "avg dynamic power usage of router arbiter of type WTBK", &cpus[i].avg_arbiter_power_dynamic[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_dynamic", "avg dynamic power usage of router crossbar of type WTBK", &cpus[i].avg_crossbar_power_dynamic[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_dynamic", "avg dynamic power usage of router link of type WTBK", &cpus[i].avg_link_power_dynamic[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_dynamic", "avg dynamic power usage of router input buffer of type RESP", &cpus[i].avg_input_buffer_power_dynamic[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_dynamic", "avg dynamic power usage of router arbiter of type RESP", &cpus[i].avg_arbiter_power_dynamic[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_dynamic", "avg dynamic power usage of router crossbar of type RESP", &cpus[i].avg_crossbar_power_dynamic[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_dynamic", "avg dynamic power usage of router link of type RESP", &cpus[i].avg_link_power_dynamic[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_IOpad_power_dynamic", "avg dynamic power usage of IOpad", &cpus[i].avg_IOpad_power_dynamic, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_clocktree_power_dynamic", "avg dynamic power usage of clock", &cpus[i].avg_clocktree_power_dynamic, 0, NULL);

    stat_reg_double(cpus[i].sdb, "avg_cpu_power_clock", "avg clock power usage of the whold cpu", &cpus[i].avg_cpu_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_bpred_power_clock", "avg clock power usage of bpred unit", &cpus[i].avg_bpred_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_decode_power_clock", "avg clock power usage of decode", &cpus[i].avg_decode_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_rename_power_clock", "avg clock power usage of rename unit", &cpus[i].avg_rename_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_window_power_clock", "avg clock power usage of instruction window", &cpus[i].avg_window_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_regfile_power_clock", "avg clock power usage of arch. regfile", &cpus[i].avg_regfile_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_ialu_power_clock", "avg clock power usage of ialu", &cpus[i].avg_ialu_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_falu_power_clock", "avg clock power usage of falu", &cpus[i].avg_falu_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_lsq_power_clock", "avg clock power usage of load/store queue", &cpus[i].avg_lsq_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_roq_power_clock", "avg clock power usage of roq", &cpus[i].avg_roq_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_icache_power_clock", "avg clock power usage of icache", &cpus[i].avg_icache_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_dcache_power_clock", "avg clock power usage of dcache", &cpus[i].avg_dcache_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_itlb_power_clock", "avg clock power usage of itlb", &cpus[i].avg_itlb_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_dtlb_power_clock", "avg clock power usage of dtlb", &cpus[i].avg_dtlb_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_cache_dl2_power_clock", "avg clock power usage of cache_dl2", &cpus[i].avg_cache_dl2_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_cache2mem_power_clock", "avg clock power usage of cache2mem", &cpus[i].avg_cache2mem_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_router_power_clock", "avg clock power usage of router", &cpus[i].avg_router_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_clock", "avg clock power usage of router input buffer of type REQ", &cpus[i].avg_input_buffer_power_clock[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_clock", "avg clock power usage of router arbiter of type REQ", &cpus[i].avg_arbiter_power_clock[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_clock", "avg clock power usage of router crossbar of type REQ", &cpus[i].avg_crossbar_power_clock[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_clock", "avg clock power usage of router link of type REQ", &cpus[i].avg_link_power_clock[REQ], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_clock", "avg clock power usage of router input buffer of type INVN", &cpus[i].avg_input_buffer_power_clock[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_clock", "avg clock power usage of router arbiter of type INVN", &cpus[i].avg_arbiter_power_clock[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_clock", "avg clock power usage of router crossbar of type INVN", &cpus[i].avg_crossbar_power_clock[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_clock", "avg clock power usage of router link of type INVN", &cpus[i].avg_link_power_clock[INVN], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_clock", "avg clock power usage of router input buffer of type WTBK", &cpus[i].avg_input_buffer_power_clock[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_clock", "avg clock power usage of router arbiter of type WTBK", &cpus[i].avg_arbiter_power_clock[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_clock", "avg clock power usage of router crossbar of type WTBK", &cpus[i].avg_crossbar_power_clock[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_clock", "avg clock power usage of router link of type WTBK", &cpus[i].avg_link_power_clock[WTBK], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_input_buffer_power_clock", "avg clock power usage of router input buffer of type RESP", &cpus[i].avg_input_buffer_power_clock[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_arbiter_power_clock", "avg clock power usage of router arbiter of type RESP", &cpus[i].avg_arbiter_power_clock[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_crossbar_power_clock", "avg clock power usage of router crossbar of type RESP", &cpus[i].avg_crossbar_power_clock[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_link_power_clock", "avg clock power usage of router link of type RESP", &cpus[i].avg_link_power_clock[RESP], 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_IOpad_power_clock", "avg clock power usage of IOpad", &cpus[i].avg_IOpad_power_clock, 0, NULL);
    stat_reg_double(cpus[i].sdb, "avg_clocktree_power_clock", "avg clock power usage of clock", &cpus[i].avg_clocktree_power_clock, 0, NULL);
  }
}

#endif
