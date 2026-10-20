// Recorded electrical inputs remain fixed in both branches: this is not a cell model.
struct RecordedPoint {
  uint32_t ms;
  float voltage, current, capacity, remaining, soc, soh;
  unsigned ccl, dcl, cycles;
  std::array<float, 6> temperatures;
  std::array<unsigned, 18> flags;
  std::array<unsigned, 16> cells_mv;
};

void inject_words(const std::vector<uint16_t> &words) {
  std::vector<uint8_t> bytes;
  for (auto word : words) {
    bytes.push_back(word >> 8);
    bytes.push_back(word & 255);
  }
  inject(4, bytes);
}

void receive_record(const RecordedPoint &point) {
  std::vector<uint16_t> words(17);
  words[0] = std::lround(point.voltage * 100);
  words[1] = int16_t(std::lround(point.current * 100));
  words[2] = std::lround(point.remaining * 100);
  words[3] = std::lround(point.capacity * 100);
  words[5] = std::lround(point.soc * 10);
  words[6] = std::lround(point.soh * 10);
  words[7] = point.cycles;
  words[10] = *std::max_element(point.cells_mv.begin(), point.cells_mv.end());
  words[11] = *std::min_element(point.cells_mv.begin(), point.cells_mv.end());
  words[12] = std::lround((*std::max_element(point.temperatures.begin(), point.temperatures.begin()+4) + 273.15f) * 10);
  words[13] = std::lround((*std::min_element(point.temperatures.begin(), point.temperatures.begin()+4) + 273.15f) * 10);
  words[15] = point.dcl;
  words[16] = point.ccl;
  inject_words(words);
  words.assign(26, 0);
  std::copy(point.cells_mv.begin(), point.cells_mv.end(), words.begin());
  for (unsigned i = 0; i < point.temperatures.size(); ++i) {
    const unsigned offset = i < 4 ? 16+i : 24+i-4;
    words[offset] = std::lround((point.temperatures[i] + 273.15f) * 10);
  }
  inject_words(words);
  for (unsigned i = 0; i < pic.size(); ++i) pic[i] = point.flags[i];
  status();
  assert(seplos_v3.metadata_ready());
  assert(std::abs(raw_voltage.state - point.voltage) < 0.011f);
  assert(std::abs(raw_current.state - point.current) < 0.011f);
  assert(raw_ccl.state == point.ccl && raw_dcl.state == point.dcl);
  for (unsigned i = 0; i < point.cells_mv.size(); ++i)
    assert(std::abs(trial_metadata[9+i].state * 1000 - point.cells_mv[i]) < 0.1f);
  for (unsigned i = 0; i < point.temperatures.size(); ++i)
    assert(std::abs(trial_metadata[25+i].state - point.temperatures[i]) < 0.11f);
  bms1_current.raw_state = raw_current.raw_state;
  bms1_max_cell_voltage.raw_state = trial_metadata[6].raw_state;
}

void replay_merge() {
  bms1_total_voltage.state = raw_voltage.state;
  bms1_current.state = raw_current.state;
  bms1_power.state = raw_voltage.state * raw_current.state;
  bms1_battery_capacity_remaining.state = trial_metadata[0].state;
  bms1_battery_capacity.state = trial_metadata[1].state;
  bms1_battery_soc.state = trial_metadata[2].state;
  bms1_battery_soh.state = trial_metadata[3].state;
  bms1_charging_cycles.state = trial_metadata[4].state;
  bms1_min_cell_voltage.state = trial_metadata[5].state;
  bms1_max_cell_voltage.state = trial_metadata[6].state;
  bms1_min_temperature.state = trial_metadata[7].state;
  bms1_max_temperature.state = trial_metadata[8].state;
  bms1_online_status.state = run_adapter_online();
  bms1_charging_allowed.state = run_adapter_charge();
  bms1_discharging_allowed.state = run_adapter_discharge();
  bms1_bms_equalizing.state = run_adapter_equalizing();
  bms1_equalizing.state = run_merged_equalizing();
  bms1_yambms_errors_bitmask.state = run_adapter_errors();
  bms1_max_charge_current.state = run_adapter_ccl();
  bms1_max_discharge_current.state = run_adapter_dcl();
  bms1_can_be_combined.state = run_eligible();
  run_merge();
}

int main(int argc, char **argv) {
  assert(argc == 3);
  const std::string mode = argv[1];
  const bool auto_on = mode.rfind("on_", 0) == 0;
  const bool core_first = mode.find("core_first") != std::string::npos;
  std::ifstream input(argv[2]);
  assert(input.good());
  std::vector<RecordedPoint> points;
  RecordedPoint point;
  while (input >> point.ms >> point.voltage >> point.current >> point.ccl >> point.dcl
               >> point.capacity >> point.remaining >> point.soc >> point.soh >> point.cycles) {
    for (auto &value : point.temperatures) input >> value;
    for (auto &value : point.flags) input >> value;
    for (auto &value : point.cells_mv) input >> value;
    assert(!input.fail());
    points.push_back(point);
  }
  assert(input.eof() && !points.empty());
  fixture();
  seplos_v3.set_status_profile("v3_sp18b_fw15_balancing");
  run_finish_source_init();
  yambms1_max_requested_charge_current.state = 50;
  yambms1_max_requested_discharge_current.state = 100;
  yambms1_switch_auto_ccl.state = auto_on;
  yambms1_switch_auto_cvl.state = 1;
  yambms1_auto_cvl_kp.state = 0.05;
  yambms1_auto_cvl_ki.state = 0.5;
  yambms1_auto_cvl_boost_charge_v.state = 0;
  yambms1_switch_eoc_timer.state = 1;
  yambms1_var_auto_ccl_functions_count = 1;
  yambms1_var_charge_current_step = 1;
  yambms1_var_discharge_current_step = 1;
  yambms1_var_eoc_timestamp = previous_completion;
  yambms1_script_cutoff_timer.delay_ms = 600000;
  yambms1_script_eoc_timer.delay_ms = 1800000;
  yambms1_script_eoc_timer.callback = [](int attempt, bool) { run_eoc(attempt, true); };
  yambms1_script_cutoff_timer.callback = [](int attempt, bool) { run_eoc(attempt, false); };
  size_t index = 0;
  bool received = false;
  const uint32_t start = points.front().ms;
  std::cout << "ms,native_ccl_a,protected_ccl_a,base_ccl_a,auto_delta_a,requested_ccl_a,requested_cvl_v,current_a,high_cell_v,low_cell_v,finish_allowed,finish_active,attempt,stage,eoc,short_elapsed_ms,backstop_elapsed_ms,equalizing\n";
  for (uint32_t t = start; t <= points.back().ms + 1000; t += 1000) {
    now_ms = t;
    while (index < points.size() && points[index].ms <= t) {
      receive_record(points[index++]);
      received = true;
    }
    if (!received) continue;
    replay_merge();
    if (!core_first) { run_auto_ccl(); run_auto_cvl(); }
    run_core();
    yambms1_requested_charge_voltage.state = run_filter_cvl(yambms1_requested_charge_voltage.state);
    yambms1_requested_charge_current.state = run_filter_ccl(yambms1_requested_charge_current.state);
    yambms1_requested_discharge_current.state = run_filter_dcl(yambms1_requested_discharge_current.state);
    yambms1_script_eoc_timer.poll();
    yambms1_script_cutoff_timer.poll();
    // Snapshot before preparing automatic reductions for the next core cycle.
    std::cout << std::fixed << std::setprecision(4) << t << ',' << points[index-1].ccl << ','
      << seplos_v3.operating_limits().charge_limit << ',' << yambms1_max_charge_current.state << ','
      << yambms1_var_auto_ccl << ',' << yambms1_requested_charge_current.state << ','
      << yambms1_requested_charge_voltage.state << ',' << yambms1_current.state << ','
      << yambms1_max_cell_voltage.state << ',' << yambms1_min_cell_voltage.state << ','
      << yambms1_var_finish_allowed << ',' << yambms1_var_finish_active << ','
      << yambms1_var_finish_attempt << ',' << yambms1_var_charge_status << ',' << yambms1_var_eoc << ','
      << (yambms1_script_cutoff_timer.is_running() ? t-yambms1_var_cutoff_timer_started : 0) << ','
      << (yambms1_script_eoc_timer.is_running() ? t-yambms1_var_eoc_timer_started : 0) << ','
      << yambms1_equalizing.state << '\n';
    if (core_first) { run_auto_ccl(); run_auto_cvl(); }
  }
}
