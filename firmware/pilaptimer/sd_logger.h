#pragma once

#include <Arduino.h>

bool sd_logger_init();
bool sd_logger_is_ready();

void sd_logger_start_new_session();
uint32_t sd_logger_session_id();

void sd_logger_log_lap(uint8_t driver,
                       uint16_t lap_index,
                       uint32_t lap_time_ms,
                       uint32_t best_lap_ms,
                       uint16_t target_laps);

void sd_logger_log_rt(uint8_t driver,
                      uint32_t reaction_time_ms,
                      uint32_t best_rt_ms);

void sd_logger_loop();
