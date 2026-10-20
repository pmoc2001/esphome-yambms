// This fixture follows the ordinary or real-parser fixture in the generated test.
constexpr time_t previous_completion = 1791000000;

void assert_cancelled() {
  assert(!yambms1_var_eoc);
  assert(yambms1_var_eoc_timestamp == previous_completion);
  assert(!yambms1_var_finish_active);
  assert(!yambms1_script_eoc_timer.is_running());
  assert(!yambms1_script_cutoff_timer.is_running());
}

void finish_fixture(bool backstop) {
  fixture();
  yambms1_var_cell_min_charge_v = 3.37f;
  yambms1_var_cell_max_charge_v = 3.65f;
  yambms1_var_eoc_timestamp = previous_completion;
  yambms1_switch_eoc_timer.state = 1;
  yambms1_script_cutoff_timer.delay_ms = 600000;
  yambms1_script_eoc_timer.delay_ms = 1800000;
  yambms1_script_cutoff_timer.delay_function = run_cutoff_delay;
  yambms1_script_eoc_timer.delay_function = run_eoc_delay;
  yambms1_script_eoc_timer.callback = [](int attempt, bool) { run_eoc(attempt, true); };
  yambms1_script_cutoff_timer.callback = [](int attempt, bool) { run_eoc(attempt, false); };
#ifdef OPERATING_MODE
  seplos_v3.set_status_profile("v3_sp18b_fw15_balancing");
  run_finish_source_init();
  input_current = 3;
  input_voltage = 55.15f;
  trial_max_cell = 3.50f;
  if (backstop) pic[6] = 8;
  fresh(); tick();
  now_ms += 10001;
  fresh(); tick();
  assert(yambms1_var_charge_status == "Cut-Off");
  assert(yambms1_script_eoc_timer.is_running());
  assert(yambms1_script_cutoff_timer.is_running() != backstop);
#else
  bms1_max_cell_voltage.state = 3.50f;
  bms1_total_voltage.state = 55.15f;
  bms1_current.state = 3;
  bms1_equalizing.state = backstop;
  tick();
  now_ms += 10001;
  tick();
  assert(yambms1_var_charge_status == "Cut-Off");
  assert(yambms1_script_eoc_timer.is_running());
  assert(yambms1_script_cutoff_timer.is_running() != backstop);
#endif
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  const bool backstop = scenario.find("backstop") != std::string::npos;
  finish_fixture(backstop);
  const int original_attempt = yambms1_var_finish_attempt;
  const uint32_t deadline = backstop ? yambms1_script_eoc_timer.deadline :
                                      yambms1_script_cutoff_timer.deadline;
#ifdef OPERATING_MODE
  now_ms = deadline;
  fresh();
  const auto suffix = scenario.substr(backstop ? 13 : 10);
  if (suffix == "ccl_zero") { input_ccl = 0; pack(); }
  else if (suffix == "charge_fet") { pic[15] = 1; status(); }
  else if (suffix == "protection") { pic[9] = 2; status(); }
  else if (suffix == "unknown") { pic[16] = 0x80; status(); }
  else if (suffix == "master_off") live_bench_output_enabled.state = 0;
  else if (suffix == "unsaved") control_intent_saved = false;
  else if (suffix == "high_cell") { trial_max_cell = 3.56f; fresh(); }
  else if (suffix == "cold") { trial_temperature = 4; fresh(); }
  else if (suffix == "hot") { trial_temperature = 46; fresh(); }
  else if (suffix == "discharge") { input_current = -3.1f; pack(); }
  else if (suffix == "qualification") { input_current = 20; pack(); }
  else if (suffix == "balancing_at_expiry") {
    pic[6] = 8; status();
    run_eoc(original_attempt, backstop);
    assert(yambms1_var_eoc == backstop);
    std::cout << "PASS " << scenario << "\n";
    return 0;
  }
  else if (suffix.rfind("stale_", 0) == 0) {
    now_ms += 31000;
    if (suffix != "stale_pack") pack();
    if (suffix != "stale_cells") cells();
    if (suffix != "stale_pic") status();
  } else if (suffix == "normal" || suffix == "warning" || suffix == "accepted_status") {
    if (suffix == "warning") pic[9] = 1;
    if (suffix == "accepted_status") { pic[15] = 0x43; pic[16] = 0x20; }
    status();
    run_eoc(original_attempt, backstop);
    assert(yambms1_var_eoc && yambms1_var_charge_status == "Float");
    std::cout << "PASS " << scenario << "\n";
    return 0;
  } else assert(false);
  // Deliberately do NOT refresh merged entities before the delayed callback.
  run_eoc(original_attempt, backstop);
  assert_cancelled();
#else
  const auto suffix = scenario.substr(backstop ? 9 : 6);
  if (suffix == "normal" || scenario == "float_disabled" || scenario == "no_shunt_guarantee") {
    if (scenario == "float_disabled") yambms1_switch_float.state = 0;
    if (scenario == "no_shunt_guarantee") bms1_total_voltage.state = 54.7f;
    now_ms = deadline; tick();
    assert(yambms1_var_eoc && yambms1_var_eoc_timestamp > previous_completion);
    assert(yambms1_var_charge_status == (scenario == "float_disabled" ? "EOC" : "Float"));
  } else if (suffix == "cancel_recovery" || scenario == "old_callback") {
    yambms1_switch_charging.state = 0;
    now_ms += 1000; tick(); assert_cancelled();
    yambms1_switch_charging.state = 1;
    now_ms += 2000; tick();
    assert(!yambms1_var_finish_active);
    now_ms += 10001; tick();
    assert(yambms1_var_finish_attempt != original_attempt);
    const auto next_deadline = backstop ? yambms1_script_eoc_timer.deadline : yambms1_script_cutoff_timer.deadline;
    run_eoc(original_attempt, backstop);
    assert(!yambms1_var_eoc && yambms1_var_finish_active);
    now_ms = deadline; tick(); assert(!yambms1_var_eoc);
    now_ms = next_deadline; tick(); assert(yambms1_var_eoc);
  } else if (scenario == "balancer_restarts_short") {
    bms1_equalizing.state = 1;
    now_ms += 1000; tick();
    assert(!yambms1_var_cutoff_timer_armed);
    now_ms += 1000; bms1_equalizing.state = 0; tick();
    const auto next_deadline = yambms1_script_cutoff_timer.deadline;
    now_ms = deadline; run_eoc(original_attempt, false);
    assert(!yambms1_var_eoc);
    now_ms = next_deadline; tick(); assert(yambms1_var_eoc);
  } else if (scenario == "backstop_disabled") {
    yambms1_switch_eoc_timer.state = 0;
    now_ms += 1000; tick();
    assert(!yambms1_script_eoc_timer.is_running());
    now_ms = deadline; tick(); assert(!yambms1_var_eoc);
  } else if (scenario == "bulk_balancing_status") {
    bms1_current.state = 20;
    bms1_equalizing.state = 1;
    now_ms += 1000; tick(); assert_cancelled();
    now_ms += 10001; tick();
    assert(yambms1_var_charge_status == "Balancing");
  } else if (suffix == "limit_zero_at_expiry") {
    yambms1_max_requested_charge_current.state = 0;
    now_ms = deadline; tick(); assert_cancelled();
  } else if (scenario == "backstop_disabled_at_expiry" || scenario == "short_balancing_at_expiry") {
    now_ms = deadline;
    if (backstop) yambms1_switch_eoc_timer.state = 0;
    else yambms1_equalizing.state = 1;
    run_eoc(original_attempt, backstop); assert(!yambms1_var_eoc);
  } else if (scenario == "duplicate_callback") {
    now_ms = deadline; tick(); assert(yambms1_var_eoc);
    const auto stamp = yambms1_var_eoc_timestamp;
    now_ms += 5000; run_eoc(original_attempt, false);
    assert(yambms1_var_eoc_timestamp == stamp);
  } else if (scenario == "attempt_wrap") {
    run_finish_cancel();
    yambms1_var_finish_attempt = 2147483647;
    now_ms += 10001; tick();
    assert(yambms1_var_finish_attempt == 1);
    now_ms = yambms1_script_cutoff_timer.deadline; tick(); assert(yambms1_var_eoc);
  } else if (scenario == "millis_wrap") {
    run_finish_cancel();
    now_ms = UINT32_MAX - 20000;
    run_finish_cancel(); tick();
    now_ms += 10001; tick();
    now_ms += 600000; tick(); assert(yambms1_var_eoc);
  } else {
    now_ms += 1000;
    if (suffix == "charge_off" || suffix == "exact_expiry" || scenario == "timestamp_preserved")
      yambms1_switch_charging.state = 0;
    else if (suffix == "permission") bms1_charging_allowed.state = 0;
    else if (suffix == "offline") bms1_online_status.state = 0;
    else if (suffix == "discharge") bms1_current.state = -3.1f;
    else if (suffix == "zero_limit") bms1_max_charge_current.state = 0;
    else if (suffix == "invalid") bms1_current.state = NAN;
    else if (suffix == "qualification") bms1_max_cell_voltage.state = 3.30f;
    else if (suffix == "guard") yambms1_var_finish_source_guard = [](bool) { return false; };
    else if (suffix == "force_float") yambms1_var_force_float = true;
    else assert(false);
    if (suffix == "exact_expiry") now_ms = deadline;
    tick(); assert_cancelled();
    now_ms = deadline + 1000; tick(); assert_cancelled();
  }
#endif
  std::cout << "PASS " << scenario << " completion=" << yambms1_var_eoc
            << " outcome=" << yambms1_var_finish_outcome << "\n";
}
