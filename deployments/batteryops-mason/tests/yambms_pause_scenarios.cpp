// Included after the original finishing fixture and its actual-source functions.
void pause_sample(float current, bool balancing) {
#ifdef OPERATING_MODE
  input_current = current;
  pic[6] = balancing ? 8 : 0;
  fresh();
#else
  bms1_current.state = current;
  bms1_equalizing.state = balancing;
  // Core-only tests have no deployment hook; inject the fresh merged reading.
  yambms1_current.state = current;
#endif
}

void pause_tick() {
  yambms1_script_eoc_timer.poll();
  yambms1_script_cutoff_timer.poll();
  tick();
}

void advance(uint32_t duration, float current = 3, bool balancing = false) {
  while (duration) {
    const uint32_t dt = std::min(duration, uint32_t(1000));
    now_ms += dt;
    pause_sample(current, balancing);
    pause_tick();
    duration -= dt;
  }
}

void assert_paused(int attempt) {
  assert(!yambms1_var_eoc && yambms1_var_finish_active);
  assert(yambms1_var_finish_paused);
  assert(yambms1_var_finish_attempt == attempt);
  assert(yambms1_var_eoc_timestamp == previous_completion);
}

void assert_completed() {
  assert(yambms1_var_eoc && yambms1_var_charge_status == "Float");
  assert(yambms1_var_eoc_timestamp > previous_completion);
}

int main(int argc, char **argv) {
  assert(argc >= 2);
  const std::string name = argv[1];
  const bool backstop = name.find("backstop") != std::string::npos;
  finish_fixture(backstop);
  const int attempt = yambms1_var_finish_attempt;
  const uint32_t started = now_ms;
  const uint32_t duration = backstop ? 1800000 : 600000;
  if (name == "short_pause_resume" || name == "backstop_pause_resume") {
    advance(120000, 3, backstop);
    advance(20000, -1, backstop);
    assert_paused(attempt);
    advance(duration - 140000, 3, backstop);
    assert(!yambms1_var_eoc);
    advance(22000, 3, backstop);
    assert_completed();
  } else if (name == "negative_at_deadline") {
    advance(599000);
    advance(1000, -1);
    assert_paused(attempt);
    advance(19000, -1);
    assert_paused(attempt);
    advance(1000, 3);
    assert(!yambms1_var_eoc);
    advance(2000, 3);
    assert_completed();
  } else if (name == "exact_three_amps" || name == "exact_sixty_seconds") {
    advance(name == "exact_three_amps" ? 1000 : 59000, -3);
    assert_paused(attempt);
    advance(1000, 3);
    assert(yambms1_var_finish_active);
    assert(yambms1_var_finish_reverse_amp_ms <= 180000.0);
    advance(601000);
    assert_completed();
  } else if (name == "over_three_amps") {
    advance(1000, -3.01f);
    assert_cancelled();
    assert(yambms1_var_finish_outcome.find("3 A") != std::string::npos);
  } else if (name == "over_sixty_seconds") {
    advance(60000, -1);
    assert_paused(attempt);
    advance(1000, -1);
    assert_cancelled();
    assert(yambms1_var_finish_outcome.find("60 seconds") != std::string::npos);
  } else if (name == "cumulative_budget") {
    for (int n = 0; n < 2; ++n) { advance(20000, -3); advance(10000, 3); }
    advance(19000, -3);
    assert_cancelled();
    assert(yambms1_var_finish_outcome.find("0.05 Ah") != std::string::npos);
  } else if (name == "duplicate_checks") {
    advance(1000, -1);
    const double budget = yambms1_var_finish_reverse_amp_ms;
    const auto paused = yambms1_var_finish_pause_ms;
    const auto timer_start = yambms1_var_cutoff_timer_started;
    for (int n = 0; n < 10; ++n) { run_finish_check(); run_eoc(attempt, false); }
    assert(yambms1_var_finish_reverse_amp_ms == budget);
    assert(yambms1_var_finish_pause_ms == paused);
    assert(yambms1_var_cutoff_timer_started == timer_start);
    assert_paused(attempt);
  } else if (name == "no_start_while_discharging") {
    run_finish_cancel();
    advance(20000, -1);
    assert_cancelled();
  } else if (name == "low_voltage_during_pause") {
#ifdef OPERATING_MODE
    trial_max_cell = 3.42f;
#else
    bms1_max_cell_voltage.state = 3.42f;
#endif
    advance(1000, -1);
    assert_cancelled();
  } else if (name == "balancing_stays_on") {
    yambms1_switch_eoc_timer.state = 0;
    advance(7200000, 1, true);
    assert(!yambms1_var_eoc && yambms1_var_finish_active);
    assert(!yambms1_var_cutoff_timer_armed);
    assert(yambms1_var_finish_outcome == "Waiting: balancing active");
  } else if (name == "balancing_restarts") {
    advance(500000);
    advance(1000, 3, true);
    assert(!yambms1_var_cutoff_timer_armed);
    advance(1000, 3, false);
    advance(599000);
    assert(!yambms1_var_eoc);
    advance(1000);
    assert_completed();
  } else if (name == "stop_during_pause" || name == "old_callback_after_pause_cancel") {
    advance(10000, -1);
    yambms1_switch_charging.state = 0;
    advance(1000, -1);
    assert_cancelled();
    yambms1_switch_charging.state = 1;
    advance(12000);
    assert(yambms1_var_finish_active && yambms1_var_finish_attempt != attempt);
    run_eoc(attempt, false);
    assert(!yambms1_var_eoc);
  } else if (name == "millis_wrap_pause") {
    run_finish_cancel();
    now_ms = UINT32_MAX - 20000;
    run_finish_cancel();
    pause_sample(3, false); pause_tick();
    advance(11000);
    advance(20000, -1);
    assert(yambms1_var_finish_active && yambms1_var_finish_paused);
    advance(602000);
    assert_completed();
  } else if (name == "repeated_reversals_resume") {
    for (int n = 0; n < 5; ++n) { advance(10000, -1); advance(20000); }
    assert(yambms1_var_finish_attempt == attempt);
    advance(450000);
    assert(!yambms1_var_eoc);
    advance(56000);
    assert_completed();
  } else if (name == "backstop_disabled_during_pause") {
    advance(10000, -1, true);
    yambms1_switch_eoc_timer.state = 0;
    advance(1800000, 1, true);
    assert(!yambms1_var_eoc && !yambms1_var_eoc_timer_armed);
#ifdef OPERATING_MODE
  } else if (name == "raw_negative_before_merge") {
    advance(599000);
    now_ms += 1000;
    input_current = -1; pack();
    assert(yambms1_current.state > 0);
    run_eoc(attempt, false);
    assert_paused(attempt);
    advance(1000, -1);
    advance(4000);
    assert_completed();
  } else if (name == "raw_positive_merged_negative") {
    advance(1000, -1);
    now_ms += 1000;
    input_current = 3; pack();
    assert(yambms1_current.state < 0);
    run_finish_check();
    assert_paused(attempt);
  } else if (name == "raw_charge_stop_during_pause" || name == "raw_stale_during_pause" ||
             name == "raw_high_cell_during_pause" || name == "raw_invalid_current" ||
             name == "raw_voltage_drop_during_pause") {
    advance(10000, -1);
    now_ms += 1000;
    if (name == "raw_charge_stop_during_pause") { input_ccl = 0; pack(); }
    if (name == "raw_stale_during_pause") { now_ms += 31000; cells(); status(); }
    if (name == "raw_high_cell_during_pause") { trial_max_cell = 3.56f; fresh(); }
    if (name == "raw_invalid_current") bms1_current.raw_state = NAN;
    if (name == "raw_voltage_drop_during_pause") { trial_max_cell = 3.42f; fresh(); }
    run_eoc(attempt, false);
    assert_cancelled();
  } else if (name == "raw_balancing_before_expiry") {
    advance(599000);
    now_ms += 1000;
    pic[6] = 8; status();
    run_eoc(attempt, false);
    assert(!yambms1_var_eoc);
#endif
  } else assert(false);
  std::cout << "PASS " << name << " elapsed_ms=" << uint32_t(now_ms - started)
            << " pause_ms=" << yambms1_var_finish_pause_ms
            << " reverse_ah=" << yambms1_var_finish_reverse_amp_ms / 3600000.0
            << " outcome=" << yambms1_var_finish_outcome << "\n";
}
