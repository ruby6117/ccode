#include "Conf.h"

namespace Conf
{
  namespace Sections {
    const std::string General("General");
    const std::string Layout("Layout");
    const std::string Devices("Devices");
    const std::string Monitor("Monitor");
  };
  namespace Keys {
    const std::string listen_port("listen_port");    
    const std::string listen_address("listen_address");    
    const std::string description("description");    
    const std::string connection_timeout_seconds("connection_timeout_seconds");
    const std::string name("name");    
    const std::string comediboards("comediboards");    
    const std::string rtdaq_tasks("rtdaq_tasks");    
    const std::string banks("banks");    
    const std::string carrier("carrier");    
    const std::string default_flow_total("default_flow_total");    
    const std::string default_odor_flow("default_odor_flow");
    const std::string mixes("mixes");    
    const std::string standalone_flows("standalone_flows");
    const std::string standalone_meters("standalone_meters");
    const std::string standalone_sensors("standalone_sensors");
    const std::string valves("valves");    
    const std::string hard_enable("hard_enable");    
    const std::string soft_enable("soft_enable");    
    const std::string flow_controller("flow_controller");    
    const std::string flow_controllers("flow_controllers");    
    const std::string type("type");
    const std::string mass_range_ml("mass_range_ml");
    const std::string read_range_override("read_range_override");    
    const std::string write_range_override("write_range_override");    
    const std::string range_override("range_override");
    const std::string readchan("readchan");    
    const std::string writechan("writechan");    
    const std::string gas_panic_secs("gas_panic_secs");
    const std::string gas_panic_actions("gas_panic_actions");
    const std::string gas_panic_beeps("gas_panic_beeps");
    const std::string calib_table("calib_table");
    const std::string curve_fit_coeffs("curve_fit_coeffs");
    const std::string Kp("Kp");
    const std::string Ki("Ki");
    const std::string Kd("Kd");
    const std::string update_rate_hz("update_rate_hz");
    const std::string num_Ki_points("num_Ki_points");
    const std::string range("range");
    const std::string aref("aref");
    const std::string boardspec("boardspec");
    const std::string rate_hz("rate_hz");
    const std::string dio("dio");
    const std::string pwm_valves("pwm_valves");
    const std::string unit_range("unit_range"); 
    const std::string units("units");
    const std::string odor_table("odor_table");
    const std::string write_range_clip("write_range_clip");    
 };

};
