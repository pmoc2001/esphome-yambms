// Replay observed inputs, not hypothetical cell responses to a different controller.
int main(int argc, char **argv) {
  assert(argc == 3);
  const bool timer_first = std::string(argv[1]) == "recording_timer_first";
  std::ifstream input(argv[2]);
  assert(input.good());
  std::vector<RecordedPoint> points;
  RecordedPoint point;
  while (input >> point.ms >> point.voltage >> point.current >> point.ccl >> point.dcl
               >> point.capacity >> point.remaining >> point.soc >> point.soh >> point.cycles) {
    for (auto &v : point.temperatures) input >> v;
    for (auto &v : point.flags) input >> v;
    for (auto &v : point.cells_mv) input >> v;
    assert(!input.fail());
    points.push_back(point);
  }
  assert(input.eof() && !points.empty());
  fixture();
  seplos_v3.set_status_profile("v3_sp18b_fw15_balancing");
  run_finish_source_init();
  yambms1_switch_eoc_timer.state = 0;
  yambms1_max_requested_charge_current.state = 50;
  yambms1_max_requested_discharge_current.state = 100;
  yambms1_var_eoc_timestamp = previous_completion;
  yambms1_script_cutoff_timer.delay_function = run_cutoff_delay;
  yambms1_script_eoc_timer.delay_function = run_eoc_delay;
  yambms1_script_eoc_timer.callback = [](int attempt, bool) { run_eoc(attempt, true); };
  yambms1_script_cutoff_timer.callback = [](int attempt, bool) { run_eoc(attempt, false); };
  size_t index = 0;
  unsigned paused_seconds = 0, active_seconds = 0, max_qualified_ms = 0;
  unsigned cancelled = 0, old_guard_negative_seconds = 0;
  std::map<std::string, unsigned> cancel_reasons;
  for (uint32_t t = points.front().ms; t <= points.back().ms; t += 1000) {
    now_ms = t;
    while (index < points.size() && points[index].ms <= t) receive_record(points[index++]);
    assert(index > 0);
    replay_merge();
    const bool was_active = yambms1_var_finish_active;
    auto timers = [] {
      yambms1_script_cutoff_timer.poll();
      yambms1_script_eoc_timer.poll();
    };
    if (timer_first) timers();
    run_core();
    if (!timer_first) timers();
    if (was_active && !yambms1_var_finish_active && !yambms1_var_eoc) {
      ++cancelled;
      ++cancel_reasons[yambms1_var_finish_outcome];
    }
    paused_seconds += yambms1_var_finish_active && yambms1_var_finish_paused;
    active_seconds += yambms1_var_finish_active;
    old_guard_negative_seconds += yambms1_current.state < 0;
    if (yambms1_var_cutoff_timer_armed)
      max_qualified_ms = std::max(max_qualified_ms, t - yambms1_var_cutoff_timer_started);
    // This recording has persistent/intermittent balancing, never 600 qualifying seconds.
    assert(!yambms1_var_eoc);
    assert(!yambms1_script_eoc_timer.is_running());
    assert(yambms1_var_eoc_timestamp == previous_completion);
    if (yambms1_var_finish_active) {
      assert(seplos_v3.operating_limits().charge_limit > 0);
      assert(yambms1_var_finish_reverse_ms <= 60000);
      assert(yambms1_var_finish_reverse_amp_ms <= 180000.0);
    }
  }
  std::cout << "records=" << points.size() << " seconds=" << (points.back().ms-points.front().ms)/1000
    << " attempts=" << yambms1_var_finish_attempt << " cancellations=" << cancelled
    << " active_seconds=" << active_seconds << " paused_seconds=" << paused_seconds
    << " old_guard_negative_seconds=" << old_guard_negative_seconds
    << " max_normal_qualified_seconds=" << max_qualified_ms/1000.0
    << " completions=" << yambms1_var_eoc << '\n';
  for (const auto &entry : cancel_reasons)
    std::cout << entry.first << ": " << entry.second << '\n';
  std::cout << "PASS recorded trajectory; final=" << yambms1_var_finish_outcome << '\n';
}
