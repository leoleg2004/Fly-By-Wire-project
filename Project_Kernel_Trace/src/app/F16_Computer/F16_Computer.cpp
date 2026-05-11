#define _GNU_SOURCE
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "f16_fcs_long.hpp"
#include "f16_fcs_lat.hpp"
#include "f16_fcs_alt.hpp"
#include "f16_nav_gps.hpp"
#include "f16_nav_ins.hpp"
#include "f16_radar_air.hpp"
#include "f16_radar_gnd.hpp"
#include "f16_engine_fadec.hpp"
#include "f16_weapons.hpp"
#include "f16_displays.hpp"

#include "trace_marker.h"

#define handle_error(en, msg)                                                  \
  if (en != 0) {                                                               \
    errno = en;                                                                \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  }

int main(int argc, char *argv[]) {
  // Inizializza il file descriptor per i marker di tracciamento
  init_tracing();
  printf("Starting F-16 Real-Time Multi-Threaded Onboard Computer (20 Threads)...\n");

  int ret_err;
  cpu_set_t cpuset;

  // FCS Longitudinal - PITCH A
  pthread_t thread_pitch_a;
  pthread_attr_t attr_pitch_a;
  struct sched_param param_pitch_a;
  pthread_attr_init(&attr_pitch_a);
  pthread_attr_setinheritsched(&attr_pitch_a, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_pitch_a, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(0, &cpuset);
  pthread_attr_setaffinity_np(&attr_pitch_a, sizeof(cpu_set_t), &cpuset);

  t_f16_fcs_long_par activity_pitch_a;
  sprintf(activity_pitch_a.name, "PITCH_A");
  activity_pitch_a.period = 10;
  activity_pitch_a.parameter = 1;
  activity_pitch_a.deadline = 10;

  param_pitch_a.sched_priority = 99 - (activity_pitch_a.period / 10);
  if (param_pitch_a.sched_priority < 1) param_pitch_a.sched_priority = 1;
  pthread_attr_setschedparam(&attr_pitch_a, &param_pitch_a);
  ret_err = pthread_create(&thread_pitch_a, &attr_pitch_a, PeriodicTask_FCS_LONG, (void *)&activity_pitch_a);
  handle_error(ret_err, "Error in creating PITCH_A");
  pthread_setname_np(thread_pitch_a, "PITCH_A");

  // FCS Longitudinal - PITCH B
  pthread_t thread_pitch_b;
  pthread_attr_t attr_pitch_b;
  struct sched_param param_pitch_b;
  pthread_attr_init(&attr_pitch_b);
  pthread_attr_setinheritsched(&attr_pitch_b, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_pitch_b, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(1, &cpuset);
  pthread_attr_setaffinity_np(&attr_pitch_b, sizeof(cpu_set_t), &cpuset);

  t_f16_fcs_long_par activity_pitch_b;
  sprintf(activity_pitch_b.name, "PITCH_B");
  activity_pitch_b.period = 10;
  activity_pitch_b.parameter = 1;
  activity_pitch_b.deadline = 10;

  param_pitch_b.sched_priority = 99 - (activity_pitch_b.period / 10);
  if (param_pitch_b.sched_priority < 1) param_pitch_b.sched_priority = 1;
  pthread_attr_setschedparam(&attr_pitch_b, &param_pitch_b);
  ret_err = pthread_create(&thread_pitch_b, &attr_pitch_b, PeriodicTask_FCS_LONG, (void *)&activity_pitch_b);
  handle_error(ret_err, "Error in creating PITCH_B");
  pthread_setname_np(thread_pitch_b, "PITCH_B");

  // FCS Longitudinal - PITCH C
  pthread_t thread_pitch_c;
  pthread_attr_t attr_pitch_c;
  struct sched_param param_pitch_c;
  pthread_attr_init(&attr_pitch_c);
  pthread_attr_setinheritsched(&attr_pitch_c, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_pitch_c, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(2, &cpuset);
  pthread_attr_setaffinity_np(&attr_pitch_c, sizeof(cpu_set_t), &cpuset);

  t_f16_fcs_long_par activity_pitch_c;
  sprintf(activity_pitch_c.name, "PITCH_C");
  activity_pitch_c.period = 10;
  activity_pitch_c.parameter = 1;
  activity_pitch_c.deadline = 10;

  param_pitch_c.sched_priority = 99 - (activity_pitch_c.period / 10);
  if (param_pitch_c.sched_priority < 1) param_pitch_c.sched_priority = 1;
  pthread_attr_setschedparam(&attr_pitch_c, &param_pitch_c);
  ret_err = pthread_create(&thread_pitch_c, &attr_pitch_c, PeriodicTask_FCS_LONG, (void *)&activity_pitch_c);
  handle_error(ret_err, "Error in creating PITCH_C");
  pthread_setname_np(thread_pitch_c, "PITCH_C");

  // FCS Lateral - ROLL A
  pthread_t thread_roll_a;
  pthread_attr_t attr_roll_a;
  struct sched_param param_roll_a;
  pthread_attr_init(&attr_roll_a);
  pthread_attr_setinheritsched(&attr_roll_a, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_roll_a, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(3, &cpuset);
  pthread_attr_setaffinity_np(&attr_roll_a, sizeof(cpu_set_t), &cpuset);

  t_f16_fcs_lat_par activity_roll_a;
  sprintf(activity_roll_a.name, "ROLL_A");
  activity_roll_a.period = 10;
  activity_roll_a.parameter = 1;
  activity_roll_a.deadline = 10;

  param_roll_a.sched_priority = 99 - (activity_roll_a.period / 10);
  if (param_roll_a.sched_priority < 1) param_roll_a.sched_priority = 1;
  pthread_attr_setschedparam(&attr_roll_a, &param_roll_a);
  ret_err = pthread_create(&thread_roll_a, &attr_roll_a, PeriodicTask_FCS_LAT, (void *)&activity_roll_a);
  handle_error(ret_err, "Error in creating ROLL_A");
  pthread_setname_np(thread_roll_a, "ROLL_A");

  // FCS Lateral - ROLL B
  pthread_t thread_roll_b;
  pthread_attr_t attr_roll_b;
  struct sched_param param_roll_b;
  pthread_attr_init(&attr_roll_b);
  pthread_attr_setinheritsched(&attr_roll_b, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_roll_b, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(4, &cpuset);
  pthread_attr_setaffinity_np(&attr_roll_b, sizeof(cpu_set_t), &cpuset);

  t_f16_fcs_lat_par activity_roll_b;
  sprintf(activity_roll_b.name, "ROLL_B");
  activity_roll_b.period = 10;
  activity_roll_b.parameter = 1;
  activity_roll_b.deadline = 10;

  param_roll_b.sched_priority = 99 - (activity_roll_b.period / 10);
  if (param_roll_b.sched_priority < 1) param_roll_b.sched_priority = 1;
  pthread_attr_setschedparam(&attr_roll_b, &param_roll_b);
  ret_err = pthread_create(&thread_roll_b, &attr_roll_b, PeriodicTask_FCS_LAT, (void *)&activity_roll_b);
  handle_error(ret_err, "Error in creating ROLL_B");
  pthread_setname_np(thread_roll_b, "ROLL_B");

  // FCS Lateral - ROLL C
  pthread_t thread_roll_c;
  pthread_attr_t attr_roll_c;
  struct sched_param param_roll_c;
  pthread_attr_init(&attr_roll_c);
  pthread_attr_setinheritsched(&attr_roll_c, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_roll_c, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(5, &cpuset);
  pthread_attr_setaffinity_np(&attr_roll_c, sizeof(cpu_set_t), &cpuset);

  t_f16_fcs_lat_par activity_roll_c;
  sprintf(activity_roll_c.name, "ROLL_C");
  activity_roll_c.period = 10;
  activity_roll_c.parameter = 1;
  activity_roll_c.deadline = 10;

  param_roll_c.sched_priority = 99 - (activity_roll_c.period / 10);
  if (param_roll_c.sched_priority < 1) param_roll_c.sched_priority = 1;
  pthread_attr_setschedparam(&attr_roll_c, &param_roll_c);
  ret_err = pthread_create(&thread_roll_c, &attr_roll_c, PeriodicTask_FCS_LAT, (void *)&activity_roll_c);
  handle_error(ret_err, "Error in creating ROLL_C");
  pthread_setname_np(thread_roll_c, "ROLL_C");

  // FCS Altitude - ALT A
  pthread_t thread_alt_a;
  pthread_attr_t attr_alt_a;
  struct sched_param param_alt_a;
  pthread_attr_init(&attr_alt_a);
  pthread_attr_setinheritsched(&attr_alt_a, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_alt_a, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(6, &cpuset);
  pthread_attr_setaffinity_np(&attr_alt_a, sizeof(cpu_set_t), &cpuset);

  t_f16_fcs_alt_par activity_alt_a;
  sprintf(activity_alt_a.name, "ALT_A");
  activity_alt_a.period = 20;
  activity_alt_a.parameter = 1;
  activity_alt_a.deadline = 20;

  param_alt_a.sched_priority = 99 - (activity_alt_a.period / 10);
  if (param_alt_a.sched_priority < 1) param_alt_a.sched_priority = 1;
  pthread_attr_setschedparam(&attr_alt_a, &param_alt_a);
  ret_err = pthread_create(&thread_alt_a, &attr_alt_a, PeriodicTask_FCS_ALT, (void *)&activity_alt_a);
  handle_error(ret_err, "Error in creating ALT_A");
  pthread_setname_np(thread_alt_a, "ALT_A");

  // FCS Altitude - ALT B
  pthread_t thread_alt_b;
  pthread_attr_t attr_alt_b;
  struct sched_param param_alt_b;
  pthread_attr_init(&attr_alt_b);
  pthread_attr_setinheritsched(&attr_alt_b, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_alt_b, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(0, &cpuset);
  pthread_attr_setaffinity_np(&attr_alt_b, sizeof(cpu_set_t), &cpuset);

  t_f16_fcs_alt_par activity_alt_b;
  sprintf(activity_alt_b.name, "ALT_B");
  activity_alt_b.period = 20;
  activity_alt_b.parameter = 1;
  activity_alt_b.deadline = 20;

  param_alt_b.sched_priority = 99 - (activity_alt_b.period / 10);
  if (param_alt_b.sched_priority < 1) param_alt_b.sched_priority = 1;
  pthread_attr_setschedparam(&attr_alt_b, &param_alt_b);
  ret_err = pthread_create(&thread_alt_b, &attr_alt_b, PeriodicTask_FCS_ALT, (void *)&activity_alt_b);
  handle_error(ret_err, "Error in creating ALT_B");
  pthread_setname_np(thread_alt_b, "ALT_B");

  // NAV GPS - GPS 1
  pthread_t thread_gps_1;
  pthread_attr_t attr_gps_1;
  struct sched_param param_gps_1;
  pthread_attr_init(&attr_gps_1);
  pthread_attr_setinheritsched(&attr_gps_1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_gps_1, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(1, &cpuset);
  pthread_attr_setaffinity_np(&attr_gps_1, sizeof(cpu_set_t), &cpuset);

  t_f16_nav_gps_par activity_gps_1;
  sprintf(activity_gps_1.name, "GPS_1");
  activity_gps_1.period = 50;
  activity_gps_1.parameter = 1;
  activity_gps_1.deadline = 50;

  param_gps_1.sched_priority = 99 - (activity_gps_1.period / 10);
  if (param_gps_1.sched_priority < 1) param_gps_1.sched_priority = 1;
  pthread_attr_setschedparam(&attr_gps_1, &param_gps_1);
  ret_err = pthread_create(&thread_gps_1, &attr_gps_1, PeriodicTask_NAV_GPS, (void *)&activity_gps_1);
  handle_error(ret_err, "Error in creating GPS_1");
  pthread_setname_np(thread_gps_1, "GPS_1");

  // NAV GPS - GPS 2
  pthread_t thread_gps_2;
  pthread_attr_t attr_gps_2;
  struct sched_param param_gps_2;
  pthread_attr_init(&attr_gps_2);
  pthread_attr_setinheritsched(&attr_gps_2, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_gps_2, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(2, &cpuset);
  pthread_attr_setaffinity_np(&attr_gps_2, sizeof(cpu_set_t), &cpuset);

  t_f16_nav_gps_par activity_gps_2;
  sprintf(activity_gps_2.name, "GPS_2");
  activity_gps_2.period = 50;
  activity_gps_2.parameter = 1;
  activity_gps_2.deadline = 50;

  param_gps_2.sched_priority = 99 - (activity_gps_2.period / 10);
  if (param_gps_2.sched_priority < 1) param_gps_2.sched_priority = 1;
  pthread_attr_setschedparam(&attr_gps_2, &param_gps_2);
  ret_err = pthread_create(&thread_gps_2, &attr_gps_2, PeriodicTask_NAV_GPS, (void *)&activity_gps_2);
  handle_error(ret_err, "Error in creating GPS_2");
  pthread_setname_np(thread_gps_2, "GPS_2");

  // NAV INS - INS 1
  pthread_t thread_ins_1;
  pthread_attr_t attr_ins_1;
  struct sched_param param_ins_1;
  pthread_attr_init(&attr_ins_1);
  pthread_attr_setinheritsched(&attr_ins_1, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_ins_1, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(3, &cpuset);
  pthread_attr_setaffinity_np(&attr_ins_1, sizeof(cpu_set_t), &cpuset);

  t_f16_nav_ins_par activity_ins_1;
  sprintf(activity_ins_1.name, "INS_1");
  activity_ins_1.period = 10;
  activity_ins_1.parameter = 1;
  activity_ins_1.deadline = 10;

  param_ins_1.sched_priority = 99 - (activity_ins_1.period / 10);
  if (param_ins_1.sched_priority < 1) param_ins_1.sched_priority = 1;
  pthread_attr_setschedparam(&attr_ins_1, &param_ins_1);
  ret_err = pthread_create(&thread_ins_1, &attr_ins_1, PeriodicTask_NAV_INS, (void *)&activity_ins_1);
  handle_error(ret_err, "Error in creating INS_1");
  pthread_setname_np(thread_ins_1, "INS_1");

  // NAV INS - INS 2
  pthread_t thread_ins_2;
  pthread_attr_t attr_ins_2;
  struct sched_param param_ins_2;
  pthread_attr_init(&attr_ins_2);
  pthread_attr_setinheritsched(&attr_ins_2, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_ins_2, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(4, &cpuset);
  pthread_attr_setaffinity_np(&attr_ins_2, sizeof(cpu_set_t), &cpuset);

  t_f16_nav_ins_par activity_ins_2;
  sprintf(activity_ins_2.name, "INS_2");
  activity_ins_2.period = 10;
  activity_ins_2.parameter = 1;
  activity_ins_2.deadline = 10;

  param_ins_2.sched_priority = 99 - (activity_ins_2.period / 10);
  if (param_ins_2.sched_priority < 1) param_ins_2.sched_priority = 1;
  pthread_attr_setschedparam(&attr_ins_2, &param_ins_2);
  ret_err = pthread_create(&thread_ins_2, &attr_ins_2, PeriodicTask_NAV_INS, (void *)&activity_ins_2);
  handle_error(ret_err, "Error in creating INS_2");
  pthread_setname_np(thread_ins_2, "INS_2");

  // NAV INS - INS 3
  pthread_t thread_ins_3;
  pthread_attr_t attr_ins_3;
  struct sched_param param_ins_3;
  pthread_attr_init(&attr_ins_3);
  pthread_attr_setinheritsched(&attr_ins_3, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_ins_3, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(5, &cpuset);
  pthread_attr_setaffinity_np(&attr_ins_3, sizeof(cpu_set_t), &cpuset);

  t_f16_nav_ins_par activity_ins_3;
  sprintf(activity_ins_3.name, "INS_3");
  activity_ins_3.period = 10;
  activity_ins_3.parameter = 1;
  activity_ins_3.deadline = 10;

  param_ins_3.sched_priority = 99 - (activity_ins_3.period / 10);
  if (param_ins_3.sched_priority < 1) param_ins_3.sched_priority = 1;
  pthread_attr_setschedparam(&attr_ins_3, &param_ins_3);
  ret_err = pthread_create(&thread_ins_3, &attr_ins_3, PeriodicTask_NAV_INS, (void *)&activity_ins_3);
  handle_error(ret_err, "Error in creating INS_3");
  pthread_setname_np(thread_ins_3, "INS_3");

  // RADAR AIR - RADAR_A
  pthread_t thread_radar_a;
  pthread_attr_t attr_radar_a;
  struct sched_param param_radar_a;
  pthread_attr_init(&attr_radar_a);
  pthread_attr_setinheritsched(&attr_radar_a, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_radar_a, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(6, &cpuset);
  pthread_attr_setaffinity_np(&attr_radar_a, sizeof(cpu_set_t), &cpuset);

  t_f16_radar_air_par activity_radar_a;
  sprintf(activity_radar_a.name, "RADAR_A");
  activity_radar_a.period = 50;
  activity_radar_a.parameter = 2;
  activity_radar_a.deadline = 50;

  param_radar_a.sched_priority = 99 - (activity_radar_a.period / 10);
  if (param_radar_a.sched_priority < 1) param_radar_a.sched_priority = 1;
  pthread_attr_setschedparam(&attr_radar_a, &param_radar_a);
  ret_err = pthread_create(&thread_radar_a, &attr_radar_a, PeriodicTask_RADAR_AIR, (void *)&activity_radar_a);
  handle_error(ret_err, "Error in creating RADAR_A");
  pthread_setname_np(thread_radar_a, "RADAR_A");

  // RADAR GROUND - RADAR_G
  pthread_t thread_radar_g;
  pthread_attr_t attr_radar_g;
  struct sched_param param_radar_g;
  pthread_attr_init(&attr_radar_g);
  pthread_attr_setinheritsched(&attr_radar_g, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_radar_g, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(0, &cpuset);
  pthread_attr_setaffinity_np(&attr_radar_g, sizeof(cpu_set_t), &cpuset);

  t_f16_radar_gnd_par activity_radar_g;
  sprintf(activity_radar_g.name, "RADAR_G");
  activity_radar_g.period = 100;
  activity_radar_g.parameter = 5;
  activity_radar_g.deadline = 100;

  param_radar_g.sched_priority = 99 - (activity_radar_g.period / 10);
  if (param_radar_g.sched_priority < 1) param_radar_g.sched_priority = 1;
  pthread_attr_setschedparam(&attr_radar_g, &param_radar_g);
  ret_err = pthread_create(&thread_radar_g, &attr_radar_g, PeriodicTask_RADAR_GND, (void *)&activity_radar_g);
  handle_error(ret_err, "Error in creating RADAR_G");
  pthread_setname_np(thread_radar_g, "RADAR_G");

  // ENGINE FADEC - FADEC A
  pthread_t thread_fadec_a;
  pthread_attr_t attr_fadec_a;
  struct sched_param param_fadec_a;
  pthread_attr_init(&attr_fadec_a);
  pthread_attr_setinheritsched(&attr_fadec_a, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_fadec_a, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(1, &cpuset);
  pthread_attr_setaffinity_np(&attr_fadec_a, sizeof(cpu_set_t), &cpuset);

  t_f16_engine_fadec_par activity_fadec_a;
  sprintf(activity_fadec_a.name, "FADEC_A");
  activity_fadec_a.period = 10;
  activity_fadec_a.parameter = 1;
  activity_fadec_a.deadline = 10;

  param_fadec_a.sched_priority = 99 - (activity_fadec_a.period / 10);
  if (param_fadec_a.sched_priority < 1) param_fadec_a.sched_priority = 1;
  pthread_attr_setschedparam(&attr_fadec_a, &param_fadec_a);
  ret_err = pthread_create(&thread_fadec_a, &attr_fadec_a, PeriodicTask_ENGINE_FADEC, (void *)&activity_fadec_a);
  handle_error(ret_err, "Error in creating FADEC_A");
  pthread_setname_np(thread_fadec_a, "FADEC_A");

  // ENGINE FADEC - FADEC B
  pthread_t thread_fadec_b;
  pthread_attr_t attr_fadec_b;
  struct sched_param param_fadec_b;
  pthread_attr_init(&attr_fadec_b);
  pthread_attr_setinheritsched(&attr_fadec_b, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_fadec_b, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(2, &cpuset);
  pthread_attr_setaffinity_np(&attr_fadec_b, sizeof(cpu_set_t), &cpuset);

  t_f16_engine_fadec_par activity_fadec_b;
  sprintf(activity_fadec_b.name, "FADEC_B");
  activity_fadec_b.period = 10;
  activity_fadec_b.parameter = 1;
  activity_fadec_b.deadline = 10;

  param_fadec_b.sched_priority = 99 - (activity_fadec_b.period / 10);
  if (param_fadec_b.sched_priority < 1) param_fadec_b.sched_priority = 1;
  pthread_attr_setschedparam(&attr_fadec_b, &param_fadec_b);
  ret_err = pthread_create(&thread_fadec_b, &attr_fadec_b, PeriodicTask_ENGINE_FADEC, (void *)&activity_fadec_b);
  handle_error(ret_err, "Error in creating FADEC_B");
  pthread_setname_np(thread_fadec_b, "FADEC_B");

  // WEAPONS - WEAPONS
  pthread_t thread_weapons;
  pthread_attr_t attr_weapons;
  struct sched_param param_weapons;
  pthread_attr_init(&attr_weapons);
  pthread_attr_setinheritsched(&attr_weapons, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_weapons, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(3, &cpuset);
  pthread_attr_setaffinity_np(&attr_weapons, sizeof(cpu_set_t), &cpuset);

  t_f16_weapons_par activity_weapons;
  sprintf(activity_weapons.name, "WEAPONS");
  activity_weapons.period = 100;
  activity_weapons.parameter = 1;
  activity_weapons.deadline = 100;

  param_weapons.sched_priority = 99 - (activity_weapons.period / 10);
  if (param_weapons.sched_priority < 1) param_weapons.sched_priority = 1;
  pthread_attr_setschedparam(&attr_weapons, &param_weapons);
  ret_err = pthread_create(&thread_weapons, &attr_weapons, PeriodicTask_WEAPONS, (void *)&activity_weapons);
  handle_error(ret_err, "Error in creating WEAPONS");
  pthread_setname_np(thread_weapons, "WEAPONS");

  // DISPLAYS - HUD
  pthread_t thread_hud;
  pthread_attr_t attr_hud;
  struct sched_param param_hud;
  pthread_attr_init(&attr_hud);
  pthread_attr_setinheritsched(&attr_hud, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_hud, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(4, &cpuset);
  pthread_attr_setaffinity_np(&attr_hud, sizeof(cpu_set_t), &cpuset);

  t_f16_displays_par activity_hud;
  sprintf(activity_hud.name, "HUD");
  activity_hud.period = 30;
  activity_hud.parameter = 2;
  activity_hud.deadline = 30;

  param_hud.sched_priority = 99 - (activity_hud.period / 10);
  if (param_hud.sched_priority < 1) param_hud.sched_priority = 1;
  pthread_attr_setschedparam(&attr_hud, &param_hud);
  ret_err = pthread_create(&thread_hud, &attr_hud, PeriodicTask_DISPLAYS, (void *)&activity_hud);
  handle_error(ret_err, "Error in creating HUD");
  pthread_setname_np(thread_hud, "HUD");

  // DISPLAYS - MFD
  pthread_t thread_mfd;
  pthread_attr_t attr_mfd;
  struct sched_param param_mfd;
  pthread_attr_init(&attr_mfd);
  pthread_attr_setinheritsched(&attr_mfd, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr_mfd, SCHED_FIFO);
  CPU_ZERO(&cpuset); CPU_SET(5, &cpuset);
  pthread_attr_setaffinity_np(&attr_mfd, sizeof(cpu_set_t), &cpuset);

  t_f16_displays_par activity_mfd;
  sprintf(activity_mfd.name, "MFD");
  activity_mfd.period = 30;
  activity_mfd.parameter = 2;
  activity_mfd.deadline = 30;

  param_mfd.sched_priority = 99 - (activity_mfd.period / 10);
  if (param_mfd.sched_priority < 1) param_mfd.sched_priority = 1;
  pthread_attr_setschedparam(&attr_mfd, &param_mfd);
  ret_err = pthread_create(&thread_mfd, &attr_mfd, PeriodicTask_DISPLAYS, (void *)&activity_mfd);
  handle_error(ret_err, "Error in creating MFD");
  pthread_setname_np(thread_mfd, "MFD");

  // Wait for threads
  pthread_join(thread_pitch_a, NULL);
  pthread_join(thread_pitch_b, NULL);
  pthread_join(thread_pitch_c, NULL);
  pthread_join(thread_roll_a, NULL);
  pthread_join(thread_roll_b, NULL);
  pthread_join(thread_roll_c, NULL);
  pthread_join(thread_alt_a, NULL);
  pthread_join(thread_alt_b, NULL);
  pthread_join(thread_gps_1, NULL);
  pthread_join(thread_gps_2, NULL);
  pthread_join(thread_ins_1, NULL);
  pthread_join(thread_ins_2, NULL);
  pthread_join(thread_ins_3, NULL);
  pthread_join(thread_radar_a, NULL);
  pthread_join(thread_radar_g, NULL);
  pthread_join(thread_fadec_a, NULL);
  pthread_join(thread_fadec_b, NULL);
  pthread_join(thread_weapons, NULL);
  pthread_join(thread_hud, NULL);
  pthread_join(thread_mfd, NULL);

  close_tracing();
  exit(0);
}
