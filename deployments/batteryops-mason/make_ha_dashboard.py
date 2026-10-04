"""Build native HA monitoring views from verified entity IDs, without controls."""
import json
from pathlib import Path


def build(inventory):
    ids={e['entity_id'] for e in inventory}
    def entity(e):
        assert e in ids,e
        return e
    def row(e,name): return {'entity':entity(e),'name':name}
    def tile(e,name,icon=None):
        result={'type':'tile','entity':entity(e),'name':name,'tap_action':{'action':'more-info'}}
        if icon: result['icon']=icon
        return result
    def group(title,cards):
        return {'type':'grid','cards':[{'type':'heading','heading':title,'heading_style':'title'},*cards]}
    def values(title,entries):
        return {'type':'entities','title':title,'show_header_toggle':False,
                'entities':[row(*item) for item in entries]}
    def history(title,entries,hours=6):
        return {'type':'history-graph','title':title,'hours_to_show':hours,
                'entities':[row(*item) for item in entries]}
    def text(title,e):
        entity(e)
        return {'type':'markdown','title':title,'content':"{{ states('"+e+"') }}"}
    yam='sensor.yambms_gx_trial_'
    raw='sensor.seplos_shadow_bms0_'
    qual='sensor.qualification_'
    overview={'title':'Overview','path':'overview','icon':'mdi:battery-high',
        'type':'sections','max_columns':3,'sections':[
            group('Battery',[
                *([tile('sensor.victron_system_battery_state','Battery Flow','mdi:swap-horizontal')]
                  if 'sensor.victron_system_battery_state' in ids else []),
                tile(yam+'battery_soc','State of Charge','mdi:battery-high'),
                tile(yam+'power','Battery Power','mdi:flash'),
                tile(yam+'total_voltage','Battery Voltage'),tile(yam+'current','Battery Current'),
                history('State of Charge',[(yam+'battery_soc','SOC')]),
                history('Battery Power',[(yam+'power','Battery')])]),
            group('Charging & Permissions',[
                values('Charging State',[(yam+'charging_status','Charging Stage'),
                    (yam+'charging_instruction','Charge Instruction'),
                    (yam+'requested_charge_voltage','Requested Voltage'),
                    (yam+'requested_charge_current','Charge Allowance'),
                    (yam+'requested_discharge_current','Discharge Allowance')]),
                values('BMS & YamBMS Limits',[(raw+'maxchgcurt','Seplos Charge Allowance'),
                    (raw+'maxdiscurt','Seplos Discharge Allowance'),
                    *([( 'sensor.victron_bms_max_charge_current','GX Received Charge Limit'),
                       ('sensor.victron_bms_max_discharge_current','GX Received Discharge Limit')]
                      if 'sensor.victron_bms_max_charge_current' in ids else []),
                    (yam+'ccl_derating_reason','Charge Reduction Reason'),
                    (yam+'dcl_derating_reason','Discharge Reduction Reason')]),
                text('Protection Decision',qual+'operating_limits')]),
            group('Health',[
                values('Live Status',[
                    ('binary_sensor.qualification_complete_data_fresh','Battery Data Fresh'),
                    ('binary_sensor.qualification_metadata_ready','Metadata Ready'),
                    ('binary_sensor.qualification_control_intent_saved','Control Intent Saved'),
                    ('binary_sensor.yambms_gx_trial_inverter_communication_status','Inverter Communication'),
                    (yam+'alarm','Alarm'),(yam+'warning','Warning'),
                    (qual+'cell_spread','Cell Spread'),
                    (yam+'min_temperature','Coldest Sensor'),(yam+'max_temperature','Warmest Sensor')]),
                text('Startup Admission',qual+'startup_admission'),
                *([values('GX Power',[
                    ('sensor.victron_active_input_source','AC Source'),
                    ('sensor.victron_grid_l1_power','Grid Power'),
                    ('sensor.victron_ac_consumption_l1','House Consumption')])]
                  if 'sensor.victron_grid_l1_power' in ids else []),
                text('CAN Link',qual+'can_controller_status')]) ]}
    cells=[(raw+'cell_'+str(i),'Cell '+str(i)) for i in range(1,17)]
    diagnostic={'title':'Cells & Balancing','path':'cells','icon':'mdi:chart-line',
        'type':'sections','max_columns':3,'sections':[
            group('Cell Voltages',[
                tile(qual+'cell_spread','Cell Spread','mdi:arrow-expand-vertical'),
                values('Cells 1-8',cells[:8]),values('Cells 9-16',cells[8:])]),
            group('Cell Trends',[
                history('Cells 1-8',cells[:8]),history('Cells 9-16',cells[8:]),
                history('Cell Spread',[(qual+'cell_spread','Spread')]),
                values('Cell Extremes',[(qual+'lowest_cell_number','Lowest Cell'),
                    (qual+'highest_cell_number','Highest Cell'),
                    (yam+'min_cell_voltage','Minimum Voltage'),(yam+'max_cell_voltage','Maximum Voltage')])]),
            group('Balancing & Protection',[
                text('Seplos Reported Balancing Flags',qual+'reported_balancing_flags'),
                values('Cell Temperature Sensors',[(raw+'cell_temp_'+str(i),'Sensor '+str(i)) for i in range(1,5)]),
                text('Seplos FET Permissions',qual+'documented_fet_diagnostics'),
                text('Seplos Status Flags',qual+'documented_status_diagnostics'),
                values('Adapter Diagnostics',[(qual+'adapter_data_alarm_mask','Adapter Alarm Mask')]
                    if qual+'adapter_data_alarm_mask' in ids else
                    [('sensor.seplos_shadow_adapter_data_alarm_mask','Adapter Alarm Mask'),
                     (qual+'rs485_valid_frames','Valid RS485 Frames')]),
                text('Firmware',qual+'firmware_role')]) ]}
    return {'title':'BatteryOps','views':[overview,diagnostic]}


if __name__=='__main__':
    root=Path(__file__).resolve().parent
    result=build(json.loads((root/'HA_ENTITY_INVENTORY.json').read_text()))
    (root/'HA_BATTERYOPS_DASHBOARD.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Two native monitoring views built; all referenced entities exist')
