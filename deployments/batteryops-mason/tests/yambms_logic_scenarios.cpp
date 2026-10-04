void fixture() {
  yambms1_var_bms_counter = 1;
  yambms1_cell_count.state = 16;
  yambms1_bulk_voltage.state = 55.2;
  yambms1_float_voltage.state = 53.6;
  yambms1_rebulk_voltage.state = 52.8;
  yambms1_rebulk_soc.state = 85;
  yambms1_var_cell_min_charge_v = 3.45;
  yambms1_var_cell_max_charge_v = 3.60;
  yambms1_switch_charging.state = 1;
  yambms1_switch_discharging.state = 1;
  yambms1_switch_float.state = 1;
  yambms1_switch_eoc_timer.state = 1;
  yambms1_max_requested_charge_current.state = 35;
  yambms1_max_requested_discharge_current.state = 85;
  bms1_online_status.state = 1;
  bms1_charging_allowed.state = 1;
  bms1_discharging_allowed.state = 1;
  bms1_total_voltage.state = 54;
  bms1_current.state = 20;
  bms1_power.state = 1080;
  bms1_battery_soc.state = 90;
  bms1_battery_soh.state = 100;
  bms1_battery_capacity.state = 280;
  bms1_battery_capacity_remaining.state = 252;
  bms1_max_charge_current.state = 180;
  bms1_max_discharge_current.state = 180;
  bms1_min_cell_voltage.state = 3.36;
  bms1_max_cell_voltage.state = 3.38;
  bms1_min_temperature.state = 20;
  bms1_max_temperature.state = 22;
  bms1_cell_ovp.state = 3.66;
  bms1_cell_uvp.state = 2.7;
  bms1_balance_trigger_voltage.state = 0.05;
  yambms1_auto_float_mode.selection = 0;
  yambms1_auto_float_step_v.state = 0.1;
  yambms1_auto_float_interval.state = 1;
  yambms1_script_eoc_timer.delay_ms = 30 * 60000;
  yambms1_script_cutoff_timer.delay_ms = 60 * 1000;
  yambms1_script_eoc_timer.callback = run_eoc;
  yambms1_script_cutoff_timer.callback = run_eoc;
}

void tick() {
  yambms1_script_eoc_timer.poll();
  yambms1_script_cutoff_timer.poll();
  bms1_can_be_combined.state = run_eligible();
  run_merge();
  // All auto-derating modules have finished this scheduler cycle.
  yambms1_var_charge_current_step = yambms1_var_auto_ccl_functions_count + 1;
  yambms1_var_discharge_current_step = yambms1_var_auto_dcl_functions_count + 1;
  run_core();
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  fixture();
  if (scenario == "startup") {
    bms1_online_status.state = 0;
    tick();
    assert(!yambms1_var_initialized);
    assert(yambms1_requested_charge_current.state == 0);
  } else if (scenario == "zero_ccl") {
    bms1_max_charge_current.state = 0;
    tick();
    std::cout << "combined=" << yambms1_bms_combined.state << " ";
    assert(yambms1_bms_combined.state == 1);
    assert(yambms1_requested_charge_current.state == 0);
    assert(yambms1_requested_discharge_current.state > 0);
  } else if (scenario == "zero_dcl") {
    bms1_max_discharge_current.state = 0;
    tick();
    std::cout << "combined=" << yambms1_bms_combined.state << " ";
    assert(yambms1_bms_combined.state == 1);
    assert(yambms1_requested_discharge_current.state == 0);
  } else if (scenario == "zero_after_charge" || scenario == "both_zero") {
    tick();
    bms1_max_charge_current.state = 0;
    if (scenario == "both_zero") bms1_max_discharge_current.state = 0;
    tick();
    assert(yambms1_bms_combined.state == 1);
    assert(yambms1_requested_charge_current.state == 0);
    if (scenario == "both_zero") assert(yambms1_requested_discharge_current.state == 0);
    else assert(yambms1_requested_discharge_current.state > 0);
  } else if (scenario == "invalid_limit" || scenario == "infinite_limit") {
    tick();
    bms1_max_charge_current.state = scenario == "invalid_limit" ? NAN : INFINITY;
    tick();
    assert(!yambms1_var_initialized);
    assert(yambms1_requested_charge_current.state == 0);
  } else if (scenario == "discharge_off") {
    tick();
    bms1_discharging_allowed.state = 0;
    tick();
    assert(yambms1_requested_discharge_current.state == 0);
    assert(yambms1_requested_charge_current.state > 0);
  } else if (scenario == "early_soc_100") {
    bms1_battery_soc.state = 100;
    bms1_battery_capacity_remaining.state = 280;
    tick();
    assert(yambms1_var_charge_status == "Bulk");
    assert(!yambms1_var_eoc);
  } else if (scenario == "balancing_timer") {
    bms1_max_cell_voltage.state = 3.46;
    bms1_current.state = 0.1;
    bms1_equalizing.state = 1;
    tick(); now_ms = 10001; tick();
    assert(yambms1_var_charge_status == "Cut-Off");
    assert(yambms1_script_eoc_timer.is_running());
    assert(!yambms1_script_cutoff_timer.is_running());
    now_ms += 60001; tick();
    assert(!yambms1_var_eoc);
    now_ms = 1810002; tick();
    assert(yambms1_var_eoc && yambms1_var_charge_status == "Float");
  } else if (scenario == "float_charge_off") {
    tick(); yambms1_var_charge_status = "Float";
    bms1_charging_allowed.state = 0; tick();
    assert(yambms1_requested_charge_current.state == 0);
  } else if (scenario == "charge_off") {
    tick();
    bms1_charging_allowed.state = 0;
    tick();
    assert(yambms1_requested_charge_current.state == 0);
    assert(yambms1_requested_discharge_current.state > 0);
  } else if (scenario == "disconnect") {
    tick(); assert(yambms1_var_initialized);
    bms1_online_status.state = 0;
    tick();
    assert(!yambms1_var_initialized);
    assert(yambms1_requested_charge_current.state == 0);
    assert(yambms1_requested_discharge_current.state == 0);
  } else if (scenario == "missing_measurement") {
    tick();
    bms1_current.present = false;
    tick();
    assert(!yambms1_var_initialized);
  } else if (scenario == "cutoff_float" || scenario == "no_float") {
    bms1_max_cell_voltage.state = 3.46;
    bms1_total_voltage.state = 55.2;
    bms1_current.state = 0.1;
    if (scenario == "no_float") yambms1_switch_float.state = 0;
    tick();
    now_ms = 10001; tick();
    assert(yambms1_var_charge_status == "Cut-Off");
    assert(yambms1_script_cutoff_timer.is_running());
    now_ms += 60001; tick();
    assert(yambms1_var_eoc);
    if (scenario == "cutoff_float") {
      assert(yambms1_var_charge_status == "Float");
      assert(std::abs(yambms1_requested_charge_voltage.state - 53.6) < 0.01);
      assert(std::abs(run_float_voltage() - 53.6) < 0.01);
    } else {
      assert(yambms1_var_charge_status == "EOC");
      assert(yambms1_requested_charge_current.state == 0);
    }
  } else if (scenario == "auto_ccl_stop") {
    tick();
    yambms1_switch_auto_ccl.state = 1;
    yambms1_max_cell_voltage.state = 3.66;
    yambms1_var_auto_ccl_functions_count = 1;
    yambms1_var_charge_current_step = 1;
    run_auto_ccl();
    assert(yambms1_var_auto_ccl <= -yambms1_max_charge_current.state);
    tick(); assert(yambms1_requested_charge_current.state == 0);
  } else {
    tick(); assert(yambms1_var_initialized);
    assert(yambms1_requested_charge_current.state > 0);
    assert(yambms1_requested_charge_current.state <= 35);
    assert(yambms1_requested_discharge_current.state <= 85);
  }
  std::cout << "PASS " << scenario << " CVL=" << yambms1_requested_charge_voltage.state
            << " CCL=" << yambms1_requested_charge_current.state
            << " DCL=" << yambms1_requested_discharge_current.state
            << " state=" << yambms1_var_charge_status << "\n";
}
