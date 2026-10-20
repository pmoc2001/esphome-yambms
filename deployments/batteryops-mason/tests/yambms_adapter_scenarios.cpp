esphome::sensor::Sensor raw_voltage, raw_current, raw_ccl, raw_dcl;
std::vector<uint8_t> pic(18, 0);
uint16_t input_ccl = 180, input_dcl = 180;
float input_current = 20, input_voltage = 54;
#ifdef OPERATING_MODE
constexpr float normal_charge = 35, normal_discharge = 100;
#else
constexpr float normal_charge = 32, normal_discharge = 50;
#endif
#ifdef HANDOVER_MODE
std::array<esphome::sensor::Sensor, 31> trial_metadata;
float trial_temperature = 21, trial_min_cell = 3.36f, trial_max_cell = 3.38f;
#endif

void inject(uint8_t function, std::vector<uint8_t> data, bool corrupt = false) {
#ifdef FINISH_SESSION_MODE
  const uint16_t reg = function == 1 ? 0x1200 : data.size() == 34 ? 0x1000 : 0x1100;
  const uint16_t count = function == 1 ? 144 : data.size() / 2;
  std::deque<uint8_t> request{0, function, uint8_t(reg >> 8), uint8_t(reg), uint8_t(count >> 8), uint8_t(count)};
  const uint16_t request_crc = seplos_v3.calculate_modbus_crc(request, request.size());
  request.push_back(request_crc & 255); request.push_back(request_crc >> 8);
  seplos_v3.incoming.insert(seplos_v3.incoming.end(), request.begin(), request.end());
  while (seplos_v3.available()) seplos_v3.loop();
#endif
  std::deque<uint8_t> frame{0, function, uint8_t(data.size())};
  frame.insert(frame.end(), data.begin(), data.end());
  uint16_t crc = 0xffff;
  for (auto byte : frame) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
  }
  frame.push_back(crc & 255); frame.push_back((crc >> 8) ^ (corrupt ? 1 : 0));
  seplos_v3.incoming.insert(seplos_v3.incoming.end(), frame.begin(), frame.end());
  while (seplos_v3.available()) seplos_v3.loop();
}
void pack() {
  std::vector<uint16_t> words(17);
  words[0] = std::lround(input_voltage * 100);
  words[1] = int16_t(std::lround(input_current * 100));
  words[15] = input_dcl; words[16] = input_ccl;
#ifdef HANDOVER_MODE
  words[2] = 25200; words[3] = 28000; words[5] = 900; words[6] = 1000; words[7] = 35;
  words[10] = std::lround(trial_max_cell * 1000);
  words[11] = std::lround(trial_min_cell * 1000);
  words[12] = words[13] = std::lround((trial_temperature + 273.15f) * 10);
#endif
  std::vector<uint8_t> data;
  for (auto word : words) { data.push_back(word >> 8); data.push_back(word & 255); }
  inject(4, data);
#ifdef FINISH_SESSION_MODE
  bms1_current.raw_state = raw_current.raw_state;
  bms1_max_cell_voltage.raw_state = trial_metadata[6].raw_state;
#endif
}
void cells() {
  std::vector<uint8_t> data(52);
#ifdef HANDOVER_MODE
  for (unsigned i = 0; i < 26; ++i) {
    uint16_t word = i < 16 ? 3375 : std::lround((trial_temperature + 273.15f) * 10);
    if (i == 0) word = std::lround(trial_min_cell * 1000);
    if (i == 1) word = std::lround(trial_max_cell * 1000);
    data[2*i] = word >> 8; data[2*i+1] = word & 255;
  }
#endif
  inject(4, data);
}
void status(bool corrupt = false) { inject(1, pic, corrupt); }
void fresh() { pack(); cells(); status(); }
void fixture() {
  yambms1_script_finish_check.callback = [](int, bool) { run_finish_check(); };
  yambms1_script_finish_cancel.callback = [](int, bool) { run_finish_cancel(); };
  seplos_v3.set_bms_count(1); seplos_v3.set_address_base(0);
#ifdef FINISH_SESSION_MODE
  seplos_v3.set_transaction_tracking(true);
#endif
  raw_voltage.name = "bms0 pack_voltage"; raw_current.name = "bms0 current";
  raw_ccl.name = "bms0 maxchgcurt"; raw_dcl.name = "bms0 maxdiscurt";
  for (auto sensor : {&raw_voltage, &raw_current, &raw_ccl, &raw_dcl}) seplos_v3.register_sensor(sensor);
#ifdef HANDOVER_MODE
  const char *names[] = {"remaining_capacity","total_capacity","soc","soh","cycle_count",
    "min_cell_voltage","max_cell_voltage","min_cell_temp","max_cell_temp",
    "cell_1","cell_2","cell_3","cell_4","cell_5","cell_6","cell_7","cell_8",
    "cell_9","cell_10","cell_11","cell_12","cell_13","cell_14","cell_15","cell_16",
    "cell_temp_1","cell_temp_2","cell_temp_3","cell_temp_4","case_temp","power_temp"};
  for (unsigned i = 0; i < trial_metadata.size(); ++i) {
    trial_metadata[i].name = std::string("bms0 ") + names[i];
    seplos_v3.register_sensor(&trial_metadata[i]);
  }
  live_bench_output_enabled.state = 1;
#ifdef RESTART_MODE
  control_intent_saved = true;
#endif
#endif
  seplos_v3.setup(); pic[15] = 3;
  yambms1_var_bms_counter = 1;
  yambms1_cell_count.state = 16; yambms1_bulk_voltage.state = 55.2;
  yambms1_float_voltage.state = 53.6; yambms1_rebulk_voltage.state = 52.8;
  yambms1_rebulk_soc.state = 85; yambms1_var_cell_min_charge_v = 3.45;
  yambms1_var_cell_max_charge_v = 3.6;
#ifdef HANDOVER_MODE
  yambms1_var_cell_min_charge_v = 3.37;
  yambms1_var_cell_max_charge_v = 3.65;
#endif
  yambms1_switch_charging.state = yambms1_switch_discharging.state = 1;
  yambms1_switch_float.state = 1;
  yambms1_max_requested_charge_current.state = 35;
  yambms1_max_requested_discharge_current.state = 85;
  // Non-tested metadata is supplied directly; decoded PIC and limits are not.
  bms1_power.state = 1080; bms1_battery_soc.state = 90;
  bms1_battery_soh.state = 100; bms1_battery_capacity.state = 280;
  bms1_battery_capacity_remaining.state = 252;
  bms1_min_cell_voltage.state = 3.36; bms1_max_cell_voltage.state = 3.38;
  bms1_min_temperature.state = 20; bms1_max_temperature.state = 22;
  bms1_cell_ovp.state = 3.66; bms1_cell_uvp.state = 2.7;
  bms1_balance_trigger_voltage.state = 0.05;
}
void tick() {
  bms1_total_voltage.state = raw_voltage.state; bms1_current.state = raw_current.state;
#ifdef HANDOVER_MODE
  bms1_min_cell_voltage.state = trial_min_cell; bms1_max_cell_voltage.state = trial_max_cell;
  bms1_min_temperature.state = bms1_max_temperature.state = trial_temperature;
#ifdef OPERATING_MODE
  yambms1_max_requested_discharge_current.state = 100;
#else
  yambms1_max_requested_discharge_current.state = 56;
#endif
#endif
#ifndef HANDOVER_MODE
  bms1_reported_ccl.state = raw_ccl.state; bms1_reported_dcl.state = raw_dcl.state;
#endif
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
  yambms1_var_charge_current_step = yambms1_var_auto_ccl_functions_count + 1;
  yambms1_var_discharge_current_step = yambms1_var_auto_dcl_functions_count + 1;
  run_core();
#ifdef HANDOVER_MODE
  yambms1_requested_charge_voltage.state = run_filter_cvl(yambms1_requested_charge_voltage.state);
  yambms1_requested_charge_current.state = run_filter_ccl(yambms1_requested_charge_current.state);
  yambms1_requested_discharge_current.state = run_filter_dcl(yambms1_requested_discharge_current.state);
#endif
}
void stopped() {
  assert(yambms1_requested_charge_current.state == 0);
  assert(yambms1_requested_discharge_current.state == 0);
}
int main(int argc, char **argv) {
  assert(argc == 2); std::string scenario = argv[1]; fixture();
  const bool compatible = scenario.rfind("v3_fet_extension", 0) == 0 || scenario.rfind("trial_",0) == 0 || scenario.rfind("restart_",0) == 0 || scenario.rfind("operating_",0) == 0;
  if (compatible) {
#ifdef BALANCE_STATUS_MODE
    seplos_v3.set_status_profile("v3_sp18b_fw15_balancing");
    pic[6] = 8; pic[16] = 0x21;
#else
    seplos_v3.set_status_profile("v3_sp18b_fw15");
#endif
    pic[15] = 0x43;
    const std::string prefix = "v3_fet_extension_";
    const std::string suffix = scenario.substr(std::min(scenario.size(), prefix.size()));
    if (suffix == "protections" || suffix == "startup" || suffix == "crc_rejected" ||
        suffix.rfind("stale_", 0) == 0 || suffix.rfind("zero_", 0) == 0 || suffix == "both_zero")
      scenario = suffix;
  }
  if (scenario == "startup") { tick(); stopped(); assert(!yambms1_var_initialized); }
  else {
    fresh(); tick(); assert(yambms1_var_initialized);
#ifdef HANDOVER_MODE
    assert(seplos_v3.metadata_ready());
#ifdef OPERATING_MODE
    if (scenario.rfind("operating_",0) == 0) {
      assert(yambms1_requested_charge_current.state == 35);
      assert(yambms1_requested_discharge_current.state == 100);
      if (scenario == "operating_bms_reduction") {
        input_dcl = 60; fresh(); tick();
        assert(yambms1_requested_discharge_current.state == 54);
      } else if (scenario == "operating_output_caps") {
        assert(run_filter_ccl(200) == 35);
        assert(run_filter_dcl(200) == 100);
        assert(run_filter_dcl(NAN) == 0);
      } else if (scenario == "operating_discharge_cold") {
        trial_temperature = -1; fresh(); tick(); stopped();
      } else if (scenario == "operating_invalid_temperature") {
        trial_metadata[25].state = NAN; tick(); stopped();
        assert(!bms1_can_be_combined.state);
      } else if (scenario == "operating_legacy_caps") {
        const auto legacy = seplos_v3.supervised_trial_limits();
        assert(legacy.charge_limit == 35 && legacy.discharge_limit == 56);
      } else if (scenario.rfind("operating_temperature_",0) == 0) {
        yambms1_switch_temperature_based_current_limitation.state = 1;
        yambms1_var_auto_dcl_functions_count = 1;
        yambms1_var_discharge_current_step = 1;
        yambms1_battery_capacity.state = 280;
        yambms1_max_discharge_current.state = 100;
        const float temperature = scenario == "operating_temperature_cool" ? 10 :
                                  scenario == "operating_temperature_hot" ? 55 : 20;
        yambms1_min_temperature.state = yambms1_max_temperature.state = temperature;
        if (scenario == "operating_temperature_missing") yambms1_min_temperature.state = NAN;
        run_temperature_dcl();
        const float delta = yambms1_var_auto_temperature_dcl;
        if (scenario == "operating_temperature_cool") assert(std::abs(delta + 44) < 0.01);
        else if (scenario == "operating_temperature_hot") assert(delta == -100);
        else assert(delta == 0);
        yambms1_var_discharge_current_step = 2;
        run_core();
        yambms1_requested_discharge_current.state = run_filter_dcl(yambms1_requested_discharge_current.state);
        const float expected = scenario == "operating_temperature_cool" ? 56 :
                               scenario == "operating_temperature_hot" ? 0 : 100;
        assert(yambms1_requested_discharge_current.state == expected);
      } else { assert(scenario == "operating_normal"); }
      std::cout << "PASS operating source caps/protections: " << scenario << "\n";
      return 0;
    }
#endif
#ifdef RESTART_MODE
    if (scenario.rfind("restart_",0) == 0) {
      if (scenario == "restart_not_saved" || scenario == "restart_return") {
        control_intent_saved = false; tick(); stopped();
        assert(!bms1_can_be_combined.state);
      } else if (scenario == "restart_manual_stop") {
        live_bench_output_enabled.state = 0; tick(); stopped();
        assert(!bms1_can_be_combined.state);
      } else if (scenario == "restart_commit_ok") {
        control_intent_saved = false; run_commit();
        assert(control_intent_saved && test_preferences.calls == 1);
      } else if (scenario == "restart_commit_failure") {
        test_preferences.succeeds = false; run_commit(); tick(); stopped();
        assert(!control_intent_saved && !live_bench_output_enabled.state);
      } else { assert(false); }
      if (scenario == "restart_return") {
        control_intent_saved = true; fresh(); tick();
        assert(bms1_can_be_combined.state);
        assert(yambms1_requested_charge_current.state == normal_charge);
        assert(yambms1_requested_discharge_current.state == normal_discharge);
      }
      std::cout << "PASS restart-safe source admission: " << scenario << "\n";
      return 0;
    }
#endif
    if (scenario.rfind("trial_",0) == 0) {
      if (scenario == "trial_cold" || scenario == "trial_recovery") trial_temperature = 5;
      if (scenario == "trial_hot") trial_temperature = 45;
      if (scenario == "trial_derating") trial_temperature = 10;
      if (scenario == "trial_cell_high") trial_max_cell = 3.55f;
      if (scenario == "trial_cell_low") trial_min_cell = 2.90f;
      fresh();
      if (scenario == "trial_invalid_metadata") trial_metadata[9].state = NAN;
      if (scenario == "trial_zero_charge_switch") yambms1_switch_charging.state = 0;
      if (scenario == "trial_zero_discharge_switch") yambms1_switch_discharging.state = 0;
      tick();
      if (scenario == "trial_derating") assert(yambms1_requested_charge_current.state <= 28);
      else if (scenario != "trial_cell_low" && scenario != "trial_zero_discharge_switch")
        assert(yambms1_requested_charge_current.state == 0);
      if (scenario == "trial_hot" || scenario == "trial_cell_low" ||
          scenario == "trial_zero_discharge_switch" || scenario == "trial_invalid_metadata")
        assert(yambms1_requested_discharge_current.state == 0);
      if (scenario == "trial_invalid_metadata") assert(!yambms1_var_initialized);
      if (scenario == "trial_recovery") {
        trial_temperature = 21; fresh(); tick();
        assert(yambms1_requested_charge_current.state == normal_charge);
      }
      std::cout << "PASS handover source -> real parser metadata -> YamBMS: " << scenario << "\n";
      return 0;
    }
#endif
    if (scenario == "v3_fet_extension_protection") {
      pic[9] = 2; status(); tick(); stopped();
      assert(yambms1_errors_bitmask_alarm.state == 2);
      assert(seplos_v3.raw_pic().substr(30,2) == "43");
    } else if (scenario == "v3_fet_extension_other_unknown") {
      pic[15] |= 0x80; status(); tick(); stopped();
      assert(yambms1_errors_bitmask_alarm.state == 0x0800);
    } else if (scenario == "v3_fet_extension_charge_off") {
      pic[15] = 0x41; status(); tick();
      assert(yambms1_requested_charge_current.state == 0);
      assert(yambms1_requested_discharge_current.state > 0);
    } else if (scenario == "v3_fet_extension") {
#ifdef HANDOVER_MODE
      assert(yambms1_requested_charge_current.state == normal_charge);
      assert(yambms1_requested_discharge_current.state == normal_discharge);
#else
      assert(yambms1_requested_charge_current.state == 35);
#endif
      assert(yambms1_requested_discharge_current.state > 0);
      assert(yambms1_errors_bitmask_alarm.state == 0);
#ifdef BALANCE_STATUS_MODE
      assert(seplos_v3.pic_status().reserved_flags() == 2);
      assert(seplos_v3.raw_pic().substr(32,2) == "21");
#else
      assert(seplos_v3.pic_status().reserved_flags() == 1);
#endif
      assert(seplos_v3.blocking_unknown_flags() == 0);
      assert(seplos_v3.raw_pic().substr(30,2) == "43");
    } else if (scenario == "zero_ccl" || scenario == "zero_dcl" || scenario == "both_zero") {
      if (scenario != "zero_dcl") input_ccl = 0;
      if (scenario != "zero_ccl") input_dcl = 0;
      pack(); tick(); assert(yambms1_bms_combined.state == 1);
      if (input_ccl == 0) assert(yambms1_requested_charge_current.state == 0);
      else assert(yambms1_requested_charge_current.state > 0);
      if (input_dcl == 0) assert(yambms1_requested_discharge_current.state == 0);
      else assert(yambms1_requested_discharge_current.state > 0);
    } else if (scenario == "charge_off" || scenario == "discharge_off") {
      pic[15] = scenario == "charge_off" ? 1 : 2; status(); tick();
      if (scenario == "charge_off") {
        assert(yambms1_requested_charge_current.state == 0);
        assert(yambms1_requested_discharge_current.state > 0);
      } else {
        assert(yambms1_requested_discharge_current.state == 0);
        assert(yambms1_requested_charge_current.state > 0);
      }
    } else if (scenario == "protections") {
      const unsigned coils[][2] = {
        {0x49,2},{0x4b,4},{0x4d,2},{0x4f,4},{0x51,0x20},{0x53,0x40},
        {0x55,8},{0x57,0x10},{0x59,8},{0x5b,0x10},{0x5d,8},
        {0x61,0x100},{0x62,0x100},{0x64,0x80},{0x65,0x80},{0x66,0x400},
        {0x68,0x400},{0x69,0x800},{0x6a,0x100},{0x6b,0x80},{0x73,0x800},{0x76,0x800},
        {0x88,0x800},{0x89,0x800},{0x8a,0x800},{0x8b,0x800},
        {0x8c,0x800},{0x8d,0x800},{0x8e,0x800},{0x8f,0x800}
      };
      for (const auto &entry : coils) {
        auto coil = entry[0];
        pic.assign(18, 0); pic[15] = compatible ? 0x43 : 3; pic[coil / 8] |= 1 << (coil % 8);
#ifdef BALANCE_STATUS_MODE
        pic[6] = 8; pic[16] |= 0x21;
#endif
        status(); tick(); stopped();
        assert(yambms1_errors_bitmask_alarm.state == entry[1]);
      }
    } else if (scenario == "warning_only") {
      pic[9] = 1; status(); tick();
      assert(yambms1_requested_charge_current.state > 0);
      assert(yambms1_errors_bitmask_alarm.state == 0);
    } else if (scenario == "reserved") {
      pic[15] = 0x43; status(); tick(); stopped();
      assert(bms1_yambms_errors_bitmask.state == 0x0800);
      pic[9] = 2; status(); tick(); stopped();
      assert(bms1_yambms_errors_bitmask.state == 0x0802);
    } else if (scenario == "balancing" || scenario == "temperature_inhibit") {
      pic[6] = 1; pic[7] = 128;
      if (scenario == "temperature_inhibit") pic[16] = 8;
      status(); tick();
      assert(bool(yambms1_equalizing.state) == (scenario == "balancing"));
    } else if (scenario == "crc_rejected") {
      now_ms = 30000; status(true); pack(); cells(); tick(); stopped();
      assert(!bms1_online_status.state && !yambms1_var_initialized);
    } else if (scenario.rfind("stale_", 0) == 0) {
      now_ms = 30000;
      if (scenario != "stale_pack") pack();
      if (scenario != "stale_cells") cells();
      if (scenario != "stale_pic") status();
      tick(); stopped(); assert(!yambms1_var_initialized);
      fresh(); tick(); assert(yambms1_requested_charge_current.state > 0);
    } else { assert(yambms1_requested_charge_current.state == 35); }
  }
  std::cout << "PASS raw parser -> adapter -> YamBMS: " << argv[1]
            << " CCL=" << yambms1_requested_charge_current.state
            << " DCL=" << yambms1_requested_discharge_current.state << "\n";
}
