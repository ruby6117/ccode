#ifndef Conf_H
#define Conf_H

#include <string>

namespace Conf
{
  namespace Sections {
    extern const std::string General;
    extern const std::string Layout;
    extern const std::string Devices;
    extern const std::string Monitor;
  };
  namespace Keys {
    // General Section keys
    extern const std::string listen_port;
    extern const std::string listen_address;
    extern const std::string description;
    extern const std::string connection_timeout_seconds;
    extern const std::string name;
    
    // Devices Section keys
    extern const std::string comediboards;
    extern const std::string rtdaq_tasks;

    // DAQ Device Section keys
    extern const std::string boardspec;
    extern const std::string rate_hz;

    // Other keys
    extern const std::string banks;
    extern const std::string carrier;
    extern const std::string default_flow_total;
    extern const std::string default_odor_flow;
    extern const std::string flow_controllers;
    extern const std::string mixes;
    extern const std::string standalone_flows;
    extern const std::string standalone_meters;
    extern const std::string standalone_sensors;
    extern const std::string valves;
    extern const std::string hard_enable;
    extern const std::string soft_enable;
    extern const std::string flow_controller;
    extern const std::string type;
    extern const std::string mass_range_ml;
    extern const std::string read_range_override;
    extern const std::string write_range_override;
    extern const std::string range_override;
    extern const std::string readchan;
    extern const std::string writechan;
    extern const std::string calib_table;
    extern const std::string curve_fit_coeffs;
    extern const std::string Kp;
    extern const std::string Ki;
    extern const std::string Kd;
    extern const std::string update_rate_hz;
    extern const std::string num_Ki_points;
    extern const std::string range;
    extern const std::string aref;
    extern const std::string dio;
    extern const std::string pwm_valves;
    extern const std::string unit_range;
    extern const std::string units;
    extern const std::string odor_table;
    extern const std::string write_range_clip;

    // Monitor Section keys
    extern const std::string gas_panic_secs;
    extern const std::string gas_panic_actions;
    extern const std::string gas_panic_beeps;

  };


};

#endif
